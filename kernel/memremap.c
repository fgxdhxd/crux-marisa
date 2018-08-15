/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2015 Intel Corporation. All rights reserved. */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kasan.h>
#include <linux/memory_hotplug.h>
#include <linux/mm.h>
#include <linux/pfn_t.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/types.h>
#include <linux/wait_bit.h>
#include <linux/xarray.h>

static DEFINE_XARRAY(pgmap_array);
#define SECTION_MASK ~((1UL << PA_SECTION_SHIFT) - 1)
#define SECTION_SIZE (1UL << PA_SECTION_SHIFT)

#if IS_ENABLED(CONFIG_DEVICE_PRIVATE)
int device_private_entry_fault(struct vm_area_struct *vma,
		       unsigned long addr,
		       swp_entry_t entry,
		       unsigned int flags,
		       pmd_t *pmdp)
{
	struct page *page = device_private_entry_to_page(entry);

	/*
	 * The page_fault() callback must migrate page back to system memory
	 * so that CPU can access it. This might fail for various reasons
	 * (device issue, device was unsafely unplugged, ...). When such
	 * error conditions happen, the callback must return VM_FAULT_SIGBUS.
	 *
	 * Note that because memory cgroup charges are accounted to the device
	 * memory, this should never fail because of memory restrictions (but
	 * allocation of regular system page might still fail because we are
	 * out of memory).
	 *
	 * There is a more in-depth description of what that callback can and
	 * cannot do, in include/linux/memremap.h
	 */
	return page->pgmap->page_fault(vma, addr, page, flags, pmdp);
}
EXPORT_SYMBOL(device_private_entry_fault);
#endif /* CONFIG_DEVICE_PRIVATE */

static void pgmap_array_delete(struct resource *res)
{
	xa_store_range(&pgmap_array, PHYS_PFN(res->start), PHYS_PFN(res->end),
			NULL, GFP_KERNEL);
	synchronize_rcu();
}

static unsigned long pfn_first(struct dev_pagemap *pgmap)
{
	const struct resource *res = &pgmap->res;
	struct vmem_altmap *altmap = &pgmap->altmap;
	unsigned long pfn;

	pfn = res->start >> PAGE_SHIFT;
	if (pgmap->altmap_valid)
		pfn += vmem_altmap_offset(altmap);
	return pfn;
}

static unsigned long pfn_end(struct dev_pagemap *pgmap)
{
	const struct resource *res = &pgmap->res;

	return (res->start + resource_size(res)) >> PAGE_SHIFT;
}

#define for_each_device_pfn(pfn, map) \
	for (pfn = pfn_first(map); pfn < pfn_end(map); pfn++)

static void devm_memremap_pages_release(void *data)
{
	struct dev_pagemap *pgmap = data;
	struct device *dev = pgmap->dev;
	struct resource *res = &pgmap->res;
	unsigned long pfn;

	for_each_device_pfn(pfn, pgmap)
		put_page(pfn_to_page(pfn));

	if (percpu_ref_tryget_live(pgmap->ref)) {
		dev_WARN(dev, "%s: page mapping is still live!\n", __func__);
		percpu_ref_put(pgmap->ref);
	}

	/* pages are dead and unused, undo the arch mapping */
	nid = page_to_nid(pfn_to_page(PHYS_PFN(res->start)));
	
	mem_hotplug_begin();
	arch_remove_memory(res->start, resource_size(res), pgmap->altmap);
	kasan_remove_zero_shadow(__va(res->start), resource_size(res));
	mem_hotplug_done();

	untrack_pfn(NULL, PHYS_PFN(align_start), align_size);
	pgmap_array_delete(res);
	dev_WARN_ONCE(dev, pgmap->altmap.alloc,
		      "%s: failed to free all reserved pages\n", __func__);
}

/**
 * devm_memremap_pages - remap and provide memmap backing for the given resource
 * @dev: hosting device for @res
 * @pgmap: pointer to a struct dev_pgmap
 *
 * Notes:
 * 1/ At a minimum the res, ref and type members of @pgmap must be initialized
 *    by the caller before passing it to this function
 *
 * 2/ The altmap field may optionally be initialized, in which case altmap_valid
 *    must be set to true
 *
 * 3/ pgmap.ref must be 'live' on entry and 'dead' before devm_memunmap_pages()
 *    time (or devm release event). The expected order of events is that ref has
 *    been through percpu_ref_kill() before devm_memremap_pages_release(). The
 *    wait for the completion of all references being dropped and
 *    percpu_ref_exit() must occur after devm_memremap_pages_release().
 *
 * 4/ res is expected to be a host memory range that could feasibly be
 *    treated as a "System RAM" range, i.e. not a device mmio range, but
 *    this is not enforced.
 */
void *devm_memremap_pages(struct device *dev, struct dev_pagemap *pgmap)
{
	struct vmem_altmap *altmap = pgmap->altmap_valid ?
			&pgmap->altmap : NULL;
	struct resource *res = &pgmap->res;
	unsigned long pfn;
	pgprot_t pgprot = PAGE_KERNEL;
	int error, nid, is_ram, i = 0;
	struct dev_pagemap *conflict_pgmap;
	struct resource *res = &pgmap->res;

	conflict_pgmap = get_dev_pagemap(PHYS_PFN(res->start), NULL);
	if (conflict_pgmap) {
		dev_WARN(dev, "Conflicting mapping in same section\n");
		put_dev_pagemap(conflict_pgmap);
		return ERR_PTR(-ENOMEM);
	}

	conflict_pgmap = get_dev_pagemap(PHYS_PFN(res->end), NULL);
	if (conflict_pgmap) {
		dev_WARN(dev, "Conflicting mapping in same section\n");
		put_dev_pagemap(conflict_pgmap);
		return ERR_PTR(-ENOMEM);
	}

	is_ram = region_intersects(res->start, resource_size(res),
		IORESOURCE_SYSTEM_RAM, IORES_DESC_NONE);

	if (is_ram != REGION_DISJOINT) {
		WARN_ONCE(1, "%s attempted on %s region %pr\n", __func__,
				is_ram == REGION_MIXED ? "mixed" : "ram", res);
		return ERR_PTR(-ENXIO);
	}

	if (!pgmap->ref)
		return ERR_PTR(-EINVAL);

	pgmap->dev = dev;

	error = xa_err(xa_store_range(&pgmap_array, PHYS_PFN(res->start),
				PHYS_PFN(res->end), pgmap, GFP_KERNEL));
	if (error)
		goto err_array;

	nid = dev_to_node(dev);
	if (nid < 0)
		nid = numa_mem_id();

	error = track_pfn_remap(NULL, &pgprot, PHYS_PFN(res->start), 0,
			resource_size(res));
	if (error)
		goto err_pfn_remap;

	mem_hotplug_begin();
	error = kasan_add_zero_shadow(__va(res->start), resource_size(res));
	if (error) {
		mem_hotplug_done();
		goto err_kasan;
	}
	error = arch_add_memory(nid, res->start, resource_size(res), altmap, false);
	if (!error)
		move_pfn_range_to_zone(&NODE_DATA(nid)->node_zones[ZONE_DEVICE],
					PHYS_PFN(res->start) >> PAGE_SHIFT,
					resource_size(res) >> PAGE_SHIFT, altmap);
	mem_hotplug_done();
	if (error)
		goto err_add_memory;

	for_each_device_pfn(pfn, pgmap) {
		struct page *page = pfn_to_page(pfn);

		/*
		 * ZONE_DEVICE pages union ->lru with a ->pgmap back
		 * pointer.  It is a bug if a ZONE_DEVICE page is ever
		 * freed or placed on a driver-private list.  Seed the
		 * storage with LIST_POISON* values.
		 */
		list_del(&page->lru);
		page->pgmap = pgmap;
		percpu_ref_get(pgmap->ref);
		if (!(++i % 1024))
			cond_resched();
	}

	devm_add_action(dev, devm_memremap_pages_release, pgmap);

	return __va(res->start);

 err_add_memory:
	kasan_remove_zero_shadow(__va(res->start), resource_size(res));
 err_kasan:
	untrack_pfn(NULL, PHYS_PFN(res->start), resource_size(res));
 err_pfn_remap:
	pgmap_array_delete(res);
 err_array:
	return ERR_PTR(error);
}
EXPORT_SYMBOL_GPL(devm_memremap_pages);

unsigned long vmem_altmap_offset(struct vmem_altmap *altmap)
{
	/* number of pfns from base where pfn_to_page() is valid */
	return altmap->reserve + altmap->free;
}

void vmem_altmap_free(struct vmem_altmap *altmap, unsigned long nr_pfns)
{
	altmap->alloc -= nr_pfns;
}

/**
 * get_dev_pagemap() - take a new live reference on the dev_pagemap for @pfn
 * @pfn: page frame number to lookup page_map
 * @pgmap: optional known pgmap that already has a reference
 *
 * If @pgmap is non-NULL and covers @pfn it will be returned as-is.  If @pgmap
 * is non-NULL but does not cover @pfn the reference to it will be released.
 */
struct dev_pagemap *get_dev_pagemap(unsigned long pfn,
		struct dev_pagemap *pgmap)
{
	resource_size_t phys = PFN_PHYS(pfn);

	/*
	 * In the cached case we're already holding a live reference.
	 */
	if (pgmap) {
		if (phys >= pgmap->res.start && phys <= pgmap->res.end)
			return pgmap;
		put_dev_pagemap(pgmap);
	}

	/* fall back to slow path lookup */
	rcu_read_lock();
	pgmap = xa_load(&pgmap_array, PHYS_PFN(phys));
	if (pgmap && !percpu_ref_tryget_live(pgmap->ref))
		pgmap = NULL;
	rcu_read_unlock();

	return pgmap;
}
#endif /* CONFIG_ZONE_DEVICE */

#if IS_ENABLED(CONFIG_DEVICE_PRIVATE) ||  IS_ENABLED(CONFIG_DEVICE_PUBLIC)
void put_zone_device_private_or_public_page(struct page *page)
{
	int count = page_ref_dec_return(page);

	/*
	 * If refcount is 1 then page is freed and refcount is stable as nobody
	 * holds a reference on the page.
	 */
	if (count == 1) {
		/* Clear Active bit in case of parallel mark_page_accessed */
		__ClearPageActive(page);
		__ClearPageWaiters(page);

		page->mapping = NULL;
		mem_cgroup_uncharge(page);

		page->pgmap->page_free(page, page->pgmap->data);
	} else if (!count)
		__put_page(page);
}
EXPORT_SYMBOL(put_zone_device_private_or_public_page);
#endif /* CONFIG_DEVICE_PRIVATE || CONFIG_DEVICE_PUBLIC */

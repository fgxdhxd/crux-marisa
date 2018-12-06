// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Christoph Hellwig.
 *
 * DMA operations that map physical memory directly without using an IOMMU.
 */
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/dma-direct.h>
#include <linux/scatterlist.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-noncoherent.h>
#include <linux/pfn.h>
#include <linux/set_memory.h>
#include <linux/swiotlb.h>

/*
 * Most architectures use ZONE_DMA for the first 16 Megabytes, but
 * some use it for entirely different regions:
 */
#ifndef ARCH_ZONE_DMA_BITS
#define ARCH_ZONE_DMA_BITS 24
#endif

/*
 * For AMD SEV all DMA must be to unencrypted addresses.
 */
static inline bool force_dma_unencrypted(void)
{
	return sev_active();
}

static bool
check_addr(struct device *dev, dma_addr_t dma_addr, size_t size,
		const char *caller)
{
	if (unlikely(dev && !dma_capable(dev, dma_addr, size))) {
		if (!dev->dma_mask) {
			dev_err(dev,
				"%s: call on device without dma_mask\n",
				caller);
			return false;
		}

		if (*dev->dma_mask >= DMA_BIT_MASK(32)) {
			dev_err(dev,
				"%s: overflow %pad+%zu of device mask %llx\n",
				caller, &dma_addr, size, *dev->dma_mask);
		}
		return false;
	}
	return true;
}

static bool dma_coherent_ok(struct device *dev, phys_addr_t phys, size_t size)
{
	dma_addr_t addr = force_dma_unencrypted() ?
		__phys_to_dma(dev, phys) : phys_to_dma(dev, phys);
	return addr + size - 1 <= dev->coherent_dma_mask;
}

struct page *__dma_direct_alloc_pages(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;
	int page_order = get_order(size);
	struct page *page = NULL;

	/* we always manually zero the memory once we are done: */
	gfp &= ~__GFP_ZERO;

	/* GFP_DMA32 and GFP_DMA are no ops without the corresponding zones: */
	if (dev->coherent_dma_mask <= DMA_BIT_MASK(ARCH_ZONE_DMA_BITS))
		gfp |= GFP_DMA;
	if (dev->coherent_dma_mask <= DMA_BIT_MASK(32) && !(gfp & GFP_DMA))
		gfp |= GFP_DMA32;

again:
	/* CMA can be used only in the context which permits sleeping */
	if (gfpflags_allow_blocking(gfp)) {
		page = dma_alloc_from_contiguous(dev, count, page_order,
						 gfp & __GFP_NOWARN);
		if (page && !dma_coherent_ok(dev, page_to_phys(page), size)) {
			dma_release_from_contiguous(dev, page, count);
			page = NULL;
		}
	}
	if (!page)
		page = alloc_pages_node(dev_to_node(dev), gfp, page_order);

	if (page && !dma_coherent_ok(dev, page_to_phys(page), size)) {
		__free_pages(page, page_order);
		page = NULL;

		if (IS_ENABLED(CONFIG_ZONE_DMA32) &&
		    dev->coherent_dma_mask < DMA_BIT_MASK(64) &&
		    !(gfp & (GFP_DMA32 | GFP_DMA))) {
			gfp |= GFP_DMA32;
			goto again;
		}

		if (IS_ENABLED(CONFIG_ZONE_DMA) &&
		    dev->coherent_dma_mask < DMA_BIT_MASK(32) &&
		    !(gfp & GFP_DMA)) {
			gfp = (gfp & ~GFP_DMA32) | GFP_DMA;
			goto again;
		}
	}

	return page;
}

void *dma_direct_alloc_pages(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs)
{
	struct page *page;
	void *ret;

	page = __dma_direct_alloc_pages(dev, size, dma_handle, gfp, attrs);
	if (!page)
		return NULL;

	if (PageHighMem(page)) {
		/*
		 * Depending on the cma= arguments and per-arch setup
		 * dma_alloc_from_contiguous could return highmem pages.
		 * Without remapping there is no way to return them here,
		 * so log an error and fail.
		 */
		dev_info(dev, "Rejecting highmem page from CMA.\n");
		__dma_direct_free_pages(dev, size, page);
		return NULL;
	}

	ret = page_address(page);
	if (force_dma_unencrypted()) {
		set_memory_decrypted((unsigned long)ret, 1 << get_order(size));
		*dma_handle = __phys_to_dma(dev, page_to_phys(page));
	} else {
		*dma_handle = phys_to_dma(dev, page_to_phys(page));
	}
	memset(ret, 0, size);
	return ret;
}

void __dma_direct_free_pages(struct device *dev, size_t size, struct page *page)
{
	unsigned int count = PAGE_ALIGN(size) >> PAGE_SHIFT;

	if (!dma_release_from_contiguous(dev, page, count))
		__free_pages(page, get_order(size));
}

void dma_direct_free_pages(struct device *dev, size_t size, void *cpu_addr,
		dma_addr_t dma_addr, unsigned long attrs)
{
	unsigned int page_order = get_order(size);

	if (force_dma_unencrypted())
		set_memory_encrypted((unsigned long)cpu_addr, 1 << page_order);
	__dma_direct_free_pages(dev, size, virt_to_page(cpu_addr));
}

void *dma_direct_alloc(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs)
{
	if (!dev_is_dma_coherent(dev))
		return arch_dma_alloc(dev, size, dma_handle, gfp, attrs);
	return dma_direct_alloc_pages(dev, size, dma_handle, gfp, attrs);
}

void dma_direct_free(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_addr, unsigned long attrs)
{
	if (!dev_is_dma_coherent(dev))
		arch_dma_free(dev, size, cpu_addr, dma_addr, attrs);
	else
		dma_direct_free_pages(dev, size, cpu_addr, dma_addr, attrs);
}

#if defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_DEVICE) || \
    defined(CONFIG_SWIOTLB)
void dma_direct_sync_single_for_device(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (unlikely(is_swiotlb_buffer(paddr)))
		swiotlb_tbl_sync_single(dev, paddr, size, dir, SYNC_FOR_DEVICE);

	if (!dev_is_dma_coherent(dev))
		arch_sync_dma_for_device(dev, paddr, size, dir);
}
EXPORT_SYMBOL(dma_direct_sync_single_for_device);

void dma_direct_sync_sg_for_device(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i) {
		if (unlikely(is_swiotlb_buffer(sg_phys(sg))))
			swiotlb_tbl_sync_single(dev, sg_phys(sg), sg->length,
					dir, SYNC_FOR_DEVICE);

		if (!dev_is_dma_coherent(dev))
			arch_sync_dma_for_device(dev, sg_phys(sg), sg->length,
					dir);
	}
}
EXPORT_SYMBOL(dma_direct_sync_sg_for_device);
#endif

#if defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU) || \
    defined(CONFIG_ARCH_HAS_SYNC_DMA_FOR_CPU_ALL) || \
    defined(CONFIG_SWIOTLB)
void dma_direct_sync_single_for_cpu(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (!dev_is_dma_coherent(dev)) {
		arch_sync_dma_for_cpu(dev, paddr, size, dir);
		arch_sync_dma_for_cpu_all(dev);
	}

	if (unlikely(is_swiotlb_buffer(paddr)))
		swiotlb_tbl_sync_single(dev, paddr, size, dir, SYNC_FOR_CPU);
}
EXPORT_SYMBOL(dma_direct_sync_single_for_cpu);

void dma_direct_sync_sg_for_cpu(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i) {
		if (!dev_is_dma_coherent(dev))
			arch_sync_dma_for_cpu(dev, sg_phys(sg), sg->length, dir);
	
		if (unlikely(is_swiotlb_buffer(sg_phys(sg))))
			swiotlb_tbl_sync_single(dev, sg_phys(sg), sg->length, dir,
					SYNC_FOR_CPU);
	}

	if (!dev_is_dma_coherent(dev))
		arch_sync_dma_for_cpu_all(dev);
}
EXPORT_SYMBOL(dma_direct_sync_sg_for_cpu);

void dma_direct_unmap_page(struct device *dev, dma_addr_t addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = dma_to_phys(dev, addr);

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_direct_sync_single_for_cpu(dev, addr, size, dir);

	if (unlikely(is_swiotlb_buffer(phys)))
		swiotlb_tbl_unmap_single(dev, phys, size, dir, attrs);
}
EXPORT_SYMBOL(dma_direct_unmap_page);

void dma_direct_unmap_sg(struct device *dev, struct scatterlist *sgl,
		int nents, enum dma_data_direction dir, unsigned long attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i)
		dma_direct_unmap_page(dev, sg->dma_address, sg_dma_len(sg), dir,
			     attrs);
}
EXPORT_SYMBOL(dma_direct_unmap_sg);
#endif

static inline bool dma_direct_possible(struct device *dev, dma_addr_t dma_addr,
		size_t size)
{
	return swiotlb_force != SWIOTLB_FORCE &&
		(!dev || dma_capable(dev, dma_addr, size));
}

dma_addr_t dma_direct_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	phys_addr_t phys = page_to_phys(page) + offset;
	dma_addr_t dma_addr = phys_to_dma(dev, phys);

	if (unlikely(!dma_direct_possible(dev, dma_addr, size)) &&
	    !swiotlb_map(dev, &phys, &dma_addr, size, dir, attrs)) {
		report_addr(dev, dma_addr, size);
		return DMA_MAPPING_ERROR;
	}

	if (!dev_is_dma_coherent(dev) && !(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		arch_sync_dma_for_device(dev, phys, size, dir);
	return dma_addr;
}
EXPORT_SYMBOL(dma_direct_map_page);

int dma_direct_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, unsigned long attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = dma_direct_map_page(dev, sg_page(sg),
				sg->offset, sg->length, dir, attrs);
		if (sg->dma_address == DMA_MAPPING_ERROR)
			goto out_unmap;
		sg_dma_len(sg) = sg->length;
	}

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		dma_direct_sync_sg_for_device(dev, sgl, nents, dir);
	return nents;

out_unmap:
	dma_direct_unmap_sg(dev, sgl, i, dir, attrs | DMA_ATTR_SKIP_CPU_SYNC);
	return 0;
}
EXPORT_SYMBOL(dma_direct_map_sg);

int dma_direct_supported(struct device *dev, u64 mask)
{
#ifdef CONFIG_ZONE_DMA
	if (mask < DMA_BIT_MASK(ARCH_ZONE_DMA_BITS))
		return 0;
#else
	/*
	 * Because 32-bit DMA masks are so common we expect every architecture
	 * to be able to satisfy them - either by not supporting more physical
	 * memory, or by providing a ZONE_DMA32.  If neither is the case, the
	 * architecture needs to use an IOMMU instead of the direct mapping.
	 */
	if (mask < DMA_BIT_MASK(32))
		return 0;
#endif
	/*
	 * Upstream PCI/PCIe bridges or SoC interconnects may not carry
	 * as many DMA address bits as the device itself supports.
	 */
	if (dev->bus_dma_mask && mask > dev->bus_dma_mask)
		return 0;
	return 1;
}

int dma_direct_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return dma_addr == DIRECT_MAPPING_ERROR;
}

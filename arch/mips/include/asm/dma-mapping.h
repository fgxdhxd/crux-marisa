/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_DMA_MAPPING_H
#define _ASM_DMA_MAPPING_H

#include <linux/scatterlist.h>
#include <asm/dma-coherence.h>
#include <asm/cache.h>

#ifndef CONFIG_SGI_IP27 /* Kludge to fix 2.6.39 build for IP27 */
#include <dma-coherence.h>
#endif

extern const struct dma_map_ops *mips_dma_map_ops;

static inline const struct dma_map_ops *get_arch_dma_ops(struct bus_type *bus)
{
<<<<<<< HEAD
	return mips_dma_map_ops;
=======
#if defined(CONFIG_MACH_JAZZ)
	return &jazz_dma_ops;
#else
	return NULL;
#endif
>>>>>>> bc3ec75de545 (dma-mapping: merge direct and noncoherent ops)
}

<<<<<<< HEAD
<<<<<<< HEAD
static inline bool dma_capable(struct device *dev, dma_addr_t addr, size_t size)
{
	if (!dev->dma_mask)
		return false;

	return addr + size <= *dev->dma_mask;
}

=======
>>>>>>> ea8c64ace866 (dma-mapping: move swiotlb arch helpers to a new header)
static inline void dma_mark_clean(void *addr, size_t size) {}

<<<<<<< HEAD
extern void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
	       enum dma_data_direction direction);

=======
>>>>>>> b49efd762482 (dma-mapping: move dma_mark_clean to dma-direct.h)
=======
>>>>>>> c9eb6172c328 (dma-mapping: turn dma_cache_sync into a dma_map_ops method)
#define arch_setup_dma_ops arch_setup_dma_ops
static inline void arch_setup_dma_ops(struct device *dev, u64 dma_base,
				      u64 size, const struct iommu_ops *iommu,
				      bool coherent)
{
#ifdef CONFIG_DMA_PERDEV_COHERENT
	dev->dma_coherent = coherent;
#endif
}

#endif /* _ASM_DMA_MAPPING_H */

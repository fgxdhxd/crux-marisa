/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_DMA_MAPPING_H
#define _ASM_DMA_MAPPING_H

#include <asm/cache.h>
#include <asm/cacheflush.h>

extern unsigned long __nongprelbss dma_coherent_mem_start;
extern unsigned long __nongprelbss dma_coherent_mem_end;

extern const struct dma_map_ops frv_dma_ops;

static inline const struct dma_map_ops *get_arch_dma_ops(struct bus_type *bus)
{
	return &frv_dma_ops;
}

<<<<<<< HEAD
static inline
void dma_cache_sync(struct device *dev, void *vaddr, size_t size,
		    enum dma_data_direction direction)
{
	flush_write_buffers();
}

=======
>>>>>>> c9eb6172c328 (dma-mapping: turn dma_cache_sync into a dma_map_ops method)
#endif  /* _ASM_DMA_MAPPING_H */

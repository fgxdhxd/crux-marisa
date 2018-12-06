/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ALPHA_DMA_MAPPING_H
#define _ALPHA_DMA_MAPPING_H

extern const struct dma_map_ops *dma_ops;

static inline const struct dma_map_ops *get_arch_dma_ops(struct bus_type *bus)
{
<<<<<<< HEAD
	return dma_ops;
=======
#ifdef CONFIG_ALPHA_JENSEN
	return NULL;
#else
	return &alpha_pci_ops;
#endif
>>>>>>> 356da6d0cde3 (dma-mapping: bypass indirect calls for dma-direct)
}

#endif	/* _ALPHA_DMA_MAPPING_H */

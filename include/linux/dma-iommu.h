/*
 * Copyright (C) 2014-2015 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __DMA_IOMMU_H
#define __DMA_IOMMU_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <asm/errno.h>

#ifdef CONFIG_IOMMU_DMA
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/msi.h>

/* Domain management interface for IOMMU drivers */
void iommu_setup_dma_ops(struct device *dev, u64 dma_base, u64 size);

/* The DMA API isn't _quite_ the whole story, though... */
void iommu_dma_map_msi_msg(int irq, struct msi_msg *msg);
void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list);

#else

struct iommu_domain;
struct msi_msg;
struct device;

static inline void iommu_setup_dma_ops(struct device *dev, u64 dma_base,
		u64 size)
{
}

static inline int iommu_get_dma_cookie(struct iommu_domain *domain)
{
	return -ENODEV;
}

static inline int iommu_get_msi_cookie(struct iommu_domain *domain, dma_addr_t base)
{
	return -ENODEV;
}

static inline void iommu_put_dma_cookie(struct iommu_domain *domain)
{
}

static inline void iommu_dma_map_msi_msg(int irq, struct msi_msg *msg)
{
}

static inline void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list)
{
}

#endif	/* CONFIG_IOMMU_DMA */
#endif	/* __KERNEL__ */
#endif	/* __DMA_IOMMU_H */

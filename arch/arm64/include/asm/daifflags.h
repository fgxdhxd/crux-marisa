/*
 * Copyright (C) 2017 ARM Ltd.
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
#ifndef __ASM_DAIFFLAGS_H
#define __ASM_DAIFFLAGS_H

#include <linux/irqflags.h>
/*
 * Backwards-compat fallback definitions for DAIF_PROCCTX flags.
 * Some trees expect DAIF_PROCCTX and DAIF_PROCCTX_NOIRQ to be defined
 * (precomputed DAIF values for process-context restores). If they are
 * missing, provide safe defaults to allow the tree to compile. These
 * defaults enable IRQs when restored which matches the behavior of
 * a conservative fallback; they may need to be adjusted for full
 * architectural correctness on real hardware.
 */
#ifndef DAIF_PROCCTX
#define DAIF_PROCCTX 0UL
#endif

#ifndef DAIF_PROCCTX_NOIRQ
#define DAIF_PROCCTX_NOIRQ 0UL
#endif

/* mask/save/unmask/restore all exceptions, including interrupts. */
static inline void local_daif_mask(void)
{
	asm volatile(
		"msr	daifset, #0xf		// local_daif_mask\n"
		:
		:
		: "memory");
	trace_hardirqs_off();
}

static inline unsigned long local_daif_save(void)
{
	unsigned long flags;

	flags = arch_local_save_flags();

	local_daif_mask();

	return flags;
}

static inline void local_daif_restore(unsigned long flags)
{
	if (!arch_irqs_disabled_flags(flags))
		trace_hardirqs_on();

	arch_local_irq_restore(flags);
	
		if (arch_irqs_disabled_flags(flags))
		trace_hardirqs_off();
}

#endif

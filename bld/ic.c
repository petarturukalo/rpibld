/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "ic.h"
#include "mmio.h"
#include "timer.h"
#include "sd/cmd.h"
#include "bits.h"

enum arm_local_register {
	IRQ_SOURCE0
};

static struct periph_access arm_local_access = {
	.periph_base_off = 0x3800000,
	.register_offsets = {
		[IRQ_SOURCE0] = 0x60
	}
};

/* IRQ_SOURCE[0-3] register fields. */
#define IRQ_SOURCE_CORE_IRQ  BIT(8)


enum armc_register {
	IRQ0_PENDING0,
	IRQ0_PENDING1,
	IRQ0_PENDING2,
	IRQ0_SET_EN_0,
	IRQ0_SET_EN_1,
	IRQ0_SET_EN_2,
	IRQ0_CLR_EN_0,
	IRQ0_CLR_EN_1,
	SWIRQ_SET
};

static struct periph_access armc_access = {
	.periph_base_off = 0x200b000,
	.register_offsets = {
		[IRQ0_PENDING0] = 0x200,
		[IRQ0_PENDING1] = 0x204,
		[IRQ0_PENDING2] = 0x208,
		[IRQ0_SET_EN_0] = 0x210,
		[IRQ0_SET_EN_1] = 0x214,
		[IRQ0_SET_EN_2] = 0x218,
		[IRQ0_CLR_EN_0] = 0x220,
		[IRQ0_CLR_EN_1] = 0x224,
		[SWIRQ_SET]     = 0x3f0
	}
};

/* IRQ[0-3]_PENDING2 register fields. */
/* Interrupt is a VideoCore interrupt in range 31 to 0. */
#define IRQ_PENDING2_INT31_0   BIT(24)
/* Interrupt is a VideoCore interrupt in range 63 to 32. */
#define IRQ_PENDING2_INT63_32  BIT(25)

/* 
 * Register fields for registers IRQ[0-3]_SET_EN_0, 
 * IRQ[0-3]_CLR_EN_0, IRQ[0-3]_PENDING0.
 */
#define IRQ_INT31_0_TIMER1  BIT(1)
/* 
 * Register fields for registers IRQ[0-3]_SET_EN_1, 
 * IRQ[0-3]_CLR_EN_1, IRQ[0-3]_PENDING1.
 */
#define IRQ_INT63_32_EMMC2  BIT(30)

/* Interrupt request. */
enum irq {
	IRQ_UNIMPLEMENTED,
	/* VideoCore interrupts. */
	IRQ_VC_TIMER1,
	IRQ_VC_EMMC2
};

void ic_enable_interrupts(void)
{
/* Enable VideoCore interrupts: */
	/* Enable system timer channel 1. */
	register_set(&armc_access, IRQ0_SET_EN_0, IRQ_INT31_0_TIMER1);
	/* Enable SD/MMC. */
	register_set(&armc_access, IRQ0_SET_EN_1, IRQ_INT63_32_EMMC2);
}

void ic_disable_interrupts(void)
{
	register_set(&armc_access, IRQ0_CLR_EN_0, IRQ_INT31_0_TIMER1);
	register_set(&armc_access, IRQ0_CLR_EN_1, IRQ_INT63_32_EMMC2);
}

static enum irq get_irq_source(void)
{
	/* 
	 * Refer 'Figure 9. Legacy IRQ status registers' from section '6.4 Legacy 
	 * interrupt controller' of the BCM2711 datasheet for the logic here.
	 */
	if (register_get(&arm_local_access, IRQ_SOURCE0)&IRQ_SOURCE_CORE_IRQ) {
		uint32_t pending2 = register_get(&armc_access, IRQ0_PENDING2);

		if (pending2&IRQ_PENDING2_INT31_0) {
			if (register_get(&armc_access, IRQ0_PENDING0)&IRQ_INT31_0_TIMER1) 
				return IRQ_VC_TIMER1;
		} else if (pending2&IRQ_PENDING2_INT63_32) {
			if (register_get(&armc_access, IRQ0_PENDING1)&IRQ_INT63_32_EMMC2) 
				return IRQ_VC_EMMC2;
		}
	}
	return IRQ_UNIMPLEMENTED;
}

void ic_irq_exception_handler(void)
{
	enum irq irq = get_irq_source();

	switch (irq) {
		case IRQ_VC_TIMER1:
			timer_isr();
			break;
		case IRQ_VC_EMMC2:
			/* 
			 * Polling used instead. 
			 * See comments in sd_enable_interrupts() for why. 
			 */
			break;
		default:
		case IRQ_UNIMPLEMENTED:
			break;
	}
}

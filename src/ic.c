#include "ic.h"
#include "mmio.h"
#include "timer.h"
#include "int.h"

enum arm_local_registers {
	IRQ_SOURCE0
};

struct periph_access arm_local_access = {
	.periph_base_off = 0x3800000,
	.register_offsets = {
		[IRQ_SOURCE0] = 0x60
	}
};

enum armc_registers {
	IRQ0_PENDING0,
	IRQ0_PENDING1,
	IRQ0_PENDING2,
	IRQ0_SET_EN_0,
	IRQ0_SET_EN_2,
	SWIRQ_SET
};

struct periph_access armc_access = {
	.periph_base_off = 0x200b000,
	.register_offsets = {
		[IRQ0_PENDING0] = 0x200,
		[IRQ0_PENDING1] = 0x204,
		[IRQ0_PENDING2] = 0x208,
		[IRQ0_SET_EN_0] = 0x210,
		[IRQ0_SET_EN_2] = 0x218,
		[SWIRQ_SET]     = 0x3f0
	}
};

void ic_enable_interrupts(void)
{
	/* Enable system timer channel 1. */
	register_set(&armc_access, IRQ0_SET_EN_0, 1<<1);
	/* Enable software-triggered interrupt 0. */
	// TODO #ifdef DEBUG?
	/*register_set(&armc_access, IRQ0_SET_EN_2, 1<<8);*/
}

static enum irq get_irq_source(void)
{
	/* 
	 * Refer 'Figure 9. Legacy IRQ status registers' from section '6.4 Legacy 
	 * interrupt controller' of the BCM2711 datasheet for the logic here.
	 */
	if (register_get(&arm_local_access, IRQ_SOURCE0)&1<<8) {
		if (register_get(&armc_access, IRQ0_PENDING2)&1<<24) {
			if (register_get(&armc_access, IRQ0_PENDING0)&1<<1) 
				return IRQ_VC_TIMER1;
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
		default:
		case IRQ_UNIMPLEMENTED:
			break;
	}
}

void trigger_software_interrupt_0(void)
{
	register_set(&armc_access, SWIRQ_SET, 1);
}


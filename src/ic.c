#include "ic.h"
#include "mmio.h"
#include "timer.h"
#include "int.h"
#include "led.h" // TODO rm
#include "mmc.h"

enum arm_local_registers {
	IRQ_SOURCE0
};

static struct periph_access arm_local_access = {
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
	IRQ0_SET_EN_1,
	IRQ0_SET_EN_2,
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
		[SWIRQ_SET]     = 0x3f0
	}
};

// TODO have the peripheral register its interrupt in its init code? maybe not
void ic_enable_interrupts(void)
{
/* Enable VideoCore interrupts: */
	/* Enable system timer channel 1. */
	register_set(&armc_access, IRQ0_SET_EN_0, 1<<1);
	// TODO do i want SDHOST VC interrupt 56 or EMMC VC interrupt 62
	// (enable both for now, but it's probably 62)
	/* Enable MMC. */
	// TODO know it's write-set but try register_enable_bits()
	register_set(&armc_access, IRQ0_SET_EN_1, 1<<24);
	register_set(&armc_access, IRQ0_SET_EN_1, 1<<30);
}

static enum irq get_irq_source(void)
{
	/* 
	 * Refer 'Figure 9. Legacy IRQ status registers' from section '6.4 Legacy 
	 * interrupt controller' of the BCM2711 datasheet for the logic here.
	 */
	if (register_get(&arm_local_access, IRQ_SOURCE0)&1<<8) {
		word_t pending2 = register_get(&armc_access, IRQ0_PENDING2);

		// TODO split this into handling ARMC interrupts in PENDING 2 and
		// VC interrupts in PENDING0/1
		if (pending2&1<<24) {
			if (register_get(&armc_access, IRQ0_PENDING0)&1<<1) 
				return IRQ_VC_TIMER1;
		} else if (pending2&1<<25) {
			word_t pending1 = register_get(&armc_access, IRQ0_PENDING1);
		
			// TODO name these shifts duplicated here and in ic_enable_interrupts()
			if (pending1&1<<24)
				return IRQ_VC_SDHOST;
			if (pending1&1<<30) 
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
		case IRQ_VC_SDHOST:
			/* TODO get rid of this interrupt if find it never gets triggered. */
			led_init();
			led_turn_on();
			break;
		case IRQ_VC_EMMC2:
			mmc_dummy_isr();
			break;
		default:
		case IRQ_UNIMPLEMENTED:
			break;
	}
}

#include "mmc.h"
#include "mmio.h"
#include "debug.h"  // TODO rm
#include "timer.h"  // TODO can keep this?
#include "error.h"  // TODO rm?

/* TODO explain where these are from? */
enum mmc_registers {
	CMDTM,
	DATA,
	STATUS,
	CONTROL0,
	CONTROL1,
	INTERRUPT,
	IRPT_MASK,
	IRPT_EN,
	FORCE_IRPT
};

static struct periph_access mmc_access = {
	// TODO explain where get this from after choose the correct one.
	// TODO delete unused after get a response from mmc
	// 0x7e30,0000 (addr of mmc base from bcm2835) sub bcm2835 main periph base
	/*.periph_base_off = 0x300000,  */
	// 0x7e30,0000 (addr of dt mmc node) sub bcm2711 main periph base
	/*.periph_base_off = 0x2300000, */
	// 0x7e20,2000 (addr of dt mmc sdhost node) sub bcm2711 main periph base
	/*.periph_base_off = 0x2202000,     */
	// 0x7e34,0000 (addr of dt emmc2) sub bcm2711 main periph base
	// TODO seems like this offset is correct because it's the only one that shows the more
	// (almost most) significant bits of the STATUS reg being reset to 1
	.periph_base_off = 0x2340000,
	.register_offsets = {
		[CMDTM]      = 0x0c,
		[DATA]	     = 0x20,
		[STATUS]     = 0x24,
		[CONTROL0]   = 0x28,
		[CONTROL1]   = 0x2c,
		[INTERRUPT]  = 0x30,
		[IRPT_MASK]  = 0x34,
		[IRPT_EN]    = 0x38,
		[FORCE_IRPT] = 0x50
	}
};

void mmc_init(void)
{
	// TODO apparently need to set up clock before can receive an interrupt
	// (follow instructions in 3.2 SD clock control)
	// TODO explain where got these steps from after get it working
	// TODO is this supplying to the card and should only be done if there's a card present?
	// if so should only be setting clock and power for card if can detect card is connected.
	// can detect whether card connected using a GPIO pin? NEED TO EXPLAIN THAT THIS IS
	// ACTUALLY SUPPLYING CLOCK TO THE SD CARD

	/* Use 4 data lines. TODO in correct spot? */
	/*register_enable_bits(&mmc_access, CONTROL0, 1);*/

	/*
	 * The core clock of this MMC controller is the EMMC2 clock, which has a 
	 * 100 MHz clock rate (the clock is accessible through the VC mailbox).
	 * By default this will be operating in the "default speed" bus speed mode, which has
	 * a maximum clock rate of 25 MHz. A clock divider of 4 is used to reach this
	 * 25 MHz clock rate.
	 *
	 * The host controller uses a 10-bit divided clock mode. A value of N defined across the
	 * fields that make up the 10 bits implies the base clock is divided by 2N, so to achieve
	 * a clock divider of 4, N is set 2 (0b10).
	 * TODO explain why it's operating in default speed mode (assertions on GPIO expander
	 * values: 100 MHz clock rate, 3.3V, power on, etc.)
	 * TODO calculate SDCLK Frequency Select by actually reading EMMC2 clock value? or just signal
	 *	error if assertions on how VC firmware left things aren't correct
	 */
	register_enable_bits(&mmc_access, CONTROL1, 0b10<<8);
	/* Enable internal clock. */
	register_enable_bits(&mmc_access, CONTROL1, 1);
	/* 
	 * Wait for internal clock to become stable. From testing this only takes 
	 * around 5 iterations, so don't bother sleeping. 
	 */
	while (!(register_get(&mmc_access, CONTROL1)&1<<1)) 
		;
	/* Enable clock. */
	register_enable_bits(&mmc_access, CONTROL1, 1<<2);
	// TODO still can't trigger interrupt after setting up the clock

	/* Enable TODO X interrupt. */
	// TODO use register_enable_bits() instead?
	register_set(&mmc_access, IRPT_MASK, 1<<16);
	register_set(&mmc_access, IRPT_EN, 1<<16);
}

void mmc_trigger_dummy_interrupt(void)
{
	/* Trigger TODO x interrupt. */
	register_set(&mmc_access, FORCE_IRPT, 1<<16);
}

#include "led.h"//TODO rm

void mmc_dummy_isr(void)
{
	/* Clear TODO x interrupt. */
	register_set(&mmc_access, INTERRUPT, 1<<16);

	led_init();
	led_turn_on();
}

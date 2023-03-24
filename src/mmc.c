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
	/* Set clock base divider to 1 and enable internal clock. */
	register_set(&mmc_access, CONTROL1, 1<<8|1);
	/* 
	 * Wait for internal clock to become stable. From testing this only takes 
	 * around 5 iterations, so don't bother sleeping. 
	 */
	while (!(register_get(&mmc_access, CONTROL1)&1<<1)) 
		;
	/* Enable clock. */
	register_set(&mmc_access, CONTROL1, 1<<2);
	// TODO still can't trigger interrupt after setting up the clock
	// TODO need to give a better clock divider or something?
	// TODO try triggering interrupt manually by removing card
	// TODO need power and clock to do anything with the card?

	// TODO set up
	// - bus power 3.3?
	// - chg bus width 3.4?
	// - then card init 3.6?

	/* Enable CARD interrupt. */
	register_set(&mmc_access, IRPT_MASK, 1<<8);
	register_set(&mmc_access, IRPT_EN, 1<<8);
}

void mmc_trigger_dummy_interrupt(void)
{
	/* Trigger CARD interrupt. */
	register_set(&mmc_access, FORCE_IRPT, 1<<8);
}

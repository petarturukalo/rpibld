#include "mmc.h"
#include "mmio.h"
#include "tag.h"
#include "debug.h"  // TODO rm
#include "timer.h"  // TODO can keep this?
#include "error.h"  // TODO rm?

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
	/* 
	 * This offset comes from the BCM2711 RPI 4 B device tree 
	 * EMMC2 node at /emmc2bus/mmc. 
	 */
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

/* 100 MHz. */
#define EMMC2_EXPECTED_BASE_CLOCK_HZ 100000000

/*
 * Assert that the EMMC2 base clock has a clock rate of EMMC2_EXPECTED_BASE_CLOCK_HZ.
 */
static void mmc_assert_base_clock(void)
{
	struct clock_state state;
	int rate;

	state = tag_clock_get_state(CLK_EMMC2);
	if (state.clk_not_exists || !state.on)
		signal_error(ERROR_VC_NOT_INIT_MMC);
	rate = tag_clock_get_rate(CLK_EMMC2);
	if (rate != EMMC2_EXPECTED_BASE_CLOCK_HZ)
		signal_error(ERROR_VC_NOT_INIT_MMC);
}

/*
 * Assert that the supply for the bus IO line power is 3.3V.
 */
static void mmc_assert_voltage(void)
{
	struct gpio_expander_pin_config cfg;
	int state;

	cfg = tag_gpio_get_config(GPIO_EXPANDER_VDD_SD_IO_SEL);
	if (!gpio_config_pin_is_output(&cfg) || !gpio_config_pin_is_active_high(&cfg))
		signal_error(ERROR_VC_NOT_INIT_MMC);
	state = tag_gpio_get_state(GPIO_EXPANDER_VDD_SD_IO_SEL);
	/* From the BCM2711 RPI 4 B device tree 0 is 3.3V, 1 is 1.8V. */
	if (state) 
		signal_error(ERROR_VC_NOT_INIT_MMC);
}

/*
 * Assert that the card is supplied power.
 */
static void mmc_assert_card_power(void)
{	
	struct gpio_expander_pin_config cfg;
	int state;

	cfg = tag_gpio_get_config(GPIO_EXPANDER_SD_PWR_ON);
	if (!gpio_config_pin_is_output(&cfg) || !gpio_config_pin_is_active_high(&cfg))
		signal_error(ERROR_VC_NOT_INIT_MMC);
	state = tag_gpio_get_state(GPIO_EXPANDER_SD_PWR_ON);
	if (!state) 
		signal_error(ERROR_VC_NOT_INIT_MMC);
}

/*
 * Assert that the VideoCore firmware which loaded this bootloader program set up
 * the MMC controller as expected.
 *
 * Signal error with error ERROR_VC_NOT_INIT_MMC if the assertion fails.
 */
static void mmc_assert_vc_init(void)
{
	mmc_assert_base_clock();
	mmc_assert_voltage();
	mmc_assert_card_power();
}

/*
 * Supply the clock to the card. Given that this is called after mmc_assert_vc_init()
 * it can safely be assumed that the base clock is 100 MHz and that 3.3V power is being
 * supplied. 
 *
 * The only two bus speed modes supported at 3.3V are default speed and high speed. Default
 * speed has a max clock speed of 25 MHz, and high speed 50 MHz. High speed mode requires
 * setting a register field to use; because the register fields relevant to bus speed modes
 * are reset to 0x0 on boot, default speed can be assumed, and so the base 100 MHz clock rate 
 * is divided to get a 25 MHz rate.
 */
static void mmc_supply_clock(void)
{
	/*
	 * The host controller uses a 10-bit divided clock mode. A value of N defined across the
	 * fields that make up the 10 bits results in the base clock being divided by 2N, so to 
	 * achieve a clock divider of 4, N is set to 2 (0b10).
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
}

void mmc_init(void)
{
	mmc_assert_vc_init();
	mmc_supply_clock();
	/* Now in default speed mode. */
	// TODO detect whether card is present (but can't?). maybe wait for timeout eventually instead.

	/* 
	 * Use 4 data lines. 
	 * TODO in correct spot, can defualt speed only operate with 4 data lines? 
	 */
	/*register_enable_bits(&mmc_access, CONTROL0, 1);*/

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

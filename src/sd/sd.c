/*
 * This source implements the following standards.
 * - SD Specifications Part 1 Physical Layer Specification Version 3.01
 * - SD Specifications Part A2 SD Host Controller Specification Version 3.00
 */
#include "sd.h"
#include "../mmio.h"
#include "../tag.h"
#include "../error.h"
#include "../help.h"
#include "reg.h"
#include "cmd.h"
#include "../led.h"//TODO rm
#include "../debug.h"//TODO rm

/* 100 MHz. */
#define EMMC2_EXPECTED_BASE_CLOCK_HZ 100000000
/* 
 * 400 KHz, the clock frequency a card operates in after power-up
 * and during the card identification process.
 */
#define IDENTIFICATION_CLOCK_RATE_HZ	400000


enum card_state {
/* Inactive operation mode. */
	CARD_STATE_INACTIVE,
/* Card identification operation mode. */
	CARD_STATE_IDLE,
	CARD_STATE_READY,
	CARD_STATE_IDENTIFICATION,
/* Data transfer operation mode. */
	CARD_STATE_STANDBY,
	CARD_STATE_TRANFSFER,
// TODO if not going to use below states don't worry about setting states at all?
	CARD_STATE_SENDING_DATA,
	CARD_STATE_RECEIVE_DATA,
	CARD_STATE_PROGRAMMING,
	CARD_STATE_DISCONNECT
};

enum operation_mode {
	OPMODE_INACTIVE,
	OPMODE_CARD_IDENTIFICATION,
	OPMODE_DATA_TRANSFER
};

// TODO delete if unused (and other unused stuff)
enum operation_mode card_state_opmode(enum card_state state)
{
	switch (state) {
		case CARD_STATE_INACTIVE:
			return OPMODE_INACTIVE;
		case CARD_STATE_IDLE:
		case CARD_STATE_READY:
		case CARD_STATE_IDENTIFICATION:
			return OPMODE_CARD_IDENTIFICATION;
		case CARD_STATE_STANDBY:
		case CARD_STATE_TRANFSFER:
		case CARD_STATE_SENDING_DATA:
		case CARD_STATE_RECEIVE_DATA:
		case CARD_STATE_PROGRAMMING:
		case CARD_STATE_DISCONNECT:
			return OPMODE_DATA_TRANSFER;
	}
}

/*
 * Card (SD card) metadata and bookkeeping data.
 *
 * @state: the card's current state
 * @version_2_or_later: whether the card supports the physical layer
 *	specification version 2.00 or later. For reference SDSC (or just
 *	SD) was defined in version 2, SDHC in version 2, and SDXC in version 3.
 * @sdhc_or_sdxc: whether the card is either of SDHC (high capacity) or SDXC
 *	(extended capacity). If false the card is SDSC (standard capacity).
 * @rca: card's relative card address
 */
struct card {
	enum card_state state;
	bool version_2_or_later;
	bool sdhc_or_sdxc;
	int rca;
};


/*
 * Assert that the EMMC2 base clock has a clock rate of EMMC2_EXPECTED_BASE_CLOCK_HZ.
 */
static void sd_assert_base_clock(void)
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
 * Assert that the supply for the bus IO line power is 3.3V (the expected voltage
 * after a power cycle).
 */
static void sd_assert_voltage(void)
{
	struct gpio_expander_pin_config cfg;
	int state;

	cfg = tag_gpio_get_config(GPIO_EXPANDER_VDD_SD_IO_SEL);
	if (!gpio_config_pin_is_output(&cfg) || !gpio_config_pin_is_active_high(&cfg))
		signal_error(ERROR_VC_NOT_INIT_MMC);
	state = tag_gpio_get_state(GPIO_EXPANDER_VDD_SD_IO_SEL);
	if (state) 
		signal_error(ERROR_VC_NOT_INIT_MMC);
}

/*
 * Assert that the card is supplied power. 
 */
static void sd_assert_card_power(void)
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

static void sd_assert_vc_init(void)
{
	sd_assert_base_clock();
	sd_assert_voltage();
	sd_assert_card_power();
}

/*
 * Reset the entire host controller. Clears register bits, so needs to be
 * called before any other initialisation. 
 * TODO don't need this?
 */
static void sd_reset_host(void)
{
	register_enable_bits(&sd_access, CONTROL1, 1<<24);
	while (register_get(&sd_access, CONTROL1)&1<<24)
		;
}

/*
 * Supply 3.3V SD bus power.
 *
 * This was the final (undocumented, and so took forever to find) piece 
 * to the SD puzzle, required to be able to receive a command complete
 * interrupt after issuing a command.
 */
static void sd_supply_bus_power(void)
{
	/*
	 * The BCM2835 datasheet lists these bits as reserved, but from the
	 * SD Host Controller spec they make up the power control register.
	 * TODO use defines or struct so know what the the 0xf doing:
	 * selecting 3.3V and turning power on
	 */
	register_enable_bits(&sd_access, CONTROL0, 0xf<<8);
}

/*
 * Get the 8-bit clock divider which when used in the clock control register
 * SDCLK frequency select field results in the base clock being divided to
 * a clock rate <= the target clock rate.
 */
static int sd_8bit_clock_divider(int base_rate, int target_rate)
{
	int divisor = 1;

	/* 
	 * A clock divider of N divides the base clock rate by 2N. In 8-bit
	 * mode N must be a power of 2 (only 1 bit can be set). Here divisor
	 * is 2N: after the loop it's halved to return the clock divider N.
	 */
	while (base_rate/divisor > target_rate) 
		divisor <<= 1;
	return divisor >>= 1;
}

/*
 * Supply the clock at the given clock rate to the card.
 */
static void sd_supply_clock(int clock_rate)
{
	// TODO explain why using 8-bit clock divider (if even needed)
	int clock_divider = sd_8bit_clock_divider(EMMC2_EXPECTED_BASE_CLOCK_HZ, clock_rate);

	/* Set clock divider and enable internal clock. */
	register_enable_bits(&sd_access, CONTROL1, clock_divider<<8|1);
	/* 
	 * Wait for internal clock to become stable. From testing this only takes 
	 * around 5 iterations, so don't bother sleeping. 
	 */
	while (!(register_get(&sd_access, CONTROL1)&1<<1)) 
		;
	/* Enable clock. */
	register_enable_bits(&sd_access, CONTROL1, 1<<2);
}

/*
 * Supply the clock to the card. Given that this is called after sd_assert_vc_init()
 * it can safely be assumed that the base clock is 100 MHz and that 3.3V power is being
 * supplied. 
 *
 * The only two bus speed modes supported at 3.3V are default speed and high speed. Default
 * speed has a max clock speed of 25 MHz, and high speed 50 MHz. High speed mode requires
 * setting a register field to use; because the register fields relevant to bus speed modes
 * are reset to 0x0 on boot, default speed can be assumed, and so the base 100 MHz clock rate 
 * is divided to get a 25 MHz rate.
 */
static void sd_supply_clock_tmp(void)
{
	// TODO don't use this. delete after sd_supply_clock() called with 25 MHz so can move
	// these comments there
	/*
	 * The host controller uses a 10-bit divided clock mode. A value of N defined across the
	 * fields that make up the 10 bits results in the base clock being divided by 2N. The base
	 * clock is 100 MHz but want 25 MHz, so set the clock divider to 4 by setting N to 2 (0b10).
	 */
	register_enable_bits(&sd_access, CONTROL1, 0b10<<8);
	/* Enable internal clock. */
	register_enable_bits(&sd_access, CONTROL1, 1);
	/* 
	 * Wait for internal clock to become stable. From testing this only takes 
	 * around 5 iterations, so don't bother sleeping. 
	 */
	while (!(register_get(&sd_access, CONTROL1)&1<<1)) 
		;
	/* Enable clock. */
	register_enable_bits(&sd_access, CONTROL1, 1<<2);
}

static void sd_enable_interrupts(void)
{
	struct interrupt irpt;

	mzero(&irpt, sizeof(irpt));
	irpt.cmd_complete = 1;
	irpt.cmd_timeout_error = 1;
	// TODO need CMD59 to even use CRC?
	irpt.cmd_crc_error = 1;
	irpt.cmd_end_bit_error = 1;
	irpt.cmd_index_error = 1;

	/* Enable triggered interrupts to be flagged in the INTERRUPT register. */
	register_set_ptr(&sd_access, IRPT_MASK, &irpt);
	/* Enable interrupt generation. */
	register_set_ptr(&sd_access, IRPT_EN, &irpt);
}

/*
 * Do all of the intialisation needed to issue a command successfully.
 */
static void sd_pre_cmd_init(void)
{
	sd_assert_vc_init();
	sd_reset_host();
	sd_supply_bus_power();
	/* 
	 * TODO because some cards may have operating frequency restrictions during card identification
	 * mode (pg 39/27)
	 */
	sd_supply_clock(IDENTIFICATION_CLOCK_RATE_HZ);
	sd_enable_interrupts();
}

enum sd_error sd_init(void)
{
	enum cmd_error error;
	struct card card;
	bool ccs, cmd8_response;

	mzero(&card, sizeof(card));
	card.state = CARD_STATE_IDLE;

	sd_pre_cmd_init();
	/* TODO mention in 1-bit bus width <= 400 KHz mode? */
	/* TODO mention below is card initialisation and identification process? */

	/* Command is issued assuming card supports 3.3V */
	error = sd_issue_cmd(CMD_IDX_GO_IDLE_STATE, 0);
	if (error != CMD_ERROR_NONE) 
		return SD_ERROR_ISSUE_CMD;

	/* Verify card supports 3.3V */
	error = sd_issue_cmd8();
	if (error == CMD_ERROR_RESPONSE_CONTENTS)
		return SD_ERROR_UNUSABLE_CARD;
	if (error != CMD_ERROR_NONE && error != CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT) 
		return SD_ERROR_ISSUE_CMD;
	/* 
	 * CMD8 was defined in phys layer version 2: only a version >= 2 card will respond.
	 * No response is indicated by CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT.
	 * TODO is CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT the correct way to test for no response?
	 */
	cmd8_response = error == CMD_ERROR_NONE;
	card.version_2_or_later = cmd8_response;

	/* Power up card. */
	error = sd_issue_acmd41(cmd8_response, &ccs);
	if (error == CMD_ERROR_RESPONSE_CONTENTS || error == CMD_ERROR_GENERAL_TIMEOUT) 
		return SD_ERROR_UNUSABLE_CARD;
	if (error != CMD_ERROR_NONE) 
		return SD_ERROR_ISSUE_CMD;
	card.sdhc_or_sdxc = card.version_2_or_later && ccs;

	card.state = CARD_STATE_READY;

	/* Issue CMD2. */
	error = sd_issue_cmd(CMD_IDX_ALL_SEND_CID, 0);
	if (error != CMD_ERROR_NONE) 
		return SD_ERROR_ISSUE_CMD;

	card.state = CARD_STATE_IDENTIFICATION;

	error = sd_issue_cmd3(&card.rca);
	if (error != CMD_ERROR_NONE) 
		return SD_ERROR_ISSUE_CMD;

	card.state = CARD_STATE_STANDBY;

	signal_error(2);
}


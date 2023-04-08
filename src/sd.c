/*
 * This source implements the following standards.
 * - SD Specifications Part 1 Physical Layer Specification Version 3.01
 * - SD Specifications Part A2 SD Host Controller Specification Version 3.00
 */
#include "sd.h"
#include "mmio.h"
#include "tag.h"
#include "help.h"
#include "debug.h"  // TODO rm
#include "timer.h"  // TODO can keep this?
#include "error.h"  // TODO rm?
#include "led.h"//TODO rm

enum sd_registers {
	ARG1,
	CMDTM,
	RESP0,
	DATA,
	STATUS,
	CONTROL0,
	CONTROL1,
	INTERRUPT,
	IRPT_MASK,
	IRPT_EN,
	FORCE_IRPT
};

static struct periph_access sd_access = {
	/* 
	 * This offset comes from the BCM2711 RPI 4 B device tree 
	 * EMMC2 node at /emmc2bus/mmc. 
	 */
	.periph_base_off = 0x2340000,
	.register_offsets = {
		[ARG1]       = 0x08,
		[CMDTM]      = 0x0c,
		[RESP0]      = 0x10,
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

/* Format of registers INTERRUPT, IRPT_MASK, IRPT_EN, FORCE_IRPT. */
struct reg_interrupt {
	bits_t command_complete : 1;
	bits_t transfer_complete : 1;
	bits_t block_gap : 1;
	bits_t reserved1 : 1;
	bits_t write_ready : 1;
	bits_t read_ready : 1;
	bits_t reserved2 : 2;
	bits_t card : 1;
	bits_t reserved3 : 3;
	bits_t retune : 1;
	bits_t bootack : 1;
	bits_t endboot : 1;
/* Errors below. */
	/* The "error" field only applies to the INTERRUPT register. */
	bits_t error : 1;  
	bits_t command_timeout_error : 1;
	bits_t command_crc_error : 1;
	bits_t command_end_bit_error : 1;
	bits_t command_index_error : 1;
	bits_t data_timeout_error : 1;
	bits_t data_crc_error : 1;
	bits_t data_end_bit_error : 1;
	bits_t reserved4 : 1;
	bits_t auto_cmd_error : 1;
	bits_t reserved5 : 7;
} __attribute__((packed));


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
 */
struct card {
	enum card_state state;
	bool version_2_or_later;
	bool sdhc_or_sdxc;
};


// TODO put this in mmc/sd dir so can separate into multiple files?
/*
 * Index identifier for a command. 
 * TODO make this explanation better or put it in struct command?
 */
enum command_index {
	CMD_IDX_GO_IDLE_STATE = 0,
	CMD_IDX_SEND_IF_COND  = 8  /* Send interface condition. */
};

/*
 * @CMD_TYPE_BC: broadcast: broadcast the command to all cards. 
 *	Doesn't expect a response.
 * @CMD_TYPE_BCR: broadcast response: broadcast the command to all cards.
 *	 Does expect a response.
 * @CMD_TYPE_AC: addressed command: send a command to the addressed card. 
 *	No data transfer.
 * @CMD_TYPE_ADTC: addressed data transfer command: send a command to the 
 *	addressed card. Transfers data.
 */
enum command_type {
	CMD_TYPE_BC,
	CMD_TYPE_BCR,
	CMD_TYPE_AC,
	CMD_TYPE_ADTC
};

/*
 * A response to a command.
 * TODO define the struct that pairs to this as well for interpreting the return?
 * TODO explain what these actually are?
 */
enum command_response {
	// TODO if not expecting response driver needs to clear cmd inhibit bit if cmd is successful?
	CMD_RESP_NONE,
	CMD_RESP_NORMAL,
	CMD_RESP_NORMAL_BUSY,
	CMD_RESP_CID_OR_CSD_REG,
	CMD_RESP_OCR_REG,
	CMD_RESP_PUBLISHED_RCA,
	CMD_RESP_CARD_INTERFACE_CONDITION,
};

// TODO define the enum's that are only used from within this struct, in the struct itself?
/*
 * A command to control the SD card.
 */
struct command {
	enum command_index index;
	enum command_type type;
	enum command_response response;
};

struct command commands[] = {
	{ CMD_IDX_GO_IDLE_STATE, CMD_TYPE_BC,  CMD_RESP_NONE },
	{ CMD_IDX_SEND_IF_COND,  CMD_TYPE_BCR, CMD_RESP_CARD_INTERFACE_CONDITION },
	{ 0 }
};

static struct command *get_command(enum command_index idx)
{
	int n = array_len(commands);
	struct command *cmd = commands;

	for (int i = 0; i < n; ++i, ++cmd) {
		if (cmd->index == idx)
			return cmd;
	}
	return NULL;
}

/*
 * Issue a command to the SD card.
 * @args: arguments optionally used by the command
 */
static void sd_issue_command(enum command_index idx, uint32_t args)
{
	struct command *cmd;
	// TODO document what this is (fmt of CMDTM reg)
	struct {
		bits_t todo1 : 16;  /* TODO expand this to the actual fields */
		bits_t cmd_rspns_type : 2;
		bits_t reserved1 : 1;
		bits_t cmd_crcchk_en : 1;
		bits_t cmd_ixchk_en : 1;
		bits_t todo2 : 3; /* TODO expand this to the actual fields. */
		bits_t cmd_index : 6;
		bits_t reserved2 : 2;
	/* TODO where should attribute be? */
	} __attribute__((packed)) cmdtm;

	/* TODO should check CMD_INHIBIT field? */

	/* TODO need to use ARG2 if it's ACMD23 */
	// TODO mention setting args
	register_set(&sd_access, ARG1, args);

	cmd = get_command(idx);
	// TODO error return command unimplemented
	/*if (!cmd) {*/
	/*}*/
	mzero(&cmdtm, sizeof(cmdtm));
	cmdtm.cmd_index = idx;
	// TODO don't do it like this (just temporary)
	if (cmd->response == CMD_RESP_CARD_INTERFACE_CONDITION) {
		// TODO see sdhost spec 'Command Register'.'Response Type Select'
		cmdtm.cmd_rspns_type = 0b10;
		cmdtm.cmd_crcchk_en = true;
		cmdtm.cmd_ixchk_en = true;
	}

	register_set(&sd_access, CMDTM, cast_bitfields(cmdtm, uint32_t));
// TODO if not expecting a response from a command still need to wait for the data
// transfer to finish (e.g. see phys spec 4.7.2)
// TODO does interrupt 0 CMD_DONE apply to all commands, even those that don't expect a response?
}

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
	struct reg_interrupt irpt;

	mzero(&irpt, sizeof(irpt));
	irpt.command_complete = 1;
	irpt.command_timeout_error = 1;
	// TODO need CMD59 to even use CRC?
	irpt.command_crc_error = 1;
	irpt.command_end_bit_error = 1;
	irpt.command_index_error = 1;

	/* Enable triggered interrupts to be flagged in the INTERRUPT register. */
	register_set_ptr(&sd_access, IRPT_MASK, &irpt);
	/*
	 * Interrupt generation is purposefully NOT enabled through the IRPT_EN register.  
	 * All the ISR would be doing is clearing
	 * the interrupts and setting a global variable identifying which interrupts were
	 * triggered, for use by the non-ISR code. But this is the same info already available
	 * in the INTERRUPT register, so polling of the INTERRUPT register is opted for instead,
	 * to avoid the potential race conditions introduced by using interrupts. by  and polling of interrupts through the INTERRUPT register opted
	 * for instead to avoid race conditions.
	 * TODO maybe not - write the code and check the asm 
	 * TODO if end up polling then rename this fn?
	 */
	/* Enable interrupt generation. */
	register_set_ptr(&sd_access, IRPT_EN, &irpt);
}

/*
 * Do all of the intialisation that must be done before being able to
 * successfully issue a command.
 */
static void sd_pre_cmd_init(void)
{
	sd_assert_vc_init();
	/*sd_reset_host();*/
	sd_supply_bus_power();
	sd_supply_clock(IDENTIFICATION_CLOCK_RATE_HZ);
	sd_enable_interrupts();
}

/* 
 * The interrupts generated from the most recent interrupt. 
 * This is actually a struct reg_interrupt masquerading as a
 * word/int so it can have atomic access.
 * TODO confirm
 */
struct reg_interrupt interrupt;

void sd_init(void)
{
	sd_pre_cmd_init();
	/* TODO mention in 1-bit bus width 400 KHz mode? */
	/* TODO because don't have a zeroed .bss section. */
	mzero(&interrupt, sizeof(interrupt));

	// TODO check sd_issue_command() works after able to get a response from CMD8
	/*issue_command(CMD_IDX_GO_IDLE_STATE, 0);*/
	register_set(&sd_access, ARG1,  0x00000000);
	register_set(&sd_access, CMDTM, 0x00000000);
	// TODO explain that this has the potential to get stuck if the interrupt happens
	// after the load to access interrupt but before the wfi. also under the assumption
	// that nothing else will interrupt this and it won't interrupt before the first sleep
	while (!cast_reg(interrupt))
		__asm__("wfi");
	
	mzero(&interrupt, sizeof(interrupt));

	register_set(&sd_access, ARG1,  0x000001aa);
	/*register_set(&sd_access, CMDTM, 8<<24|0b11<<19|0b10<<16);*/
	register_set(&sd_access, CMDTM, 8<<24|0b10<<16);

	while (!cast_reg(interrupt))
		__asm__("wfi");
	if (register_get(&sd_access, RESP0) == 0x000001aa)
		signal_error(1);
	else
		signal_error(2);

	/*struct {*/
		/*bits_t chkpat : 8;*/
		/*bits_t supply_voltage_vhs : 4;*/
		/*bits_t reserved : 20;*/
	/*} __attribute__((packed)) cmd8;*/
	/*mzero(&cmd8, sizeof(cmd8));*/
	/*cmd8.chkpat = 0b10101010;*/
	/*cmd8.supply_voltage_vhs = 1;*/
	/*issue_command(CMD_IDX_SEND_IF_COND, cast_bitfields(cmd8, uint32_t));*/

	/*mmc_supply_clock();*/
	/* Now in default speed mode. */
}

void sd_isr(void)
{
	/* Set global for use by non-ISR code. */
	register_get_out(&sd_access, INTERRUPT, &interrupt);
	/* Clear all high interrupts. */
	register_set_ptr(&sd_access, INTERRUPT, &interrupt);
}

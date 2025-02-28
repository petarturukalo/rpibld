/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
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
#include "../debug.h"
#include "sd_blksz.h"

/* 100 MHz. */
#define EMMC2_EXPECTED_BASE_CLOCK_HZ 100000000
/* 
 * 400 KHz, the clock frequency a card operates in after power-up
 * and during the card identification process.
 */
#define IDENTIFICATION_CLOCK_RATE_HZ	400000
/* 25 MHz. */
#define DEFAULT_SPEED_CLOCK_RATE_HZ   25000000

enum card_state {
/* Inactive operation mode. */
	CARD_STATE_INACTIVE = -1,
/* Card identification operation mode. */
	CARD_STATE_IDLE,
	CARD_STATE_READY,
	CARD_STATE_IDENTIFICATION,
/* Data transfer operation mode. */
	CARD_STATE_STANDBY,
	CARD_STATE_TRANSFER,
/* Below aren't set explicitly (but are still data transfer). */
	CARD_STATE_SENDING_DATA,
	CARD_STATE_RECEIVE_DATA,
	CARD_STATE_PROGRAMMING,
	CARD_STATE_DISCONNECT
};

/**
 * @brief Card (SD card) metadata and bookkeeping data.
 */
struct card {
	enum card_state state;  /**< The card's current state */
	/** 
	 * Whether the card is either of SDHC (high capacity) or SDXC (extended capacity). 
	 * If false the card is SDSC (standard capacity). 
	 */
	bool sdhc_or_sdxc;  
	int rca;  /**< Card's relative card address */
	bool cmd23_supported;
};

/**
 * @brief Assert that the EMMC2 base clock has a clock rate of EMMC2_EXPECTED_BASE_CLOCK_HZ.
 */
static void sd_assert_base_clock(void)
{
	struct clock_state state;
	int rate;

	state = tag_clock_get_state(CLK_EMMC2);
	if (state.clk_not_exists || !state.on) {
		serial_log("SD init error: EMMC2 clock either doesn't exist or isn't on");
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
	rate = tag_clock_get_rate(CLK_EMMC2);
	if (rate != EMMC2_EXPECTED_BASE_CLOCK_HZ) {
		serial_log("SD init error: EMMC2 clock rate %u Hz doesn't match expected clock rate %u Hz",
			   rate, EMMC2_EXPECTED_BASE_CLOCK_HZ);
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
}

/**
 * @brief Assert that the supply for the bus IO line power is 3.3V (the expected voltage
 *	  after a power cycle).
 */
static void sd_assert_voltage(void)
{
	struct gpio_expander_pin_config cfg;
	int state;

	cfg = tag_gpio_get_config(GPIO_EXPANDER_VDD_SD_IO_SEL);
	if (!gpio_config_pin_is_output(&cfg) || !gpio_config_pin_is_active_high(&cfg)) {
		serial_log("SD init error: expected GPIO expander pin SD voltage select to be both "
			   "an output and active high");
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
	state = tag_gpio_get_state(GPIO_EXPANDER_VDD_SD_IO_SEL);
	if (state) {
		serial_log("SD init error: GPIO expander pin SD voltage select has 1.8V selected, "
			   "but expected 3.3V");
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
}

/**
 * @brief Assert that the card is supplied power. 
 */
static void sd_assert_card_power(void)
{	
	struct gpio_expander_pin_config cfg;
	int state;

	cfg = tag_gpio_get_config(GPIO_EXPANDER_SD_PWR_ON);
	if (!gpio_config_pin_is_output(&cfg) || !gpio_config_pin_is_active_high(&cfg)) {
		serial_log("SD init error: expected GPIO expander pin SD card power to be both "
			   "an output and active high");
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
	state = tag_gpio_get_state(GPIO_EXPANDER_SD_PWR_ON);
	if (!state) {
		serial_log("SD init error: GPIO expander pin SD card power not selected to supply power");
		signal_error(ERROR_VC_NOT_INIT_MMC);
	}
}

/**
 * @brief Assert that the VideoCore firmware which loaded this bootloader program set up
 *	  the MMC controller as expected.
 *
 * Signal error with error ERROR_VC_NOT_INIT_MMC if the assertion fails.
 */
static void sd_assert_vc_init(void)
{
	sd_assert_base_clock();
	sd_assert_voltage();
	sd_assert_card_power();
}

static bool sd_sw_reset_hc_bit_set(void)
{
	return register_get(&sd_access, CONTROL1)&CONTROL1_SW_RESET_HC;
}

/**
 * Reset the entire host controller. Clears register bits, so needs to be
 * called before any other initialisation. 
 */
static void sd_reset_host(void)
{
	register_enable_bits(&sd_access, CONTROL1, CONTROL1_SW_RESET_HC);
	while_cond_timeout_infinite(sd_sw_reset_hc_bit_set, 20);
}

/**
 * @brief Supply 3.3V SD bus power.
 *
 * This was the final (undocumented, and so took forever to find) piece 
 * to the SD puzzle, required to be able to receive a command complete
 * interrupt after issuing a command.
 */
static void sd_supply_bus_power(void)
{
	uint32_t pwr_ctl_bits = (PWR_CTL_SD_BUS_VOLT_SEL_3V3|PWR_CTL_SD_BUS_POWER);

	register_enable_bits(&sd_access, CONTROL0, pwr_ctl_bits<<CONTROL0_PWR_CTL_SHIFT);
}

/**
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

static bool sd_internal_clock_not_stable(void)
{
	return !(register_get(&sd_access, CONTROL1)&CONTROL1_INT_CLK_STABLE);
}

/**
 * @brief Supply the clock at the given clock rate (in Hz) to the card.
 */
static void sd_supply_clock(int clock_rate)
{
	/* 
	 * Using 8-bit clock divider under the assumption that earlier cards won't support
	 * the version 3 10-bit divider. 
	 */
	int clock_divider = sd_8bit_clock_divider(EMMC2_EXPECTED_BASE_CLOCK_HZ, clock_rate);

	/* Turn off clock in case it was already on (required to change frequency). */
	register_disable_bits(&sd_access, CONTROL1, CONTROL1_CLK_EN|CONTROL1_INT_CLK_EN);
	/* Zero previous clock divider bits. */
	register_disable_bits(&sd_access, CONTROL1, CONTROL1_CLK_FREQ_SEL);
	/* Set clock divider and enable internal clock. */
	register_enable_bits(&sd_access, CONTROL1, 
			     clock_divider<<CONTROL1_CLK_FREQ_SEL_SHIFT | CONTROL1_INT_CLK_EN);
	/* 
	 * Wait for internal clock to become stable. From testing this only takes 
	 * around 5 iterations, so don't bother sleeping. 
	 */
	while_cond_timeout_infinite(sd_internal_clock_not_stable, 20);
	/* Enable clock. */
	register_enable_bits(&sd_access, CONTROL1, CONTROL1_CLK_EN);
}

static void sd_enable_interrupts(struct interrupt irpt)
{
	/* Enable triggered interrupts to be flagged in the INTERRUPT register. */
	register_enable_bits(&sd_access, IRPT_MASK, cast_bitfields(irpt, uint32_t));
	/*
	 * Interrupt generation is not enabled in the IRPT_EN register, and polling opted
	 * for instead because of race conditions that would make the multi block read hang
	 * if reading more than around 300 blocks at a time.
	 */
}

static void sd_enable_cmd_interrupts(void)
{
	struct interrupt irpt;

	mzero(&irpt, sizeof(irpt));
	irpt.cmd_complete = true;
	irpt.cmd_timeout_error = true;
	irpt.cmd_crc_error = true;
	irpt.cmd_end_bit_error = true;
	irpt.cmd_index_error = true;

	sd_enable_interrupts(irpt);
}

static void sd_enable_transfer_interrupts(void)
{
	struct interrupt irpt;

	mzero(&irpt, sizeof(irpt));
	irpt.read_ready = true;
	irpt.transfer_complete = true;
	irpt.data_timeout_error = true;
	irpt.data_crc_error = true;
	irpt.data_end_bit_error = true;

	sd_enable_interrupts(irpt);
}

/**
 * @brief Do all of the intialisation needed to issue a command successfully.
 */
static void sd_pre_cmd_init(void)
{
	sd_assert_vc_init();
	sd_reset_host();
	sd_supply_bus_power();
	/* 
	 * Set clock frequency to 400 KHz for the card initialisation and identification 
	 * period as some cards may have operating frequency restrictions during it.
	 */
	sd_supply_clock(IDENTIFICATION_CLOCK_RATE_HZ);
	sd_enable_cmd_interrupts();
}

/**
 * Go through the card intialisation and identification process, moving the card
 * from the start of card identification mode to the start of data transfer mode.
 */
static enum sd_init_error sd_card_init_and_identify(struct card *card)
{
	enum cmd_error error;
	bool ccs;

	card->state = CARD_STATE_IDLE;

	/* Command is issued assuming card supports 3.3V */
	error = sd_issue_cmd(CMD_IDX_GO_IDLE_STATE, 0);
	if (error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;

	/* 
	 * CMD8 was defined in phys layer version 2: only a version >= 2 card will respond.
	 * Apparently SDSC is version 1, but all of the 2 GB SDSC cards I've tested with have all
	 * been version 2/3 still, so don't worry about supporting version 1 cards and let it
	 * fail here.
	 */
	error = sd_issue_cmd8();
	if (error == CMD_ERROR_RESPONSE_CONTENTS)
		return SD_INIT_ERROR_UNUSABLE_CARD;
	if (error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;

	error = sd_issue_acmd41(&ccs);
	if (error == CMD_ERROR_RESPONSE_CONTENTS) 
		return SD_INIT_ERROR_UNUSABLE_CARD;
	if (error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;
	card->sdhc_or_sdxc = ccs;

	card->state = CARD_STATE_READY;

	/* Issue CMD2. */
	error = sd_issue_cmd(CMD_IDX_ALL_SEND_CID, 0);
	if (error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;

	card->state = CARD_STATE_IDENTIFICATION;

	error = sd_issue_cmd3(&card->rca);
	if (error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;

	card->state = CARD_STATE_STANDBY;
	return SD_INIT_ERROR_NONE;
}

static enum sd_init_error sd_set_4bit_data_bus_width(int rca)
{
	struct interrupt irpt_mask;
	enum cmd_error error;
	uint32_t prev_irpt_mask = register_get(&sd_access, IRPT_MASK);

	mzero(&irpt_mask, sizeof(irpt_mask));

	/* Mask incorrect interrupts that may occur while changing bus width. */
	irpt_mask.card = 1;
	register_disable_bits(&sd_access, IRPT_MASK, cast_bitfields(irpt_mask, uint32_t));

	/* Change card to 4-bit data bus width. */
	error = sd_issue_acmd6(rca, true);
	if (error == CMD_ERROR_NONE) {
		/* Change host to 4-bit data bus width. */
		register_enable_bits(&sd_access, CONTROL0, CONTROL0_DATA_TRANSFER_WIDTH);
	}
	register_set(&sd_access, IRPT_MASK, prev_irpt_mask);
	return error == CMD_ERROR_NONE ? SD_INIT_ERROR_NONE : SD_INIT_ERROR_ISSUE_CMD;
}

enum sd_init_error sd_init_card(struct card *card_out)
{
	enum sd_init_error sd_init_error;
	enum cmd_error cmd_error;
	struct card_status cs;
	struct scr scr;

	serial_log("Initialising SD...");
	mzero(card_out, sizeof(struct card));

	sd_pre_cmd_init();
	/* After sd_pre_cmd_init() have a 1-bit data bus width and <= 400 KHz clock. */
	sd_init_error = sd_card_init_and_identify(card_out);
	if (sd_init_error != SD_INIT_ERROR_NONE)
		return sd_init_error;

	/*
	 * Given that this is called after sd_assert_vc_init() it can safely be assumed 
	 * that 3.3V power is being supplied. The only two bus speed modes supported at 
	 * 3.3V are default speed and high speed. Default speed has a max clock speed of 
	 * 25 MHz, and high speed 50 MHz. High speed mode requires setting a register 
	 * field to use; because the register fields relevant to bus speed modes are reset 
	 * to 0x0 on boot, default speed can be assumed - change the clock to 25 MHz for 
	 * default speed.
	 */
	sd_supply_clock(DEFAULT_SPEED_CLOCK_RATE_HZ);

	/* Put card in transfer state. */
	cmd_error = sd_issue_cmd7(card_out->rca);
	if (cmd_error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;
	/* Verify card got put into transfer state. */
	cmd_error = sd_issue_cmd13(card_out->rca, &cs);
	if (cmd_error != CMD_ERROR_NONE)
		return SD_INIT_ERROR_ISSUE_CMD;
	if (cs.current_state != CARD_STATE_TRANSFER) {
		serial_log("SD init error: cmd 13 didn't put card into transfer state");
		return SD_INIT_ERROR_ISSUE_CMD;
	}

	card_out->state = CARD_STATE_TRANSFER;

	sd_enable_transfer_interrupts();

	/* Check card's configuration register for support info. */
	cmd_error = sd_issue_acmd51(card_out->rca, &scr);
	if (cmd_error != CMD_ERROR_NONE) 
		return SD_INIT_ERROR_ISSUE_CMD;
	card_out->cmd23_supported = scr.cmd23_supported;

	/* Set 4-bit data bus width if supported. */
	if (scr.bus_widths&SCR_BUS_WIDTHS_4BIT) {
		sd_init_error = sd_set_4bit_data_bus_width(card_out->rca);
		if (sd_init_error != SD_INIT_ERROR_NONE)
			return sd_init_error;
	}
	serial_log("Successfully initialised SD: %s capacity, CMD23 %s, "
		   "%s-bit data bus width, 25 MHz clock, default speed bus mode",
		   card_out->sdhc_or_sdxc ? "SDHC/SDXC" : "SDSC",
		   card_out->cmd23_supported ? "supported" : "not supported",
		   scr.bus_widths&SCR_BUS_WIDTHS_4BIT ? "4" : "1");
	return SD_INIT_ERROR_NONE;
}

static struct card card;

enum sd_init_error sd_init(void)
{
	return sd_init_card(&card);
}

static bool _sd_read_blocks_card(byte_t *ram_dest_addr, uint32_t sd_src_lba, uint16_t nblks, 
				 struct card *card)
{
	enum cmd_error error = CMD_ERROR_NONE;
	void *sd_src_addr = (void *)sd_src_lba;

	if (!address_aligned(ram_dest_addr, 4)) {
		serial_log("SD read error: RAM destination address %08x not 4-byte aligned",
			   ram_dest_addr);
		return false;
	}
	/* Convert LBA / block unit address to byte unit address for SDSC. */
	if (!card->sdhc_or_sdxc) 
		sd_src_addr = (void *)(sd_src_lba*SD_BLKSZ);
	
	if (nblks == 1) {
		/* Single block transfer. */
		error = sd_issue_cmd17(ram_dest_addr, sd_src_addr);
	} else if (nblks > 1) {
		/* Multi block transfer. */
		if (card->cmd23_supported) {
			/* Read all blocks in one multi block transfer. */
			error = sd_issue_cmd(CMD_IDX_SET_BLOCK_COUNT, nblks);
			if (error == CMD_ERROR_NONE) 
				error = sd_issue_cmd18(ram_dest_addr, sd_src_addr, nblks);
		} else {
			/* Read all blocks over multiple single block transfers. */
			for (; nblks; --nblks) {
				error = sd_issue_cmd17(ram_dest_addr, sd_src_addr);
				if (error != CMD_ERROR_NONE)
					break;
				ram_dest_addr += SD_BLKSZ;
				sd_src_addr += card->sdhc_or_sdxc ? 1 : SD_BLKSZ;  /* As above. */
			}
		}
	}
	if (error != CMD_ERROR_NONE) 
		serial_log("SD read error: RAM dest addr %08x, SD src addr %08x, number "
			   "of blocks %u", ram_dest_addr, sd_src_addr, nblks);
	return error == CMD_ERROR_NONE;
}

static bool sd_read_blocks_card(byte_t *ram_dest_addr, uint32_t sd_src_lba, int nblks, 
				struct card *card)
{
	bool read_ok;

	while (nblks > 0) {
		/* 
		 * Read command block count field is only 16 bits wide, meaning a multi block
		 * transfer can only read up to ~33.55 MB, so must read in multiple passes
		 * if read size is greater than this.
		 */
		read_ok = _sd_read_blocks_card(ram_dest_addr, sd_src_lba, 
					       nblks < UINT16_MAX ? nblks : UINT16_MAX,
					       card);
		if (!read_ok)
			return false;

		nblks -= UINT16_MAX;
		ram_dest_addr += SD_BLKSZ*UINT16_MAX;
		sd_src_lba += UINT16_MAX;
	}
	return true;
}

bool sd_read_blocks(byte_t *ram_dest_addr, uint32_t sd_src_lba, int nblks)
{
	return sd_read_blocks_card(ram_dest_addr, sd_src_lba, nblks, &card);
}

int bytes_to_blocks(int bytes)
{
	int nblks = bytes/SD_BLKSZ;
	if (bytes%SD_BLKSZ)
		++nblks;
	return nblks;
}

bool sd_read_bytes(byte_t *ram_dest_addr, uint32_t sd_src_lba, int bytes)
{
	int nblks = bytes_to_blocks(bytes);
	return sd_read_blocks(ram_dest_addr, sd_src_lba, nblks);
}

static bool sd_reset_card(struct card *card)
{
	enum cmd_error error;

	/* Reset card. */
	error = sd_issue_cmd(CMD_IDX_GO_IDLE_STATE, 0);
	if (error != CMD_ERROR_NONE) 
		return false;

	card->state = CARD_STATE_IDLE;

	sd_reset_host();
	return true;
}

bool sd_reset(void)
{
	return sd_reset_card(&card);
}

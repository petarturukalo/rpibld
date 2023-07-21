/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef CMD_H
#define CMD_H

#include "../type.h"
#include "../bits.h"

/* A cmd_index has this bit set if it's an application command. */
#define IS_APP_CMD 0x80

/*
 * Index identifier for a command. 
 */
enum cmd_index {
	CMD_IDX_GO_IDLE_STATE       = 0,
	CMD_IDX_ALL_SEND_CID        = 2,
	CMD_IDX_SEND_RELATIVE_ADDR  = 3,
	CMD_IDX_SELECT_CARD         = 7,
	CMD_IDX_SEND_IF_COND        = 8,  /* Send interface condition. */
	CMD_IDX_SEND_STATUS         = 13,
	CMD_IDX_READ_SINGLE_BLOCK   = 17,
	CMD_IDX_READ_MULTIPLE_BLOCK = 18,
	CMD_IDX_SET_BLOCK_COUNT     = 23,
	CMD_IDX_APP_CMD             = 55,
/* Application commands. */
	ACMD_IDX_SET_BUS_WIDTH      = 6|IS_APP_CMD,
	ACMD_IDX_SD_SEND_OP_COND    = 41|IS_APP_CMD, /* Send operating condition register. */
	ACMD_IDX_SD_SEND_SCR        = 51|IS_APP_CMD  /* Send SD configuration register. */
};

/*
 * @CMD_ERROR_COMMAND_INHIBIT_DAT_BIT_SET: command which uses DAT line found DAT line
 *	high (i.e. a data transfer command or a command which uses busy signal, since 
 *	the busy signal is transmitted on the data line)
 * @CMD_ERROR_INTERRUPT_ERROR: one of the error interrupts was flagged
 * @CMD_ERROR_CARD_STATUS_ERROR: a command with a card status response had an error bit
 *	set in the card status 
 * @CMD_ERROR_RESPONSE_CONTENTS: contents of command response not as expected
 */
enum cmd_error {
	CMD_ERROR_NONE,
	CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET,
	CMD_ERROR_COMMAND_INHIBIT_DAT_BIT_SET,
	CMD_ERROR_COMMAND_UNIMPLEMENTED,
	CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT,
	CMD_ERROR_INTERRUPT_ERROR,
	CMD_ERROR_EXPECTED_INTERRUPT_NOT_TRIGGERED,
	CMD_ERROR_CARD_STATUS_ERROR,
 /* Above are returns from sd_issue_cmd(). Below are extra returns from sd_issue_cmd() wrappers. */
	CMD_ERROR_RESPONSE_CONTENTS,
	CMD_ERROR_GENERAL_TIMEOUT
};

/*
 * Issue a command to the SD card. If the return is successful (CMD_ERROR_NONE)
 * and the command expects a response, check the RESP* registers for the response.
 *
 * @idx: index of a standard (not application) command (unless this is being called 
 *	from sd_issue_acmd()) 
 * @args: arguments optionally used by the command
 */
enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args);
void sd_isr(void);
/* 
 * @idx: index of an application command
 * @rca: relative card address used as argument to CMD55
 *
 * Return CMD_ERROR_RESPONSE_CONTENTS if the APP_CMD bit was not set in
 * the CMD55 response.
 */
enum cmd_error sd_issue_acmd(enum cmd_index idx, uint32_t args, int rca);

/*
 * Verify the card can operate on the 2.7-3.6V host supply voltage.
 *
 * Return CMD_ERROR_RESPONSE_CONTENTS if the voltage isn't supported or there
 * was an error with the response check pattern.
 */
enum cmd_error sd_issue_cmd8(void);

/*
 * Power up the card.
 *
 * @card_capacity_support_out: out-param card capacity support, whether card is SDHC/SDXC (true) or SDSC (false).
 *	Only valid on success.
 *
 * Return
 * - CMD_ERROR_RESPONSE_CONTENTS voltage range not supported in card's OCR register
 * - CMD_ERROR_GENERAL_TIMEOUT if the card did not power up in 1 second
 */
enum cmd_error sd_issue_acmd41(bool *card_capacity_support_out);

/*
 * Publish a new relative card address for the card in out-param rca_out.
 */
enum cmd_error sd_issue_cmd3(int *rca_out);

/*
 * Toggle a card between the stand-by and transfer states dependent on RCA (see phys spec 
 * version 3 for more info). 
 *
 * The command's card status response isn't returned or checked for its current state 
 * because it will be the state of the card from before the command caused a state change.
 * To get the state that was toggled to, issue CMD13 after this.
 */
enum cmd_error sd_issue_cmd7(int rca);

/* 
 * Card status response from RESPONSE_R1_NORMAL and RESPONSE_R1B_NORMAL_BUSY. 
 * @app_cmd: signals whether the next issued command is expected to be an application command
 * @card_state: stores an enum card_state except for CARD_STATE_INACTIVE. 
 */
struct card_status {
	bits_t reserved1 : 3;
	bits_t ake_seq_error : 1;
	bits_t reserved2 : 1;
	bits_t app_cmd : 1;
	bits_t reserved3 : 2;
	bits_t ready_for_data : 1;
	bits_t current_state : 4;
	bits_t erase_reset : 1;
	bits_t card_ecc_disabled : 1;
	bits_t wp_erase_skip : 1;
	bits_t csd_overwrite : 1;
	bits_t reserved4 : 2;
	bits_t error : 1;
	bits_t cc_error : 1;
	bits_t card_ecc_failed : 1;
	bits_t illegal_command : 1;
	bits_t com_crc_error : 1;
	bits_t lock_unlock_failed : 1;
	bits_t card_is_locked : 1;
	bits_t wp_violation : 1;
	bits_t erase_param : 1;
	bits_t erase_seq_error : 1;
	bits_t block_len_error : 1;
	bits_t address_error : 1;
	bits_t out_of_range : 1;
} __attribute__((packed));

/*
 * Get the addressed card's status.
 */
enum cmd_error sd_issue_cmd13(int rca, struct card_status *cs_out);

/*
 * Set the addressed card's data bus width.
 * @four_bit: true for 4-bit data bus width, false for 1-bit.
 */
enum cmd_error sd_issue_acmd6(int rca, bool four_bit);

/* Read block size in bytes. */
#define READ_BLKSZ 512

/*
 * Read a single block (CMD17) or multiple blocks (CMD18) of size READ_BLKSZ 
 * from the SD card into RAM.
 *
 * @ram_dest_addr: 4-byte aligned destination address in RAM to copy read data to
 * @sd_src_addr: source SD card address to read data from. If the card
 *	is SDSC this should be a byte unit address. If the card is SDHC 
 *	or SDXC	this should be a block unit address (LBA).
 * @nblks: number of blocks to read
 */
enum cmd_error sd_issue_cmd17(byte_t *ram_dest_addr, void *sd_src_addr);
enum cmd_error sd_issue_cmd18(byte_t *ram_dest_addr, void *sd_src_addr, int nblks);

/* SD card configuration register. */
struct scr {
	bits_t ignore1 : 1;
	bits_t cmd23_supported : 1; 
	bits_t ignore2 : 14;
	bits_t bus_widths : 4;
	bits_t ignore3 : 12;
} __attribute__((packed));

#define SCR_BUS_WIDTHS_4BIT BIT(2)

/* Get the SD card to send its SCR register. */
enum cmd_error sd_issue_acmd51(int rca, struct scr *scr_out);

#endif

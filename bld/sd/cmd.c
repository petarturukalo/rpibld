/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#include "cmd.h"
#include "reg.h"
#include "../help.h"
#include "../timer.h"
#include "../debug.h"

/*
 * Arguments used by some addressed commands requiring
 * a relative card address as input.
 */
struct ac_rca_args {
	bits_t unused : 16;
	bits_t rca : 16;
} __attribute__((packed));

/*
 * @CMD_TYPE_BC: broadcast: command is broadcast to all cards.
 *	Doesn't expect a response.
 * @CMD_TYPE_BCR: broadcast response: command is broadcast to all cards.
 *	 Does expect a response.
 * @CMD_TYPE_AC: addressed command: command is sent to an addressed card.
 *	No data transfer.
 * @CMD_TYPE_ADTC: addressed data transfer command: command is sent to an
 *	addressed card. Transfers data.
 */
enum cmd_type {
	CMD_TYPE_BC,
	CMD_TYPE_BCR,
	CMD_TYPE_AC,
	CMD_TYPE_ADTC
};

enum cmd_response {
	CMD_RESPONSE_NONE,
	CMD_RESPONSE_R1_NORMAL,
	CMD_RESPONSE_R1B_NORMAL_BUSY,
	CMD_RESPONSE_R2_CID_OR_CSD_REG,
	CMD_RESPONSE_R3_OCR_REG,
	CMD_RESPONSE_R6_PUBLISHED_RCA,
	CMD_RESPONSE_R7_CARD_INTERFACE_CONDITION,
};

/*
 * A command to control the SD card.
 */
struct command {
	enum cmd_index index;
	enum cmd_type type;
	enum cmd_response response;
};

/* List of implemented SD commands. */
static struct command commands[] = {
	{ CMD_IDX_GO_IDLE_STATE,       CMD_TYPE_BC,   CMD_RESPONSE_NONE },
	{ CMD_IDX_ALL_SEND_CID,        CMD_TYPE_BCR,  CMD_RESPONSE_R2_CID_OR_CSD_REG },
	{ CMD_IDX_SEND_RELATIVE_ADDR,  CMD_TYPE_BCR,  CMD_RESPONSE_R6_PUBLISHED_RCA },
	{ CMD_IDX_SELECT_CARD,         CMD_TYPE_AC,   CMD_RESPONSE_R1B_NORMAL_BUSY },
	{ CMD_IDX_SEND_IF_COND,        CMD_TYPE_BCR,  CMD_RESPONSE_R7_CARD_INTERFACE_CONDITION },
	{ CMD_IDX_SEND_STATUS,         CMD_TYPE_AC,   CMD_RESPONSE_R1_NORMAL },
	{ CMD_IDX_READ_SINGLE_BLOCK,   CMD_TYPE_ADTC, CMD_RESPONSE_R1_NORMAL },
	{ CMD_IDX_READ_MULTIPLE_BLOCK, CMD_TYPE_ADTC, CMD_RESPONSE_R1_NORMAL },
	{ CMD_IDX_SET_BLOCK_COUNT,     CMD_TYPE_AC,   CMD_RESPONSE_R1_NORMAL },
	{ CMD_IDX_APP_CMD,             CMD_TYPE_AC,   CMD_RESPONSE_R1_NORMAL },
	{ ACMD_IDX_SET_BUS_WIDTH,      CMD_TYPE_AC,   CMD_RESPONSE_R1_NORMAL },
	{ ACMD_IDX_SD_SEND_OP_COND,    CMD_TYPE_BCR,  CMD_RESPONSE_R3_OCR_REG },
	{ ACMD_IDX_SD_SEND_SCR,	       CMD_TYPE_ADTC, CMD_RESPONSE_R1_NORMAL }
};

static struct command *get_command(enum cmd_index idx)
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
 * Set cmdtm response type from the command's response. The term "response type" is used
 * in the spec but the differentiating factor here is really response size (in bits).
 */
static void set_cmdtm_response_type(struct command *cmd, struct cmdtm *cmdtm)
{
	switch (cmd->response) {
		case CMD_RESPONSE_NONE:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_NONE;
			break;
		case CMD_RESPONSE_R1_NORMAL:
		case CMD_RESPONSE_R3_OCR_REG:
		case CMD_RESPONSE_R6_PUBLISHED_RCA:
		case CMD_RESPONSE_R7_CARD_INTERFACE_CONDITION:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_48_BITS;
			break;
		case CMD_RESPONSE_R1B_NORMAL_BUSY:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_48_BITS_BUSY;
			break;
		case CMD_RESPONSE_R2_CID_OR_CSD_REG:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_136_BITS;
			break;
	}
}

/* 
 * Set index check enable and CRC check enable from response.
 * If a case doesn't set one of these it means it's supposed to be disabled
 * (and it's already been disabled from zeroing).
 */
static void set_cmdtm_idx_and_crc_chk(struct command *cmd, struct cmdtm *cmdtm)
{
	switch (cmd->response) {
		case CMD_RESPONSE_NONE:
		case CMD_RESPONSE_R3_OCR_REG:
			break; 
		case CMD_RESPONSE_R2_CID_OR_CSD_REG:
			cmdtm->cmd_crc_chk_en = true;
			break;
		case CMD_RESPONSE_R1_NORMAL:
		case CMD_RESPONSE_R1B_NORMAL_BUSY:
		case CMD_RESPONSE_R6_PUBLISHED_RCA:
		case CMD_RESPONSE_R7_CARD_INTERFACE_CONDITION:
			cmdtm->cmd_index_chk_en = true;
			cmdtm->cmd_crc_chk_en = true;
			break;
	}
}

/* 
 * Set the fields of the transfer mode register relevant to the command.
 * The transfer mode register is the lower 16 bits of cmdtm.
 */
static void set_cmdtm_transfer_mode(struct command *cmd, struct cmdtm *cmdtm)
{
	/* As more commands are implemented they will need to be added in conditions here. */
	if (cmd->index == CMD_IDX_READ_SINGLE_BLOCK || cmd->index == CMD_IDX_READ_MULTIPLE_BLOCK ||
	    cmd->index == ACMD_IDX_SD_SEND_SCR)
		cmdtm->data_transfer_direction = CMDTM_TM_DAT_DIR_READ;
	if (cmd->index == CMD_IDX_READ_MULTIPLE_BLOCK) {
		cmdtm->block_cnt_en = true;
		cmdtm->multi_block = true;
	}
}

/*
 * Set the fields of the cmdtm register required to issue the
 * given command.
 */
static void set_cmdtm(struct command *cmd, struct cmdtm *cmdtm)
{
	mzero(cmdtm, sizeof(struct cmdtm));

	cmdtm->cmd_index = cmd->index & ~IS_APP_CMD;
	set_cmdtm_response_type(cmd, cmdtm);
	set_cmdtm_idx_and_crc_chk(cmd, cmdtm);
	/* 
	 * Addressed data transfer commands which transfer data on DAT will 
	 * (of course) have data present on the DAT line.
	 */
	if (cmd->type == CMD_TYPE_ADTC)
		cmdtm->data_present = true;
	set_cmdtm_transfer_mode(cmd, cmdtm);
}

/* 
 * If the return is zero then timed out waiting for any interrupt.
 */
static struct interrupt sd_wait_for_any_interrupt(void)
{
	timestamp_t ts;
	struct interrupt irpt;

	mzero(&irpt, sizeof(irpt));

	ts = timer_poll_start(500);
	do {
		register_get_out(&sd_access, INTERRUPT, &irpt);

		if (cast_bitfields(irpt, uint32_t)) {
			/* Clear triggered interrupts. */
			register_set_ptr(&sd_access, INTERRUPT, &irpt);
			break;
		}
		usleep(50);
	} while (!timer_poll_done(ts));

	return irpt;
}

/*
 * Wait for a particular interrupt. 
 * @interrupt: bit mask for the interrupt's field in the INTERRUPT register
 */
enum cmd_error sd_wait_for_interrupt(int interrupt_mask)
{
	struct interrupt irpt = sd_wait_for_any_interrupt();

	if (!cast_bitfields(irpt, uint32_t)) {
		serial_log("SD cmd error: timeout waiting for interrupt %08x", interrupt_mask);
		return CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT;
	}
	if (irpt.error) {
		/* 
		 * Note might be worth checking which error it was and then retrying 
		 * instead of failing. 
		 */
		serial_log("SD cmd error: error interrupt triggered: %08x", 
			   cast_bitfields(irpt, uint32_t));
		return CMD_ERROR_INTERRUPT_ERROR;
	}
	if (!(cast_bitfields(irpt, uint32_t)&interrupt_mask)) {
		serial_log("SD cmd error: expected interrupt %08x not triggered: %08x",
			   interrupt_mask, cast_bitfields(irpt, uint32_t));
		return CMD_ERROR_EXPECTED_INTERRUPT_NOT_TRIGGERED;
	}
	return CMD_ERROR_NONE;
}

static bool sd_cmd_has_card_status_response(struct command *cmd)
{
	return cmd->response == CMD_RESPONSE_R1_NORMAL ||
	       cmd->response == CMD_RESPONSE_R1B_NORMAL_BUSY;
}

/*
 * Get whether at least one error bit in the card status is set high.
 */
static bool sd_card_status_error_bit_set(struct card_status *cs)
{
	return cs->ake_seq_error || cs->wp_erase_skip || cs->csd_overwrite ||
	       cs->error || cs->cc_error || cs->card_ecc_failed || cs->illegal_command ||
	       cs->com_crc_error || cs->lock_unlock_failed || cs->wp_violation || 
	       cs->erase_param || cs->erase_seq_error || cs->block_len_error || 
	       cs->address_error || cs->out_of_range;
}

/* 
 * IDX_SPEC is the command index conversion specifier used in the format string 
 * of serial_log() to substitute in a command index and whether it's an application 
 * command. IDX_SPEC_ARGS are the arguments used to expand the specifier. 
 */
#define IDX_SPEC "%s%u"
#define IDX_SPEC_ARGS(idx) idx&IS_APP_CMD ? "APP" : "", idx&~IS_APP_CMD

enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args)
{
	struct command *cmd;
	struct cmdtm cmdtm;
	uint32_t status;
	enum cmd_error error;

	cmd = get_command(idx);
	if (!cmd) {
		serial_log("SD cmd error: command " IDX_SPEC " not implemented", IDX_SPEC_ARGS(idx));
		return CMD_ERROR_COMMAND_UNIMPLEMENTED;
	}
	status = register_get(&sd_access, STATUS);
	if (status&STATUS_COMMAND_INHIBIT_CMD) {
		serial_log("SD cmd error: CMD line already set when trying to issue command " IDX_SPEC,
			   IDX_SPEC_ARGS(idx));
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;
	}
	/* Note if implement an abort command such as CMD12 then need to let it skip this step. */
	if ((cmd->type == CMD_TYPE_ADTC || cmd->response == CMD_RESPONSE_R1B_NORMAL_BUSY) && 
	    status&STATUS_COMMAND_INHIBIT_DAT) {
		serial_log("SD cmd error: command " IDX_SPEC " uses DAT line but DAT line already set", 
			   IDX_SPEC_ARGS(idx));
		return CMD_ERROR_COMMAND_INHIBIT_DAT_BIT_SET;
	}

	/* Set the command's arguments. Note if implement ACMD23 it needs to use ARG2 instead. */
	register_set(&sd_access, ARG1, args);

	set_cmdtm(cmd, &cmdtm);

	/* Issue the command, which should trigger an interrupt. */
	register_set_ptr(&sd_access, CMDTM, &cmdtm);
	error = sd_wait_for_interrupt(INTERRUPT_CMD_COMPLETE);

	/* For cards with a card status response check for any card status error bits. */
	if (error == CMD_ERROR_NONE && sd_cmd_has_card_status_response(cmd)) {
		struct card_status cs;
		register_get_out(&sd_access, RESP0, &cs);
		if (sd_card_status_error_bit_set(&cs)) {
			serial_log("SD cmd error: command " IDX_SPEC " has error returned in card "
				   "status register: %08x", IDX_SPEC_ARGS(idx), cast_bitfields(cs, uint32_t));
			return CMD_ERROR_CARD_STATUS_ERROR;;
		}
	} else if (error != CMD_ERROR_NONE) {
		serial_log("SD cmd error: command " IDX_SPEC " failed", IDX_SPEC_ARGS(idx));
	}
	return error;
}

enum cmd_error sd_issue_acmd(enum cmd_index idx, uint32_t args, int rca)
{
	struct ac_rca_args cmd55_args;
	enum cmd_error error;
	struct card_status cs;

	mzero(&cmd55_args, sizeof(cmd55_args));
	cmd55_args.rca = rca;

	/* Indicate to card that next command is an application command. */
	error = sd_issue_cmd(CMD_IDX_APP_CMD, cast_bitfields(cmd55_args, uint32_t));
	if (error != CMD_ERROR_NONE)
		return error;
	register_get_out(&sd_access, RESP0, &cs);
	if (!cs.app_cmd) {
		serial_log("SD cmd error: application command bit not set by cmd 55 when "
			   "trying to issue command " IDX_SPEC, IDX_SPEC_ARGS(idx));
		return CMD_ERROR_RESPONSE_CONTENTS;
	}
	return sd_issue_cmd(idx, args);
}

enum cmd_error sd_issue_cmd8(void)
{
	struct {
		bits_t check_pattern : 8;
		enum {
			CMD8_VHS_2V7_TO_3V6 = 0x1,  /* 2.7 to 3.6V */
		} supply_voltage : 4;
		bits_t reserved : 20;
	} __attribute__((packed)) args, resp;
	enum cmd_error error;

	mzero(&args, sizeof(args));
	args.check_pattern = 0b10101010;
	args.supply_voltage = CMD8_VHS_2V7_TO_3V6;

	error = sd_issue_cmd(CMD_IDX_SEND_IF_COND, cast_bitfields(args, uint32_t));
	if (error == CMD_ERROR_NONE) {
		/* Assert sent voltage range and check pattern are echoed back in the response. */
		register_get_out(&sd_access, RESP0, &resp);
		if (!mcmp(&args, &resp, sizeof(args))) {
			serial_log("SD cmd error: cmd 8: sent voltage and check pattern %08x doesn't "
				   "match response %08x", cast_bitfields(args, uint32_t), cast_bitfields(resp, uint32_t));
			error = CMD_ERROR_RESPONSE_CONTENTS;
		}
	}
	return error;
}

/* OCR register fields. */
/* Voltage window. */
#define OCR_VDD_2V7_TO_3V6		0x00ff8000
/* 0 is SDSC, 1 is SDHC/SDXC. */
#define OCR_CARD_CAPACITY_STATUS	BIT(30)
/* Whether card power up procedure has finished. */
#define OCR_CARD_POWER_UP_STATUS	BIT(31)

/* ACMD41 argument fields. */
/* Whether the host supports SDHC/SDXC. */
#define ACMD41_HOST_CAPACITY_SUPPORT	BIT(30)

enum cmd_error sd_issue_acmd41(bool *card_capacity_support_out)
{
	uint32_t args, ocr;
	enum cmd_error error;
	timestamp_t ts;
	/* Card has a default RCA of 0 when in idle state. */
	int rca = 0;
	int i = 0;

	/* 
	 * When args voltage window is zero ACMD41 is inquiry ACMD41. When args voltage window is 
	 * non-zero ACMD41 is init ACMD41. Do inquiry first so can get the card's voltage window, 
	 * since if it doesn't support the voltage window the init ACMD41 would put the card into 
	 * the inactive state.
	 */
	mzero(&args, sizeof(args));
	error = sd_issue_acmd(ACMD_IDX_SD_SEND_OP_COND, args, rca);
	if (error != CMD_ERROR_NONE)
		return error;
	ocr = register_get(&sd_access, RESP0);
	/* Assert the card supports the voltage range. */
	if (ocr&OCR_VDD_2V7_TO_3V6 != OCR_VDD_2V7_TO_3V6) {
		serial_log("SD cmd error: app cmd 41: card does not support the voltage range: "
			   "ocr reg %08x", ocr);
		return CMD_ERROR_RESPONSE_CONTENTS;
	}

	/* Set args for init ACMD41. */
	args |= OCR_VDD_2V7_TO_3V6;
	args |= ACMD41_HOST_CAPACITY_SUPPORT;

	/* Wait for card to finish power up, which should take at most 1 second from the first init ACMD41. */
	ts = timer_poll_start(1000);
	do {
		if (i++)
			sleep(20);
		error = sd_issue_acmd(ACMD_IDX_SD_SEND_OP_COND, args, rca);
		if (error != CMD_ERROR_NONE)
			return error;
		ocr = register_get(&sd_access, RESP0);
	} while (!(ocr&OCR_CARD_POWER_UP_STATUS) && !timer_poll_done(ts));

	if (ocr&OCR_CARD_POWER_UP_STATUS) {
		*card_capacity_support_out = ocr&OCR_CARD_CAPACITY_STATUS;
		return CMD_ERROR_NONE;
	}
	serial_log("SD cmd error: app cmd 41: timeout waiting for card to power up");
	return CMD_ERROR_GENERAL_TIMEOUT;
}

enum cmd_error sd_issue_cmd3(int *rca_out)
{
	struct {
		/* First 16 bits is a subset of card_status. */
		bits_t cs_reserved : 3;
		bits_t cs_ake_seq_error : 1;
		bits_t cs_ignore : 9;
		bits_t cs_error : 1;
		bits_t cs_illegal_command : 1;
		bits_t cs_com_crc_error : 1;
		/* End card_status subset. */
		bits_t rca : 16;
	} __attribute__((packed)) resp;
	enum cmd_error error;

	error = sd_issue_cmd(CMD_IDX_SEND_RELATIVE_ADDR, 0);
	if (error == CMD_ERROR_NONE) {
		register_get_out(&sd_access, RESP0, &resp);
		if (resp.cs_ake_seq_error || resp.cs_error || 
		    resp.cs_illegal_command || resp.cs_com_crc_error) {
			serial_log("SD cmd error: cmd 3: error set in response: %08x",
				   cast_bitfields(resp, uint32_t));
			return CMD_ERROR_CARD_STATUS_ERROR;
		}
		*rca_out = resp.rca;
	}
	return error;
}

enum cmd_error sd_issue_cmd7(int rca)
{
	struct ac_rca_args args;

	mzero(&args, sizeof(args));
	args.rca = rca;

	return sd_issue_cmd(CMD_IDX_SELECT_CARD, cast_bitfields(args, uint32_t));
}

enum cmd_error sd_issue_cmd13(int rca, struct card_status *cs_out)
{
	struct ac_rca_args args;
	enum cmd_error error;

	mzero(&args, sizeof(args));
	args.rca = rca;

	error = sd_issue_cmd(CMD_IDX_SEND_STATUS, cast_bitfields(args, uint32_t));
	if (error == CMD_ERROR_NONE) 
		register_get_out(&sd_access, RESP0, cs_out);
	return error;
}

enum cmd_error sd_issue_acmd6(int rca, bool four_bit)
{
	struct {
		enum {
			ACMD6_BUS_WIDTH_1BIT = 0b00,
			ACMD6_BUS_WIDTH_4BIT = 0b10
		} bus_width : 2;
		bits_t unused : 30;
	} __attribute__((packed)) args;

	mzero(&args, sizeof(args));
	args.bus_width = four_bit ? ACMD6_BUS_WIDTH_4BIT : ACMD6_BUS_WIDTH_1BIT;

	return sd_issue_acmd(ACMD_IDX_SET_BUS_WIDTH, cast_bitfields(args, uint32_t), rca);
}

/* Set block size and block count for data transfer. */
static void set_blkszcnt(int blksz, int blkcnt)
{
	struct blksizecnt blkszcnt;

	mzero(&blkszcnt, sizeof(blkszcnt));
	blkszcnt.blksize = blksz;
	blkszcnt.blkcnt = blkcnt;
	register_set(&sd_access, BLKSIZECNT, cast_bitfields(blkszcnt, uint32_t));
}

enum cmd_error sd_issue_read_cmd(enum cmd_index idx, byte_t *ram_dest_addr, void *sd_src_addr, int nblks)
{
	enum cmd_error error;
	/* 
	 * Address of the SD DATA register in RAM. Peripheral access functions 
	 * such as register_get() are avoided for performance reasons.
	 */
	byte_t *sd_data_addr = (byte_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR
					  + sd_access.periph_base_off
					  + sd_access.register_offsets[DATA]);

	set_blkszcnt(READ_BLKSZ, nblks);

	error = sd_issue_cmd(idx, (uint32_t)sd_src_addr);
	if (error != CMD_ERROR_NONE)
		return error;
	/*
	 * Parts of this function are written in assembly for the performance 
	 * improvement. Here the non-scratch registers starting at r4 and 
	 * upwards are used.
	 */
	__asm__("push {r4-r7}\n\t"
		"mov r4, %0\n\t"
		"mov r5, %1"
		: 
		: "r" (sd_data_addr), "r" (ram_dest_addr));

	while (nblks--) {
		error = sd_wait_for_interrupt(INTERRUPT_READ_READY);
		if (error != CMD_ERROR_NONE) 
			goto sd_issue_read_cmd_cleanup;
		/* Copy read block from host buffer to RAM. */
		__asm__("mov r6, #" MSTRFY(READ_BLKSZ) "\n\t"
			"read_data:\n\t"
			"ldr r7, [r4]\n\t"
			"str r7, [r5]\n\t"
			"add r5, r5, #4\n\t"
			"subs r6, r6, #4\n\t"
			"bne read_data");
	}
	/* 
	 * There is no explicit read memory barrier here because it will be done in 
	 * register_get() reading the INTERRUPT register.
	 */
	error = sd_wait_for_interrupt(INTERRUPT_TRANSFER_COMPLETE);
sd_issue_read_cmd_cleanup:
	__asm__("pop {r4-r7}");
	return error;
}

enum cmd_error sd_issue_cmd17(byte_t *ram_dest_addr, void *sd_src_addr)
{
	return sd_issue_read_cmd(CMD_IDX_READ_SINGLE_BLOCK, ram_dest_addr, sd_src_addr, 1);
}

enum cmd_error sd_issue_cmd18(byte_t *ram_dest_addr, void *sd_src_addr, int nblks)
{
	return sd_issue_read_cmd(CMD_IDX_READ_MULTIPLE_BLOCK, ram_dest_addr, sd_src_addr, nblks);
}

enum cmd_error sd_issue_acmd51(int rca, struct scr *scr_out)
{
	enum cmd_error error;	
	uint32_t data;

	mzero(scr_out, sizeof(struct scr));
	/* The actual SCR register is 8 bytes, 4 of which is reserved and we don't need. */
	set_blkszcnt(sizeof(struct scr)+sizeof(uint32_t), 1);

	error = sd_issue_acmd(ACMD_IDX_SD_SEND_SCR, 0, rca);
	if (error != CMD_ERROR_NONE) 
		return error;
	error = sd_wait_for_interrupt(INTERRUPT_READ_READY);
	if (error != CMD_ERROR_NONE) 
		return error;
	data = bswap32(register_get(&sd_access, DATA));
	mcopy(&data, scr_out, sizeof(struct scr));
	/* Throw away the reserved part. */
	data = register_get(&sd_access, DATA);

	return sd_wait_for_interrupt(INTERRUPT_TRANSFER_COMPLETE);
}

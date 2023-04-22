#include "cmd.h"
#include "reg.h"
#include "../help.h"
#include "../timer.h"
#include "../debug.h"// TODO rm
#include "../error.h"// TODO rm
#include "../led.h"// TODO rm

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
struct command commands[] = {
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
	{ ACMD_IDX_SD_SEND_OP_COND,    CMD_TYPE_BCR,  CMD_RESPONSE_R3_OCR_REG }
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
	if (cmd->index == CMD_IDX_READ_SINGLE_BLOCK || cmd->index == CMD_IDX_READ_MULTIPLE_BLOCK)
		cmdtm->data_transfer_direction = CMDTM_TM_DAT_DIR_READ;
	if (cmd->index == CMD_IDX_READ_MULTIPLE_BLOCK) {
		cmdtm->block_cnt_en = true;
		cmdtm->multi_block = true;
		/* TODO Don't do this automatically? also need to check whether card supports CMD23? */
		/*cmdtm->auto_cmd_en = CMDTM_TM_AUTO_CMD_EN_CMD23;*/
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
 * The interrupt flags set from the most recently generated interrupt.
 */
static struct interrupt interrupt;

/* 
 * If the return is zero then timed out waiting for any interrupt.
 *
 * Ensure the 'interrupt' global variable is zeroed before the interrupt
 * has a chance to be triggered (and before calling this) to avoid returning 
 * a false positive.
 */
static struct interrupt sd_wait_for_any_interrupt(void)
{
	struct timestamp ts;
	struct interrupt irpt;

	mzero(&irpt, sizeof(irpt));

	/*
	 * The sleep is done instead of a wfi to avoid a race condition: if the interrupt 
	 * happens after the load instruction to load the interrupt variable, but before 
	 * the wfi instruction, this would get stuck waiting for an interrupt that will 
	 * never trigger. 
	 */
	timer_poll_start(500, &ts);
	do {
		if (cast_bitfields(interrupt, uint32_t))   {
			mcopy(&interrupt, &irpt, sizeof(irpt));
			break;
		}
		usleep(50);
	} while (!timer_poll_done(&ts));

	/* 
	 * Zero global in case subsequent calls to this are chained together. 
	 *
	 * WARNING if the subsequent interrupt triggers before this zeroing
	 * then the interrupt will be lost (race condition).
	 */
	mzero(&interrupt, sizeof(interrupt));

	return irpt;
}

/*
 * Wait for a particular interrupt. 
 * @interrupt: bit mask for the interrupt's field in the INTERRUPT register
 */
enum cmd_error sd_wait_for_interrupt(int interrupt_mask)
{
	struct interrupt irpt = sd_wait_for_any_interrupt();

	if (!cast_bitfields(irpt, uint32_t))
		return CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT;
	if (interrupt.error) {
		/* 
		 * Note might be worth checking which error it was and then retrying 
		 * instead of failing. 
		 */
		return CMD_ERROR_INTERRUPT_ERROR;
	}
	if (!(cast_bitfields(irpt, uint32_t)&interrupt_mask)) 
		return CMD_ERROR_EXPECTED_INTERRUPT_NOT_TRIGGERED;
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
	return  cs->ake_seq_error || cs->wp_erase_skip || cs->csd_overwrite ||
		cs->error || cs->cc_error || cs->card_ecc_failed || cs->illegal_command ||
		cs->com_crc_error || cs->lock_unlock_failed || cs->wp_violation || 
		cs->erase_param || cs->erase_seq_error || cs->block_len_error || 
		cs->address_error || cs->out_of_range;
}

enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args)
{
	struct command *cmd;
	struct cmdtm cmdtm;
	uint32_t status;
	enum cmd_error error;

	cmd = get_command(idx);
	if (!cmd) 
		return CMD_ERROR_COMMAND_UNIMPLEMENTED;
	status = register_get(&sd_access, STATUS);
	if (status&STATUS_COMMAND_INHIBIT_CMD) 
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;
	/* Note if implement an abort command such as CMD12 then need to let it skip this step. */
	if ((cmd->type == CMD_TYPE_ADTC || cmd->response == CMD_RESPONSE_R1B_NORMAL_BUSY) && 
	    status&STATUS_COMMAND_INHIBIT_DAT) 
		return CMD_ERROR_COMMAND_INHIBIT_DAT_BIT_SET;

	/* Set the command's arguments. Note if implement ACMD23 it needs to use ARG2 instead. */
	register_set(&sd_access, ARG1, args);

	set_cmdtm(cmd, &cmdtm);
	mzero(&interrupt, sizeof(interrupt));

	/* Issue the command, which should trigger an interrupt. */
	register_set_ptr(&sd_access, CMDTM, &cmdtm);
	error = sd_wait_for_interrupt(INTERRUPT_CMD_COMPLETE);

	/* For cards with a card status response check for any card status error bits. */
	if (error == CMD_ERROR_NONE && sd_cmd_has_card_status_response(cmd)) {
		struct card_status cs;
		register_get_out(&sd_access, RESP0, &cs);
		if (sd_card_status_error_bit_set(&cs))
			return CMD_ERROR_CARD_STATUS_ERROR;;
	}
	return error;
}

void sd_isr(void)
{
	/* Set global for use by non-ISR code. */
	register_get_out(&sd_access, INTERRUPT, &interrupt);
	/* Clear all triggered interrupts. */
	register_set_ptr(&sd_access, INTERRUPT, &interrupt);
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
	if (!cs.app_cmd)
		return CMD_ERROR_RESPONSE_CONTENTS;
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
		if (!mcmp(&args, &resp, sizeof(args)))
			error = CMD_ERROR_RESPONSE_CONTENTS;
	}
	return error;
}

/* OCR register fields. */
/* Voltage window. */
#define OCR_VDD_2V7_TO_3V6		0x00ff8000
/* 0 is SDSC, 1 is SDHC/SDXC. */
#define OCR_CARD_CAPACITY_STATUS	0x40000000
/* Whether card power up procedure has finished. */
#define OCR_CARD_POWER_UP_STATUS	0x80000000

/* ACMD41 argument fields. */
/* Whether the host supports SDHC/SDXC. */
#define ACMD41_HOST_CAPACITY_SUPPORT_SHIFT 30

enum cmd_error sd_issue_acmd41(bool host_capacity_support, bool *card_capacity_support_out)
{
	uint32_t args, ocr;
	enum cmd_error error;
	struct timestamp ts;
	/* Card has a default RCA of 0 when in idle state. */
	int rca = 0;
	int i = 0;

	/* 
	 * When args voltage window is zero ACMD41 is inquiry ACMD41. When args voltage window is 
	 * non-zero ACMD41 is init ACMD41. Do inquiry first so can get the card's voltage window, 
	 * since if it doesn't support the voltage window the init ACMD41 would put the card into 
	 * the inactive state.
	 * TOD confirm when get a card
	 */
	mzero(&args, sizeof(args));
	error = sd_issue_acmd(ACMD_IDX_SD_SEND_OP_COND, args, rca);
	if (error != CMD_ERROR_NONE)
		return error;
	register_get_out(&sd_access, RESP0, &ocr);
	/* Assert the card supports the voltage range. */
	if (ocr&OCR_VDD_2V7_TO_3V6 != OCR_VDD_2V7_TO_3V6)
		return CMD_ERROR_RESPONSE_CONTENTS;

	/* Set args for init ACMD41. */
	args |= OCR_VDD_2V7_TO_3V6;
	args |= host_capacity_support<<ACMD41_HOST_CAPACITY_SUPPORT_SHIFT;

	/* Wait for card to finish power up, which should take at most 1 second from the first init ACMD41. */
	timer_poll_start(1000, &ts);
	do {
		if (i++)
			sleep(20);
		error = sd_issue_acmd(ACMD_IDX_SD_SEND_OP_COND, args, rca);
		if (error != CMD_ERROR_NONE)
			return error;
		register_get_out(&sd_access, RESP0, &ocr);
	} while (!(ocr&OCR_CARD_POWER_UP_STATUS) && !timer_poll_done(&ts));

	if (ocr&OCR_CARD_POWER_UP_STATUS) {
		*card_capacity_support_out = ocr&OCR_CARD_CAPACITY_STATUS;
		return CMD_ERROR_NONE;
	}
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
	} __attribute((packed)) resp;
	enum cmd_error error;

	error = sd_issue_cmd(CMD_IDX_SEND_RELATIVE_ADDR, 0);
	if (error == CMD_ERROR_NONE) {
		register_get_out(&sd_access, RESP0, &resp);
		if (resp.cs_ake_seq_error || resp.cs_error || 
		    resp.cs_illegal_command || resp.cs_com_crc_error)
			return CMD_ERROR_CARD_STATUS_ERROR;
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

enum cmd_error sd_issue_read_cmd(enum cmd_index idx, byte_t *ram_dest_addr, void *sd_src_addr, int nblks)
{
	enum cmd_error error;
	uint32_t data;
	int bytes_remaining;
	int bytes_read = sizeof(data);

	error = sd_issue_cmd(idx, (uint32_t)sd_src_addr);
	if (error != CMD_ERROR_NONE)
		return error;
	/*
	 * WARNING the wait for interrupts done here after sd_issue_cmd() are subject 
	 * to race conditions. Although unlikely to happen, the race conditions can
	 * be prevented by polling for triggered interrupts instead of servicing them.
	 */
	while (nblks--) {
		error = sd_wait_for_interrupt(INTERRUPT_READ_READY);
		if (error != CMD_ERROR_NONE)
			return error;
		/* Copy read block from host buffer to RAM. */
		bytes_remaining = READ_BLKSZ;
		while (bytes_remaining > 0) {
			data = register_get(&sd_access, DATA);

			/* 
			 * The default read block size is a multiple of the size of the DATA 
			 * register in bytes, so it's always safe to copy all of what was read.
			 */
			mcopy(&data, ram_dest_addr, bytes_read);

			ram_dest_addr += bytes_read;
			bytes_remaining -= bytes_read;
		}
	}
	return sd_wait_for_interrupt(INTERRUPT_TRANSFER_COMPLETE);
}

enum cmd_error sd_issue_cmd17(byte_t *ram_dest_addr, void *sd_src_addr)
{
	return sd_issue_read_cmd(CMD_IDX_READ_SINGLE_BLOCK, ram_dest_addr, sd_src_addr, 1);
}

enum cmd_error sd_issue_cmd18(byte_t *ram_dest_addr, void *sd_src_addr, int nblks)
{
	return sd_issue_read_cmd(CMD_IDX_READ_MULTIPLE_BLOCK, ram_dest_addr, sd_src_addr, nblks);
}

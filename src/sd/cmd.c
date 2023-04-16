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
 * @CMD_TYPE_BC: broadcast: broadcast the command to all cards. 
 *	Doesn't expect a response.
 * @CMD_TYPE_BCR: broadcast response: broadcast the command to all cards.
 *	 Does expect a response.
 * @CMD_TYPE_AC: addressed command: send a command to the addressed card. 
 *	No data transfer.
 * @CMD_TYPE_ADTC: addressed data transfer command: send a command to the 
 *	addressed card. Transfers data.
 * TODO where are these used? if not needed delete
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
 *	or can that be done in the cmd?
 * TODO explain what these actually are?
 */
enum response {
	RESPONSE_NONE,
	RESPONSE_R1_NORMAL,
	RESPONSE_R1B_NORMAL_BUSY,
	RESPONSE_R2_CID_OR_CSD_REG,
	RESPONSE_R3_OCR_REG,
	RESPONSE_R6_PUBLISHED_RCA,
	RESPONSE_R7_CARD_INTERFACE_CONDITION,
};

// TODO define the enum's that are only used from within this struct, in the struct itself?
/*
 * A command to control the SD card.
 */
struct command {
	enum cmd_index index;
	enum command_type type;
	enum response response;
};

struct command commands[] = {
	{ CMD_IDX_GO_IDLE_STATE,      CMD_TYPE_BC,   RESPONSE_NONE },
	{ CMD_IDX_ALL_SEND_CID,       CMD_TYPE_BCR,  RESPONSE_R2_CID_OR_CSD_REG },
	{ CMD_IDX_SEND_RELATIVE_ADDR, CMD_TYPE_BCR,  RESPONSE_R6_PUBLISHED_RCA },
	{ CMD_IDX_SELECT_CARD,        CMD_TYPE_AC,   RESPONSE_R1B_NORMAL_BUSY },
	{ CMD_IDX_SEND_IF_COND,       CMD_TYPE_BCR,  RESPONSE_R7_CARD_INTERFACE_CONDITION },
	{ CMD_IDX_SEND_STATUS,        CMD_TYPE_AC,   RESPONSE_R1_NORMAL },
	{ CMD_IDX_READ_SINGLE_BLOCK,  CMD_TYPE_ADTC, RESPONSE_R1_NORMAL },
	{ CMD_IDX_APP_CMD,            CMD_TYPE_AC,   RESPONSE_R1_NORMAL },
	{ ACMD_IDX_SET_BUS_WIDTH,     CMD_TYPE_AC,   RESPONSE_R1_NORMAL },
	{ ACMD_IDX_SD_SEND_OP_COND,   CMD_TYPE_BCR,  RESPONSE_R3_OCR_REG }
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
 * Set size of response from response type. 
 */
static void sd_set_cmdtm_response(struct command *cmd, struct cmdtm *cmdtm)
{
	switch (cmd->response) {
		case RESPONSE_NONE:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_NONE;
			break;
		case RESPONSE_R1_NORMAL:
		case RESPONSE_R3_OCR_REG:
		case RESPONSE_R6_PUBLISHED_RCA:
		case RESPONSE_R7_CARD_INTERFACE_CONDITION:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_48_BITS;
			break;
		case RESPONSE_R1B_NORMAL_BUSY:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_48_BITS_BUSY;
			break;
		case RESPONSE_R2_CID_OR_CSD_REG:
			cmdtm->response_type = CMDTM_RESPONSE_TYPE_136_BITS;
			break;
	}
}

/* 
 * Set index check enable and CRC check enable from response.
 * If a case doesn't set one of these it means it's supposed to be disabled
 * (and it's already been disabled from zeroing).
 */
static void sd_set_cmdtm_idx_and_crc_chk(struct command *cmd, struct cmdtm *cmdtm)
{
	switch (cmd->response) {
		case RESPONSE_NONE:
		case RESPONSE_R3_OCR_REG:
			break; 
		case RESPONSE_R2_CID_OR_CSD_REG:
			cmdtm->cmd_crc_chk_en = true;
			break;
		case RESPONSE_R1_NORMAL:
		case RESPONSE_R1B_NORMAL_BUSY:
		case RESPONSE_R6_PUBLISHED_RCA:
		case RESPONSE_R7_CARD_INTERFACE_CONDITION:
			cmdtm->cmd_index_chk_en = true;
			cmdtm->cmd_crc_chk_en = true;
			break;
	}
}

/* 
 * Set the fields of the transfer mode register relevant to the command.
 * The transfer mode register is the lower 16 bits of cmdtm.
 */
static void sd_set_cmdtm_transfer_mode(struct command *cmd, struct cmdtm *cmdtm)
{
	/* As more commands are implemented they will need to be added here. */
	if (cmd->index == CMD_IDX_READ_SINGLE_BLOCK)
		cmdtm->data_transfer_direction = CMDTM_TM_DAT_DIR_READ;
}

/*
 * Set the fields of the cmdtm register required to issue the
 * given command.
 */
static void sd_set_cmdtm(struct command *cmd, struct cmdtm *cmdtm)
{
	mzero(cmdtm, sizeof(struct cmdtm));

	cmdtm->cmd_index = cmd->index & ~IS_APP_CMD;
	sd_set_cmdtm_response(cmd, cmdtm);
	sd_set_cmdtm_idx_and_crc_chk(cmd, cmdtm);
	/* 
	 * Addressed data transfer commands which transfer data on DAT will 
	 * (of course) have data present on the DAT line.
	 */
	if (cmd->type == CMD_TYPE_ADTC)
		cmdtm->data_present = true;
	sd_set_cmdtm_transfer_mode(cmd, cmdtm);
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
	 * TODO note the race condition here
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
	if (interrupt.error) 
		/* TODO check which error it was instead and retry instead of failing? */
		return CMD_ERROR_ERROR_INTERRUPT;
	/* TODO need to cast to uint32_t? */
	if (!(cast_bitfields(irpt, uint32_t)&interrupt_mask)) 
		return CMD_ERROR_NOT_EXPECTED_INTERRUPT;
	return CMD_ERROR_NONE;
}

enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args)
{
	struct command *cmd;
	struct cmdtm cmdtm;
	bool timeout;
	uint32_t status;

	cmd = get_command(idx);
	if (!cmd) 
		return CMD_ERROR_COMMAND_UNIMPLEMENTED;
	status = register_get(&sd_access, STATUS);
	if (status&STATUS_COMMAND_INHIBIT_CMD) 
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;
	/* 
	 * Note if implement an abort command such as CMD12 then need to let it skip this step
	 * (and then delete this comment).
	 */
	if ((cmd->type == CMD_TYPE_ADTC || cmd->response == RESPONSE_R1B_NORMAL_BUSY) && 
	    status&STATUS_COMMAND_INHIBIT_DAT) 
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;

	/* Set the command's arguments. Note if implement ACMD23 it needs to use ARG2 instead. */
	register_set(&sd_access, ARG1, args);

	sd_set_cmdtm(cmd, &cmdtm);
	mzero(&interrupt, sizeof(interrupt));

	/* Issue the command, which should trigger an interrupt. */
	register_set_ptr(&sd_access, CMDTM, &cmdtm);
	return sd_wait_for_interrupt(INTERRUPT_CMD_COMPLETE);
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
	/* TODO this was rca 16, unused 16 (wrong?). don't know how it worked before. */
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
	/* TODO does cs.error cover all errors like the interrupt error does? */
	if (cs.error || !cs.app_cmd)
		return CMD_ERROR_RESPONSE_CONTENTS;
	return sd_issue_cmd(idx, args);
}

enum cmd_error sd_issue_cmd8(void)
{
	struct {
		bits_t check_pattern : 8;
		enum {
			/* TODO rm unused? */
			CMD8_VHS_UNDEFINED  = 0x0,
			CMD8_VHS_2V7_TO_3V6 = 0x1,  /* 2.7 to 3.6V */
			CMD8_VHS_RESERVED_FOR_LOW_VOLTAGE_RANGE = 0x2,
			CMD8_VHS_RESERVED1  = 0x4,
			CMD8_VHS_RESERVED2  = 0x8
		} supply_voltage : 4;
		bits_t reserved : 20;
	} __attribute__((packed)) args, resp;
	enum cmd_error error;

	mzero(&args, sizeof(args));
	args.check_pattern = 0b10101010;
	args.supply_voltage = CMD8_VHS_2V7_TO_3V6;  /* 3.3V */

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
		bits_t unused : 16;
		bits_t rca : 16;
	} __attribute((packed)) resp;
	enum cmd_error error;

	error = sd_issue_cmd(CMD_IDX_SEND_RELATIVE_ADDR, 0);
	if (error == CMD_ERROR_NONE) {
		register_get_out(&sd_access, RESP0, &resp);
		*rca_out = resp.rca;
	}
	return error;
}

enum cmd_error sd_issue_cmd7(int rca)
{
	struct ac_rca_args args;
	enum cmd_error error;

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
	enum cmd_error error;

	mzero(&args, sizeof(args));
	args.bus_width = four_bit ? ACMD6_BUS_WIDTH_4BIT : ACMD6_BUS_WIDTH_1BIT;

	return sd_issue_acmd(ACMD_IDX_SET_BUS_WIDTH, cast_bitfields(args, uint32_t), rca);
	// TODO need to check error in R1 response?
}

enum cmd_error sd_issue_cmd17(byte_t *ram_dest_addr, byte_t *sd_src_addr)
{
	enum cmd_error error;
	bool timeout;
	uint32_t data;
	int bytes_remaining = DEFAULT_READ_BLKSZ;
	int bytes_read = sizeof(data);

	error = sd_issue_cmd(CMD_IDX_READ_SINGLE_BLOCK, (uint32_t)sd_src_addr);
	/* TODO check response?*/
	if (error != CMD_ERROR_NONE)
		return error;
	/* TODO race condition here? */
	error = sd_wait_for_interrupt(INTERRUPT_READ_READY);
	if (error != CMD_ERROR_NONE)
		return error;
	// TODO do transfer here or in wrapper?
	/* Copy read block from host buffer to RAM. */
	while (bytes_remaining > 0) {
		data = register_get(&sd_access, DATA);

		/* 
		 * The default read block size is a multiple of the size of the DATA 
		 * register in bytes, so it's always safe to copy all of what was read.
		 * TODO need to worry about endianness here? (refer sdhost spec pg ~17)
		 */
		mcopy(&data, ram_dest_addr, bytes_read);

		ram_dest_addr += bytes_read;
		bytes_remaining -= bytes_read;
	}
	/* TODO race condition here? */
	return sd_wait_for_interrupt(INTERRUPT_TRANSFER_COMPLETE);
}

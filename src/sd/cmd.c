#include "cmd.h"
#include "reg.h"
#include "../help.h"
#include "../timer.h"
#include "../debug.h"// TODO rm
#include "../error.h"// TODO rm
#include "../led.h"// TODO rm

/* 
 * Card status response from RESPONSE_R1_NORMAL. 
 * @app_cmd: signals whether it expects the next issued command to
 *	be an application command
 * TODO mv this somewhere else?
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
	{ CMD_IDX_GO_IDLE_STATE,      CMD_TYPE_BC,  RESPONSE_NONE },
	{ CMD_IDX_ALL_SEND_CID,       CMD_TYPE_BCR, RESPONSE_R2_CID_OR_CSD_REG },
	{ CMD_IDX_SEND_RELATIVE_ADDR, CMD_TYPE_BCR, RESPONSE_R6_PUBLISHED_RCA },
	{ CMD_IDX_SEND_IF_COND,       CMD_TYPE_BCR, RESPONSE_R7_CARD_INTERFACE_CONDITION },
	{ CMD_IDX_APP_CMD,            CMD_TYPE_AC,  RESPONSE_R1_NORMAL },
	{ ACMD_IDX_SD_SEND_OP_COND,   CMD_TYPE_BCR, RESPONSE_R3_OCR_REG }
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
 * Set the fields of the cmdtm register required to issue the
 * given command.
 */
static void sd_set_cmdtm(struct command *cmd, struct cmdtm *cmdtm)
{
	mzero(cmdtm, sizeof(struct cmdtm));

	cmdtm->cmd_index = cmd->index & ~IS_APP_CMD;

	// TODO put in funcs?
	/* Set size of response from response type. */
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

	/* 
	 * Set index check enable and CRC check enable from response.
	 * If a case doesn't set one of these it means it's supposed to be disabled
	 * (and it's already been disabled from zeroing).
	 */
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
 * The interrupt flags set from the most recently generated interrupt.
 */
static struct interrupt interrupt;

/* 
 * Return false if timed out waiting for an interrupt. 
 * If successful (returned true) check the 'interrupt' global variable.
 */
static bool sd_wait_for_interrupt(void)
{
	struct timestamp ts;

	/*
	 * The sleep is done instead of a wfi to avoid a race condition: if the interrupt 
	 * happens after the load instruction to load the interrupt variable, but before 
	 * the wfi instruction, this would get stuck waiting for an interrupt that will 
	 * never trigger. 
	 */
	timer_poll_start(600, &ts);
	do {
		if (cast_bitfields(interrupt, uint32_t))  
			return true;
		usleep(50);
	} while (!timer_poll_done(&ts));

	return false;
}

enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args)
{
	/* 
	 * TODO current implementation is only for no data transfer commands.
	 * will need to refactor later on when using data transfer commands.
	 */
	struct command *cmd;
	struct cmdtm cmdtm;
	bool timeout;

	cmd = get_command(idx);
	if (!cmd) 
		return CMD_ERROR_COMMAND_UNIMPLEMENTED;
	if (register_get(&sd_access, STATUS)&STATUS_COMMAND_INHIBIT_CMD) 
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;
	/* 
	 * TODO if issuing a SD cmd with busy signal that isn't the (an?) abort command
	 * then need to check command inhibit DAT line as well 
	 */

	/* TODO need to use ARG2 if it's ACMD23 */
	/* Set the command's arguments. */
	register_set(&sd_access, ARG1, args);

	sd_set_cmdtm(cmd, &cmdtm);
	mzero(&interrupt, sizeof(interrupt));

	/* Issue the command, which should trigger an interrupt. */
	register_set_ptr(&sd_access, CMDTM, &cmdtm);
	timeout = !sd_wait_for_interrupt();
	if (timeout)
		return CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT;
	if (interrupt.error) 
		/* TODO check which error it was instead and retry instead of failing? */
		return CMD_ERROR_ERROR_INTERRUPT;
	if (!interrupt.cmd_complete) 
		return CMD_ERROR_NO_COMMAND_COMPLETE_INTERRUPT;
	/* TODO if cmd uses transfer complete interrupt then need to wait for that */
	return CMD_ERROR_NONE;
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
	struct {
		bits_t rca : 16;
		bits_t unused : 16;
	} __attribute__((packed)) cmd55_args;
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
	} resp;
	enum cmd_error error;

	error = sd_issue_cmd(CMD_IDX_SEND_RELATIVE_ADDR, 0);
	if (error == CMD_ERROR_NONE) {
		register_get_out(&sd_access, RESP0, &resp);
		*rca_out = resp.rca;
	}
	return error;
}

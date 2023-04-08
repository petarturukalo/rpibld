#include "cmd.h"
#include "reg.h"
#include "../help.h"
#include "../timer.h"
#include "../debug.h"// TODO rm

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
	enum command_index index;
	enum command_type type;
	enum response response;
};

struct command commands[] = {
	{ CMD_IDX_GO_IDLE_STATE, CMD_TYPE_BC,  RESPONSE_NONE },
	{ CMD_IDX_SEND_IF_COND,  CMD_TYPE_BCR, RESPONSE_R7_CARD_INTERFACE_CONDITION }
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
 * Set the fields of the cmdtm register required to issue the
 * given command.
 */
static void sd_set_cmdtm(struct command *cmd, struct cmdtm *cmdtm)
{
	mzero(cmdtm, sizeof(struct cmdtm));

	cmdtm->cmd_index = cmd->index;

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
	/*
	 * The sleep is done instead of a wfi to avoid a race condition: if the interrupt 
	 * happens after the load instruction to load the interrupt variable, but before 
	 * the wfi instruction, this would get stuck waiting for an interrupt that will 
	 * never trigger. 
	 */
	timer_poll_start(600);
	do {
		if (cast_bitfields(interrupt, uint32_t))  {
			timer_poll_stop();
			return true;
		}
		usleep(20);
	} while (!timer_poll_done());

	return false;
}

enum command_error sd_issue_command(enum command_index idx, uint32_t args)
{
	/* 
	 * TODO current implementation is only for no data transfer commands.
	 * will need to refactor later on when using data transfer commands.
	 */
	struct command *cmd;
	struct cmdtm cmdtm;
	bool timeout;

	if (register_get(&sd_access, STATUS)&STATUS_COMMAND_INHIBIT_CMD_MASK) 
		return CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET;
	/* 
	 * TODO if issuing a SD cmd with busy signal that isn't the (an?) abort command
	 * then need to check command inhibit DAT line as well 
	 */
	cmd = get_command(idx);
	if (!cmd) 
		return CMD_ERROR_COMMAND_UNIMPLEMENTED;
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

enum command_error sd_issue_cmd8(void)
{
	struct {
		bits_t check_pattern : 8;
		enum {
			CMD8_VHS_UNDEFINED  = 0x0,
			CMD8_VHS_2V7_TO_3V6 = 0x1,  /* 2.7 to 3.6V */
			CMD8_VHS_RESERVED_FOR_LOW_VOLTAGE_RANGE = 0x2,
			CMD8_VHS_RESERVED1  = 0x4,
			CMD8_VHS_RESERVED2  = 0x8
		} supply_voltage : 4;
		bits_t reserved : 20;
	} __attribute__((packed)) args, resp;
	enum command_error error;

	mzero(&args, sizeof(args));
	args.check_pattern = 0b10101010;
	args.supply_voltage = CMD8_VHS_2V7_TO_3V6;  /* 3.3V */

	error = sd_issue_command(CMD_IDX_SEND_IF_COND, cast_bitfields(args, uint32_t));
	if (error == CMD_ERROR_NONE) {
		/* Assert sent voltage range and check pattern are echoed back in the response. */
		mzero(&resp, sizeof(resp));
		register_get_out(&sd_access, RESP0, &resp);
		if (!mcmp(&args, &resp, sizeof(args)))
			error = CMD_ERROR_RESPONSE_CONTENTS;
	}
	return error;
}

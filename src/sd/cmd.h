#ifndef CMD_H
#define CMD_H

#include "../type.h"

/* A cmd_index has this bet set if it's an application command. */
#define IS_APP_CMD 0x80

/*
 * Index identifier for a command. 
 */
enum cmd_index {
	CMD_IDX_GO_IDLE_STATE      = 0,
	CMD_IDX_ALL_SEND_CID       = 2,
	CMD_IDX_SEND_RELATIVE_ADDR = 3,
	CMD_IDX_SEND_IF_COND       = 8,  /* Send interface condition. */
	CMD_IDX_APP_CMD            = 55,
/* Application commands. */
	ACMD_IDX_SD_SEND_OP_COND   = 41|IS_APP_CMD  /* Send operating condition register. */
};

/*
 * @CMD_ERROR_ERROR_INTERRUPT: one of the error interrupts was flagged
 * @CMD_ERROR_NO_COMMAND_COMPLETE_INTERRUPT: did not receive a command
 *	complete interrupt
 * @CMD_ERROR_RESPONSE_CONTENTS: contents of command response not as expected
 */
enum cmd_error {
	CMD_ERROR_NONE,
	CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET,
	CMD_ERROR_COMMAND_UNIMPLEMENTED,
	CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT,
	CMD_ERROR_ERROR_INTERRUPT,
	CMD_ERROR_NO_COMMAND_COMPLETE_INTERRUPT,
 /* Above are returns from sd_issue_cmd(). Below are extra returns from sd_issue_cmd() wrappers. */
	CMD_ERROR_RESPONSE_CONTENTS,
	CMD_ERROR_GENERAL_TIMEOUT
};

/*
 * Issue a command to the SD card. If the return is successful (CMD_ERROR_NONE)
 * and the command expects a response, check the RESP* registers for the response.
 *
 * @idx: index of a standard (not application) command
 * @args: arguments optionally used by the command
 */
enum cmd_error sd_issue_cmd(enum cmd_index idx, uint32_t args);
/* 
 * @idx: index of an application command
 * @rca: relative card address used as argument to CMD55
 *
 * Return CMD_ERROR_RESPONSE_CONTENTS if an error was identified in
 * the CMD55 response or its APP_CMD bit was not set.
 */
enum cmd_error sd_issue_acmd(enum cmd_index idx, uint32_t args, int rca);

// TODO document?
void sd_isr(void);

/* TODO should wrappers of sd_issue_cmd() go here or in sd.c? */

/*
 * Issue command 8 to the SD card. If successful the card can operate on the 
 * 2.7-3.6V host supply voltage.
 *
 * Return CMD_ERROR_RESPONSE_CONTENTS if the voltage isn't supported or there
 * was an error with the response check pattern.
 */
enum cmd_error sd_issue_cmd8(void);

/*
 * Issue application command 41 to the SD card. If successful the card is powered up.
 *
 * @host_capacity_support: whether the host supports SDHC/SDXC. This should be false if the
 *	card did not respond to CMD8.
 * @card_capacity_support_out: out-param card capacity support, whether card is SDHC/SDXC (true) or SDSC (false).
 *	Only valid on success.
 *
 * Return
 * - CMD_ERROR_RESPONSE_CONTENTS voltage range not supported in card's OCR register, or there was
 *	an error from sd_issue_acmd()
 * - CMD_ERROR_GENERAL_TIMEOUT if the card did not power up in 1 second
 */
enum cmd_error sd_issue_acmd41(bool host_capacity_support, bool *card_capacity_support_out);

/*
 * Issue command 3 to the SD card. If successful the card's new published relative
 * card address is stored in out-param rca_out.
 */
enum cmd_error sd_issue_cmd3(int *rca_out);

#endif

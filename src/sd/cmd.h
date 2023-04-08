#ifndef CMD_H
#define CMD_H

#include "../type.h"

/*
 * Index identifier for a command. 
 * TODO make this explanation better or put it in struct command?
 */
enum command_index {
	CMD_IDX_GO_IDLE_STATE = 0,
	CMD_IDX_SEND_IF_COND  = 8  /* Send interface condition. */
};

/*
 * @CMD_ERROR_ERROR_INTERRUPT: one of the error interrupts was flagged
 * @CMD_ERROR_NO_COMMAND_COMPLETE_INTERRUPT: did not receive a command
 *	complete interrupt
 * @CMD_ERROR_RESPONSE_CONTENTS: contents of command response not as expected
 */
enum command_error {
	CMD_ERROR_NONE,
	CMD_ERROR_COMMAND_INHIBIT_CMD_BIT_SET,
	CMD_ERROR_COMMAND_UNIMPLEMENTED,
	CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT,
	CMD_ERROR_ERROR_INTERRUPT,
	CMD_ERROR_NO_COMMAND_COMPLETE_INTERRUPT,
/* TODO above are return from sd_issue_command(), below are extra from wrappers? */
	CMD_ERROR_RESPONSE_CONTENTS
};

/*
 * Issue a command to the SD card. If the return is successful (CMD_ERROR_NONE)
 * and the command expects a response, check the RESP* registers for the response.
 *
 * @args: arguments optionally used by the command
 */
enum command_error sd_issue_command(enum command_index idx, uint32_t args);
// TODO document?
void sd_isr(void);

/* TODO should wrappers of sd_issue_command() go here or in sd.c? */

/*
 * Issue command 8 to the SD card. 
 * TODO don't worry about having this doc here - have it document itself where
 * this is called?
 * - A return of CMD_ERROR_NONE means the card supports the physical layer 
 * specification version 2.00 or later
 * - A return of CMD_ERROR_NONE means the card supports 3.3V
 * - A return of CMD_ERROR_RESPONSE_CONTENTS means the card supports the physical
 *   layer specification version 2.00 or later but either doesn't support the voltage
 *   or there was a CRC error
 * - A return of CMD_ERROR_WAIT_FOR_INTERRUPT_TIMEOUT means the card doesn't support
 *	the physical layer specification version 2.00 or later. TODO maybe not? need
 *	to buy an old card SDSC card to test this
 */
enum command_error sd_issue_cmd8(void);

#endif

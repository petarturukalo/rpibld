#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"
#include "mmc.h" // TODO rm
#include "led.h" // TODO rm
#include "debug.h" // TODO rm
#include "vcmailbox.h" // TODO rm
#include "help.h" // TODO rm

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enum vcmailbox_error err;
	struct tag_io_hw_get_board_rev rev;

	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();

	/*mmc_init();*/
	/*mmc_trigger_dummy_interrupt();*/

	err = vcmailbox_request_tags((struct tag_request[]){ HW_GET_BOARD_REV, &rev }, 1);
	if (err != VCMBOX_ERROR_NONE)
		signal_error(err);
	uint32_t val = 0;
	mcopy(&rev, &val, sizeof(uint32_t));
	print_word(val);

	__asm__("wfi");
}

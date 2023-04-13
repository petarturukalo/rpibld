#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"
#include "sd/sd.h" // TODO rm
#include "led.h" // TODO rm
#include "debug.h" // TODO rm
#include "timer.h" // TODO rm

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();

	sd_init();
	signal_error(1);
	/*sleep(1000);*/
	/*sd_trigger_dummy_interrupt();*/

	__asm__("wfi");
}

#include "led.h"  // TODO rm
#include "int.h"
#include "gic.h"
#include "ic.h"
#include "mmio.h"  // TODO rm
#include "timer.h"  // TODO rm
#include "debug.h"  // TODO rm

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();
	led_init();
	sleep(1000);
	led_turn_on();
	sleep(1000);
	led_turn_off();
	sleep(1000);
	led_turn_on();
	sleep(3000);
	led_turn_off();
	sleep(1000);
	led_turn_on();
	sleep(1000);
	led_turn_off();
}

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
	sleep(1000);
	led_init();
	for (int i = 0; ; ++i) {
		if (i%2 == 0) 
			led_turn_on();
		else
			led_turn_off();
		sleep(1000);
	}
}

#include "led.h"

/*
 * Entry point to the C code, the function branched to when switching from 
 * assembly to C.
 */
void c_entry(void)
{
	led_init();
	led_turn_on();
}

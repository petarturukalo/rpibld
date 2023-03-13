#include "debug.h"
#include "led.h"

void sleep_short(void)
{
	for (int i = 0; i < 1200000; ++i) 
		;
}

void sleep_long(void)
{
	for (int i = 0; i < 4000000; ++i) 
		;
}

void print_word(word_t word)
{
	for (int i = 0; i < 32; ++i) {
		/* 0 is 1 beep, 1 is 2 beeps */
		if (word&(1<<i)) {
			led_turn_on();
			sleep_short();
			led_turn_off();
			sleep_short();
			led_turn_on();
			sleep_short();
			led_turn_off();
		} else {
			led_turn_on();
			sleep_short();
			led_turn_off();
		}
		sleep_long();
	}
}

void print_cpsr(void)
{
	int n = 0;
	__asm__("mrs %0, cpsr"
		: "=r" (n));
	print_word(n);
}

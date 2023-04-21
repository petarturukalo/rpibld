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

void print_bit(uint32_t word, int bit)
{
	/* 0 is 1 beep, 1 is 2 beeps */
	if (word&(1<<bit)) {
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
}

void print_word(uint32_t word)
{
	for (int i = 0; i < 32; ++i) {
		print_bit(word, i);
		sleep_long();
	}
}

void print_byte(char byte)
{	
	for (int i = 0; i < 8; ++i) {
		print_bit(byte, i);
		sleep_long();
	}
}

void print_word_reverse(uint32_t word)
{
	for (int i = 31; i >= 0; --i) {
		print_bit(word, i);
		sleep_long();
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

/*
 * From the BCM2711 RPI 4 B device tree source the ACT LED is accessible through pin 42
 * and has normal polarity (is active high).
 */
#include "led.h"
#include "mmio.h"

enum gpio_register {
	GPFSEL4,
	GPSET1,
	GPCLR1
};

struct periph_access gpio_access = {
	.periph_base_off = 0x2200000,
	.register_offsets = {
		[GPFSEL4] = 0x10,
		[GPSET1]  = 0x20,
		[GPCLR1]  = 0x2c,
		[UNNAMED] = 0xd0
	}
};

void led_init(void)
{
	/* Set bits 8:6 of the GPFSEL4 register to 001, selecting pin 42 to be an output. */
	register_enable_bits(&gpio_access, GPFSEL4, 1<<6);
}

void led_turn_on(void)
{
	 /* Set the value of pin 42 to 1. Because the pin is active high this will turn on the LED. */
	register_set(&gpio_access, GPSET1, 1<<10);
}

void led_turn_off(void) 
{
	/* Set the value of pin 42 to 0. */
	register_set(&gpio_access, GPCLR1, 1<<10);
}


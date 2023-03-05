/*
 * From the BCM2711 RPI 4 B device tree source the ACT LED is accessible through pin 42
 * and has normal polarity (is active high).
 */
#include "led.h"
#include "mmio.h"

// TODO is init needed before switching between turning on or off or is it only needed once?
void led_init(void)
{
	/* Set bits 8:6 of the GPFSEL4 register to 001, selecting pin 42 to be an output. */
	register_set(PERIPH_BASE_OFF_GPIO, REG_OFF_GPIO_GPFSEL4, 1<<6);
}

void led_turn_on(void)
{
	 /* Set the value of pin 42 to 1. Because the pin is active high this will turn on the LED. */
	register_set(PERIPH_BASE_OFF_GPIO, REG_OFF_GPIO_GPSET1, 1<<10);
}

void led_turn_off(void) 
{
	/* Set the value of pin 42 to 0. */
	register_set(PERIPH_BASE_OFF_GPIO, REG_OFF_GPIO_GPCLR1, 1<<10);
}


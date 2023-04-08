/*
 * Control the green-coloured LED labelled 
 * "ACT" on the raspberry pi board.
 */
#ifndef LED_H
#define LED_H

// TODO mv back to .c
enum gpio_registers {
	GPFSEL4,
	GPSET1,
	GPCLR1,
	UNNAMED
};
extern struct periph_access gpio_access;

/*
 * Call led_init() before calling any of the other functions here.
 */
void led_init(void);
void led_turn_on(void);
void led_turn_off(void);

#endif

/*
 * Control the green-coloured LED labelled 
 * "ACT" on the raspberry pi board.
 */
#ifndef LED_H
#define LED_H

/*
 * Call led_init() before calling any of the other functions here.
 */
void led_init(void);
void led_turn_on(void);
void led_turn_off(void);

#endif

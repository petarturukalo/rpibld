/*
 * Choice of timer from the BCM2711 datasheet is between the 
 * system timer and the ARM timer. The system timer is used 
 * over the ARM timer because the ARM timer can be inaccurate.
 *
 * The system timer free running counter is driven by either the AXI/APB
 * clock or the crystal clock (see BCM2711 datasheet bit 7 PROC_CLK_TIMER of
 * the ARM_CONTROL register). The crystal (oscillator) clock, while slower than
 * the AXI/APB, is chosen because it is fixed-frequency - the AXI/APB is not 
 * fixed-frequency, which could inhibit accurate timing.
 * Note the crystal clock is selected by default.
 */
#ifndef TIMER_H
#define TIMER_H

#include "type.h"

void timer1_isr(void);
void timer3_isr(void);

/*
 * Pause the CPU, putting it in an idle state for a milliseconds/microseconds amount of time.
 *
 * WARNING do not sleep for a vert short microseconds amount of time, e.g. less than 5 microseconds
 * from testing, because of potential race conditions: either the interrupt won't be triggered or it 
 * will be triggered before sleeping, resulting in waiting for an interrupt that has already interrupted. 
 * To be on the safe side use a minimum microseconds value considerably higher than the 5 microseconds 
 * minimum mentioned earlier.
 */
void usleep(int microseconds);
void sleep(int milliseconds);

/*
 * Start a non-sleeping timer with timer_poll_start(). Whether the milliseconds amount of time argument
 * to timer_poll_start() has elapsed can be checked/polled with timer_poll_done(). 
 */
void timer_poll_start(int milliseconds);
bool timer_poll_done(void);
/* Prevent the timer from finishing and triggering an interrupt. */
void timer_poll_stop(void);

#endif

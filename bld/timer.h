/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
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

void timer_isr(void);

/*
 * Pause the CPU, putting it in an idle state for a milliseconds/microseconds amount of time.
 *
 * WARNING do not sleep for a very short microseconds amount of time, e.g. less than 5 microseconds
 * from testing, because of potential race conditions: either the interrupt won't be triggered or it 
 * will be triggered before sleeping, resulting in waiting for an interrupt that has already interrupted. 
 * The issue also seems to arise sometimes when sleeping for ~20 microseconds. To be on the safe side 
 * use a minimum microseconds value considerably higher than the times mentioned above.
 */
void usleep(int microseconds);
void sleep(int milliseconds);


/*
 * Start a non-sleeping timer with timer_poll_start(). Whether the milliseconds amount of time argument
 * to timer_poll_start() has elapsed can be checked/polled with timer_poll_done().
 * 
 * WARNING the implementation of this only uses the lower 32-bits of the 64-bit free running counter.
 * Because the system timer clock is 1 MHz, it will take ~71 minutes for the lower 32 bits to go from 0
 * to its maximum and then restart back at 0. If an overflow timestamp is taken (timestamp here is the 
 * time at which the milliseconds amount of time argument initially passes), which can happen when the 
 * pi has been running for almost ~71 minutes, calls to timer_poll_done() will return a false positive
 * until the lower 32-bits of the counter itself overflows. 
 * Conclusion: these functions are safe to use under the assumption that the low 32-bits are reset to 0 
 * at power on (the BCM2711 datasheet says they are), and that the pi hasn't been running for almost ~71 
 * minutes (which is very unlikely since this is a bootloader).
 */
typedef uint32_t timestamp_t;
timestamp_t timer_poll_start(int milliseconds);
bool timer_poll_done(timestamp_t ts);

/*
 * Get a timestamp of the current time.
 */
timestamp_t timer_current(void);

#endif

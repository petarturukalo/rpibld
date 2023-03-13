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

/*
 * Queue an interrupt request on system timer channel 1 to trigger at a 
 * milliseconds amount of time after the current time. 
 *
 * Only one interrupt can be "queued" at a time. If a second call is made 
 * to this without the first triggering an interrupt, the first queued interrupt 
 * is lost and overwritten with the second, e.g. if this is called once to queue
 * an interrupt in 5 seconds, then 2 seconds pass, and this is called again to
 * queue another interrupt, an interrupt won't trigger until another 5 seconds has
 * passed (7 seconds total elapsed).
 */
void timer_queue_irq(int milliseconds);

/*
 * Service an interrupt request on system timer channel 1 by clearing the interrupt.
 */
void timer_isr(void);

/*
 * Pause the CPU, putting it in an idle state for milliseconds amount of time.
 */
void sleep(int milliseconds);

#endif

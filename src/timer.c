#include "timer.h"
#include "mmio.h"
#include "type.h"

/*
 * Timer channels 0 and 2 are supposedly used by the VPU so only
 * 1 and 3 should be used here. I say supposedly because I couldn't
 * find it mentioned in any technical documentation (e.g. the BCM2711 
 * datasheet), but it's mentioned elsewhere online, and when testing 
 * I found that the compare registers for channels 0 and 2 reset to 
 * non-zero values, while channels 1 and 3 reset to zeroed values.
 */
enum timer_register {
	CS,
	CLO,
	C0,
	C1,
	C2,
	C3
};

static struct periph_access timer_access = {
	.periph_base_off = 0x2003000,
	.register_offsets = {
		[CS]  = 0x00,
		[CLO] = 0x04,
		[C0]  = 0x0c,
		[C1]  = 0x10,
		[C2]  = 0x14,
		[C3]  = 0x18
	}
};

/* Control/status (CS) register fields. */
#define CS_MATCH1_MASK 0x1
#define CS_MATCH3_MASK 0x8


/*
 * Cycles per millisecond of the clock driving the system timer's free
 * running counter. This is the number of ticks the counter will be incremented
 * each millisecond. (Note this is derived from the BCM2711 RPI 4 B device tree 
 * source which states the frequency of the clock that drives the system timer counter
 * is 1 MHz.)
 */
#define COUNTER_CLK_CPMS 1000
/* Convert milliseconds to microseconds. */
#define ms_to_us(ms) ms*COUNTER_CLK_CPMS

/*
 * System timer channel 1 is used for sleeping and channel 3 is used for non-sleep
 * timer polling. The two are on separate channels so both can be used at the same
 * time.
 */
static bool queued_timer1_irq_serviced;
static bool queued_timer3_irq_serviced;

/*
 * Queue an interrupt request on a system timer channel to trigger at a 
 * microseconds amount of time after the current time. 
 *
 * @compare_register: the system timer channel's compare register
 * @queued_timer_irq_serviced: out-param to set false to mark as awaiting servicing
 *
 * Only one interrupt can be "queued" at a time. If a second call is made 
 * to this without the first triggering an interrupt, the first queued interrupt 
 * is lost and overwritten with the second, e.g. if this is called once to queue
 * an interrupt in 5 seconds, then 2 seconds pass, and this is called again to
 * queue another interrupt, an interrupt won't trigger until another 5 seconds has
 * passed (7 seconds total elapsed).
 */
static void timer_queue_irq(int microseconds, enum timer_register compare_register,
			    bool *queued_timer_irq_serviced)
{
	int current_ticks = register_get(&timer_access, CLO);

	*queued_timer_irq_serviced = false;
	/* The system timer clock is 1 MHz so 1 tick is 1 microsecond here. */
	register_set(&timer_access, compare_register, current_ticks + microseconds);
}

/*
 * Service an interrupt request on a system timer channel by clearing the interrupt.
 *
 * @compare_register: the system timer channel's compare register
 * @cs_match_mask: bit mask for channel's match field in the CS register
 * @queued_timer_irq_serviced: out-param to set true to mark as being serviced
 */
static void timer_isr(enum timer_register compare_register, int cs_match_mask, 
		      bool *queued_timer_irq_serviced)
{	
	/* Reset timer. */
	register_set(&timer_access, compare_register, 0);
	/* Clear the interrupt. */
	register_enable_bits(&timer_access, CS, cs_match_mask);
	*queued_timer_irq_serviced = true;
}

void timer1_isr(void)
{
	timer_isr(C1, CS_MATCH1_MASK, &queued_timer1_irq_serviced);
}

void timer3_isr(void)
{
	timer_isr(C3, CS_MATCH3_MASK, &queued_timer3_irq_serviced);
}

void usleep(int microseconds)
{
	timer_queue_irq(microseconds, C1, &queued_timer1_irq_serviced);
	/* Can only be woken up from a timer IRQ and not a different peripheral. */
	do {
		__asm__("wfi");
	} while (!queued_timer1_irq_serviced);
}

void sleep(int milliseconds)
{
	usleep(ms_to_us(milliseconds));
}

void timer_poll_start(int milliseconds)
{
	timer_queue_irq(ms_to_us(milliseconds), C3, &queued_timer3_irq_serviced);
}

bool timer_poll_done(void)
{
	return queued_timer3_irq_serviced;
}

void timer_poll_stop(void)
{
	register_set(&timer_access, C3, 0);
}

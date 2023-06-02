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
#define CS_MATCH1 0x2


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

static bool queued_timer_irq_serviced;

/*
 * Queue an interrupt request on a system timer channel 1 to trigger at a 
 * microseconds amount of time after the current time. 
 *
 * Only one interrupt can be "queued" at a time. If a second call is made 
 * to this without the first triggering an interrupt, the first queued interrupt 
 * is lost and overwritten with the second, e.g. if this is called once to queue
 * an interrupt in 5 seconds, then 2 seconds pass, and this is called again to
 * queue another interrupt, an interrupt won't trigger until another 5 seconds has
 * passed (7 seconds total elapsed).
 */
static void timer_queue_irq(int microseconds)
{
	int current_ticks = register_get(&timer_access, CLO);

	queued_timer_irq_serviced = false;
	/* The system timer clock is 1 MHz so 1 tick is 1 microsecond here. */
	register_set(&timer_access, C1, current_ticks + microseconds);
}

/*
 * Service an interrupt request on a system timer channel 1 by clearing the interrupt.
 */
void timer_isr(void)
{	
	/* Reset timer. */
	register_set(&timer_access, C1, 0);
	/* Clear the interrupt. */
	register_enable_bits(&timer_access, CS, CS_MATCH1);
	queued_timer_irq_serviced = true;
}

void usleep(int microseconds)
{
	timer_queue_irq(microseconds);
	/* Can only be woken up from a timer IRQ and not a different peripheral. */
	do {
		__asm__("wfi");
	} while (!queued_timer_irq_serviced);
}

void sleep(int milliseconds)
{
	usleep(ms_to_us(milliseconds));
}

void timer_poll_start(int milliseconds, struct timestamp *ts)
{
	int current_ticks = register_get(&timer_access, CLO);
	ts->timestamp = current_ticks+ms_to_us(milliseconds);
}

bool timer_poll_done(struct timestamp *ts)
{
	int current_ticks = register_get(&timer_access, CLO);
	return current_ticks >= ts->timestamp;
}
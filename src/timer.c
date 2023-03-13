#include "timer.h"
#include "mmio.h"
#include "led.h"// TODO rm
#include "debug.h"// TODO rm

/*
 * Timer channels 0 and 2 are supposedly used by the VPU so only
 * 1 and 3 should be used here. I say supposedly because I couldn't
 * find it mentioned in any technical documentation (e.g. the BCM2711 
 * datasheet), but it's mentioned elsewhere online, and when testing 
 * I found that the compare registers for channels 0 and 2 reset to 
 * non-zero values, while channels 1 and 3 reset to zeroed values.
 */
enum timer_registers {
	CS,
	CLO,
	C0,
	C1,
	C2,
	C3
};

struct periph_access timer_access = {
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

/*
 * Cycles per millisecond of the clock driving the system timer's free
 * running counter. This is the number of ticks the counter will be incremented
 * each millisecond. (Note this is derived from the BCM2711 RPI 4 B device tree 
 * source which states the frequency of the clock that drives the system timer counter
 * is 1 MHz.)
 */
#define COUNTER_CLK_CPMS 1000

void timer_queue_irq(int milliseconds)
{
	int current_ticks = register_get(&timer_access, CLO);
	int wait_ticks = milliseconds*COUNTER_CLK_CPMS;
	register_set(&timer_access, C1, current_ticks + wait_ticks);
}

void timer_isr(void)
{
	register_set(&timer_access, C1, 0);
	register_enable_bits(&timer_access, CS, 1<<1);
}

void sleep(int milliseconds)
{
	timer_queue_irq(milliseconds);
	__asm__("wfi");
	// TODO need to bother checking state of global variable? prob not
}

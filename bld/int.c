#include "int.h"
#include "help.h"

#define CPSR_I  0x080  /* Disable IRQ. */

void enable_interrupts(void)
{
	__asm__("push {r4}\n\t"
		"mrs r4, cpsr\n\t"
		"bic r4, #" MSTRFY(CPSR_I) "\n\t"
		"msr cpsr_c, r4\n\t"
		"pop {r4}");
}

void disable_interrupts(void)
{
	__asm__("push {r4}\n\t"
		"mrs r4, cpsr\n\t"
		"orr r4, #" MSTRFY(CPSR_I) "\n\t"
		"msr cpsr_c, r4\n\t"
		"pop {r4}");
}

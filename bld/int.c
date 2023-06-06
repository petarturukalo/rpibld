#include "int.h"

void enable_interrupts(void)
{
	// TODO define for 0x80?
	__asm__("mrs r4, cpsr\n\t"
		"bic r4, #0x80\n\t"
		"msr cpsr_c, r4");
}

void disable_interrupts(void)
{
	__asm__("mrs r4, cpsr\n\t"
		"orr r4, #0x80\n\t"
		"msr cpsr_c, r4");
}


#include "int.h"

// TODO inline?
void enable_interrupts(void)
{
	// TODO does this need to be volatile?
	__asm__("mrs r0, cpsr\n\t"
		"bic r0, #0x80\n\t"
		"msr cpsr_c, r0");
}

void disable_interrupts(void)
{
	// TODO does this need to be volatile?
	__asm__("mrs r0, cpsr\n\t"
		"orr r0, #0x80\n\t"
		"msr cpsr_c, r0");
}


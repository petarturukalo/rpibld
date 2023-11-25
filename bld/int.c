/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#include "int.h"
#include "help.h"
#include "bits.h"

#define CPSR_I  BIT(7)  /* Disable IRQ. */

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

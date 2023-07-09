/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "gpio.h"
#include "mmio.h"

enum gpio_register {
	GPFSEL0,
	GPFSEL1,
	GPFSEL2,
	GPFSEL3,
	GPFSEL4,
	GPFSEL5,
	GPSET0,
	GPSET1,
	GPCLR0,
	GPCLR1
};

static struct periph_access gpio_access = {
	.periph_base_off = 0x2200000,
	.register_offsets = {
		[GPFSEL0] = 0x00,
		[GPFSEL1] = 0x04,
		[GPFSEL2] = 0x08,
		[GPFSEL3] = 0x0c,
		[GPFSEL4] = 0x10,
		[GPFSEL5] = 0x14,
		[GPSET0]  = 0x1c,
		[GPSET1]  = 0x20,
		[GPCLR0]  = 0x28,
		[GPCLR1]  = 0x2c
	}
};

/* 
 * Number of pins in each GPFSEL register: each register
 * is 32 bits and each pin in the register occupies GPIO_OP_BITS.
 */
#define PINS_PER_GPFSEL 10
/* Number of bits that an enum gpio_op spans. */
#define GPIO_OP_BITS 3

void gpio_pin_select_op(enum gpio_pin pin, enum gpio_op op)
{
	enum gpio_register gpfsel_regs[] = { GPFSEL0, GPFSEL1, GPFSEL2, 
					     GPFSEL3, GPFSEL4, GPFSEL5 };
	int gpfsel_index = pin/PINS_PER_GPFSEL;
	/* The GPFSEL register that the pin is in. */
	enum gpio_register gpfsel = gpfsel_regs[gpfsel_index];
	int shift = (pin%PINS_PER_GPFSEL)*GPIO_OP_BITS;

	register_set(&gpio_access, gpfsel, op<<shift);
}

/* 
 * Number of pins in each GPSET/GPCLR register 
 * (which is the same as the number of bits in the register).
 */
#define PINS_PER_SETCLR 32

static void gpio_pin_setclr(enum gpio_pin pin, enum gpio_register reg0, enum gpio_register reg1)
{
	int regnr = pin/PINS_PER_SETCLR;

	if (regnr == 0) 
		register_set(&gpio_access, reg0, 1<<pin);
	else if (regnr == 1)
		register_set(&gpio_access, reg1, 1<<(pin%PINS_PER_SETCLR));
}

void gpio_pin_set(enum gpio_pin pin)
{
	gpio_pin_setclr(pin, GPSET0, GPSET1);
}

void gpio_pin_clr(enum gpio_pin pin)
{
	gpio_pin_setclr(pin, GPCLR0, GPCLR1);
}

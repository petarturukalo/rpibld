/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "mmio.h"
#include "help.h"

static uint32_t *get_peripheral_reg_addr(struct periph_access *periph, int register_select)
{
	return (uint32_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR + periph->periph_base_off
			    + periph->register_offsets[register_select]);
}

void register_set(struct periph_access *periph, int register_select, uint32_t value)
{
	volatile uint32_t *periph_reg_addr = get_peripheral_reg_addr(periph, register_select);

	__asm__ __volatile__("dsb st" ::: "memory");  /* Memory write barrier. */
	*periph_reg_addr = value;
}

void register_set_ptr(struct periph_access *periph, int register_select, void *value)
{
	uint32_t val;
	mcopy(value, &val, sizeof(uint32_t));
	register_set(periph, register_select, val);
}

uint32_t register_get(struct periph_access *periph, int register_select)
{
	volatile uint32_t *periph_reg_addr = get_peripheral_reg_addr(periph, register_select);
	uint32_t ret = *periph_reg_addr;
	__asm__ __volatile__("dsb" ::: "memory");  /* Memory read barrier. */
	return ret;
}

void register_get_out(struct periph_access *periph, int register_select, void *out)
{
	uint32_t ret = register_get(periph, register_select);
	mcopy(&ret, out, sizeof(uint32_t));
}

void register_enable_bits(struct periph_access *periph, int register_select, uint32_t mask)
{
	uint32_t reg = register_get(periph, register_select);
	register_set(periph, register_select, reg|mask);
}

void register_disable_bits(struct periph_access *periph, int register_select, uint32_t mask)
{
	uint32_t reg = register_get(periph, register_select);
	register_set(periph, register_select, reg&(~mask));
}

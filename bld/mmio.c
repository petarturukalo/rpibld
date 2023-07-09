/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "mmio.h"
#include "help.h"

static byte_t *get_peripheral_base_address(int peripheral_base_offset)
{
	return (byte_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR + peripheral_base_offset);
}

void register_set(struct periph_access *periph, int register_select, uint32_t value)
{
	byte_t *periph_base_addr = get_peripheral_base_address(periph->periph_base_off);
	int reg_off = periph->register_offsets[register_select];

	// TODO figure out how to use memory barriers properly
	__asm__("dmb");
	*(uint32_t *)(periph_base_addr+reg_off) = value;
	__asm__("dmb");
}

void register_set_ptr(struct periph_access *periph, int register_select, void *value)
{
	uint32_t val;
	mcopy(value, &val, sizeof(uint32_t));
	register_set(periph, register_select, val);
}

uint32_t register_get(struct periph_access *periph, int register_select)
{
	byte_t *periph_base_addr;
	int reg_off;
	uint32_t ret;

       	periph_base_addr = get_peripheral_base_address(periph->periph_base_off);
	reg_off = periph->register_offsets[register_select];

	__asm__("dmb");
	ret = *(uint32_t *)(periph_base_addr+reg_off);
	__asm__("dmb");
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

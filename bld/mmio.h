/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * Access to memory-mapped I/O. 
 */
#ifndef MMIO_H
#define MMIO_H

#include "type.h"

/**
 * Base address of main peripherals section in ARM low peripheral mode 
 * view of address map.
 */
#define ARM_LO_MAIN_PERIPH_BASE_ADDR 0xfc000000

/**
 * @brief Offsets for accessing a peripheral's memory-mapped registers.
 *
 * The convention to define the format of a register's value (i.e. its fields) is either of the following.
 *
 * 1. If many of the register's fields will be used, define a struct for the register named <register name>. 
 * The register's fields shall be represented using bit-field struct members. 
 * 2. If only a few of the register's fields will be used, #define shifts and/or masks for the register's 
 * fields, of name <register_name>_<field>[_SHIFT].
 *
 * With either choice, the definitions should appear near the periph_access for the registers which
 * the definitions represent.
 */
struct periph_access {
	/**
	 * Offset in bytes to a peripheral's memory-mapped base address from the start of the 
	 * main peripherals section in the ARM low peripheral mode view of the address map 
	 * (see BCM2711 datasheet for more info; note the addresses listed there are in terms 
	 * of the legacy master view of the address map).
	 */
	int periph_base_off;
	/**
	 * Each element is an offset in bytes to a memory-mapped register from the peripheral's 
	 * base address. The convention here is to define a separate enum where each value names 
	 * a register, then use the enum to identify the offset for the register in this field. 
	 * The name used for the enum value is typically the short name of a register from the 
	 * BCM2711 datasheet or other technical documentation. 
	 */
	int register_offsets[];
};

/**
 * @defgroup reg_access_fns
 * @brief Set/get the value of a 32-bit memory-mapped register.
 *
 * @param register_select An index into periph_access.register_offsets for selecting the register
 *			  to access
 *
 * The pointer versions are for use with packed bit-field structs of 32-bit size. 
 * They take a void * to avoid having to cast. 
 * @{
 */
void register_set(struct periph_access *periph, int register_select, uint32_t value);
void register_set_ptr(struct periph_access *periph, int register_select, void *value);
uint32_t register_get(struct periph_access *periph, int register_select);
void register_get_out(struct periph_access *periph, int register_select, void *out);
/** @} */

/**
 * @defgroup reg_toggle_fns
 * @brief Enable/disable all the high bits in the mask. All low bits retain their previous value.
 *
 * @param register_select See register_set()
 * @{
 */
void register_enable_bits(struct periph_access *periph, int register_select, uint32_t mask);
void register_disable_bits(struct periph_access *periph, int register_select, uint32_t mask);
/** @} */

#endif

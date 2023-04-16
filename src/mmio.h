/*
 * Access to memory-mapped I/O. 
 */
#ifndef MMIO_H
#define MMIO_H

#include "type.h"

/*
 * Offsets for accessing a peripheral's memory-mapped registers.
 *
 * @periph_base_off: Offset in bytes to a peripheral's memory-mapped base address from
 *	the start of the main peripherals section in the ARM low peripheral
 *	mode view of the address map (see BCM2711 datasheet for more info; note
 *	the addresses listed there are in terms of the legacy master view of the
 *	address map).
 * @register_offsets: Each element is an offset in bytes to a memory-mapped register from 
 *	the peripheral's base address. The convention here is to define a separate enum where
 *	each value names a register, then use the enum to identify the offset for the register in
 *	this field. The name used for the enum value is typically the short name of a register 
 *	from the BCM2711 datasheet or other technical documentation. 
 *
 * The convention to define the format of a register's value (i.e. its fields) is either of the following.
 *
 * 1. If many of the register's fields will be used, define a separate struct for each register named 
 * <register name>. The register's fields shall be represented using bit-field struct members. 
 * 2. If only a few of the register's fields will be used, #define shifts and/or masks for the register's 
 * fields, of name <register_name>_<field>[_SHIFT].
 *
 * With either choice, the definitions should appear near the periph_access for the registers which
 * the definitions represent.
 */
struct periph_access {
	int periph_base_off;
	int register_offsets[];
};

// TODO need volatile here? (forgot to put it in .c when i added it here too)
/*
 * Set/get the value of a 32-bit memory-mapped register. The regular functions here are 32-bit;
 * functions with suffix _nbit are n-bit.
 *
 * @register_select: an index into periph_access.register_offsets for selecting the register
 *	to access
 */
volatile void register_set(struct periph_access *periph, int register_select, uint32_t value);
// TODO delete 16bit if unused
volatile void register_set_16bit(struct periph_access *periph, int register_select, uint16_t value);
volatile void register_set_ptr(struct periph_access *periph, int register_select, void *value);
volatile uint32_t register_get(struct periph_access *periph, int register_select);
volatile void register_get_out(struct periph_access *periph, int register_select, void *out);

/*
 * Enable/disable all the high bits in the mask. All low bits retain their previous value.
 *
 * @register_select: see register_set()
 */
volatile void register_enable_bits(struct periph_access *periph, int register_select, uint32_t mask);
volatile void register_disable_bits(struct periph_access *periph, int register_select, uint32_t mask);

#endif

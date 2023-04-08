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
 * The convention to define the format of a register's value (i.e. its fields) is to define a separate
 * struct for each register named reg_<register name>. These struct definitions should be near the periph_access
 * definition. The register's fields shall be represented using bit-field struct members. This is used 
 * opposed to #define for register field shifts and masks. 
 * TODO if only need a few fields then fine to use shifts?
 * TODO if reg only used in one place might be ok to define it there (e.g. in a func)
 */
struct periph_access {
	int periph_base_off;
	int register_offsets[];
};

// TODO need volatile here (forgot to put it in .c when i added it here too)
/*
 * Set/get the value of a 32-bit memory-mapped register.
 *
 * @register_select: an index into periph_access.register_offsets for selecting the register
 *	to access
 */
volatile void register_set(struct periph_access *periph, int register_select, word_t value);
volatile void register_set_ptr(struct periph_access *periph, int register_select, void *value);
volatile word_t register_get(struct periph_access *periph, int register_select);
volatile void register_get_out(struct periph_access *periph, int register_select, void *out);

/*
 * Enable/disable all the high bits in the mask. All low bits retain their previous value.
 *
 * @register_select: see register_set()
 */
volatile void register_enable_bits(struct periph_access *periph, int register_select, word_t mask);
volatile void register_disable_bits(struct periph_access *periph, int register_select, word_t mask);

#endif

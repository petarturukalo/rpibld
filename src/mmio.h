/*
 * Access to memory-mapped I/O. 
 */
#ifndef MMIO_H
#define MMIO_H

typedef int word_t;
typedef char byte_t;

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
 */
struct periph_access {
	int periph_base_off;
	int register_offsets[];
};

/*
 * Set/get the value of a 32-bit memory-mapped register.
 *
 * @register_select: an index into periph_access.register_offsets for selecting the register
 *	to access
 */
volatile void register_set(struct periph_access *periph, int register_select, word_t value);
volatile word_t register_get(struct periph_access *periph, int register_select);

/*
 * Enable/disable all the high bits in the mask. All low bits retain their previous value.
 *
 * @register_select: see register_set()
 */
volatile void register_enable_bits(struct periph_access *periph, int register_select, word_t mask);
volatile void register_disable_bits(struct periph_access *periph, int register_select, word_t mask);

#endif

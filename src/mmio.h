/*
 * Access to memory-mapped I/O. 
 */
#ifndef MMIO_H
#define MMIO_H

typedef int word_t;
typedef char byte_t;
// TODO redocument if also use ARM local peripherals, etc.

/*
 * Offset in bytes to a peripheral's memory-mapped base address from
 * the start of the main peripherals section in the ARM low peripheral
 * mode view of the address map (see BCM2711 datasheet for more info; note
 * the addresses listed there are in terms of the legacy master view of the
 * address map).
 */
enum peripheral_base_offset {
	PERIPH_BASE_OFF_GPIO = 0x2200000
};

/*
 * Offset in bytes to a memory-mapped register from a peripheral_base_offset.
 * These values follow naming convention REG_OFF_<periph>_<reg> where <periph> is a
 * peripheral named in peripheral_base_offset, and <reg> is the name of one of the 
 * peripheral's registers.
 * TODO explain where the register names come from
 */
enum register_offset {
	REG_OFF_GPIO_GPFSEL4 = 0x10,
	REG_OFF_GPIO_GPSET1 = 0x20,
	REG_OFF_GPIO_GPCLR1 = 0x2c
};

/*
 * Set the value of a 32-bit memory-mapped register.
 */
void register_set(enum peripheral_base_offset base_off, enum register_offset reg_off, word_t value);

#endif

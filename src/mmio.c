#include "mmio.h"

/*
 * Base address of main peripherals section in ARM low peripheral mode 
 * view of address map.
 */
#define ARM_LO_MAIN_PERIPH_BASE_ADDR 0xfc000000

void register_set(enum peripheral_base_offset base_off, enum register_offset reg_off, word_t value)
{
	byte_t *periph_base_addr = (byte_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR + base_off);
	*(word_t *)(periph_base_addr+reg_off) = value;
}

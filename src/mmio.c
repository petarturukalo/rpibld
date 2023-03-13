#include "mmio.h"

/*
 * Base address of main peripherals section in ARM low peripheral mode 
 * view of address map.
 */
#define ARM_LO_MAIN_PERIPH_BASE_ADDR 0xfc000000

static byte_t *get_peripheral_base_address(int peripheral_base_offset)
{
	return (byte_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR + peripheral_base_offset);
}

void register_set(struct periph_access *periph, int register_select, word_t value)
{
	byte_t *periph_base_addr = get_peripheral_base_address(periph->periph_base_off);
	int reg_off = periph->register_offsets[register_select];

	// TODO figure out how to use memory barriers properly
	__asm__("dmb");
	*(word_t *)(periph_base_addr+reg_off) = value;
	__asm__("dmb");
}

word_t register_get(struct periph_access *periph, int register_select)
{
	byte_t *periph_base_addr;
	int reg_off;
	word_t ret;

       	periph_base_addr = get_peripheral_base_address(periph->periph_base_off);
	reg_off = periph->register_offsets[register_select];

	__asm__("dmb");
	ret = *(word_t *)(periph_base_addr+reg_off);
	__asm__("dmb");
	return ret;
}

void register_enable_bits(struct periph_access *periph, int register_select, word_t mask)
{
	word_t word = register_get(periph, register_select);
	register_set(periph, register_select, word|mask);
}

void register_disable_bits(struct periph_access *periph, int register_select, word_t mask)
{
	word_t word = register_get(periph, register_select);
	register_set(periph, register_select, word&(~mask));
}

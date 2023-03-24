#include "heap.h"

byte_t *heap_get_base_address(void)
{
	/*
	 * The heap starts at ~300 MB and is expected to be
	 * grown upwards. This address is above the stacks,
	 * which grow downwards.
	 */
	return (byte_t *)0x12000000;
}

#include "heap.h"

void *heap_get_base_address(void)
{
	/*
	 * The heap starts at 256 MiB and is expected to be
	 * grown upwards. This address is above the stacks,
	 * which grow downwards.
	 */
	return (byte_t *)0x10000000;
}

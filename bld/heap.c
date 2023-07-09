/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "heap.h"
#include "addrmap.h"

void *heap_get_base_address(void)
{
	return (byte_t *)HEAP_RAM_ADDR;
}

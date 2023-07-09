/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * A large memory area in RAM for this bootloader 
 * program to use for whatever it wants.
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef HEAP_H
#define HEAP_H

#include "mmio.h"

/*
 * Get the base address of the heap. There is currently
 * no allocation / memory management implemented - it is 
 * up to the callers (me) to ensure it is used asynchronously.
 */
void *heap_get_base_address(void);

#endif

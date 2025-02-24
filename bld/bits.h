/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef BITS_H
#define BITS_H

/**
 * @def BITS
 * @brief Get a 32-bit value with all bits between start and stop high.
 *	  Both start and stop bits are inclusive.
 * @def BIT 
 * @brief Get a 32-bit value with only the nth bit high.
 *  
 * @defgroup bit_defs
 * Least significant bit is bit 0 and most significant bit is bit 31.
 * @{
 */
#define BITS(stop, start) ((0xffffffff<<start) & (0xffffffff>>(31-stop)))
#define BIT(n) (1<<n)
/** @} */

#endif

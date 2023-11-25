/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef TYPE_H
#define TYPE_H

typedef unsigned int bits_t;
typedef char byte_t;

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  /* For NULL. */

/* 
 * Cast a struct variable containing only bitfields to an integral type of the
 * the same size as the struct. E.g. use this to cast a struct with
 * four 8-bit fields to a uint32_t.
 */
#define cast_bitfields(bitfield_struct, integral_type) (*((integral_type *)&bitfield_struct))

#endif

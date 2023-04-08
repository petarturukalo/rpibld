#ifndef TYPE_H
#define TYPE_H

typedef unsigned int bits_t;
typedef char byte_t;
// TODO rm word_t and use uint32_t instead? maybe not
typedef int word_t;

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

typedef int bool;
#define true 1;
#define false 0;

#define NULL ((void *)0)

/* 
 * Cast a struct variable containing only bitfields to an integral type of the
 * the same size as the struct. E.g. use this to cast a struct with
 * four 8-bit fields to a uint32_t.
 * TODO if only use the reg version of these rm the bitfield version?
 * TODO rm these if not used at a later point?
 */
#define cast_bitfields(bitfield_struct, integral_type) (*((integral_type *)&bitfield_struct))
/* Cast a register struct variable to a 32-bit word. */
#define cast_reg(reg_struct) cast_bitfields(reg_struct, word_t)


#endif

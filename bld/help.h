/*
 * General helper functions.
 */
#ifndef HELP_H
#define HELP_H

#include "type.h"

/*
 * Get the number of elements in an array (its length) when
 * the size of the array is known.
 */
#define array_len(array) (sizeof(array)/sizeof(array[0]))

/*
 * Return the greater of two numbers.
 */
int max(int n, int m);

/*
 * Set the first n bytes of a memory area to 0.
 */
void mzero(void *mem, int n);

/*
 * Copy n bytes from src to dest.
 */
void mcopy(void *src, void *dest, int n);

/*
 * Get whether n bytes of two memory areas are equal.
 */
bool mcmp(void *mem1, void *mem2, int n);

/*
 * Get whether an address is n-byte aligned.
 */
bool address_aligned(void *addr, int n);

/*
 * Swap the bytes of a 32-bit value that was stored
 * in big-endian format on secondary storage to recover
 * its actual value.
 */
uint32_t bswap32(uint32_t value);

/*
 * Loop waiting for a condition to become false. Timeout and 
 * signal ERROR_INFINITE_LOOP after waiting timeout_ms. 
 *
 * This is typically used "just in case" for conditions dependent 
 * on hardware that are expected to change, but might not (even if
 * very unlikely).
 *
 * This way the user at least knows that an infinite could have happened,
 * opposed to getting stuck in an infinite loop and the user not knowing 
 * whether it's an infinite loop or another issue.
 */
void while_cond_timeout_infinite(bool (*condition)(void), int timeout_ms);

#endif

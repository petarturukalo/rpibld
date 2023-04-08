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

#endif

#include "help.h"

int max(int n, int m)
{
	return (n < m) ? m : n;
}

void mzero(void *mem, int n)
{
	byte_t *bytes = mem;
	for (int i = 0; i < n; ++i) 
		bytes[i] = 0;
}

void mcopy(void *src, void *dest, int n)
{
	byte_t *s = src;
	byte_t *d = dest;

	for (int i = 0; i < n; ++i)
		d[i] = s[i];
}

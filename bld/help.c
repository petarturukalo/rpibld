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

bool mcmp(void *mem1, void *mem2, int n)
{
	byte_t *b1 = mem1;
	byte_t *b2 = mem2;

	for (int i = 0; i < n; ++i) {
		if (b1[i] != b2[i])
			return false;
	}
	return true;
}

bool address_aligned(void *addr, int n)
{
	return !((int)addr%n);
}

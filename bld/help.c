#include "help.h"
#include "timer.h"
#include "error.h"

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

uint32_t bswap32(uint32_t value)
{
	/* Reverse the order of the bytes. */
	return (value&0x000000ff)<<24 |
	       (value&0x0000ff00)<<8 |
	       (value&0x00ff0000)>>8 |
	       (value&0xff000000)>>24;
}

void while_cond_timeout_infinite(bool (*condition)(void), int timeout_ms)
{
	struct timestamp ts;
	
	timer_poll_start(timeout_ms, &ts);
	while (condition()) {
		if (timer_poll_done(&ts))
			signal_error(ERROR_INFINITE_LOOP);
	}
}

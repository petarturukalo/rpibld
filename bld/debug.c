#include "debug.h"
#include "uart.h"
#include "timer.h"
#include "help.h"
#include "error.h"
#include <stdarg.h>

int Strlen(char *s)
{
	int n = 0;

	while (*s++)
		++n;
	return n;
}

/*
 * Like the regular strncpy() except returns the number of bytes 
 * copied instead.
 */
static int Strncpy(char *dst, char *src, int n)
{
	for (int i = 0; i < n; ++i)
		dst[i] = src[i];
	return n;
}

/*
 * Reverse a string, in-place.
 */
static void strreverse(char *s)
{
	int i = 0;
	int j = Strlen(s)-1;
	char tmp;

	for (; i < j; ++i, --j) {
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
}

static int char_digit_to_int(char digit)
{
	return digit-'0';
}

#define DECIMAL_BASE 10
#define HEX_BASE 16
static const char hex_digits[] = "0123456789abcdef";

/*
 * Convert a decimal integer into a string representation
 * of that integer, in either the same (decimal) or different
 * base. Max supported base is hexadecimal.
 */
static char *uint_to_str_base(uint32_t n, int base)
{
	static char str[32];
	int digit, i = 0;

	if (n == 0)
		str[i++] = '0';
	else {
		while (n) {
			digit = n%base;
			str[i++] = hex_digits[digit];
			n /= base;
		}
	}
	str[i] = '\0';
	strreverse(str);
	return str;
}

static char *uint_to_str(uint32_t n)
{
	return uint_to_str_base(n, DECIMAL_BASE);
}

static char *uint_to_hexstr(uint32_t n)
{
	return uint_to_str_base(n, HEX_BASE);
}

static int str_to_int(char *s, int len)
{
	int mult = 1;
	int n = 0;
	int i = len-1;

	for (; i >= 0; --i) {
		n += char_digit_to_int(s[i])*mult;
		mult *= DECIMAL_BASE;
	}
	return n;
}

/*
 * Return a pointer to the first occurrence of
 * a character in a string, or NULL for no such
 * occurrence.
 */
static char *str_find(char *s, char c)
{
	/* Using a do while allows the caller to search for a NULL term. */
	do {
		if (*s == c)
			return s;
	} while (*s++);
	return NULL;
}

/*
 * Return the index of the first character in the string
 * that isn't in the "set" of characters, or -1 for no
 * such character.
 */
static int str_find_char_not_in_set(char *s, char *set)
{
	for (int i = 0; s[i]; ++i) {
		if (!str_find(set, s[i]))
			return i;
	}
	return -1;
}

static bool specifier_supports_field_width(char spec)
{
	return spec == 'u' || spec == 'x';
}

/* 
 * Similar to the regular vsnprintf() (see its man page for more info),
 * except this version only supports limited functionality in comparison.
 * See the comment at debug.h:serial_log() for what this implements.
 * The last available byte will always be NULL terminated.
 *
 * WARNING if arguments are misused or wrong, e.g. if there's an unsupported 
 * or unfinished conversion specifier in the format string, etc., then this 
 * function fails and signals error ERROR_VSNPRINTF.
 */
static void Vsnprintf(char *s, int n, char *fmt, va_list ap)
{
	int r = 0;  /* Read index from source format. */
	int w = 0;  /* Write index into destination string. Where to write the next char. */
	int fmtlen = Strlen(fmt);

	for (; r < fmtlen && w < n; ++r) {
		/* 
		 * If there's a % followed by a supported conversion specifier then
		 * substitute in the next argument according to the specifier.
		 */
		if (fmt[r] == '%') {
			/* 
			 * The case implementing a conversion specifier in the below switch
			 * is expected to set sarg to the output of its conversion, so it can 
			 * be copied to the output string.
			 */
			char *sarg;
			int field_width = 0;
			char pad = ' ';
			int i;

			/* Move to char after %, but only if it doesn't overrun the length. */
			if (r++ >= fmtlen) 
				signal_error(ERROR_VSNPRINTF);

			/* Select character used to pad field width. */
			if (fmt[r] == '0') {
				pad = '0';
				if (r++ >= fmtlen)
					signal_error(ERROR_VSNPRINTF);
			}
			/* Extract field width. */
			i = str_find_char_not_in_set(fmt+r, "0123456789");
			if (i > 0) {
				field_width = str_to_int(fmt+r, i);
				/* Set index to start of specifier. */
				r += i;
				if (r >= fmtlen || !specifier_supports_field_width(fmt[r])) 
					signal_error(ERROR_VSNPRINTF);
			}

			/* Get specifier's argument.*/
			switch (fmt[r]) {
				default:
					/* Invalid specifier*/
					signal_error(ERROR_VSNPRINTF);
				case 'u':
					sarg = uint_to_str(va_arg(ap, uint32_t));
					break;
				case 'x':
					sarg = uint_to_hexstr(va_arg(ap, uint32_t));
					w += Strncpy(s+w, "0x", min(n-w, 2));  /* 2 is Strlen(0x). */
					break;
				case 's':
					sarg = va_arg(ap, char *);
					break;
			}

			/* Fill (pad) the left of the argument to meet the field width. */
			field_width -= Strlen(sarg);
			while (field_width-- > 0 && w < n) 
				s[w++] = pad;
			/* Write out arg. */
			w += Strncpy(s+w, sarg, min(n-w, Strlen(sarg)));
		} else 
			s[w++] = fmt[r];
	}
	/* Null terminate. */
	if (w < n)
		s[w] = '\0';
	else
		s[n-1] = '\0';
}

static void Snprintf(char *s, int n, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	Vsnprintf(s, n, fmt, ap);
	va_end(ap);
}

void serial_log(char *fmt, ...)
{
	static char newfmt[512];
	static char buf[1024];
	va_list ap;
	timestamp_t ts = timer_current();

	/* 
	 * Prefix the user's arguments with a timestamp padded to the max width
	 * of a 32-bit integer, and suffix it with a newline. 
	 */
	Snprintf(newfmt, sizeof(newfmt), "[%10u] %s\r\n", ts, fmt);

	va_start(ap, fmt);
	Vsnprintf(buf, sizeof(buf), newfmt, ap);
	va_end(ap);

	uart_transmit(buf, Strlen(buf));
}

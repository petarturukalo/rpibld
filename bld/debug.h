/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * Debugging utilities.
 */
#ifndef DEBUG_H
#define DEBUG_H

/*
 * Log the output of a printf style formatted string over the mini UART 
 * serial connection. The output is prefixed with a timestamp and suffixed 
 * with a newline, e.g. "[<time>] <output>\r\n".
 *
 * A single call to uart_init() must have been made before
 * any call to this.
 *
 * The following conversion specifiers are supported: 
 * - u: print unsigned int as decimal
 * - x: print (unsigned) int as hex
 * - s: string
 *
 * An optional field width can be provided before the u, x conversion specifiers, 
 * e.g. "%[<field width>]u", which left pads the output to a minimum width of 
 * <field width>. By default padding is blank, unless the field width starts with
 * a 0, in which case padding is 0. For example, formatter "%08x" with argument
 * 0xf would print 0x00001111.
 */
void serial_log(char *fmt, ...);

#endif

/*
 * Temporary debugging utilities. Everything here
 * will eventually be deleted.
 */
#ifndef DEBUG_H
#define DEBUG_H

#include "mmio.h"

void sleep_short(void);
void sleep_long(void);
void print_word(uint32_t word);
void print_word_reverse(uint32_t word);
void print_bit(uint32_t word, int bit);
void print_cpsr(void);
void print_byte(char byte);

#endif

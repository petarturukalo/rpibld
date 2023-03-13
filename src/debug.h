/*
 * Temporary debugging utilities. Everything here
 * will eventually be deleted.
 */
#ifndef DEBUG_H
#define DEBUG_H

#include "mmio.h"

void sleep_short(void);
void sleep_long(void);
void print_word(word_t word);
void print_cpsr(void);

#endif

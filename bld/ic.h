/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * The legacy interrupt controller.
 * Usable only if enable_gic=0 in config.txt.
 */
#ifndef IC_H
#define IC_H

void ic_enable_interrupts(void);
/** @brief Disable all the interrupts enabled in ic_enable_interrupts(). */
void ic_disable_interrupts(void);
void ic_irq_exception_handler(void);

#endif

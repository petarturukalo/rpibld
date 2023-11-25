/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef GIC_H
#define GIC_H

/*
 * Whether to use the GIC-400 interrupt controller in this
 * bootloader. If set enable_gic=1 must also be set in
 * config.txt for the ARM stub to initialise the GIC.
 * 
 * GIC is enabled here because it is required in order to boot
 * Linux using the default BCM2711 device tree. Without this, the 
 * kernel would hang when its SMP code started the secondary cores.
 *
 * With this enabled, however, the legacy interrupt controller 
 * implemented in ic.c and ic.h cannot be used. In this bootloader
 * properly being interrupted (opposed to polling interrupt flags)
 * is really only used by the system timer sleep functionality. 
 * To save me having to learn GIC and implement interrupts with it, 
 * the sleep implementation polls instead of actually sleeping when 
 * this is enabled. Note this bootloader used the legacy interrupt 
 * controller until I found (struggled to find, took a long time to 
 * find) that Linux required GIC enabled to boot successfully.
 */
#define ENABLE_GIC 1

#endif

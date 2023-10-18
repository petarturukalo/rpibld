/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Address map. How the bootloader uses RAM. The following diagram 
 * illustrates how the RAM is sectioned for use by the bootloader. 
 * Sections are delimited by borders: a hyphenated border (----) 
 * marks a hard border where a section stops, and a dotted border 
 * (....) marks a soft border where a section can grow into 
 * (i.e. it has a variable or unknown size).
 *
 *         +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |    heap    |
 * 256 MiB +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |    dtb     |
 * 128 MiB +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |   kernel   |
 *  32 MiB +------------+
 *         |            |
 *  31 MiB +------------+
 *         | irq stack  |
 *  30 MiB +------------+
 *         | svc stack  |
 *         |............|
 *         |            |
 *         |............|
 *         | bootloader |
 *   8 KiB +------------+
 *         |            |
 * 0 bytes +------------+
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef ADDRMAP_H
#define ADDRMAP_H

/*
 * The heap starts at 256 MiB and is expected to be
 * grown upwards. 
 */
#define HEAP_RAM_ADDR 0x10000000

/* Addresses in RAM that the kernel / device tree blob are loaded to. */
#define DTB_RAM_ADDR  0x8000000  /* 128 MiB. */
#define KERN_RAM_ADDR 0x2000000  /* 32 MiB. */

/*
 * The addresses that the IRQ and supervisor (SVC) mode stacks start at.
 * Each has 1 MiB of stack space and grows downwards. They are 4-byte 
 * aligned as they won't work if not 4-byte aligned. 
 */
#define IRQ_STACK_START_ADDR 0x1f00000  /* 31 MiB. */
#define SVC_STACK_START_ADDR 0x1e00000  /* 30 MiB. */

#endif

/* 
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * The length of the binary is restricted so it doesn't overflow into other "sections" 
 * of RAM (see diagram in bld/addrmap.h for an explanation of these "sections"). The length
 * is calculated from 29 MiB - 8 KiB, the end of the svc stack's memory region minus the
 * start address of this bootloader.
 *
 * 32-bit "kernels" (binaries; this bootloader program is replacing the kernel) 
 * are loaded to address 0x8000 by default, with ARM stub code placed at address 0x0 by
 * the firmware, which is executed at reset. This 0x8000 start address can be changed to
 * 0x0, replacing the ARM stub, if config.txt property kernel_old=1 is used. This isn't done,
 * however, since the bootloader's entry code would just be copying very closely the 
 * initialisation that would have been done by the ARM stub.
 */
MEMORY { ram : ORIGIN = 0x8000, LENGTH = 0x1cf8000 }
ENTRY(asm_entry)
SECTIONS
{
	/*
	 * Put .init before .text to guarantee that the assembly code is at the very start
	 * of the binary and comes before the C code, as it does set up required to execute
	 * the C code.
	 */
	.init : { *(.init) } >ram
	.text : { *(.text) } >ram
	.data : { *(.data) } >ram
	.bss : { *(.bss) } >ram  /* Note .bss isn't zeroed. */
	.rodata : { *(.rodata) } >ram
	/DISCARD/ : { *(*) }
}


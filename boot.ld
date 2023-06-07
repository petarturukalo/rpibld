/* 
 * The length of the binary is restricted so it doesn't overflow into other "sections" 
 * of RAM (see diagram in bld/boot.c for an explanation of these "sections"). Length is 
 * calculated as 29 MiB - 32 KiB, i.e. the end of the svc stack's memory region minus 
 * the start of this binary's memory region (see diagram in bld/boot.c again).
 *
 * 32-bit "kernels" (binaries; this bootloader program is replacing the kernel) 
 * are loaded to address 0x8000 by default. It doesn't need to be specified in this script
 * that the program should be loaded to 0x8000, since the third stage bootloader start4.elf
 * already knows, and the 0x8000 start address information is lost after objcopy conversion from 
 * elf to binary, replaced with relative addresses. The reason it's specified however is so that 
 * an objdump on the elf file shows addresses starting at 0x8000, since it's more helpful for 
 * debugging purposes to see the addresses that will be used at execution time rather than relative 
 * addresses.
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


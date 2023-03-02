ENTRY(asm_entry)
SECTIONS
{
	/*
	 * Put .init before .text to guarantee that the assembly code is at the very start
	 * of the binary and comes before the C code, as it does set up required to execute
	 * the C code.
	 * 
	 * 32-bit "kernels" (binaries, since this bootloader program is replacing the kernel) 
	 * are loaded to address 0x8000 by default. We don't really need to specify in this script
	 * that the program should be loaded to 0x8000, since the third stage bootloader start4.elf
	 * already knows, and the information is lost after objcopy conversion from elf to binary.
	 * But specify it anyway so that an objdump on the elf file shows an address starting at 0x8000,
	 * since it's more helpful for debugging purposes to see the addresses that will be used at execution
	 * time rather than an arbitrary starting address of 0.
	 */
	.init 0x8000 : { *(.init) }
	.text : { *(.text) }
	.data : { *(.data) }
	/DISCARD/ : { *(*) }
}

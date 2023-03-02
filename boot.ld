ENTRY(asm_entry)
SECTIONS
{
	/*
	 * Put .init before .text to guarantee that the assembly code is at the very start
	 * of the binary and comes before the C code, as it does set up required to execute
	 * the C code.
	 * TODO .init at 0x8000 and explanation of it?
	 */
	.init : { *(.init) }
	.text : { *(.text) }
	.data : { *(.data) }
	/DISCARD/ : { *(*) }
}

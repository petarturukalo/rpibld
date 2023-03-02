/*
 * Use .init section here and not .text so that this assembly code is 
 * guaranteed to be placed at the very start of the final bootloader binary
 * as specified in the linker script. If this were to use .text instead, 
 * because the C files also use .text, this code might not be placed at the
 * start of the file depending on the left-to-right order objects are passed
 * to ld.
 */
.section .text

// TODO where should i put these .global directives?
.global asm_entry
.global c_entry
asm_entry:
	/*
	 * Set up the stack for use by the C code. Because the stack grows downwards
	 * it is set to a reasonably high address to avoid it running into this program
	 * (this program is loaded to address 0x8000).
	 */
	mov sp, #0x10000000
	bl c_entry

loop:
	/* This shouldn't be reached. */
	b loop

	// TODO need to set up/zero-init other sections like .bss? maybe when they're added

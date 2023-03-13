/*
 * Use .init section here and not .text so that this assembly code is 
 * guaranteed to be placed at the very start of the final bootloader binary
 * as specified in the linker script. If this were to use .text instead, 
 * because the C files also use .text, this code might not be placed at the
 * start of the file depending on the left-to-right order objects are passed
 * to ld.
 */
.section .init

.global c_entry
.global ic_irq_exception_handler

/*
 * The addresses that the IRQ and supervisor (SVC) mode stacks start at.
 * Because the stacks grow downwards they are set to reasonably high addresses to 
 * avoid running into this program (this program is loaded to address 0x8000), and 
 * to separate addresses to avoid running into each other. They are also 4-byte
 * aligned as they won't work if not 4-byte aligned.
 */
.equ IRQ_STACK_START_ADDR, 0x07000000  /* ~117 MB. */
.equ SVC_STACK_START_ADDR, 0x10000000  /* ~268 MB. */

/* The first 5 bits of the CPSR register, bits 4:0, make up the mode field. */
.equ CPSR_MODE_BITS, 0b11111
.equ CPSR_IRQ_MODE,  0b10010
.equ CPSR_SVC_MODE,  0b10011

/* Offset to exception handler vectors from very start of memory. */
.equ IRQ_VECTOR_OFFSET, 0x18
.equ FIQ_VECTOR_OFFSET, 0x1c

.global asm_entry
asm_entry:
	/* 
	 * Only let the first of 4 cores/CPUs, core with CPU ID 0, continue execution.
	 * All other cores are left idle.
	 * TODO why, does it ruin setting registers?
	 * TODO can't get another core with CPU ID > 0 to run and call c_entry,
	 *	are they put in a spin loop by the firmware? 
	 * TODO either enable this if certain it's needed or remove it
	 */
	//mrc p15, 0, r0, c0, c0, 5  /* Read multiprocessor affinity register into r0. */
	//ands r0, r0, #0b11  /* Compare on CPU ID field. */
	//bne loop

	/*
	 * Switch from hypervisor mode (the mode that was booted into) to supervisor (SVC) mode. 
	 * If this isn't done then the IRQ exception is handled in hypervisor mode when it should
	 * be handled in IRQ mode.
	 */
	mrs r0, cpsr
	bic r0, #CPSR_MODE_BITS  /* Zero mode bits. */
	mov r1, r0
	orr r1, #CPSR_SVC_MODE  /* Set mode to supervisor. */
	msr spsr_hyp, r1
	/* 
	 * The program counter is 8 bytes ahead of the current instruction, so this 
	 * will have supervisor mode execution continue at the instruction directly 
	 * after eret.
	 */
	msr elr_hyp, pc
	eret

	/*
	 * Supervisor mode execution starts here. Now that we're out of the privilege 
	 * level 2 (PL2) hypervisor mode we can freely switch between the PL1 modes.
	 *
	 * Set up the stacks for use by the C code. Separate stacks are required for both
	 * IRQ mode and supervisor mode. First the IRQ mode stack is set.
	 */
	mov r1, r0
	orr r1, #CPSR_IRQ_MODE  /* Set mode to IRQ. */
	msr cpsr, r1
	mov sp, #IRQ_STACK_START_ADDR
	
	/* Set the supervisor mode stack. */
	mov r1, r0
	orr r1, #CPSR_SVC_MODE  /* Set mode back to supervisor. */
	msr cpsr, r1
	mov sp, #SVC_STACK_START_ADDR

	/*
	 * Set the exception handler vector for the IRQ exception in the vector table
	 * to an instruction that will branch to irq_exception_handler.
	 */
	ldr r0, irq_exception_vector
	ldr r1, =IRQ_VECTOR_OFFSET
	str r0, [r1]
	/*
	 * After assembly the instruction at irq_exception_vector will have its label operand replaced with
	 * a program counter relative address, expecting the operand to be 4 bytes after the instruction itself,
	 * so make sure to copy this dependency to 4 bytes after the IRQ exception vector.
	 * This will overwrite the FIQ exception vector, but that's okay because we won't be using any FIQs.
	 */
	ldr r0, addr_irq_exception_handler
	ldr r1, =FIQ_VECTOR_OFFSET
	str r0, [r1]

	bl c_entry

loop:
	// TODO power off instead?
	b loop


/*
 * Note the C function which this wraps can be jumped to directly in the IRQ exception vector if
 * it is marked with gcc ARM function attribute interrupt and parameter IRQ, e.g. __attribute__((interrupt("IRQ"))). 
 * Doing so will have the compiler generate code to return from the exception handler properly, e.g.
 * restoring the original value of the cpsr register, among other things. This isn't done, however, 
 * in order to retain documentation on how an IRQ exception handler needs to be written.
 */
irq_exception_handler:
	/* 
	 * Save the registers marked as scratch registers in the ARM Architecture Procedure
	 * Call Standard (AAPCS), plus the link register. The scratch registers are saved because
	 * compilation is free to use these registers for calculations within a procedure, and
	 * this exception could be interrupting a procedure.
	 */
	push {r0-r3, r12, lr}
	bl ic_irq_exception_handler
	pop {r0-r3, r12, lr}
	/* 
	 * Special IRQ exception handler return instruction that restores the previous value
	 * of cpsr that was implicitly saved and overwritten by the ARM core when the IRQ exception
	 * was raised.
	 */
	subs pc, lr, #4

irq_exception_vector:
	ldr pc, addr_irq_exception_handler

addr_irq_exception_handler: .word irq_exception_handler
	// TODO need to set up/zero-init other sections like .bss? maybe when they're added


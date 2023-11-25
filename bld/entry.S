/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 * 
 * Entry point to the bootloader. 
 */
#include "addrmap.h"
#include "bits.h"

/* Current program status register fields. */
#define CPSR_MODE     BITS(4, 0)  /* Mask. */
#define CPSR_MODE_IRQ 0b10010
#define CPSR_MODE_SVC 0b10011

#define VBAR 0x0  /* Vector base address. */

.extern c_entry
.extern ic_irq_exception_handler

.global asm_entry
.global vector_table
.global vector_table_pool_end

/*
 * Use .init section here and not .text so that this assembly code is 
 * guaranteed to be placed at the very start of the final bootloader binary
 * as specified in the linker script. If this were to use .text instead, 
 * because the C files also use .text, this code might not be placed at the
 * start of the file depending on the left-to-right order objects are passed
 * to ld.
 */
.section .init

/*
 * Set the stack address for a non-secure EL1 mode. 
 * The processor will remain in the chosen mode as a side effect.
 */
.macro set_stack, cpsr_mode, stack_start_addr
	mrs r0, cpsr
	bic r0, r0, #CPSR_MODE  /* Zero mode bits. */
	/* Can freely move between EL1 without needing an exception return. */
	orr r0, r0, \cpsr_mode  
	msr cpsr, r0
	mov sp, \stack_start_addr
.endm

/*
 * Only the primary core executes this. The secondary cores are spinning in the ARM
 * stub waiting to receive the kernel secondary core start address in its clear ARM mailbox
 * 3 from this primary core eventually executing the Linux kernel SMP code. 
 *
 * The spin code will not be trampled by the kernel because it is in the first 4 KiB of RAM,
 * which has been marked as reserved memory in the device tree.
 *
 * What I thought would be an issue, but turns out not to be an issue, is that these secondary
 * cores will not have the correct device tree blob address in register r2 when booting ARM Linux,
 * since this bootloader is what loads it. This is not an issue however because Linux SMP still boots 
 * successfully, presumably because the kernel already knows where the DTB is from the primary core 
 * (note, from debugging, the secondary cores jump into the kernel code with r2 set to 0).
 */
asm_entry:
	/* Set hypervisor vector base address (HVBAR). */
	mov r0, #VBAR
	mcr p15, 4, r0, c12, c0, 0

	/*
	 * Drop from non-secure hypervisor mode to non-secure supervisor mode. 
	 * The primary core will return to hypervisor mode when it boots the kernel.
	 */
	mrs r0, cpsr
	bic r0, r0, #CPSR_MODE      /* Zero mode bits. */
	orr r0, r0, #CPSR_MODE_SVC  /* Set mode to supervisor. */
	msr spsr_hyp, r0            /* Hyp eret loads spsr from spsr_hyp. */
	msr elr_hyp, pc		    /* Return to instruction after eret. */
	eret			    /* Hyp substitutes eret with load pc from elr_hyp. */

	/* 
	 * Set up stacks for the C code. 
	 * Separate stacks are required for both IRQ and supervisor mode.
	 */
	set_stack #CPSR_MODE_IRQ, #IRQ_STACK_START_ADDR
	set_stack #CPSR_MODE_SVC, #SVC_STACK_START_ADDR

	bl c_entry


/*
 * Set r0, r1, r2 required to boot ARM Linux, and then boot it.
 * 
 * @r3: kernel address to jump to
 */
_boot_kernel:
	mov r0, #0
	/* Set machine type to all ones to not match a type since it's determined by device tree. */
	mov r1, #~0  
	mov r2, #DTB_RAM_ADDR
	bx r3


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


/* So the vector table's PC-relative dependencies are immediately after it. */
.pool

/* Vector table that will be moved to the very start of RAM (address 0x0). */
vector_table:
	nop				/* 0x00: reset. */
	nop				/* 0x04: undefined instruction. */
	nop				/* 0x08: supervisor / SMC. */
	nop				/* 0x0c: prefetch abort. */
	nop				/* 0x10: data abort. */
	ldr pc, =_boot_kernel		/* 0x14: hypervisor. */
	ldr pc, =irq_exception_handler	/* 0x18: IRQ. */
	nop				/* 0x1c: FIQ. */
.pool
vector_table_pool_end:
	nop


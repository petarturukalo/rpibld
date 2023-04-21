/*
 * Interrupts.
 */
#ifndef INT_H
#define INT_H

/* Interrupt request. */
enum irq {
	IRQ_UNIMPLEMENTED,
	/* VideoCore interrupts. */
	IRQ_VC_TIMER1,
	IRQ_VC_SDC,
	IRQ_VC_SDHOST,
	IRQ_VC_EMMC2
};

/*
 * Clear/set the I bit (bit 7) of the CPSR register to enable/disable interrupts.
 */
void enable_interrupts(void);
void disable_interrupts(void);

#endif

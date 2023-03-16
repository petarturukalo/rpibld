/*
 * Interrupts.
 */
#ifndef INT_H
#define INT_H

enum irq {
	IRQ_UNIMPLEMENTED,
	IRQ_VC_TIMER1,
	IRQ_VC_MMC
};

/*
 * Clear/set the I bit (bit 7) of the CPSR register to enable/disable interrupts.
 */
void enable_interrupts(void);
void disable_interrupts(void);

#endif

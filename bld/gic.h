/*
 * Generic interrupt controller (GIC). 
 * Enabled if enable_gic=1 in config.txt.
 * TODO delete file
 */
#ifndef GIC_H
#define GIC_H

/*
 * Initialise the GIC-400 interrupt controller and enable Shared
 * Peripheral Interrupts (SPIs).
 * TODO which shared peripheral interrupts?
 */
void gic_init(void);
void gic_irq_exception_handler(void);

#endif

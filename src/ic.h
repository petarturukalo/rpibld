/*
 * The legacy interrupt controller.
 * Enabled if enable_gic=0 in config.txt.
 */
#ifndef IC_H
#define IC_H

void ic_enable_interrupts(void);
void ic_irq_exception_handler(void);

#endif

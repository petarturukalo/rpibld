/*
 * Access to (embedded) MMC (MultiMediaCard) / 
 * SD (Secure Digital) secondary storage.
 * TODO fix up comment after implemented more
 */
#ifndef MMC_H
#define MMC_H

/*
 * TODO document this after finished
 */
void mmc_init(void);
void mmc_trigger_dummy_interrupt(void);// TODO rm after testing
void mmc_dummy_isr(void);// TODO rm after testing?

#endif 

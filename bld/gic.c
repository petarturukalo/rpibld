/* TODO delete file */
#include "gic.h"
#include "mmio.h"

enum gic400_distributor_register {
	GICD_CTLR,
	GICD_IGROUPR3,
	GICD_ISENABLER3,
	GICD_IGRPMODR3
};

/* 
 * The offset for the GIC-400 is 0x3840000, with the distributor and CPU interface
 * blocks being +0x1000 and +0x2000 after that.
 */
static struct periph_access gic400_distributor_access = {
	.periph_base_off = 0x3841000,
	.register_offsets = {
		[GICD_CTLR]       = 0x0000,
		[GICD_IGROUPR3]   = 0x008c,
		[GICD_ISENABLER3] = 0x010c,
		[GICD_IGRPMODR3]  = 0x0d0c
	}
};

enum gic400_cpu_interface_register {
	GICC_CTLR
};

static struct periph_access gic400_cpu_interface_access = {
	.periph_base_off = 0x3842000,
	.register_offsets = {
		[GICC_CTLR] = 0x0000
	}
};

static void enable_distributor(void)
{
	/* Enable secure state affinity routing so can use secure group 1 interrupts. */
	register_enable_bits(&gic400_distributor_access, GICD_CTLR, 1<<4);
	// TODO after the above a read on the same register can't see bit 4 being set?
	// is the field write-only?
	/* 
	 * Enable system timer channel 0 (the first system timer) SPI INTID 96,
	 * and add it to secure group 1.
	 * TODO if 96 doesn't work try 64?
	 * TODO explanation of why it's 96?
	 * TODO move this to separate function?
	 */
	register_enable_bits(&gic400_distributor_access, GICD_ISENABLER3, 1);
	register_enable_bits(&gic400_distributor_access, GICD_IGROUPR3, 1);
	register_enable_bits(&gic400_distributor_access, GICD_IGRPMODR3, 1);
	/* Enable secure group 1 interrupts. */
	register_enable_bits(&gic400_distributor_access, GICD_CTLR, 1<<2);
}

static void enable_cpu_interface(void)
{
	/* Enable (TODO secure or non-secure?) group 1 interrupts */
	register_enable_bits(&gic400_cpu_interface_access, GICC_CTLR, 1<<1);
}

void gic_init(void)
{
	/*
	 * The interrupts we want to use, such as a system timer interrupt,
	 * are Shared Peripheral Interrupts (SPIs). When a peripheral triggers
	 * a SPI it is routed through the distributor and then to the CPU's CPU
	 * interface, so the distributor and CPU interface must both be enabled
	 * to receive these interrupts. 
	 * TODO info on using group 1 because it's IRQ
	 * TODO info on using secure parts because boots in secure mode?
	 */
	enable_distributor();
	enable_cpu_interface();
}

#include "led.h"  // TODO rm
void gic_irq_exception_handler(void)
{
	// TODO ack, etc., after test whether can trigger IRQ in the first place
	// TODO if use legacy IC instead this needs to be moved
	return;
}

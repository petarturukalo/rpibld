#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"
#include "sd/sd.h" // TODO rm
#include "led.h" // TODO rm
#include "debug.h" // TODO rm
#include "timer.h" // TODO rm
#include "heap.h" // TODO rm

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enum sd_init_error error;
	struct card card;
	byte_t *ram_addr;

	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();

	error = sd_init(&card);
	if (error != SD_INIT_ERROR_NONE) {
		signal_error(1);
		/*signal_error(ERROR_SD_INIT);*/
	}
	ram_addr = heap_get_base_address();
	if (!sd_read_block(ram_addr, (byte_t *)0, &card))
		signal_error(2);
	if (*(ram_addr+510) != 0x55)
		signal_error(3);
	if (*(ram_addr+511) != 0xaa)
		signal_error(4);
	signal_error(5);
	print_byte(*(ram_addr+511));
	print_byte(*(ram_addr+510));
	__asm("wfi");
}

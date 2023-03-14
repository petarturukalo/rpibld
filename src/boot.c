#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();
	signal_error(ERR_PLACEHOLDER);
}

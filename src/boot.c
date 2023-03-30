#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"
#include "mmc.h" // TODO rm
#include "led.h" // TODO rm
#include "debug.h" // TODO rm
#include "vcmailbox.h" // TODO rm
#include "help.h" // TODO rm
#include "timer.h" // TODO rm

void set_gpio_state(int state)
{	
	enum vcmailbox_error err;
	struct tag_io_gpio_set_state gpio;
	
	mzero(&gpio, sizeof(gpio));
	uint32_t pin = 134;  // SD PWR PIN
	/*uint32_t pin = 130;  // PWR PIN*/
	mcopy(&pin, gpio.args, sizeof(uint32_t));
	/*uint32_t state = 0;*/
	mcopy(&state, gpio.args+4, sizeof(uint32_t));

	err = vcmailbox_request_tags((struct tag_request[]){ GPIO_SET_STATE, &gpio }, 1);
	if (err != VCMBOX_ERROR_NONE)
		signal_error(err);
	/*uint32_t ret_pin = 0;*/
	/*mcopy(gpio.ret, &ret_pin, sizeof(uint32_t));*/
	/*uint32_t ret_state = 2;*/
	/*mcopy(gpio.ret+4, &ret_state, sizeof(uint32_t));*/

	/*if (pin != ret_pin)*/
		/*signal_error(1);*/
	/*if (state != ret_state)*/
		/*signal_error(2);*/
}

void get_gpio_state(void)
{
	enum vcmailbox_error err;
	struct tag_io_gpio_get_state gpio;
	
	mzero(&gpio, sizeof(gpio));
	uint32_t pin = 134;  // SD PWR PIN
	/*uint32_t pin = 132;  // VDD_SD_IO_SEL*/
	/*uint32_t pin = 130;  // PWR PIN*/
	mcopy(&pin, gpio.args, sizeof(uint32_t));

	err = vcmailbox_request_tags((struct tag_request[]){ GPIO_GET_STATE, &gpio }, 1);
	if (err != VCMBOX_ERROR_NONE)
		signal_error(err);

	uint32_t state = 2;
	mcopy(gpio.ret+4, &state, sizeof(uint32_t));
	print_bit(state, 0);
}

void get_gpio_cfg(void)
{
	enum vcmailbox_error err;
	struct tag_io_gpio_get_config cfg;

	mzero(&cfg, sizeof(cfg));
	/*uint32_t pin = 134;  // SD PWR PIN*/
	uint32_t pin = 132;  // VDD_SD_IO_SEL
	mcopy(&pin, cfg.args, sizeof(uint32_t));

	err = vcmailbox_request_tags((struct tag_request[]){ GPIO_GET_CONFIG, &cfg }, 1);
	if (err != VCMBOX_ERROR_NONE)
		signal_error(err);

	uint32_t ret, dir, pol, term_en, term_pull;
	mcopy(cfg.ret, &ret, sizeof(uint32_t));
	if (ret != 0)
		signal_error(1);
	mcopy(cfg.ret+4, &dir, sizeof(uint32_t));
	mcopy(cfg.ret+8, &pol, sizeof(uint32_t));
	mcopy(cfg.ret+12, &term_en, sizeof(uint32_t));
	mcopy(cfg.ret+16, &term_pull, sizeof(uint32_t));

	sleep_long();
	print_bit(dir, 0);
	sleep_long();
	print_bit(pol, 0);
	sleep_long();
	print_bit(term_en, 0);
	sleep_long();
	print_bit(term_pull, 0);
}

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();

	mmc_init();
	sleep(1000);
	mmc_trigger_dummy_interrupt();

	/*set_gpio_state(0);*/
	/*get_gpio_state();*/
	/*get_gpio_cfg();*/

	__asm__("wfi");
}

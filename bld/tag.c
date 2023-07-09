/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "tag.h"
#include "vcmailbox.h"
#include "error.h"
#include "debug.h"

struct power_state tag_power_get_state(uint32_t dev_id)
{
	struct {
		uint32_t dev_id;
		struct power_state state;
	} ret;
	struct tag_request req = { TAG_POWER_GET_STATE, &dev_id, sizeof(dev_id),
				   &ret, sizeof(ret) };
	enum vcmailbox_error error = vcmailbox_request_tags(&req, 1);

	if (error != VCMBOX_ERROR_NONE || ret.dev_id != dev_id) {
		serial_log("Vcmailbox error: power get state: %08x %08x", 
			   dev_id, ret.dev_id);
		signal_error(ERROR_VCMAILBOX);
	}
	return ret.state;
}


struct clock_state tag_clock_get_state(uint32_t clk_id)
{
	struct {
		uint32_t clk_id;
		struct clock_state state;
	} ret;
	struct tag_request req = { TAG_CLOCK_GET_STATE, &clk_id, sizeof(clk_id),
				   &ret, sizeof(ret) };
	enum vcmailbox_error error = vcmailbox_request_tags(&req, 1);

	if (error != VCMBOX_ERROR_NONE || ret.clk_id != clk_id) {
		serial_log("Vcmailbox error: clock get state: %08x %08x",
			   clk_id, ret.clk_id);
		signal_error(ERROR_VCMAILBOX);
	}
	return ret.state;
}

uint32_t tag_clock_get_rate(uint32_t clk_id)
{
	struct {
		uint32_t clk_id;
		uint32_t rate;
	} ret;
	struct tag_request req = { TAG_CLOCK_GET_RATE, &clk_id, sizeof(clk_id),
				   &ret, sizeof(ret) };
	enum vcmailbox_error error = vcmailbox_request_tags(&req, 1);

	if (error != VCMBOX_ERROR_NONE || ret.clk_id != clk_id) {
		serial_log("Vcmailbox error: clock get rate: %08x %08x",
			   clk_id, ret.clk_id);
		signal_error(ERROR_VCMAILBOX);
	}
	return ret.rate;
}


/* Pin number of the GPIO expander BT_ON pin in the VideoCore device tree. */
#define GPIO_EXPANDER_PIN_BASE 128

/*
 * Convert the input GPIO expander pin number to a GPIO expander pin number that 
 * the VideoCore knows about. These are pins starting at GPIO_EXPANDER_PIN_BASE.
 */
static int convert_pin_to_vc(enum gpio_expander_pin pin)
{
	return GPIO_EXPANDER_PIN_BASE+pin;
}

uint32_t tag_gpio_get_state(uint32_t pin)
{
	struct {
		uint32_t unused;
		uint32_t state;
	} ret;
	uint32_t vcpin = convert_pin_to_vc(pin);
	struct tag_request req = { TAG_GPIO_GET_STATE, &vcpin, sizeof(vcpin), 
				   &ret, sizeof(ret) };
	enum vcmailbox_error error = vcmailbox_request_tags(&req, 1);

	/* A non-zero "unused" is an error in the Linux Raspberry Pi 3 expander GPIO driver. */
	if (error != VCMBOX_ERROR_NONE || ret.unused != 0) {
		serial_log("Vcmailbox error: gpio get state: %08x", ret.unused);
		signal_error(ERROR_VCMAILBOX);
	}
	return ret.state;
}

struct gpio_expander_pin_config tag_gpio_get_config(uint32_t pin)
{
	struct gpio_expander_pin_config cfg;
	uint32_t vcpin = convert_pin_to_vc(pin);
	struct tag_request req = { TAG_GPIO_GET_CONFIG, &vcpin, sizeof(vcpin), 
				   &cfg, sizeof(cfg) };
	enum vcmailbox_error error = vcmailbox_request_tags(&req, 1);

	/* A non-zero "unused" is an error in the Linux Raspberry Pi 3 expander GPIO driver. */
	if (error != VCMBOX_ERROR_NONE || cfg.unused != 0) {
		serial_log("Vcmailbox error: gpio get config: %08x", cfg.unused);
		signal_error(ERROR_VCMAILBOX);
	}
	return cfg;
}

bool gpio_config_pin_is_output(struct gpio_expander_pin_config *cfg)
{
	return cfg->direction;
}

bool gpio_config_pin_is_active_high(struct gpio_expander_pin_config *cfg)
{
	return !cfg->polarity;
}


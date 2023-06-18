/*
 * Get tags/properties from the VideoCore.
 */
#ifndef TAG_H
#define TAG_H

#include "type.h"

enum power_device_id {
	PWR_DEV_SD = 0x0
};

/*
 * @on: whether the device is powered on
 * @dev_not_exists: whether the device does not exist
 */
struct power_state {
	bits_t on : 1;
	bits_t dev_not_exists : 1;
	bits_t reserved : 30;
} __attribute__((packed));

/*
 * @dev_id: an enum power_device_id
 */
struct power_state tag_power_get_state(uint32_t dev_id);


enum clock_id {
	CLK_CORE  = 0x4,  /* VPU. */
	CLK_EMMC2 = 0xc
};

/*
 * @on: whether the clock is on
 * @clk_not_exists: whether the clock does not exist
 */
struct clock_state {
	bits_t on : 1;
	bits_t clk_not_exists : 1;
	bits_t reserved : 30;
} __attribute__((packed));

/*
 * @clk_id: an enum clock_id
 */
struct clock_state tag_clock_get_state(uint32_t clk_id);
/* Return clock rate in Hz. */
uint32_t tag_clock_get_rate(uint32_t clk_id);


/*
 * @GPIO_EXPANDER_VDD_SD_IO_SEL: supply for SD bus IO line power.
 *	0 is 3.3V, 1 is 1.8V.
 * @GPIO_EXPANDER_SD_PWR_ON: whether the SD card is supplied 3.3V power.
 *	Note that the state of GPIO_EXPANDER_VDD_SD_IO_SEL does not affect
 *	this - this remains fixed at 3.3V.
 */
enum gpio_expander_pin {
	GPIO_EXPANDER_BT_ON,
	GPIO_EXPANDER_WL_ON,
	GPIO_EXPANDER_PWR_LED,
	GPIO_EXPANDER_GLOBAL_RESET,
	GPIO_EXPANDER_VDD_SD_IO_SEL,
	GPIO_EXPANDER_CAM,
	GPIO_EXPANDER_SD_PWR_ON
};

/*
 * Get the state of a GPIO expander pin.
 * @pin: an enum gpio_expander_pin
 */
uint32_t tag_gpio_get_state(uint32_t pin);

/*
 * Config for a GPIO expander pin.
 *
 * @direction: 0 means the pin is an input, 1 means it's an output
 * @polarity: 0 for active high, 1 for active low / inverted
 * @term_en: termination enabled; 0 for no termination, 1 for pull enabled
 * @term_pull_up: if termination is enabled then 0 is pull down, 1 is pull up
 */
struct gpio_expander_pin_config {
	uint32_t unused;
	uint32_t direction;
	uint32_t polarity;
	uint32_t term_en;
	uint32_t term_pull_up;
};

/*
 * Get a GPIO expander pin's config.
 * @pin: an enum gpio_expander_pin
 */
struct gpio_expander_pin_config tag_gpio_get_config(uint32_t pin);
bool gpio_config_pin_is_output(struct gpio_expander_pin_config *cfg);
bool gpio_config_pin_is_active_high(struct gpio_expander_pin_config *cfg);

#endif

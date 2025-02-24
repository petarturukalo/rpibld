/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * Get tags/properties from the VideoCore.
 */
#ifndef TAG_H
#define TAG_H

#include "type.h"

enum power_device_id {
	PWR_DEV_SD = 0x0
};

struct power_state {
	bits_t on : 1;  /**< Whether the device is powered on */
	bits_t dev_not_exists : 1;  /**< Whether the device does not exist */
	bits_t reserved : 30;
} __attribute__((packed));

/**
 * @param dev_id An enum power_device_id
 */
struct power_state tag_power_get_state(uint32_t dev_id);


enum clock_id {
	CLK_CORE  = 0x4,  /* VPU. */
	CLK_EMMC2 = 0xc
};

struct clock_state {
	bits_t on : 1;  /**< Whether the clock is on */
	bits_t clk_not_exists : 1;  /**< Whether the clock does not exist */
	bits_t reserved : 30;
} __attribute__((packed));

/**
 * @param clk_id An enum clock_id
 */
struct clock_state tag_clock_get_state(uint32_t clk_id);
/** @return Clock rate in Hz. */
uint32_t tag_clock_get_rate(uint32_t clk_id);


enum gpio_expander_pin {
	GPIO_EXPANDER_BT_ON,
	GPIO_EXPANDER_WL_ON,
	GPIO_EXPANDER_PWR_LED,
	GPIO_EXPANDER_GLOBAL_RESET,
	GPIO_EXPANDER_VDD_SD_IO_SEL,  /**< Supply for SD bus IO line power. 0 is 3.3V, 1 is 1.8V. */
	GPIO_EXPANDER_CAM,
	/**
	 * whether the SD card is supplied 3.3V power. Note that the state of 
	 * GPIO_EXPANDER_VDD_SD_IO_SEL does not affect this - this remains fixed at 3.3V.
	 */
	GPIO_EXPANDER_SD_PWR_ON
};

/**
 * @brief Get the state of a GPIO expander pin.
 * @param pin An enum gpio_expander_pin
 */
uint32_t tag_gpio_get_state(uint32_t pin);

/**
 * @brief Config for a GPIO expander pin.
 */
struct gpio_expander_pin_config {
	uint32_t unused;
	uint32_t direction;	/**< 0 means the pin is an input, 1 means it's an output */
	uint32_t polarity;	/**< 0 for active high, 1 for active low / inverted */
	uint32_t term_en;	/**< Termination enabled; 0 for no termination, 1 for pull enabled */
	uint32_t term_pull_up;  /**< If termination is enabled then 0 is pull down, 1 is pull up */
};

/**
 * @brief Get a GPIO expander pin's config.
 * @param pin An enum gpio_expander_pin
 */
struct gpio_expander_pin_config tag_gpio_get_config(uint32_t pin);
bool gpio_config_pin_is_output(struct gpio_expander_pin_config *cfg);
bool gpio_config_pin_is_active_high(struct gpio_expander_pin_config *cfg);

#endif

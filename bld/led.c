/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * From the BCM2711 RPI 4 B device tree source the ACT LED is accessible through pin 42
 * and has normal polarity (is active high).
 */
#include "led.h"
#include "gpio.h"

void led_init(void)
{
	gpio_pin_select_op(GPIO_PIN_LED, GPIO_OP_OUTPUT);
}

void led_turn_on(void)
{
	gpio_pin_set(GPIO_PIN_LED);
}

void led_turn_off(void) 
{
	gpio_pin_clr(GPIO_PIN_LED);
}


/*
 * Access to GPIO pins.
 */
#ifndef GPIO_H
#define GPIO_H

enum gpio_pin {
	GPIO_PIN_TXD1 = 14,
	GPIO_PIN_LED  = 42
};

/* GPIO pin operation. */
enum gpio_op {
	GPIO_OP_INPUT    = 0b000,
	GPIO_OP_OUTPUT   = 0b001,
	/* Alternate functions. */
	GPIO_OP_ALT_FN_0 = 0b100,
	GPIO_OP_ALT_FN_1 = 0b101,
	GPIO_OP_ALT_FN_2 = 0b110,
	GPIO_OP_ALT_FN_3 = 0b111,
	GPIO_OP_ALT_FN_4 = 0b011,
	GPIO_OP_ALT_FN_5 = 0b010
};

/*
 * Select a GPIO pin's operation.
 */
void gpio_pin_select_op(enum gpio_pin pin, enum gpio_op op);

/*
 * Set/clear a GPIO pin.
 */
void gpio_pin_set(enum gpio_pin pin);
void gpio_pin_clr(enum gpio_pin pin);

#endif

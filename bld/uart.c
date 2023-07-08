#include "uart.h"
#include "mmio.h"
#include "tag.h"
#include "gpio.h"
#include "help.h"
#include "gpio.h"
#include "bits.h"

enum uart_register {
	AUX_ENABLES,
	AUX_MU_IO_REG,
	AUX_MU_LCR_REG,  /* Line control register. */
	AUX_MU_LSR_REG,  /* Line status register. */
	AUX_MU_CNTL_REG,
	AUX_MU_BAUD_REG
};

static struct periph_access uart_access = {
	.periph_base_off = 0x2215000,
	.register_offsets = {
		[AUX_ENABLES]     = 0x04,
		[AUX_MU_IO_REG]   = 0x40,
		[AUX_MU_LCR_REG]  = 0x4c,
		[AUX_MU_LSR_REG]  = 0x54,
		[AUX_MU_CNTL_REG] = 0x60,
		[AUX_MU_BAUD_REG] = 0x68
	}
};

#define AUX_ENABLES_MINI_UART_ENABLE  BIT(0)

#define AUX_MU_IO_REG_TX_DATA  BITS(7, 0)  /* Transmit data. */

/* BCM2711 datasheet is wrong. Need to set bits 1:0 to 0b11 to get 8 data bits. */
#define AUX_MU_LCR_REG_DATA_SIZE  BITS(1, 0)

#define AUX_MU_LSR_REG_TX_EMPTY  BIT(5)  /* Transmitter empty. */

#define AUX_MU_CNTL_REG_RX_EN  BIT(0)  /* Receive enable. */

static void uart_set_baudrate(int baudrate)
{
	/*
	 * The value set in the baudrate register is calculated by rearranging
	 * the baudrate equation in section '2.2.1. Mini UART implementation details'
	 * from the BCM2711 datasheet.
	 *
	 * The VPU clock is used because it's the clock listed for the mini UART / 
	 * auxiliary peripherals in the BCM2711 RPI 4 B device tree. This is also the clock
	 * that was fixed with config.txt entries core_freq[_min] explained at the top of uart.h.
	 */
	int system_clk_freq = tag_clock_get_rate(CLK_CORE);
	int oversampling = 8;
	int baudrate_reg = system_clk_freq/(baudrate*oversampling) - 1;
	/* 
	 * Note this won't set the baudrate to exactly the same as the input parameter since
	 * the fractional part of the baudrate register value is lost because it's stored
	 * as an integer. But it's close enough to still work (this is just a "shortcoming"
	 * of the mini UART).
	 */
	register_set(&uart_access, AUX_MU_BAUD_REG, baudrate_reg);
}

void uart_init(void)
{
	/* Setting the pin to alt function 5 selects it as TXD1. */
	gpio_pin_select_op(GPIO_PIN_TXD1, GPIO_OP_ALT_FN_5);
	/* Enable mini UART. */
	register_set(&uart_access, AUX_ENABLES, AUX_ENABLES_MINI_UART_ENABLE);

	/* Disable receiver. */
	register_disable_bits(&uart_access, AUX_MU_CNTL_REG, AUX_MU_CNTL_REG_RX_EN);

	/* Set parameters listed in the comment at this function's prototype. */
	uart_set_baudrate(115200);
	/* Set 8 data bits. */
	register_set(&uart_access, AUX_MU_LCR_REG, AUX_MU_LCR_REG_DATA_SIZE);
	/* 
	 * No parity and 1 stop bits are default features of the mini UART and 
	 * don't need to be explicitly set. 
	 * There is no hardware flow control because the RTS/CTS lines aren't
	 * being used.
	 * The mini UART is configured for polled operation so don't need to 
	 * set up any interrupts.
	 */
}

static bool uart_lsr_transmitter_not_empty(void)
{
	return !(register_get(&uart_access, AUX_MU_LSR_REG)&AUX_MU_LSR_REG_TX_EMPTY);
}

void uart_transmit(void *data, int n)
{
	byte_t *byte = data;

	for (; n--; ++byte) {
		while_cond_timeout_infinite(uart_lsr_transmitter_not_empty, 1);
		/* Transmit a byte of data. */
		register_set(&uart_access, AUX_MU_IO_REG, (*byte)&AUX_MU_IO_REG_TX_DATA);
	}
}


/*
 * Access to the mini UART.
 *
 * To use the mini UART the following must be set in config.txt.
 * - enable_uart=1 TODO why? need it for kernel to print to serial console.
 *   its affect on bootloader is that i don't need to do half the things already
 *   done in my uart_init(). if say need it here then put a comment in mini_uart()
 *   implementation saying most of it seems to be covered by firmware when doing enable_uart=1.
 *   TODO test whether. don't think needed anymore, just clock_freq, since i enable it
 * - clock_freq=250 and clock_freq_min=250 to fix the VPU/core clock to 250 MHz.
 *   Without this the clock rate is unstable and the mini UART doesn't work.
 *   From testing also, without this the VPU clock rate is 200 MHz, but fixing
 *   the clock at 200 MHz by setting clock_freq[_min] to 200 doesn't work, so
 *   250 should be used because it's the clock rate mentioned in the BCM2711
 *   datasheet and it's what works.
 */
#ifndef UART_H
#define UART_H

/*
 * Initialise the mini UART for transmission on GPIO pin 14.
 *
 * The UART is initialised with the following parameters, which
 * the receiver is expected to match:
 * - 115,200 baudrate
 * - 8 data bits
 * - no parity
 * - 1 stop bits
 * - no hardware flow control
 */
void uart_init(void);

/*
 * Transmit n bytes of data over the mini UART.
 * A single call to uart_init() must have been made before
 * any call to this.
 */
void uart_transmit(void *data, int n);

#endif

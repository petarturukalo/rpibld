/*
 * Access to the mini UART.
 *
 * To use the mini UART clock_freq=250 and clock_freq_min=250 
 * must be set in config.txt to fix the VPU/core clock to 250 MHz.
 * Without this the clock rate is unstable and the mini UART doesn't 
 * work. From testing also, without this the VPU clock rate is 200 MHz, 
 * but fixing the clock at 200 MHz by setting clock_freq[_min] to 200 
 * doesn't work, so 250 should be used because it's the clock rate 
 * mentioned in the BCM2711 datasheet and it's what works.
 *
 * Note setting enable_uart=1 is NOT required in config.txt, and is actually
 * redundant here because uart_init() is enabling it.
 */
#ifndef UART_H
#define UART_H

/*
 * Initialise the mini UART for transmission on GPIO pin 14.
 *
 * The UART is initialised with the following parameters (115,200 8N1), 
 * which the receiver is expected to match:
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

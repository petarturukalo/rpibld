// TODO explain where i get values from (datasheet)
/*
 * Base address of main peripherals section in ARM low peripheral mode 
 * view of address map.
 */
#define ARM_LO_MAIN_PERIPH_BASE_ADDR 0xfc000000
/*
 * Offset of the GPIO controller from the address map main peripherals section.
 * Computed by 0x7e200000 - 0x7c000000 where 0x7e200000 is the address of the first GPIO 
 * register listed in the BCM2711 datasheet, GPFSEL0, and 0x7c000000 is the address of the 
 * main peripherals section in the legacy master view of the address map. This legacy master
 * view address is used because addresses listed in the BCM2711 datasheet, such as 0x7e200000 
 * for GPFSEL0, refer to the legacy master view of the address map.
 */
#define GPIO_OFFSET 0x2200000

typedef int word_t;
typedef char byte_t;

/*
 * Entry point to the C code, the function branched to when switching from 
 * assembly to C.
 */
void c_entry(void)
{
	byte_t *gpio_base_addr = (byte_t *)(ARM_LO_MAIN_PERIPH_BASE_ADDR + GPIO_OFFSET);

	/*
	 * From the BCM2711 RPI 4 B device tree source the ACT LED is accessible through pin 42
	 * and has normal polarity (is active high).
	 * Set bits 8:6 of the GPFSEL4 register to 001, selecting pin 42 to be an output.
	 */
	*(word_t *)(gpio_base_addr+0x10) = 1<<6;  /* 0x10 is offset of GPFSEL4 register. */
	/* 
	 * Set the value of pin 42 to 1. Because the pin is active high this will turn on the LED. 
	 */
	*(word_t *)(gpio_base_addr+0x20) = 1<<10;  /* 0x20 is offset of GPSET1 register. */
}

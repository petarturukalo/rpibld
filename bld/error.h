/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef ERROR_R
#define ERROR_R

/*
 * @ERROR_SECONDARY_CORE: error relocating a secondary core
 * @ERROR_VCMAILBOX: error using the VideoCore mailbox
 * @ERROR_VC_NOT_INIT_MMC: the VideoCore firmware didn't intialise
 *	MMC as expected (see mmc_assert_vc_init() for more info)
 * @ERROR_INFINITE_LOOP: see while_cond_timeout_infinite()
 * @ERROR_VSNPRINTF: Vsnprintf() failed
 * @ERROR_SD_INIT: failed to initialise the SD card with call to sd_init()
 * @ERROR_INVALID_PARTITION: the IMAGE_PARTITION macro defined in the makefile is
 *	not a valid MBR primary partition number
 * @ERROR_NO_IMAGE_MAGIC: no magic identifying an image found at the start of the
 *	image partition
 * @ERROR_IMAGE_OVERFLOW: size of image is greater than the boot partition
 * @ERROR_IMAGE_CONTENTS: the contents of the image was not as expected
 * @ERROR_KERN_OVERFLOW: the size of the kernel file loaded into RAM is too big
 *	and overflowed into the device tree blob's area of RAM
 */
enum error_code {
	/* Ensure these are set to an explicit value to know how many times it beeps. */
	ERROR_SECONDARY_CORE    = 1,
	ERROR_VCMAILBOX         = 2,
	ERROR_VC_NOT_INIT_MMC   = 3,
	ERROR_INFINITE_LOOP     = 4,
	ERROR_VSNPRINTF         = 5,
	ERROR_SD_INIT           = 6,
	ERROR_SD_READ           = 7,
	ERROR_NO_MBR_MAGIC      = 8,
	ERROR_INVALID_PARTITION = 9,
	ERROR_NO_IMAGE_MAGIC    = 10,
	ERROR_IMAGE_OVERFLOW    = 11,
	ERROR_IMAGE_CONTENTS    = 12,
	ERROR_KERN_OVERFLOW     = 13,
	ERROR_SD_RESET          = 14
};

/*
 * Continuously signal an error code to the user through the LED until the
 * raspberry pi is manually powered off. The error code is signalled by blinking
 * the LED on/off the integer version of the error code amount of times. There is a 
 * short pause between blinks in a signal and a long pause between signals.
 */
void signal_error(enum error_code error);

#endif

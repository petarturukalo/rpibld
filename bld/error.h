/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef ERROR_H
#define ERROR_H

enum error_code {
/* Ensure these are set to an explicit value to know how many times it beeps. */
	ERROR_VCMAILBOX         = 1,  /**< Error using the VideoCore mailbox */
	/** The VideoCore firmware didn't intialise MMC as expected (see @ref sd_assert_vc_init() for more info) */
	ERROR_VC_NOT_INIT_MMC   = 2,  
	ERROR_INFINITE_LOOP     = 3,  /**< See @ref while_cond_timeout_infinite() */
	ERROR_VSNPRINTF         = 4,  /**< Vsnprintf() failed */
	ERROR_SD_INIT           = 5,  /**< Failed to initialise the SD card with call to sd_init() */
	ERROR_SD_READ           = 6,
	ERROR_NO_MBR_MAGIC      = 7,
	/** The IMAGE_PARTITION macro defined in the makefile is not a valid MBR primary partition number */
	ERROR_INVALID_PARTITION = 8,  
	ERROR_NO_IMAGE_MAGIC    = 9,   /**< No magic identifying an image found at the start of the image partition */
	ERROR_IMAGE_OVERFLOW    = 10,  /**< Size of image is greater than the image partition */
	ERROR_IMAGE_CONTENTS    = 11,  /**< The contents of the image was not as expected */
	/** The size of the kernel file loaded into RAM is too big and overflowed into the device tree blob's area of RAM */
	ERROR_KERN_OVERFLOW     = 12,
	ERROR_SD_RESET          = 13
};

/**
 * Continuously signal an error code to the user through the LED until the
 * raspberry pi is manually powered off. The error code is signalled by blinking
 * the LED on/off the integer version of the error code amount of times. There is a 
 * short pause between blinks in a signal and a long pause between signals.
 */
void signal_error(enum error_code error);

#endif

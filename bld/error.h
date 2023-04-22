#ifndef ERROR_R
#define ERROR_R

/*
 * @ERROR_VCMAILBOX: error using the VideoCore mailbox
 * @ERROR_VC_NOT_INIT_MMC: the VideoCore firmware didn't intialise
 *	MMC as expected (see mmc_assert_vc_init() for more info)
 * @ERROR_SD_INIT: failed to initialise the SD card with call to sd_init()
 * @ERROR_INVALID_PARTITION: the IMAGE_PARTITION macro defined in the makefile is
 *	not a valid MBR primary partition number
 * @ERROR_NO_IMAGE_MAGIC: no magic identifying an image found at the start of the
 *	image partition
 * @ERROR_IMAGE_OVERFLOW: size of image is greater than the boot partition
 */
enum error_code {
	/* Ensure these are set to an explicit value to know how many times it beeps. */
	ERROR_VCMAILBOX         = 1,
	ERROR_VC_NOT_INIT_MMC   = 2,
	ERROR_SD_INIT           = 3,
	ERROR_SD_READ           = 4,
	ERROR_NO_MBR_MAGIC      = 5,
	ERROR_INVALID_PARTITION = 6,
	ERROR_NO_IMAGE_MAGIC    = 7,
	ERROR_IMAGE_OVERFLOW    = 8
};

/*
 * Continuously signal an error code to the user through the LED until the
 * raspberry pi is manually powered off. The error code is signalled by blinking
 * the LED on/off the integer version of the error code amount of times. There is a 
 * short pause between blinks in a signal and a long pause between signals.
 */
void signal_error(enum error_code error);

#endif

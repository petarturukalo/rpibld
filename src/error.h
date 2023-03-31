#ifndef ERROR_R
#define ERROR_R

/*
 * @ERROR_VCMAILBOX: error using the VideoCore mailbox
 * @ERROR_VC_NOT_INIT_MMC: the VideoCore firmware didn't intialise
 *	MMC as expected (see mmc_assert_vc_init() for more info)
 */
enum error_code {
	/* Ensure these are set to an explicit value to know how many times it beeps. */
	ERROR_VCMAILBOX = 1,
	ERROR_VC_NOT_INIT_MMC = 2
};

/*
 * Continuously signal an error code to the user through the LED until the
 * raspberry pi is manually powered off. The error code is signalled by blinking
 * the LED on/off the integer version of the error code amount of times. There is a 
 * short pause between blinks in a signal and a long pause between signals.
 */
void signal_error(enum error_code error);

#endif

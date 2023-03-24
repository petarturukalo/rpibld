#ifndef ERROR_R
#define ERROR_R

// TODO explanation after get more here
// TODO don't like ERR prefix, make it ERROR?
enum error_code {
	// TODO explicitly set these so you know how many times it beeps
	ERR_PLACEHOLDER1 = 1,  // TODO rm
	ERR_PLACEHOLDER2 = 2   // TODO rm
};

/*
 * Continuously signal an error code to the user through the LED until the
 * raspberry pi is manually powered off. The error code is signalled by blinking
 * the LED on/off the integer version of the error code amount of times. There is a 
 * short pause between blinks in a signal and a long pause between signals.
 */
void signal_error(enum error_code error);

#endif

/*
 * Access to SD (Secure Digital) / (embedded) MMC (MultiMediaCard) 
 * secondary storage.
 * TODO fix up comment after implemented more
 */
#ifndef SD_H
#define SD_H

/*
 * @SD_ERROR_CMD_ISSUE: error issuing a SD command
 */
enum sd_error {
	SD_ERROR_NONE,
	SD_ERROR_ISSUE_CMD,
	SD_ERROR_UNUSABLE_CARD
};

/*
 * Initialise the inserted SD card so that it is ready for data
 * transfer with TODO mention sd read/write fns. The card is 
 * initialised to 4-bit data bus width, default speed bus mode.
 */
enum sd_error sd_init(void);

#endif 

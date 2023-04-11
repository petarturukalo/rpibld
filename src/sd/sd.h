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
 * TODO document this after finished
 */
enum sd_error sd_init(void);

#endif 

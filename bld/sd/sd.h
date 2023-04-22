/*
 * Read access to SD (Secure Digital) / (embedded) MMC (MultiMediaCard) 
 * secondary storage.
 */
#ifndef SD_H
#define SD_H

#include "../type.h"

/*
 * @SD_INIT_ERROR_CMD_ISSUE: error issuing a SD command
 */
enum sd_init_error {
	SD_INIT_ERROR_NONE,
	SD_INIT_ERROR_ISSUE_CMD,
	SD_INIT_ERROR_UNUSABLE_CARD
};

/*
 * Initialise the inserted SD card so that it is ready for data
 * transfer with sd_read_block() TODO recomment if fn gets renamed. 
 * The card is initialised to 4-bit data bus width, 25 MHz clock, 
 * default speed bus mode.
 */
enum sd_init_error sd_init(void);

/*
 * Read a single block of size READ_BLKSZ from the SD card into RAM.
 *
 * @ram_dest_addr: destination address in RAM to copy read data to
 * @sd_src_lba: source SD card logical block address to read data from (blocks
 *	are of size READ_BLKSZ)
 *
 * Return whether the read was successful.
 */
bool sd_read_block(byte_t *ram_dest_addr, void *sd_src_lba);

#endif 

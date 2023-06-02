/*
 * Read access to SD (Secure Digital) secondary storage.
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
 * transfer with sd_read_blocks(). The card is initialised to 
 * 4-bit data bus width, 25 MHz clock, default speed bus mode.
 * TODO supposedly up to 12.5 MB/sec transfer rate
 */
enum sd_init_error sd_init(void);

/*
 * Read one or more blocks of size READ_BLKSZ from the SD card into RAM.
 *
 * @ram_dest_addr: 4-byte aligned destination address in RAM to copy read data to
 * @sd_src_lba: source SD card logical block address to read data from (blocks
 *	are of size READ_BLKSZ)
 * @nblks: number of blocks to read
 *
 * Return whether the read was successful.
 */
bool sd_read_blocks(byte_t *ram_dest_addr, void *sd_src_lba, int nblks);
/*
 * Read a number of bytes from the SD card into RAM.
 */
bool sd_read_bytes(byte_t *ram_dest_addr, void *sd_src_lba, int bytes);

/*
 * Get the number of blocks required to read a number of bytes.
 * If the number of bytes isn't a multiple of READ_BLKSZ the last block
 * will have unused bytes.
 */
int bytes_to_blocks(int bytes);

#endif 

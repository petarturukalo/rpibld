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

enum card_state {
/* Inactive operation mode. */
	CARD_STATE_INACTIVE = -1,
/* Card identification operation mode. */
	CARD_STATE_IDLE,
	CARD_STATE_READY,
	CARD_STATE_IDENTIFICATION,
/* Data transfer operation mode. */
	CARD_STATE_STANDBY,
	CARD_STATE_TRANFSFER,
/* Below aren't set explicitly (but are still data transfer). */
	CARD_STATE_SENDING_DATA,
	CARD_STATE_RECEIVE_DATA,
	CARD_STATE_PROGRAMMING,
	CARD_STATE_DISCONNECT
};

/*
 * Card (SD card) metadata and bookkeeping data.
 *
 * @state: the card's current state
 * @version_2_or_later: whether the card supports the physical layer
 *	specification version 2.00 or later. For reference SDSC (or just
 *	SD) was defined in version 2, SDHC in version 2, and SDXC in version 3.
 * @sdhc_or_sdxc: whether the card is either of SDHC (high capacity) or SDXC
 *	(extended capacity). If false the card is SDSC (standard capacity).
 * @rca: card's relative card address
 */
struct card {
	enum card_state state;
	bool version_2_or_later;
	bool sdhc_or_sdxc;
	int rca;
};

/*
 * Initialise the inserted SD card so that it is ready for data
 * transfer with sd_read_block() TODO recomment if fn gets renamed. 
 * The card is initialised to 4-bit data bus width, 25 MHz clock, 
 * default speed bus mode.
 */
enum sd_init_error sd_init(struct card *card_out);

/*
 * Read a single block of size READ_BLKSZ from the SD card into RAM.
 *
 * @ram_dest_addr: destination address in RAM to copy read data to
 * @sd_src_lba: source SD card logical block address to read data from (blocks
 *	are of size READ_BLKSZ)
 *
 * Return whether the read was successful.
 */
bool sd_read_block(byte_t *ram_dest_addr, byte_t *sd_src_lba, struct card *card);

#endif 

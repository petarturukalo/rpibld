/*
 * Access to SD (Secure Digital) / (embedded) MMC (MultiMediaCard) 
 * secondary storage.
 * TODO fix up comment after implemented more
 */
#ifndef SD_H
#define SD_H

#include "../type.h"

/*
 * @SD_ERROR_CMD_ISSUE: error issuing a SD command
 * TODO rename to sd_init_error if only used by sd_init()?
 */
enum sd_error {
	SD_ERROR_NONE,
	SD_ERROR_ISSUE_CMD,
	SD_ERROR_UNUSABLE_CARD
};

// TODO mv this?
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
// TODO if not going to use below states don't worry about setting states at all?
// or just comment that below are unused
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
 * transfer with TODO mention sd read/write fns. The card is 
 * initialised to 4-bit data bus width, 25 MHz clock, default speed 
 * bus mode.
 */
enum sd_error sd_init(struct card *card_out);

/*
 * Read a single block of size DEFAULT_READ_BLKSZ from the SD card into RAM.
 *
 * @ram_dest_addr: destination address in RAM to copy read data to
 * @sd_src_addr: source SD card address to read data from. If the card is
 *	SDHC or SDXC this must be DEFAULT_READ_BLKSZ-byte aligned so it
 *	can be converted into a block unit address. To be safe use a 
 *	DEFAULT_READ_BLKSZ address regardless of card capacity.
 * TODO flip this the other way around if find it simpler? i.e. pass a block address
 *
 * Return false if sd_src_addr isn't DEFAULT_READ_BLKSZ-byte aligned or there
 * was an error issuing the read command.
 */
bool sd_read_block(byte_t *ram_dest_addr, byte_t *sd_src_addr, struct card *card);

#endif 

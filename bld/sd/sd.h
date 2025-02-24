/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * Read access to SD (Secure Digital) secondary storage.
 */
#ifndef SD_H
#define SD_H

#include "../type.h"

enum sd_init_error {
	SD_INIT_ERROR_NONE,
	SD_INIT_ERROR_ISSUE_CMD,  /**< Error issuing a SD command */
	SD_INIT_ERROR_UNUSABLE_CARD
};

/**
 * Initialise the inserted SD card so that it is ready for data
 * transfer with sd_read_blocks(). The card is initialised to 
 * 4-bit data bus width, 25 MHz clock, default speed bus mode.
 *
 * This should supposedly have an up to 12.5 MB/sec transfer rate, 
 * but currently it's only 7 MB/sec. This is is plenty fast enough already,
 * but the rest of the performance can likely be gained by enabling 
 * and setting up the caches, which is left unimplemented for now.
 */
enum sd_init_error sd_init(void);

/**
 * @brief Read one or more blocks of size SD_BLKSZ from the SD card into RAM.
 *
 * @param ram_dest_addr 4-byte aligned destination address in RAM to copy read data to
 * @param sd_src_lba Source SD card logical block address to read data from (blocks
 *		     are of size SD_BLKSZ)
 * @param nblks Number of blocks to read
 *
 * @return Whether the read was successful.
 */
bool sd_read_blocks(byte_t *ram_dest_addr, uint32_t sd_src_lba, int nblks);
/**
 * @brief Read a number of bytes from the SD card into RAM.
 */
bool sd_read_bytes(byte_t *ram_dest_addr, uint32_t sd_src_lba, int bytes);

/**
 * Get the number of blocks required to read a number of bytes.
 * If the number of bytes isn't a multiple of SD_BLKSZ the last block
 * will have unused bytes.
 */
int bytes_to_blocks(int bytes);

/**
 * @brief Reset the SD card and host controller to its state at boot.
 * @return Whether successful.
 */
bool sd_reset(void);

#endif 

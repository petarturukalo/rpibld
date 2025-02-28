/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * Imager definitions shared between the bootloader and imager.
 * The imager stores an image on secondary storage and the bootloader
 * loads it.
 */
#ifndef IMG_H
#define IMG_H

#include "sd_blksz.h"

#define IMG_MAGIC 0xF00BA12

enum item_id {
	ITEM_ID_END,  /**< First so that it has value 0. */
	ITEM_ID_KERNEL,
	ITEM_ID_DEVICE_TREE_BLOB
};

/**
 * @struct item
 * @brief Storage for arbitrary data.
 *
 * @var item::id 
 * Enum item_id identifier for what is stored in the data field.
 *
 * @var item::datasz 
 * Size of the data stored in the data field, in bytes, including SD block size 
 * alignment padding bytes.
 *
 * @var item::data
 * The data field shall be padded to align the size of this to SD_BLKSZ so that
 * the item after it in an image is at the start of a block (assuming this item 
 * is at the start of a block also). This does not have an aligned attribute 
 * so that sizeof() picks up the size of the struct members excluding the data 
 * member, instead of picking up SD_BLKSZ.
 */
struct item {
	uint32_t id;
	uint32_t datasz;
	uint8_t data[];
};

/**
 * @struct image
 * A packed/serialised representation of the OS files and data required
 * to boot it. This is stored at the very start of a primary partition
 * on a block device formatted with a MBR partition table.
 *
 * @var image::magic 
 * Used to verify the start of the image after having loaded it from secondary 
 * storage. This will have value IMG_MAGIC.
 *
 * @var image::imgsz 
 * Size of the entire image in bytes, including the last zero item, etc.
 *
 * @var image::items
 * The separate OS files/data stored in the image. This is terminated by an item 
 * with ID ITEM_ID_END (its data size shall be 0). All items start at an offset 
 * that is a multiple of SD_BLKSZ, meaning each appears at the start of a block/LBA, 
 * and is able to be addressed by a SD read operation.
 */
struct image {
	uint32_t magic;
	uint32_t imgsz;
	/* Pad the image so the first item is at the start of the next block. */
	uint8_t padding[SD_BLKSZ-2*sizeof(uint32_t)];
	struct item items[];
} __attribute__((aligned(SD_BLKSZ)));

#endif

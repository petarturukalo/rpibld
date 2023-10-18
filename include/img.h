/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Imager definitions shared between the bootloader and imager.
 * The imager stores an image on secondary storage and the bootloader
 * loads it.
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef IMG_H
#define IMG_H

#define IMG_MAGIC 0xF00BA12
#define SD_BLKSZ 512

enum item_id {
	ITEM_ID_END,  /* First so that it has value 0. */
	ITEM_ID_KERNEL,
	ITEM_ID_DEVICE_TREE_BLOB
};

/*
 * Storage for arbitrary data.
 *
 * @id: enum item_id identifier for what is stored in the data field
 * @datasz: size of the data stored in the data field, in bytes, including
 *	SD block size alignment padding bytes
 *
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

/*
 * A packed/serialised representation of the OS files and data required
 * to boot it. This is stored at the very start of a primary partition
 * on a block device formatted with a MBR partition table.
 *
 * @magic: used to verify the start of the image after having loaded it from
 *	secondary storage. This will have value IMG_MAGIC.
 * @imgsz: size of the entire image in bytes, including the last zero item, etc.
 * @items: the separate OS files/data stored in the image. This is terminated
 *	by an item with ID ITEM_ID_END (its data size shall be 0).
 *	All items start at an offset that is a multiple of SD_BLKSZ, meaning each
 *	appears at the start of a block/LBA, and is able to be addressed by a
 *	SD read operation.
 */
struct image {
	uint32_t magic;
	uint32_t imgsz;
	/* Pad the image so the first item is at the start of the next block. */
	uint8_t padding[SD_BLKSZ-2*sizeof(uint32_t)];
	struct item items[];
} __attribute__((aligned(SD_BLKSZ)));

#endif

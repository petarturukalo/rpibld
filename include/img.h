/*
 * Imager definitions shared between the bootloader and imager.
 * The imager stores an image on secondary storage and the bootloader
 * loads it.
 */
#ifndef IMG_H
#define IMG_H

#define IMG_MAGIC 0xba1fe12d

enum item_id {
	ITEM_ID_END,
	ITEM_ID_KERNEL,
	ITEM_ID_DEVICE_TREE_BLOB
};

/*
 * Storage for arbitrary data.
 *
 * @id: enum item_id identifier for what is stored in the data field
 * @itemsz: size of the data stored in the data field, in bytes
 */
struct item {
	uint32_t id;
	uint32_t itemsz;
	uint8_t data[];
};

/*
 * A packed/serialised representation of the OS files and data required
 * to boot it. This is stored at the very start of a primary partition
 * on a block device formatted with a MBR partition table.
 *
 * @magic: used to verify the start of the image after having loaded it from
 *	secondary storage. This will have value IMG_MAGIC.
 * @imgsz: size of the entire image in bytes, including the last zero item
 * @items: the separate OS files/data stored in the image. This is terminated
 *	by an item with ID ITEM_ID_END (its item size shall be 0).
 */
struct image {
	uint32_t magic;
	uint32_t imgsz;
	struct item items[];
};

#endif

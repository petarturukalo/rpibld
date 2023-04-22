#include "int.h"
#include "gic.h"
#include "ic.h"
#include "error.h"
#include "mbr.h"
#include "img.h"
#include "sd/sd.h" // TODO rm unless keep like this?
#include "sd/cmd.h" // TODO rm unless keep like this?
#include "led.h" // TODO rm
#include "debug.h" // TODO rm
#include "timer.h" // TODO rm
#include "heap.h" // TODO rm
#include "help.h" // TODO rm

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enum sd_init_error error;
	byte_t *mbr_base_addr;
	uint32_t part_lba;
	uint32_t part_nblks;
	struct image *img;
	struct item *item;
	uint32_t id;// TODO rm after

	enable_interrupts();
	/*gic_init();*/
	ic_enable_interrupts();

	error = sd_init();
	if (error != SD_INIT_ERROR_NONE) 
		signal_error(ERROR_SD_INIT);
	mbr_base_addr = heap_get_base_address();
	/* Read MBR from first block on SD card into RAM. */
	if (!sd_read_blocks(mbr_base_addr, (void *)0, 1))
		signal_error(ERROR_SD_READ);
	if (!mbr_magic(mbr_base_addr))
		signal_error(ERROR_NO_MBR_MAGIC);
	if (!mbr_partition_valid(IMAGE_PARTITION))
		signal_error(ERROR_INVALID_PARTITION);

	part_lba = mbr_get_partition_lba(mbr_base_addr, IMAGE_PARTITION);
	part_nblks = mbr_get_partition_nblks(mbr_base_addr, IMAGE_PARTITION);

	/* Got all the data needed from MBR so safe to overwrite it in heap. */
	img = heap_get_base_address();
	/* Read first block of image from image partition. */
	if (!sd_read_blocks((byte_t *)img, (void *)part_lba, 1))
		/* TODO if always do a signal_error(ERROR_SD_READ) then put it in sd_read() instead? */
		signal_error(ERROR_SD_READ);
	if (img->magic != IMG_MAGIC)
		signal_error(ERROR_NO_IMAGE_MAGIC);
	if (img->imgsz > part_nblks*READ_BLKSZ)
		signal_error(ERROR_IMAGE_OVERFLOW);

	/* Read whole of image from image partition. */
	if (!sd_read_bytes((byte_t *)img, (void *)part_lba, img->imgsz))
		signal_error(ERROR_SD_READ);
	/* Validate image contents. TODO mv this later */
	item = img->items;
	id = ITEM_ID_KERNEL;
	/* TODO using mcmp() to avoid memory alignment issues. */
	if (!mcmp(&item->id, &id, sizeof(uint32_t)))
		signal_error(ERROR_IMAGE_CONTENTS);
	item = (struct item *)((byte_t *)&item->data + item->itemsz);
	id = ITEM_ID_DEVICE_TREE_BLOB;
	if (!mcmp(&item->id, &id, sizeof(uint32_t)))
		signal_error(ERROR_IMAGE_CONTENTS);
	item = (struct item *)((byte_t *)&item->data + item->itemsz);
	id = ITEM_ID_END;
	if (!mcmp(&item->id, &id, sizeof(uint32_t)))
		signal_error(ERROR_IMAGE_CONTENTS);

	signal_error(10);
	__asm("wfi");
}

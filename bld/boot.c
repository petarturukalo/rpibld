#include "int.h"
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

/* Addresses in RAM that the kernel / device tree blob are loaded to. */
/* 32 MiB. */
#define KERN_RAM_ADDR 0x2000000
/* 128 MiB. */
#define DTB_RAM_ADDR 0x8000000
/*
 * The following diagram illustrates how the RAM is sectioned
 * for use by the bootloader. Sections are delimited by borders:
 * a hyphenated border (----) marks a hard border where a section
 * stops, and a dotted border (....) marks a soft border where a
 * section can grow into (i.e. it has a variable or unknown size).
 *
 *         +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |    heap    |
 * 256 MiB +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |    dtb     |
 * 128 MiB +------------+
 *         |            |
 *         |            |
 *         |............|
 *         |   kernel   |
 *  32 MiB +------------+
 *         |            |
 *  31 MiB +------------+
 *         | irq stack  |
 *  30 MiB +------------+
 *         | svc stack  |
 *         |............|
 *         |            |
 *         |............|
 *         | bootloader |
 *  32 KiB +------------+
 *         |            |
 *         +------------+
 */

/*
 * Load an image item from the SD card into RAM.
 */
static struct item *load_item(enum item_id id, byte_t *ram_item_dest_addr, 
			      void *sd_item_src_lba)
{
	struct item *item;

	/* Read first block of item to get its size. */
	if (!sd_read_blocks(ram_item_dest_addr, sd_item_src_lba, 1))
		signal_error(ERROR_SD_READ);
	item = (struct item *)ram_item_dest_addr;
	if (item->id != id) 
		signal_error(ERROR_IMAGE_CONTENTS);
	/* Read rest of item. */
	if (item->itemsz > SD_BLKSZ) {
		if (!sd_read_bytes(ram_item_dest_addr, sd_item_src_lba, item->itemsz))
			signal_error(ERROR_SD_READ);
	}
	return item;
}

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	// TODO break down the fn into parts?
	enum sd_init_error error;
	byte_t *mbr_base_addr;
	uint32_t img_part_lba, item_lba;
	uint32_t img_part_nblks;
	struct image *img;
	struct item *item;

	enable_interrupts();
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

	img_part_lba = mbr_get_partition_lba(mbr_base_addr, IMAGE_PARTITION);
	img_part_nblks = mbr_get_partition_nblks(mbr_base_addr, IMAGE_PARTITION);

	/* Got all the data needed from MBR so safe to overwrite it in heap. */
	img = heap_get_base_address();
	/* Read first block of image from image partition into RAM. */
	if (!sd_read_blocks((byte_t *)img, (void *)img_part_lba, 1))
		signal_error(ERROR_SD_READ);
	if (img->magic != IMG_MAGIC)
		signal_error(ERROR_NO_IMAGE_MAGIC);
	if (img->imgsz > img_part_nblks*SD_BLKSZ)
		signal_error(ERROR_IMAGE_OVERFLOW);

	/* 
	 * Load kernel and device tree blob from SD image into RAM. 
	 *
	 * Below the size of the item not including its data is subtracted from the RAM 
	 * address so that the start of the item's data is loaded to the RAM address.
	 */
	item_lba = img_part_lba+1;
	item = load_item(ITEM_ID_KERNEL, (byte_t *)(KERN_RAM_ADDR-sizeof(struct item)), 
			 (void *)item_lba);
	if (KERN_RAM_ADDR+item->itemsz >= DTB_RAM_ADDR)
		signal_error(ERROR_KERN_OVERFLOW);
	item_lba += bytes_to_blocks(item->itemsz);
	item = load_item(ITEM_ID_DEVICE_TREE_BLOB, (byte_t *)(DTB_RAM_ADDR-sizeof(struct item)), 
			 (void *)item_lba);
	item_lba += bytes_to_blocks(item->itemsz);
	/* Validate that the terminating item is there. */
	item = load_item(ITEM_ID_END, heap_get_base_address(), (void *)item_lba);



	signal_error(11);
	__asm__("wfi");
}

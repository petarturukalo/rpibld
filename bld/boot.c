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
#include "tag.h" // TODO rm
#include "uart.h"

/* Addresses in RAM that the kernel / device tree blob are loaded to. */
/* 32 MiB. */
#define KERN_RAM_ADDR 0x2000000
/* 128 MiB. */
#define DTB_RAM_ADDR  0x8000000
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

#define DTB_MAGIC 0xd00dfeed
/* Kernel zImage magic number and offset to it from start of the zImage. */
#define ZIMAGE_MAGIC 0x016F2818
#define ZIMAGE_MAGIC_OFF 0x24

/*
 * Load MBR from first block on SD card into RAM.
 * Return the start address of the MBR in RAM on success.
 */
static byte_t *load_mbr(void)
{
	byte_t *mbr_base_addr = heap_get_base_address();

	serial_log("Loading MBR...");

	if (!sd_read_blocks(mbr_base_addr, (void *)0, 1))
		signal_error(ERROR_SD_READ);
	if (!mbr_magic(mbr_base_addr)) {
		serial_log("Error: couldn't find MBR on SD card: no MBR magic");
		signal_error(ERROR_NO_MBR_MAGIC);
	}
	if (!mbr_partition_valid(IMAGE_PARTITION)) {
		serial_log("Error: partition %u not a valid primary partition", 
			   IMAGE_PARTITION);
		signal_error(ERROR_INVALID_PARTITION);
	}
	serial_log("Successfully loaded and validated MBR");
	return mbr_base_addr;
}

/*
 * Load the start of the image from the image partition into RAM.
 * Return the logical block address (LBA) of the image partition on success.
 */
static uint32_t load_image_head(byte_t *mbr_base_addr)
{
	uint32_t img_part_lba = mbr_get_partition_lba(mbr_base_addr, IMAGE_PARTITION);
	uint32_t img_part_nblks = mbr_get_partition_nblks(mbr_base_addr, IMAGE_PARTITION);
	/* Got all the data needed from MBR so safe to overwrite it in heap. */
	struct image *img = heap_get_base_address();

	serial_log("Loading image head from partition %u", IMAGE_PARTITION);

	/* Read first block of image from image partition into RAM. */
	if (!sd_read_blocks((byte_t *)img, (void *)img_part_lba, 1))
		signal_error(ERROR_SD_READ);
	if (img->magic != IMG_MAGIC) {
		serial_log("Error: couldn't find image at start of partition %u: "
			   "no image magic", IMAGE_PARTITION);
		signal_error(ERROR_NO_IMAGE_MAGIC);
	}
	if (img->imgsz > img_part_nblks*SD_BLKSZ) {
		serial_log("Error: image overflow: image size %u bytes is greater than size "
			   "of image partition %u bytes", img->imgsz, img_part_nblks*SD_BLKSZ);
		signal_error(ERROR_IMAGE_OVERFLOW);
	}
	serial_log("Successfully loaded and validated image head, "
		   "image size %u bytes", img->imgsz);
	return img_part_lba;
}

/*
 * Get the string name of an item which an item ID identifies.
 */
static char *stritem(enum item_id id)
{
	switch (id) {
		case ITEM_ID_END:
			return "end";
		case ITEM_ID_KERNEL:
			return "kernel";
		case ITEM_ID_DEVICE_TREE_BLOB:
			return "device tree blob";
	}
}

/*
 * Load an image item from the SD card into RAM.
 */
static struct item *load_item(enum item_id id, byte_t *ram_item_dest_addr, 
			      void *sd_item_src_lba)
{
	struct item *item;

	serial_log("Loading %s item to RAM address %08x...", stritem(id), ram_item_dest_addr);

	/* Read first block of item to get its size. */
	if (!sd_read_blocks(ram_item_dest_addr, sd_item_src_lba, 1))
		signal_error(ERROR_SD_READ);
	item = (struct item *)ram_item_dest_addr;
	if (item->id != id) {
		serial_log("Error: loaded image item %s but expected %s",
			   stritem(item->id), stritem(id));
		signal_error(ERROR_IMAGE_CONTENTS);
	}
	/* Read rest of item. */
	if (item->itemsz > SD_BLKSZ) {
		if (!sd_read_bytes(ram_item_dest_addr, sd_item_src_lba, item->itemsz))
			signal_error(ERROR_SD_READ);
	}
	serial_log("Successfully loaded %s item, data size %u bytes", stritem(id), item->itemsz);
	return item;
}

/* 
 * Load the kernel and device tree blob from the SD image into RAM. 
 */
static void load_image_items(uint32_t img_part_lba)
{
	/*
	 * Below the size of the item not including its data is subtracted from the RAM 
	 * address so that the start of the item's data is loaded to the RAM address.
	 */
	uint32_t item_lba = img_part_lba+1;
	struct item *item = load_item(ITEM_ID_KERNEL, (byte_t *)(KERN_RAM_ADDR-sizeof(struct item)), 
				      (void *)item_lba);
	if (KERN_RAM_ADDR+item->itemsz >= DTB_RAM_ADDR) {
		serial_log("Error: kernel size %u bytes overflows into device tree blob",
			   item->itemsz);
		signal_error(ERROR_KERN_OVERFLOW);
	}
	if (*(uint32_t *)(KERN_RAM_ADDR+ZIMAGE_MAGIC_OFF) != ZIMAGE_MAGIC) {
		serial_log("Error: couldn't find kernel zImage magic");
		signal_error(ERROR_IMAGE_CONTENTS);
	}
	serial_log("Successfully validated kernel");

	item_lba += bytes_to_blocks(item->itemsz);
	item = load_item(ITEM_ID_DEVICE_TREE_BLOB, (byte_t *)(DTB_RAM_ADDR-sizeof(struct item)), 
			 (void *)item_lba);
	if (bswap32(*(uint32_t *)DTB_RAM_ADDR) != DTB_MAGIC) {
		serial_log("Error: couldn't find device tree blob magic");
		signal_error(ERROR_IMAGE_CONTENTS);
	}
	serial_log("Successfully validated device tree blob");

	/* Validate that the terminating item is there. */
	item_lba += bytes_to_blocks(item->itemsz);
	item = load_item(ITEM_ID_END, heap_get_base_address(), (void *)item_lba);
}

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	enum sd_init_error error;
	byte_t *mbr_base_addr;
	uint32_t img_part_lba;

	enable_interrupts();
	ic_enable_interrupts();
	uart_init();
	serial_log("Bootloader started: enabled interrupts and mini UART");

	error = sd_init();
	if (error != SD_INIT_ERROR_NONE) {
		serial_log("Failed to initialise SD");
		signal_error(ERROR_SD_INIT);
	}
	mbr_base_addr = load_mbr();
	img_part_lba = load_image_head(mbr_base_addr);
	load_image_items(img_part_lba);

	/* Reset CPU and peripherals to the state required to boot the kernel. */
	// TODO test which of below is actually needed
	if (!sd_reset()) {
		serial_log("Error: failed to reset SD");
		signal_error(ERROR_SD_RESET);
	}
	serial_log("Disabling interrupts and jumping to kernel...");
	ic_disable_interrupts();
	disable_interrupts();
	// TODO move back to hypervisor?

	/* 
	 * Load addresses into non-scratch registers r4, r5 before using 
	 * scratch registers r1, r2, r3 because compilation overwrites 
	 * the scratch registers when loading the addresses, e.g. instruction
	 *
	 *	mov r4, %0
	 *
	 * becomes
	 *
	 *	mov r3, <immediate>
	 *	mov r4, r3
	 *
	 * after compilation, where <immediate> is the value substituted into %0.
	 */
	__asm__("mov r4, %0\n\t"  /* Store device tree blob in r4. */
		"mov r5, %1\n\t"  /* Store kernel in r5. */
		/* Set the values of registers r0, r1, r2 required to boot the kernel. */
		"mov r0, #0\n\t"
		/* 
		 * r1 is machine type and is set to ~0 (all 1s) to not match a 
		 * machine because it's determined by the device tree instead.
		 */
		// TODO 3138 or ~0? 3138 is what the GPU firmware put in r1
		/*"mvn r1, #0\n\t"*/
		/* TODO this comes from https://www.arm.linux.org.uk/developer/machines/ */
		"mov r1, #3138\n\t"
		/* Set r2 to address of device tree blob. */
		"mov r2, r4\n\t"
		/* Jump to kernel. */
		"bx r5"
		// TODO can't get the hyp exception vector to execute */
		/*__asm__("hvc #0\n\t" */
		:
		: "r" (DTB_RAM_ADDR), "r" (KERN_RAM_ADDR));

	// NOTE disabled interrupts so signal_error() won't work
	// TODO rm
	signal_error(12);
	__asm__("wfi");
}

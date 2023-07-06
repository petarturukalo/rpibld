#include "ic.h"
#include "error.h"
#include "mbr.h"
#include "img.h"
#include "sd/sd.h"
#include "debug.h"
#include "heap.h"
#include "help.h"
#include "uart.h"
#include "addrmap.h"
#include "int.h"
#include "gic.h"

#define DTB_MAGIC 0xd00dfeed
/* Kernel zImage magic number and offset to it from start of the zImage. */
#define ZIMAGE_MAGIC 0x016F2818
#define ZIMAGE_MAGIC_OFF 0x24

/* Assembly labels. */
extern void vector_table(void);
extern void vector_table_pool_end(void);

static void install_vector_table(void)
{
	mcopy(vector_table, (byte_t *)0x00, vector_table_pool_end-vector_table);
}

/* Enable interrupts and initialise peripherals. */
static void init_peripherals(void)
{
	enum sd_init_error error;

	uart_init();
	serial_log("Bootloader started: enabled mini UART");
#if !ENABLE_GIC
	enable_interrupts();
	ic_enable_interrupts();
	serial_log("Enabled interrupts");
#endif

	error = sd_init();
	if (error != SD_INIT_ERROR_NONE) {
		serial_log("Failed to initialise SD");
		signal_error(ERROR_SD_INIT);
	}
}

/* Disable interrupts and reset the peripherals initialised in init_peripherals(). */
static void reset_peripherals(void)
{
	if (!sd_reset()) {
		serial_log("Error: failed to reset SD");
		signal_error(ERROR_SD_RESET);
	}
	/* Note the mini UART isn't reset so the kernel can use it as a serial console. */
#if !ENABLE_GIC
	ic_disable_interrupts();
	disable_interrupts();
	serial_log("Disabled interrupts");
#endif
}

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

static void boot_kernel(void)
{
	serial_log("Jumping to kernel...");
	
	__asm__("mov r3, #" MSTRFY(KERN_RAM_ADDR) "\n\t"
		"hvc #0");  /* Go to hypervisor mode which will execute _boot_kernel. */
}

/*
 * Entry point to the C code, the function branched to when switching from 
 * the assembly init code to C.
 */
void c_entry(void)
{
	byte_t *mbr_base_addr;
	uint32_t img_part_lba;

	install_vector_table();
	init_peripherals();
	mbr_base_addr = load_mbr();
	img_part_lba = load_image_head(mbr_base_addr);
	load_image_items(img_part_lba);
	reset_peripherals();
	boot_kernel();
}

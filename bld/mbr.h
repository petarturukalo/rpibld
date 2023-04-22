/*
 * Master boot record.
 */
#ifndef MBR_H
#define MBR_H

#include "type.h"

// TODO mv back to .c
/* The ARM CPU is little endian, otherwise this would be 0x55aa. */
#define MBR_MAGIC 0xaa55

/* MBR offsets to fields and sizes of fields. */
#define MBR_MAGIC_OFF 0x01fe
/* First primary partition entry of four. */
#define MBR_PART_ENTRIES_OFF 0x01be

/* First and last primary partition numbers. */
#define MBR_FIRST_PART_NR 1
#define MBR_LAST_PART_NR  4

#define PART_ENTRY_SZ 16
/* Partition entry offsets to fields and sizes of fields. */
#define PART_ENTRY_LBA_OFF     0x08
#define PART_ENTRY_NBLOCKS_OFF 0x0c


/*
 * Get whether a MBR has the magic / boot signature
 * at its end.
 */
bool mbr_magic(byte_t *mbr_base_addr);
/*
 * Get whether the partition a valid primary partition, a number in
 * range 1 to 4 (both ends inclusive).
 */
bool mbr_partition_valid(int partition);

/*
 * Get the logical block address (LBA) of a partition.
 */
uint32_t mbr_get_partition_lba(byte_t *mbr_base_addr, int partition);
/*
 * Get the size of a partition in blocks.
 */
uint32_t mbr_get_partition_nblks(byte_t *mbr_base_addr, int partition);

#endif

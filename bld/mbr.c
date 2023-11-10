/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#include "mbr.h"
#include "help.h"

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
/* Partition entry offsets to fields. */
#define PART_ENTRY_LBA_OFF     0x08
#define PART_ENTRY_NBLOCKS_OFF 0x0c

bool mbr_magic(byte_t *mbr_base_addr)
{
	return *(uint16_t *)(mbr_base_addr+MBR_MAGIC_OFF) == MBR_MAGIC;
}

bool mbr_partition_valid(int partition)
{
	return partition >= MBR_FIRST_PART_NR && partition <= MBR_LAST_PART_NR;
}

/*
 * Get the vale of a 32-bit partition entry field.
 */
static uint32_t mbr_get_part_entry_field(byte_t *mbr_base_addr, int partition, int field_offset)
{
	uint32_t val;
	byte_t *part_entry = (mbr_base_addr+MBR_PART_ENTRIES_OFF) + (partition-1)*PART_ENTRY_SZ;
	/*
	 * Use mcopy() to avoid alignment access issues: the partition entry fields are at 2-byte
	 * aligned addresses, and not 4-byte aligned, but the access is 4-bytes, which would cause 
	 * the program to hang (presumably by an exception). Function mcopy() avoids these issues
	 * by copying a byte at a time (1-byte access). 
	 */
	mcopy(part_entry+field_offset, &val, sizeof(val));
	return val;
}

uint32_t mbr_get_partition_lba(byte_t *mbr_base_addr, int partition)
{
	return mbr_get_part_entry_field(mbr_base_addr, partition, PART_ENTRY_LBA_OFF);
}

uint32_t mbr_get_partition_nblks(byte_t *mbr_base_addr, int partition)
{
	return mbr_get_part_entry_field(mbr_base_addr, partition, PART_ENTRY_NBLOCKS_OFF);
}

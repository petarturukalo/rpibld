/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Master boot record.
 *
 * Copyright (C) 2023 Petar Turukalo
 */
#ifndef MBR_H
#define MBR_H

#include "type.h"

/*
 * Get whether a MBR has the magic / boot signature
 * at its end.
 */
bool mbr_magic(byte_t *mbr_base_addr);
/*
 * Get whether the partition is a valid primary partition, a number in
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

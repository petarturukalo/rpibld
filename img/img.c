/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include "img.h"

#define RDWR_SZ 4096

static void print_usage(void)
{
	printf("Usage: 'imager <part> <kern> <dtb>' where <part> is the block\n"
	       "device partition for a MBR primary partition, e.g. /dev/sdc2, and\n"
	       "is where the remaining arguments will be stored. <kern> is the\n"
	       "(compressed) 32-bit Linux kernel ARM zImage to boot, and <dtb> is\n"
	       "the device tree blob that will be passed to the kernel.\n"
	       "\n"
	       "Use arg -h or --help to print this message again.\n");
}

/*
 * Get the size of a file in bytes. Return -1 on error.
 */
static int filesz(int fd)
{
	struct stat stat;

	if (fstat(fd, &stat) == -1)
		return -1;
	return stat.st_size;
}

/*
 * Free a pointer and set it to NULL if it's not NULL.
 */
static void freep(void **ptr)
{
	if (*ptr) {
		free(*ptr);
		*ptr = NULL;
	}
}
/* So caller doesn't have to explicitly cast. */
#define freep(p) freep((void **)p)

/*
 * Read the contents of a file into a dynamically allocated string.
 * Return NULL on error.
 *
 * @fsz_out: out-param size of the file in bytes
 */
static char *file_read(char *fpath, int *fsz_out)
{
	int fd;
	char *mem = NULL; 
	char *cur_mem;
	int n;

	fd = open(fpath, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Error opening file %s for reading: %s\n", fpath, strerror(errno));
		goto file_read_cleanup0;
	}
	*fsz_out = filesz(fd);
	if (*fsz_out == -1) {
		fprintf(stderr, "Error getting size of file %s: %s\n", fpath, strerror(errno));
		goto file_read_cleanup1;
	}
	mem = malloc(*fsz_out);
	if (!mem) {
		fprintf(stderr, "Error allocating memory: %s\n", strerror(errno));
		goto file_read_cleanup1;
	}
	/* Read whole of file into memory buffer. */
	cur_mem = mem;
	while ((n = read(fd, cur_mem, RDWR_SZ)) > 0) 
		cur_mem += n;
	if (n == -1) {
		fprintf(stderr, "Error reading from file %s: %s\n", fpath, strerror(errno));
		freep(&mem);
	}

file_read_cleanup1:
	close(fd);
file_read_cleanup0:
	return mem;
}

/*
 * Round a number n up to the nearest multiple of m.
 * If n is already a multiple of m it is not rounded up.
 */
static int round_up_multiple(int n, int m)
{
	return n%m ? n+(m-(n%m)) : n;
}

/*
 * Append an item to the image.
 *
 * @img: dynamically allocated address of image
 *
 * Uses realloc() to get new space for the item, so the return is the (potentially) 
 * new start address of the image. Return NULL on error.
 */
static struct image *image_append_item(struct image *img, enum item_id id, int datasz, void *data)
{
	struct item *item;
	/* Pad the item to ensure it's aligned to SD_BLKSZ. */
	int datasz_after_pad = round_up_multiple(sizeof(struct item)+datasz, SD_BLKSZ)
			       - sizeof(struct item);

	img = realloc(img, img->imgsz+(sizeof(struct item)+datasz_after_pad));
	if (!img) {
		fprintf(stderr, "Error allocating memory: %s\n", strerror(errno));
		return NULL;
	}
	/* Create a new item at the current end of the image. */
	item = (struct item *)((char *)img+img->imgsz);
	item->id = id;
	item->datasz = datasz_after_pad;
	if (datasz) 
		memcpy(&item->data, data, datasz);

	/* Update end of image to include new item. */
	img->imgsz += sizeof(struct item)+datasz_after_pad;

	return img;
}

/*
 * Append the contents of a file as an item to the image.
 *
 * See image_append_item() for @img and return.
 */
static struct image *image_append_file(struct image *img, char *fpath, enum item_id id)
{
	char *file_contents;
	int fsz;

	file_contents = file_read(fpath, &fsz);
	if (!file_contents) 
		return NULL;
	img = image_append_item(img, id, fsz, file_contents);
	freep(&file_contents);
	return img;
}

/*
 * Write a memory area to a file.
 *
 * @mem: memory area
 * @n: size of the memory area in bytes
 *
 * Return whether successful.
 */
static bool file_write(char *fpath, char *mem, int n)
{
	int fd;
	int bytes_written;

	fd = open(fpath, O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "Error opening file %s for writing: %s\n",
			fpath, strerror(errno));
		return false;
	}
	while (n > 0) {
		bytes_written = write(fd, mem, n < RDWR_SZ ? n : RDWR_SZ);
		if (bytes_written == -1) {
			fprintf(stderr, "Error writing to file %s: %s\n", 
				fpath, strerror(errno));
			close(fd);
			return false;
		}
		mem += bytes_written;
		n -= bytes_written;
	}
	close(fd);
	return true;
}

/*
 * Same as image_append_file() but free the input image if the append fails
 * (the realloc() failing specifically but something else might have failed instead).
 */
static struct image *image_append_file_free_on_fail(struct image *img, char *fpath, enum item_id id)
{
	struct image *new_img; 
	
	new_img = image_append_file(img, fpath, id);
	if (!new_img)  {
		freep(&img);
		return NULL;
	} 
	return new_img;
}

/*
 * Build an image out of a kernel and device tree blob files.
 */
static struct image *build_image(char *kern_fpath, char *dtb_fpath)
{
	struct image *img, *new_img;

	img = malloc(sizeof(struct image));
	img->magic = IMG_MAGIC;
	/* Set to its current size. As it grows this will be increased. */
	img->imgsz = sizeof(struct image);

	img = image_append_file_free_on_fail(img, kern_fpath, ITEM_ID_KERNEL);
	if (!img)  
		return NULL;
	img = image_append_file_free_on_fail(img, dtb_fpath, ITEM_ID_DEVICE_TREE_BLOB);
	if (!img) 
		return NULL;
	/* Append terminating item. */
	new_img = image_append_item(img, ITEM_ID_END, 0, NULL);
	if (!new_img) {
		freep(&img);
		return NULL;
	}
	return new_img;
}

/*
 * Get whether any command line argument is a help argument -h or --help.
 */
static bool any_arg_is_help(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			return true;
	}
	return false;
}

/*
 * Write an image containing a kernel and device tree files (in that order)
 * to the start of a block device partition for a MBR primary partition.
 */
int main(int argc, char *argv[])
{
	char *part, *kern_fpath, *dtb_fpath;
	struct image *img;
	int ret;

	if (any_arg_is_help(argc, argv)) {
		print_usage();
		exit(EXIT_SUCCESS);
	}
	if (argc != 4) {
		print_usage();
		exit(EXIT_FAILURE);
	}
	part = argv[1];
	kern_fpath = argv[2];
	dtb_fpath = argv[3];

	img = build_image(kern_fpath, dtb_fpath);
	if (!img)
		exit(EXIT_FAILURE);

	if (file_write(part, (void *)img, img->imgsz)) {
		printf("Wrote image of size %d bytes to partition %s\n", 
		       img->imgsz, part);
		ret = EXIT_SUCCESS;
	} else
		ret = EXIT_FAILURE;
	freep(&img);
	return ret;
}

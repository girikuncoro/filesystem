/* Author: Robbert van Renesse, August 2015
 *
 * This code implements a block store stored in memory
 * file system:
 *
 *		block_if ramdisk_init(block_t *blocks, block_no nblocks)
 *			Create a new block store, stored in the array of blocks
 *			pointed to by 'blocks', which has nblocks blocks in it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

struct ramdisk_state {
	block_t *blocks;
	block_no nblocks;
	int fd;
};

static int ramdisk_nblocks(block_if bi){
	struct ramdisk_state *rs = bi->state;

	return rs->nblocks;
}

static int ramdisk_setsize(block_if bi, block_no nblocks){
	struct ramdisk_state *rs = bi->state;

	int before = rs->nblocks;
	rs->nblocks = nblocks;
	return before;
}

static int ramdisk_read(block_if bi, block_no offset, block_t *block){
	struct ramdisk_state *rs = bi->state;

	if (offset >= rs->nblocks) {
		fprintf(stderr, "ramdisk_read: bad offset %u\n", offset);
		return -1;
	}
	memcpy(block, &rs->blocks[offset], BLOCK_SIZE);
	return 0;
}

static int ramdisk_write(block_if bi, block_no offset, block_t *block){
	struct ramdisk_state *rs = bi->state;

	if (offset >= rs->nblocks) {
		fprintf(stderr, "ramdisk_write: bad offset\n");
		return -1;
	}
	memcpy(&rs->blocks[offset], block, BLOCK_SIZE);
	return 0;
}

static void ramdisk_destroy(block_if bi){
	free(bi->state);
	free(bi);
}

block_if ramdisk_init(block_t *blocks, block_no nblocks){
	struct ramdisk_state *rs = calloc(1, sizeof(*rs));

	rs->blocks = blocks;
	rs->nblocks = nblocks;

	block_if bi = calloc(1, sizeof(*bi));
	bi->state = rs;
	bi->nblocks = ramdisk_nblocks;
	bi->setsize = ramdisk_setsize;
	bi->read = ramdisk_read;
	bi->write = ramdisk_write;
	bi->destroy = ramdisk_destroy;
	return bi;
}

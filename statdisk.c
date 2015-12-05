/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but keeps track of statistics.
 *
 *		block_if statdisk_init(block_if below){
 *			'below' is the underlying block store.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

struct statdisk_state {
	block_if below;			// block store below
	unsigned int nnblocks;	// #nblocks operations
	unsigned int nsetsize;	// #nblocks operations
	unsigned int nread;		// #read operations
	unsigned int nwrite;	// #write operations
};

static int statdisk_nblocks(block_if bi){
	struct statdisk_state *sds = bi->state;

	sds->nnblocks++;
	return (*sds->below->nblocks)(sds->below);
}

static int statdisk_setsize(block_if bi, block_no nblocks){
	struct statdisk_state *sds = bi->state;

	sds->nsetsize++;
	return (*sds->below->setsize)(sds->below, nblocks);
}

static int statdisk_read(block_if bi, block_no offset, block_t *block){
	struct statdisk_state *sds = bi->state;

	sds->nread++;
	return (*sds->below->read)(sds->below, offset, block);
}

static int statdisk_write(block_if bi, block_no offset, block_t *block){
	struct statdisk_state *sds = bi->state;

	sds->nwrite++;
	return (*sds->below->write)(sds->below, offset, block);
}

static void statdisk_destroy(block_if bi){
	free(bi->state);
	free(bi);
}

void statdisk_dump_stats(block_if bi){
	struct statdisk_state *sds = bi->state;

	printf("!$STAT: #nnblocks:  %u\n", sds->nnblocks);
	printf("!$STAT: #nsetsize:  %u\n", sds->nsetsize);
	printf("!$STAT: #nread:     %u\n", sds->nread);
	printf("!$STAT: #nwrite:    %u\n", sds->nwrite);
}

block_if statdisk_init(block_if below){
	/* Create the block store state structure.
	 */
	struct statdisk_state *sds = calloc(1, sizeof(*sds));
	sds->below = below;

	/* Return a block interface to this inode.
	 */
	block_if bi = calloc(1, sizeof(*bi));
	bi->state = sds;
	bi->nblocks = statdisk_nblocks;
	bi->setsize = statdisk_setsize;
	bi->read = statdisk_read;
	bi->write = statdisk_write;
	bi->destroy = statdisk_destroy;
	return bi;
}

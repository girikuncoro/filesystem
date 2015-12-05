/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but checks if reads correspond to prior
 * writes.
 *
 *		block_if checkdisk_init(block_if below);
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

struct block_list {
	struct block_list *next;
	block_no offset;
	block_t block;
};

struct checkdisk_state {
	block_if below;			// block store below
	char *descr;			// to disambiguate multiple instances
	struct block_list *bl;	// info about data written
};

static int checkdisk_nblocks(block_if bi){
	struct checkdisk_state *cs = bi->state;

	return (*cs->below->nblocks)(cs->below);
}

static int checkdisk_setsize(block_if bi, block_no nblocks){
	struct checkdisk_state *cs = bi->state;

	/* See if I read or wrote any blocks beyond this boundary.  Remove.
	 */
	struct block_list **pbl, *bl;
	for (pbl = &cs->bl; (bl = *pbl) != 0;) {
		if (bl->offset >= nblocks) {
			*pbl = bl->next;
			free(bl);
		}
		else {
			pbl = &bl->next;
		}
	}

	return (*cs->below->setsize)(cs->below, nblocks);
}

static int checkdisk_read(block_if bi, block_no offset, block_t *block){
	struct checkdisk_state *cs = bi->state;

	if ((*cs->below->read)(cs->below, offset, block) < 0) {
		return -1;
	}

	/* See if I read or wrote the block before.
	 */
	struct block_list *bl;
	for (bl = cs->bl; bl != 0; bl = bl->next) {
		if (bl->offset == offset) {
			/* See if it's the same.
			 */
			if (memcmp(&bl->block, block, BLOCK_SIZE) != 0) {
				fprintf(stderr, "!!CHKDISK %s: checkdisk_read: corrupted\n", cs->descr);
				exit(1);
			}
			return 0;
		}
	}

	/* Add to the block list.
	 */
	bl = calloc(1, sizeof(*bl));
	bl->offset = offset;
	bl->block = *block;
	bl->next = cs->bl;
	cs->bl = bl;
	return 0;
}

static int checkdisk_write(block_if bi, block_no offset, block_t *block){
	struct checkdisk_state *cs = bi->state;

	int result = (*cs->below->write)(cs->below, offset, block);
	if (result < 0) {
		return result;
	}

	/* See if I read or wrote the block before.
	 */
	struct block_list *bl;
	for (bl = cs->bl; bl != 0; bl = bl->next) {
		if (bl->offset == offset) {
			break;
		}
	}
	if (bl == 0) {
		bl = calloc(1, sizeof(*bl));
		bl->offset = offset;
		bl->next = cs->bl;
		cs->bl = bl;
	}
	bl->block = *block;
	return result;
}

static void checkdisk_destroy(block_if bi){
	struct checkdisk_state *cs = bi->state;
	struct block_list *bl;

	while ((bl = cs->bl) != 0) {
		cs->bl = bl->next;
		free(bl);
	}
	free(cs);
	free(bi);
}

block_if checkdisk_init(block_if below, char *descr){
	/* Create the block store state structure.
	 */
	struct checkdisk_state *cs = calloc(1, sizeof(*cs));
	cs->below = below;
	cs->descr = descr;

	/* Return a block interface to this inode.
	 */
	block_if bi = calloc(1, sizeof(*bi));
	bi->state = cs;
	bi->nblocks = checkdisk_nblocks;
	bi->setsize = checkdisk_setsize;
	bi->read = checkdisk_read;
	bi->write = checkdisk_write;
	bi->destroy = checkdisk_destroy;
	return bi;
}

/* Author: Robbert van Renesse, August 2015
 *
 * This block store module mirrors the underlying block store but contains
 * a write-through cache.  The caching strategy is CLOCK, approximating LRU.
 * The interface is as follows:
 *
 *		block_if clockdisk_init(block_if below,
 *									block_t *blocks, block_no nblocks)
 *			'below' is the underlying block store.  'blocks' points to
 *			a chunk of memory wth 'nblocks' blocks for caching.
 *
 *		void clockdisk_dump_stats(block_if bi)
 *			Prints the cache statistics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

/* Per block in the cache we keep track of the following info:
 */
struct block_info {
	enum {
		BI_EMPTY,			// cache entry not in use
		BI_UNUSED,			// in use, but not recently used
		BI_USED				// recently used
	} status;
	block_no offset;		// block being cached if not BI_EMPTY
};

/* State contains the pointer to the block module below as well as caching
 * information and caching statistics.
 */
struct clockdisk_state {
	block_if below;				// block store below
	block_t *blocks;			// memory for caching blocks
	block_no nblocks;			// size of cache (not size of block store!)
	struct block_info *binfo;	// info per block
	unsigned int clock_hand;	// rotating hand for clock algorithm

	/* Stats.
	 */
	unsigned int read_hit, read_miss, write_hit, write_miss;
};

/* The given block was just used but it's not in the cache.  Use the clock
 * algorithm to find an entry that hasn't been used recently, evict any
 * block in it, and stick the block in the entry.
 */
static void cache_update(struct clockdisk_state *cs, block_no offset, block_t *block) {
	for (;;) {
		if (cs->binfo[cs->clock_hand].status != BI_USED) {
			break;
		}
		cs->binfo[cs->clock_hand].status = BI_UNUSED;
		if (++cs->clock_hand == cs->nblocks) {
			cs->clock_hand = 0;
		}
	}
	cs->binfo[cs->clock_hand].status = BI_USED;
	cs->binfo[cs->clock_hand].offset = offset;
	memcpy(&cs->blocks[cs->clock_hand], block, BLOCK_SIZE);
}

static int clockdisk_nblocks(block_if bi){
	struct clockdisk_state *cs = bi->state;

	return (*cs->below->nblocks)(cs->below);
}

static int clockdisk_setsize(block_if bi, block_no nblocks){
	struct clockdisk_state *cs = bi->state;
	int i;

	for (i = 0; i < cs->nblocks; i++) {
		if (cs->binfo[i].status != BI_EMPTY && cs->binfo[i].offset >= nblocks) {
			cs->binfo[i].status = BI_EMPTY;
		}
	}
	return (*cs->below->setsize)(cs->below, nblocks);
}

static int clockdisk_read(block_if bi, block_no offset, block_t *block){
	struct clockdisk_state *cs = bi->state;
	unsigned int i;

	/* Check the cache first.
	 */
	for (i = 0; i < cs->nblocks; i++) {
		if (cs->binfo[i].status != BI_EMPTY && cs->binfo[i].offset == offset) {
			memcpy(block, &cs->blocks[i], BLOCK_SIZE);
			cs->binfo[i].status = BI_USED;
			cs->read_hit++;
			return 0;
		}
	}

	int r = (*cs->below->read)(cs->below, offset, block);
	if (r >= 0) {
		cache_update(cs, offset, block);
		cs->read_miss++;
	}
	return r;
}

static int clockdisk_write(block_if bi, block_no offset, block_t *block){
	struct clockdisk_state *cs = bi->state;
	unsigned int i;

	/* Check the cache first.  Even if it's in the cache, write to the
	 * block store below because this implements a write-through cache.
	 */
	for (i = 0; i < cs->nblocks; i++) {
		if (cs->binfo[i].status != BI_EMPTY && cs->binfo[i].offset == offset) {
			memcpy(&cs->blocks[i], block, BLOCK_SIZE);
			cs->binfo[i].status = BI_USED;
			cs->write_hit++;
			return (*cs->below->write)(cs->below, offset, block);
		}
	}

	cache_update(cs, offset, block);
	cs->write_miss++;
	return (*cs->below->write)(cs->below, offset, block);
}

static void clockdisk_destroy(block_if bi){
	struct clockdisk_state *cs = bi->state;

	free(cs->binfo);
	free(cs);
	free(bi);
}

void clockdisk_dump_stats(block_if bi){
	struct clockdisk_state *cs = bi->state;

	printf("!$CLOCK: #read hits:    %u\n", cs->read_hit);
	printf("!$CLOCK: #read misses:  %u\n", cs->read_miss);
	printf("!$CLOCK: #write hits:   %u\n", cs->write_hit);
	printf("!$CLOCK: #write misses: %u\n", cs->write_miss);
}

/* Create a new block store module on top of the specified module below.
 * blocks points to a chunk of memory of nblocks blocks that can be used
 * for caching.
 */
block_if clockdisk_init(block_if below, block_t *blocks, block_no nblocks){
	/* Create the block store state structure.
	 */
	struct clockdisk_state *cs = calloc(1, sizeof(*cs));
	cs->below = below;
	cs->blocks = blocks;
	cs->nblocks = nblocks;
	cs->binfo = calloc(nblocks, sizeof(*cs->binfo));

	/* Return a block interface to this inode.
	 */
	block_if bi = calloc(1, sizeof(*bi));
	bi->state = cs;
	bi->nblocks = clockdisk_nblocks;
	bi->setsize = clockdisk_setsize;
	bi->read = clockdisk_read;
	bi->write = clockdisk_write;
	bi->destroy = clockdisk_destroy;
	return bi;
}

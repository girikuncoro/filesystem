/* This block store module mirrors the underlying block store but contains
 * a write-through cache.
 *
 *		block_if cachedisk_init(block_if below,
 *									block_t *blocks, block_no nblocks)
 *			'below' is the underlying block store.  'blocks' points to
 *			a chunk of memory wth 'nblocks' blocks for caching.
 *			NO OTHER MEMORY MAY BE USED FOR STORING DATA.  However,
 *			malloc etc. may be used for meta-data.
 *
 *		void cachedisk_dump_stats(block_if bi)
 *			Prints cache statistics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

/* State contains the pointer to the block module below as well as caching
 * information and caching statistics.
 */
struct cachedisk_state {
	block_if below;				// block store below
	block_t *blocks;			// memory for caching blocks
	block_no nblocks;			// size of cache (not size of block store!)

	/* Stats.
	 */
	unsigned int read_hit, read_miss, write_hit, write_miss;
};

static int cachedisk_nblocks(block_if bi){
	struct cachedisk_state *cs = bi->state;

	return (*cs->below->nblocks)(cs->below);
}

static int cachedisk_setsize(block_if bi, block_no nblocks){
	struct cachedisk_state *cs = bi->state;

	// TODO: remove any blocks from the cache that have offsets
	//		 >= nblocks.

	return (*cs->below->setsize)(cs->below, nblocks);
}

static int cachedisk_read(block_if bi, block_no offset, block_t *block){
	struct cachedisk_state *cs = bi->state;

	// TODO: check the cache first.  Otherwise read from the underlying
	//		 store and, if so desired, place the new block in the cache,
	//		 possibly evicting a block if there is no space.

	cs->read_miss++;
	return (*cs->below->read)(cs->below, offset, block);
}

static int cachedisk_write(block_if bi, block_no offset, block_t *block){
	struct cachedisk_state *cs = bi->state;

	// TODO: this is a write-through cache, so update the layer below.
	//		 However, if the block is in the cache, it should be updated
	//		 as well.

	cs->write_miss++;
	return (*cs->below->write)(cs->below, offset, block);
}

static void cachedisk_destroy(block_if bi){
	struct cachedisk_state *cs = bi->state;

	// TODO: clean up any allocated meta-data.

	free(cs);
	free(bi);
}

void cachedisk_dump_stats(block_if bi){
	struct cachedisk_state *cs = bi->state;

	printf("!$CACHE: #read hits:    %u\n", cs->read_hit);
	printf("!$CACHE: #read misses:  %u\n", cs->read_miss);
	printf("!$CACHE: #write hits:   %u\n", cs->write_hit);
	printf("!$CACHE: #write misses: %u\n", cs->write_miss);
}

/* Create a new block store module on top of the specified module below.
 * blocks points to a chunk of memory of nblocks blocks that can be used
 * for caching.
 */
block_if cachedisk_init(block_if below, block_t *blocks, block_no nblocks){
	/* Create the block store state structure.
	 */
	struct cachedisk_state *cs = calloc(1, sizeof(*cs));
	cs->below = below;
	cs->blocks = blocks;
	cs->nblocks = nblocks;

	/* Return a block interface to this inode.
	 */
	block_if bi = calloc(1, sizeof(*bi));
	bi->state = cs;
	bi->nblocks = cachedisk_nblocks;
	bi->setsize = cachedisk_setsize;
	bi->read = cachedisk_read;
	bi->write = cachedisk_write;
	bi->destroy = cachedisk_destroy;
	return bi;
}

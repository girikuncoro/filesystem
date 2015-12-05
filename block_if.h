/* Author: Robbert van Renesse, August 2015
 *
 * The include file for all block store modules.  Each such module has an
 * 'init' function that returns a 'block_if' type.  The block_if type is
 * a pointer to a structure that contains the following five methods:
 *
 *		int nblocks(block_if)
 *			returns the size of the block store
 *
 *		int setsize(block_if, block_no newsize)
 *			set the size of the block store; returns the old size
 *
 *		int read(block_if, block_no offset, block_t *block)
 *			read the block at offset and return in *block
 *			returns 0
 *
 *		int write(block_if, block_no offset, block_t *block)
 *			write *block to the block at the given offset
 *			returns 0
 *
 *		void destroy(block_if)
 *			clean up the block store interface;	returns 0
 *
 * All these return -1 upon error (typically after printing the
 * reason for the error).
 *
 * A 'block_t' is a block of BLOCK_SIZE bytes.  A block store is an array
 * of blocks.  A 'block_no' holds the index of the block in the block store.
 *
 * A block_if also maintains a void* pointer called 'state' to internal
 * state the block store module needs to keep.
 */

#define BLOCK_SIZE		512			// # bytes in a block

typedef unsigned int block_no;		// index of a block

struct block {
	char bytes[BLOCK_SIZE];
};
typedef struct block block_t;

struct block_if {
	void *state;
	int (*nblocks)(struct block_if *bi);
	int (*read)(struct block_if *bi, block_no offset, block_t *block);
	int (*write)(struct block_if *bi, block_no offset, block_t *block);
	int (*setsize)(struct block_if *bi, block_no size);
	void (*destroy)(struct block_if *bi);
};

typedef struct block_if *block_if;

/* Convenient function for error handling.
 */
void panic(char *s);

/* 'init' functions of various available block store types.
 */
block_if disk_init(char *file_name, block_no nblocks);
block_if ramdisk_init(block_t *blocks, block_no nblocks);
block_if treedisk_init(block_if below, unsigned int inode_no);
block_if debugdisk_init(block_if below, char *descr);
block_if sandboxdisk_init(block_if below);
block_if partdisk_init(block_if below, block_no delta, block_no nblocks);
block_if cachedisk_init(block_if below, block_t *blocks, block_no nblocks);
block_if clockdisk_init(block_if below, block_t *blocks, block_no nblocks);
block_if LRUdisk_init(block_if below, block_t *blocks, block_no nblocks);
block_if statdisk_init(block_if below);
block_if checkdisk_init(block_if below, char *descr);
block_if tracedisk_init(block_if below, char *trace, unsigned int n_inodes);
block_if raid0disk_init(block_if *below, unsigned int nbelow);
block_if raid1disk_init(block_if *below, unsigned int nbelow);

/* Some useful functions on some block store types.
 */
int treedisk_create(block_if below, unsigned int n_inodes);
int treedisk_check(block_if below);
void clockdisk_dump_stats(block_if bi);
void LRUdisk_dump_stats(block_if bi);
void statdisk_dump_stats(block_if bi);
int sandboxdisk_ischild(block_if bi);
void sandboxdisk_rundisk(block_if bi, block_if within);

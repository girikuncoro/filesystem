# Filesystem
CS4410 Operating System last assignment about caching layer in layered file system. The files were originally written by Professor Robert van Renassee with some missing parts that we had to implement in class: size init for blockstore and caching.

# Description
This directory contains an implementation of a layered block store.
A block store is much like a file, except that the unit of access
is a block, not a byte.  Each block store has the following simple
interface:

	#include "block_if.h"
		Defines BLOCK_SIZE, the size of a block.  Also has typedefs
		for:
			block_if: a pointer to a block store interface
			block_t:  a block of size BLOCK_SIZE
			block_no: an offset into a block store

	int nblocks = (*block_store->nblocks)(block_if block_store);
		Returns the size of the block store in #blocks, or -1 if error.

	int (*block_store->read)(block_store, block_no offset, OUT block_t *block);
		Reads the block at the given offset.  Return 0 upon success,
		-1 upon error.

	int (*block_store->write)(block_store, block_no offset, IN block_t *block);
		Writes the block at the given offset.  Some block stores support
		automatically growing.  Returns -1 upon error.

	int (*block_store->setsize)(block_store, block_no size);
		Set the size of the block store to 'size' blocks.  May either
		truncate or grow the underlying block store.  Not all sizes
		may be supported.  Returns the old size, or -1 upon error.

	void (*block_store->destroy)(block_store);
		Clean up the block store.  In case of a low-level storage (an
		actual disk), will typically leave the blocks intact so it can
		be reloaded again.

To create a block store, you need the block stores init function.
The simplest two block stores are the following:

	block_if disk_init(char *file_name, block_no nblocks);
		Implements a block store with 'nblocks' blocks in the POSIX
		file 'file_name'.  The file simply stores the list of blocks.

	block_if ramdisk_init(block_t *blocks, block_no nblocks);
		Implements a block store with 'nblocks' blocks in the provided
		memory, pointed to by 'blocks'.

For example, if you want caching, you can invoke:

	block_if clockdisk_init(block_if below, block_t *blocks, block_no nblocks);

This adds a caching layer to 'below' with 'nblocks' of cache, pointed to by 'blocks',
but they use different algorithms.  So if you run:

	block_if lower = disk_init("file", 100);
	block_t cache[10];
	block_if higher = clockdisk_init(lower, cache, 10);
	
then 'higher' is a block store that stores its blocks in "file" and
caches its blocks in the given (write-through) cache.  One can still
read and write 'lower', by-passing the cache (but particularly writing
would be dangerous---the cache would not reflect the latest content).
If you like, you can dump caching statistics:

	void clockdisk_dump_stats(block_if bi);

There's a disk layer that does nothing but count operations:

	block_if higher = statdisk_init(lower);

You can dump the stats using:

	void statdisk_dump_stats(block_if bi);

A handy debugging tool is:

	block_if debugdisk_init(block_if below, char *descr);
		This prints any invocation to and result from nblocks(), read(),
		and write() along with the string 'descr'.

Another useful layer is the "check disk" that checks if what's read is what
has been written:

	block_if checkdisk_init(block_if below, char *descr);

Sometimes it's useful to partition a block store:

	block_if partdisk_init(block_if below, block_no delta, block_no nblocks);
		Creates a partition of the underlying block store, starting at
		offset 'delta' and having 'nblocks' total.

For example:

	block_if lower = disk_init("file", 100);
	block_if part1 = partdisk_init(lower, 0, 50);
	block_if part2 = partdisk_init(lower, 50, 50);

This takes the block store in "file" and creates two partitions of 50 blocks.
Obviously, it's good practice to make sure the partitions don't overlap.

One can also virtualize the underlying block store, creating multiple
virtual block stores on a single underlying block store.  Currently, there
is one such module available:

	void treedisk_create(block_if below,
							block_no nblocks, unsigned int n_inodes)
		Create a file system on the block store 'below' consisting of
		'nblocks' total.  The number of inodes (virtual block stores)
		is n_inodes.  Each such virtual block store is initially
		empty (0 blocks), but grows dynamically as blocks are written.

	int treedisk_check(block_if below)
		Checks the integrity of a tree virtual block store.  Returns
		0 if the block store is broken, and 1 if it's in good shape.
		Useful for testing.

	block_if treedisk_init(block_if below, unsigned int inode_no)
		Return a block store interface to the virtual block store identified
		by the inode number.

For example:

	block_t cache[10];
	block_if disk = disk_init("file", 100);
	block_if cdisk = clockdisk_init(disk, cache, 10);
	treedisk_create(cdisk, 100, 2);
	block_if vdisk0 = treedisk_init(cdisk, 0);
	block_if vdisk1 = treedisk_init(cdisk, 1);

This creates two virtual block stores vdisk0 and vdisk1 on top of a
single cached block store stored in "file".

A "trace disk" is a top-level block store (does not support layers
on top of it) that generates a load on the underlying layers.
It is supposed to run over a treedisk-virtualized block store.
The load is read from a file that is given as the first argument.
The file contains entries of the following form:

	command:inode-number:block-number

There are 4 supported commands:

	R: read a block of the given inode
	W: write a block of the given inode
	S: set the size of the given inode
	N: check the size of the given inode

To use a tracedisk, run

	block_if tracedisk_init(block_if below, char *trace, unsigned int n_inodes);
		'trace' specifies the name of the trace file
		'n_inodes' specifies the number of inodes in the underlying file

A tracedisk automatically adds treedisk layers as needed for each inode,
and also adds a checkdisk layer for each to check that the content read
is the same as the last content written.

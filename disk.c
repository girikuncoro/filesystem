/* Author: Robbert van Renesse, August 2015
 *
 * This code implements a block store on top of the underlying POSIX
 * file system:
 *
 *		block_if disk_init(char *file_name, block_no nblocks)
 *			Create a new block store, stored in the file by the given
 *			name and with the given number of blocks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "block_if.h"

struct disk_state {
	block_no nblocks;			// #blocks in the block store
	int fd;						// POSIX file descriptor of underlying file
};

static int disk_nblocks(block_if bi){
	struct disk_state *ds = bi->state;

	return ds->nblocks;
}

static int disk_setsize(block_if bi, block_no nblocks){
	struct disk_state *ds = bi->state;

	int before = ds->nblocks;
	ds->nblocks = nblocks;
	ftruncate(ds->fd, (off_t) nblocks * BLOCK_SIZE);
	return before;
}

static struct disk_state *disk_seek(block_if bi, block_no offset){
	struct disk_state *ds = bi->state;

	if (offset >= ds->nblocks) {
		fprintf(stderr, "--> %u %u\n", offset, ds->nblocks);
		panic("disk_seek: offset too large");
	}
	lseek(ds->fd, (off_t) offset * BLOCK_SIZE, SEEK_SET);
	return ds;
}

static int disk_read(block_if bi, block_no offset, block_t *block){
	struct disk_state *ds = disk_seek(bi, offset);

	int n = read(ds->fd, (void *) block, BLOCK_SIZE);
	if (n < 0) {
		perror("disk_read");
		return -1;
	}
	if (n < BLOCK_SIZE) {
		memset((char *) block + n, 0, BLOCK_SIZE - n);
	}
	return 0;
}

static int disk_write(block_if bi, block_no offset, block_t *block){
	struct disk_state *ds = disk_seek(bi, offset);

	int n = write(ds->fd, (void *) block, BLOCK_SIZE);
	if (n < 0) {
		perror("disk_write");
		return -1;
	}
	if (n != BLOCK_SIZE) {
		fprintf(stderr, "disk_write: wrote only %d bytes\n", n);
		return -1;
	}
	return 0;
}

static void disk_destroy(block_if bi){
	struct disk_state *ds = bi->state;

	close(ds->fd);
	free(ds);
	free(bi);
}

block_if disk_init(char *file_name, block_no nblocks){
	struct disk_state *ds = calloc(1, sizeof(*ds));

	ds->fd = open(file_name, O_RDWR | O_CREAT, 0600);
	if (ds->fd < 0) {
		perror(file_name);
		panic("disk_init");
	}
	ds->nblocks = nblocks;

	block_if bi = calloc(1, sizeof(*bi));
	bi->state = ds;
	bi->nblocks = disk_nblocks;
	bi->setsize = disk_setsize;
	bi->read = disk_read;
	bi->write = disk_write;
	bi->destroy = disk_destroy;
	return bi;
}

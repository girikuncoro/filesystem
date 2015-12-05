/* Author: Robbert van Renesse, October 2015
 *
 * This is a disk layer that is supposed to run as a top layer and simply
 * replays a trace of read and write operations from a file.  Usage:
 *
 *		block_if tracedisk_init(block_if below, char *trace, unsigned int n_inodes);
 *
 * The file is supposed to contain commands of the form:
 *
 *			R:inode:block		// read(inode, block)
 *			W:inode:block		// write(inode, block)
 *			S:inode:nblocks		// setsize(inode, nblocks)
 *			N:inode:nblocks		// nblocks(inode) == nblocks?
 *
 * with 0 <= inode < n_inodes and 0 <= block < MAX_BLOCKS, as defined here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block_if.h"

#define MAX_BLOCKS			(1 << 27)

struct tracedisk_state {
	block_if below;				// block store below
};

struct virtdisk {
	block_if treedisk;
	block_if checkdisk;
};

static void tracedisk_run(struct tracedisk_state *ts,
									char *trace, unsigned int n_inodes){
	FILE *fp;
	struct virtdisk *inodes = calloc(n_inodes, sizeof(*inodes));
	block_if virt;
	unsigned int cnt = 1;

	if ((fp = fopen(trace, "r")) == 0) {
		fprintf(stderr, "tracedisk_run: can't open %s\n", trace);
		return;
	}

	char cmd;
	unsigned int inode, bno;
	while (fscanf(fp, "%c:%u:%u\n", &cmd, &inode, &bno) == 3) {
		if (inode >= n_inodes) {
			fprintf(stderr, "inode number too large\n");
			break;
		}
		if (bno >= MAX_BLOCKS) {
			fprintf(stderr, "block number too large\n");
			break;
		}
		if (inodes[inode].treedisk == 0) {
			inodes[inode].treedisk = treedisk_init(ts->below, inode);
			inodes[inode].checkdisk = checkdisk_init(inodes[inode].treedisk, "tre");
		}
		virt = inodes[inode].checkdisk;
		static block_t block;
		int result;
		switch (cmd) {
		case 'R':
			// fprintf(stderr, "%u: read %u %u\n", cnt, (unsigned int) inode, (unsigned int) bno);
			result = (*virt->read)(virt, bno, &block);
			if (result < 0) {
				fprintf(stderr, "!!ERROR: tracedisk_run: read(%u, %u) failed\n", inode, bno);
				break;
			}
			if ((((unsigned int *) &block)[0] != inode && ((unsigned int *) &block)[0] != 0) || (((unsigned int *) &block)[1] != bno && ((unsigned int *) &block)[1] != 0)) {
				fprintf(stderr, "!!ERROR: tracedisk_run: unexpected content %u %u %u %u\n", inode, bno, ((unsigned int *) &block)[0], ((unsigned int *) &block)[1]);
			}
			break;
		case 'W':
			// fprintf(stderr, "%u: write %u %u\n", cnt, (unsigned int) inode, (unsigned int) bno);
			((unsigned int *) &block)[0] = inode;
			((unsigned int *) &block)[1] = bno;
			result = (*virt->write)(virt, bno, &block);
			if (result < 0) {
				fprintf(stderr, "!!ERROR: tracedisk_run: write(%u, %u) failed\n", inode, bno);
				break;
			}
			break;
		case 'S':
			// fprintf(stderr, "%u: setsize %u %u\n", cnt, (unsigned int) inode, (unsigned int) bno);
			result = (*virt->setsize)(virt, bno);
			if (result < 0) {
				fprintf(stderr, "!!ERROR: tracedisk_run: setsize(%u, %u) failed\n", inode, bno);
				break;
			}
			break;
		case 'N':
			// fprintf(stderr, "%u: nblocks %u %u\n", cnt, (unsigned int) inode, (unsigned int) bno);
			result = (*virt->nblocks)(virt);
			if (result != bno) {
				fprintf(stderr, "!!CHKSIZE %u: nblocks %u: %u != %d\n", cnt, (unsigned int) inode, (unsigned int) bno, result);
			}
			break;
		default:
			fprintf(stderr, "tracedisk_run: unexpected command\n");
		}
		cnt++;
	}

	fclose(fp);
	for (inode = 0; inode < n_inodes; inode++) {
		if ((virt = inodes[inode].checkdisk) != 0) {
			(*virt->destroy)(virt);
		}
		if ((virt = inodes[inode].treedisk) != 0) {
			(*virt->destroy)(virt);
		}
	}
	free(inodes);
}

static void tracedisk_destroy(block_if bi){
	struct tracedisk_state *ts = bi->state;

	free(ts);
	free(bi);
}

block_if tracedisk_init(block_if below, char *trace, unsigned int n_inodes){
	/* Create the block store state structure.
	 */
	struct tracedisk_state *ts = calloc(1, sizeof(*ts));
	ts->below = below;

	tracedisk_run(ts, trace, n_inodes);

	/* Return a block interface to this inode.
	 */
	block_if bi = calloc(1, sizeof(*bi));
	bi->state = ts;
	bi->destroy = tracedisk_destroy;
	return bi;
}

/* Author: Robbert van Renesse, August 2015
 */

#include <stdio.h>
#include <stdlib.h>
#include "block_if.h"

void panic(char *s){
	fprintf(stderr, "!!PANIC: %s\n", s);
	exit(1);
}

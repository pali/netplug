#include <stdlib.h>
#include <stdio.h>

#include "netplug.h"


void *
xmalloc(size_t n)
{
    void *x = malloc(n);

    if (n > 0 && x == NULL) {
	perror("malloc");
	exit(1);
    }

    return x;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

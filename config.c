#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netplug.h"


void read_config(char *filename)
{
    FILE *fp;

    if (filename == NULL || strcmp(filename, "-") == 0) {
	filename = "stdin";
	fp = stdin;
    } else if ((fp = fopen(filename, "r")) == NULL) {
	perror(filename);
	exit(1);
    }

    char buf[8192];

    while (fgets(buf, sizeof(buf), fp)) {
	// do summat
    }
    
    if (ferror(fp)) {
	fprintf(stderr, "Error reading %s: %s\n", filename, strerror(errno));
	exit(1);
    }

    if (fp != stdin) {
	fclose(stdin);
    }
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

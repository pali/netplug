#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netplug.h"


struct if_pat {
    char *pat;
    struct if_pat *next;
};

static struct if_pat *pats;
static struct if_pat *memo;


int
if_match(char *name)
{
    struct if_pat *pat;

    if (memo && fnmatch(memo->pat, name, 0) == 0) {
	return 1;
    }
    
    for (pat = pats; pat != NULL; pat = pat->next) {
	if (fnmatch(pat->pat, name, 0) == 0) {
	    memo = pat;
	    return 1;
	}
    }

    return 0;
}


void
read_config(char *filename)
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

    for (int line = 1; fgets(buf, sizeof(buf), fp); line++) {
	char *l, *r;

	for (l = buf; *l != '\0' && isspace(*l); l++) {
	}
	for (r = l; *r != '\0' && !isspace(*r); r++) {
	}

	*r = '\0';

	char *h;

	if ((h = strchr(l, '#')) != NULL) {
	    *h = '\0';
	}

	int len = strlen(l);

	if (len == 0) {
	    continue;
	}

	int x = fnmatch(l, "eth0", 0);

	if (x != 0 && x != FNM_NOMATCH) {
	    fprintf(stderr, "%s:%d:bad pattern: %s\n", filename, line, l);
	    exit(1);
	}
	
	struct if_pat *pat = xmalloc(sizeof(*pat));

	pat->pat = xmalloc(len + 1);
	memcpy(pat->pat, l, len + 1);
	pat->next = pats;
	pats = pat->next;
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

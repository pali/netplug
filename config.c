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


int
save_pattern(char *name)
{
    int len = strlen(name);

    if (len == 0) {
	return 0;
    }

    int x = fnmatch(name, "eth0", 0);

    if (x != 0 && x != FNM_NOMATCH) {
	return -1;
    }
	
    struct if_pat *pat = xmalloc(sizeof(*pat));

    pat->pat = xmalloc(len + 1);
    memcpy(pat->pat, name, len + 1);
    pat->next = pats;
    pats = pat;

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
	return;
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

	if (save_pattern(l) == -1) {
	    fprintf(stderr, "%s, line %d: bad pattern: %s\n",
		    filename, line, l);
	    exit(1);
	}
    }
    
    if (ferror(fp)) {
	fprintf(stderr, "Error reading %s: %s\n", filename, strerror(errno));
	exit(1);
    }

    if (fp != stdin) {
	fclose(stdin);
    }
}


static int
has_meta(char *s)
{
    static const char meta[] = "[]*?";

    for (char *x = s; *x != '\0'; x++) {
	for (const char *m = meta; *m != '\0'; m++) {
	    if (*x == *m) {
		return x - s;
	    }
	}
    }

    return -1;
}


int
try_probe(char *iface)
{
    const char fmt[] = "exec /sbin/ip link set %s up >/dev/null 2>&1";
    char cmd[sizeof(fmt) + strlen(iface)];
    
    sprintf(cmd, fmt, iface);
    
    int ret = system(cmd);

    return ret == 0 ? 1 : 0;
}


void
probe_interfaces(void)
{
    int nmatch = 0;
    
    for (struct if_pat *p = pats; p != NULL; p = p->next) {
	int m;
	
	if ((m = has_meta(p->pat)) == -1) {
	    nmatch += try_probe(p->pat);
	}
	else if (m == 0) {
	    fprintf(stderr, "Warning: Don't know how to probe for interfaces "
		    "matching %s\n", p->pat);
	    continue;
	}
	else {
	    char *z = xmalloc(m + 4);
	    
	    strncpy(z, p->pat, m);
	    
	    for (int i = 0; i < 16; i++) {
		sprintf(z + m, "%d", i);
		if (fnmatch(p->pat, z, 0) == 0) {
		    nmatch += try_probe(z);
		}
	    }

	    free(z);
	}
    }

    if (nmatch == 0) {
	fprintf(stderr, "Warning: Could not probe for any interfaces\n");
    }
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "netplug.h"


pid_t
run_netplug_bg(char *ifname, char *action)
{
    pid_t pid;

    if ((pid = fork()) == -1) {
	perror("fork");
	exit(1);
    }
    else if (pid != 0) {
	return pid;
    }
    
    printf("%s %s %s\n", NP_SCRIPT, ifname, action);
    fflush(stdout);
    
    execl(NP_SCRIPT, NP_SCRIPT, ifname, action, NULL);

    perror(NP_SCRIPT);
    exit(1);
}


int
run_netplug(char *ifname, char *action)
{
    pid_t pid = run_netplug_bg(ifname, action);
    int status;
    waitpid(pid, &status, WNOHANG);
    return status;
}


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

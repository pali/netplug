#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "netplug.h"


pid_t
run_netplug_bg(char *ifname, char *action)
{
    pid_t pid;

    if ((pid = fork()) == -1) {
	do_log(LOG_ERR, "fork: %m");
	exit(1);
    }
    else if (pid != 0) {
	return pid;
    }
    
    do_log(LOG_INFO, "%s %s %s", NP_SCRIPT, ifname, action);
    
    execl(NP_SCRIPT, NP_SCRIPT, ifname, action, NULL);

    do_log(LOG_ERR, NP_SCRIPT ": %m");
    exit(1);
}


int
run_netplug(char *ifname, char *action)
{
    pid_t pid = run_netplug_bg(ifname, action);
    int status, ret;

    if ((ret = waitpid(pid, &status, 0)) == -1) {
	do_log(LOG_ERR, "waitpid: %m");
	exit(1);
    }
    
    return WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
}


void *
xmalloc(size_t n)
{
    void *x = malloc(n);

    if (n > 0 && x == NULL) {
	do_log(LOG_ERR, "malloc: %m");
	exit(1);
    }

    return x;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

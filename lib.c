/*
 * lib.c - random library routines
 *
 * Copyright 2003 PathScale, Inc.
 * Copyright 2003, 2004, 2005 Bryan O'Sullivan
 * Copyright 2003 Jeremy Fitzhardinge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.  You are
 * forbidden from redistributing or modifying it under the terms of
 * any other license, including other versions of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "netplug.h"


void
do_log(int pri, const char *fmt, ...)
{
    extern int use_syslog;
    va_list ap;
    va_start(ap, fmt);

    if (pri == LOG_DEBUG && !debug)
        return;

    if (use_syslog) {
        vsyslog(pri, fmt, ap);
    } else {
        FILE *fp;

        switch (pri) {
        case LOG_INFO:
        case LOG_NOTICE:
        case LOG_DEBUG:
            fp = stdout;
            break;
        default:
            fp = stderr;
            break;
        }

        switch (pri) {
        case LOG_WARNING:
            fputs("Warning: ", fp);
            break;
        case LOG_NOTICE:
            fputs("Notice: ", fp);
            break;
        case LOG_CRIT:
        case LOG_ERR:
            fputs("Error: ", fp);
            break;
        case LOG_INFO:
        case LOG_DEBUG:
            break;
        default:
            fprintf(fp, "Log type %d: ", pri);
            break;
        }

        vfprintf(fp, fmt, ap);
        fputc('\n', fp);
    }

    va_end(ap);
}


void
close_on_exec(int fd)
{
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
        do_log(LOG_ERR, "can't set fd %d to close on exec: %m", fd);
        exit(1);
    }
}


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

    setpgrp();                  /* become group leader */

    do_log(LOG_INFO, "%s %s %s -> pid %d",
           NP_SCRIPT, ifname, action, getpid());

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


/*
   Synchronously kill a script

   Assumes the pid is actually a leader of a group.  Kills first with
   SIGTERM at first; if that doesn't work, follow up with a SIGKILL.
 */
void
kill_script(pid_t pid)
{
    pid_t ret;
    int status;
    sigset_t mask, origmask;

    if (pid == -1)
        return;

    assert(pid > 0);

    /* Block SIGCHLD while we go around killing things, so the SIGCHLD
       handler doesn't steal things behind our back. */
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &origmask);

    /* ask nicely */
    if (killpg(pid, SIGTERM) == -1) {
        do_log(LOG_ERR, "Can't kill script pgrp %d: %m", pid);
        goto done;
    }

    sleep(1);

    ret = waitpid(pid, &status, WNOHANG);

    if (ret == -1) {
        do_log(LOG_ERR, "Failed to wait for %d: %m?!", pid);
        goto done;
    } else if (ret == 0) {
        /* no more Mr. nice guy */
        if (killpg(pid, SIGKILL) == -1) {
            do_log(LOG_ERR, "2nd kill %d failed: %m?!", pid);
            goto done;
        }
        ret = waitpid(pid, &status, 0);
    }

    assert(ret == pid);

 done:
    sigprocmask(SIG_SETMASK, &origmask, NULL);
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


void
__assert_fail(const char *assertion, const char *file,
              unsigned int line, const char *function)
{
    do_log(LOG_CRIT, "%s:%u: %s%sAssertion `%s' failed",
           file, line,
           function ? function : "",
           function ? ": " : "",
           assertion);

    abort();
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

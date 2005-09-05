/*
 * netlink.c - interface with kernel's netlink facility
 *
 * Copyright 2003 PathScale, Inc.
 * Copyright 2003, 2004, 2005 Bryan O'Sullivan
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
 *
 * Portions of this file are based on code from Alexey Kuznetsov's
 * iproute2 package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "netplug.h"


static int seq, dump;


void
netlink_request_dump(int fd)
{
    struct {
        struct nlmsghdr hdr;
        struct rtgenmsg msg;
    } req;
    struct sockaddr_nl addr;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;

    memset(&req, 0, sizeof(req));
    req.hdr.nlmsg_len = sizeof(req);
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    req.hdr.nlmsg_pid = 0;
    req.hdr.nlmsg_seq = dump = ++seq;
    req.msg.rtgen_family = AF_UNSPEC;

    if (sendto(fd, (void*) &req, sizeof(req), 0,
               (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        do_log(LOG_ERR, "Could not request interface dump: %m");
        exit(1);
    }
}


typedef enum {
    ok,		/* handle the message */
    skip,	/* skip the message */
    done,	/* all's well, no more processing */
    bail,	/* something's wrong, no more processing */
    user,	/* packet came from someone naughty in user space */
} todo;


static todo
receive(int fd, struct msghdr *msg, int *status)
{
    *status = recvmsg(fd, msg, 0);

    if (*status == -1) {
	if (errno == EINTR) {
	    return skip;
	}
	if (errno == EAGAIN) {
	    /* XXX when will this ever happen? */
	    return done;
	}

	do_log(LOG_ERR, "Netlink receive error: %m");
	return done;
    }
    else if (*status == 0) {
	do_log(LOG_ERR, "Unexpected EOF on netlink");
	return bail;
    }

    if (msg->msg_namelen != sizeof(struct sockaddr_nl)) {
	do_log(LOG_ERR, "Unexpected sender address length: got %d, expected %d",
	       msg->msg_namelen, (int) sizeof(struct sockaddr_nl));
	return done;
    }

    if (((struct sockaddr_nl *) msg->msg_name)->nl_pid != 0) {
	do_log(LOG_ERR, "Netlink packet came from pid %d, not from kernel",
	       ((struct sockaddr_nl *) msg->msg_name)->nl_pid);
	return user;
    }

    return ok;
}


/*
 * Return values:
 *
 * 0  - exit calling loop
 * !0 - we have a valid event
 */
int
netlink_listen(int fd, netlink_callback callback, void *arg)
{
    char   buf[8192];
    struct iovec iov = { buf, sizeof(buf) };
    struct sockaddr_nl addr;
    struct msghdr msg = {
        .msg_name    = (void *) &addr,
        .msg_namelen = sizeof(addr),
        .msg_iov     = &iov,
        .msg_iovlen  = 1,
    };

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = 0;
    addr.nl_groups = 0;

    while (1) {
	int status;
	
	switch (receive(fd, &msg, &status)) {
	case user:
	case done:
	    return 1;
	case bail:
	    return 0;
	case skip:
	    continue;
	case ok:
	    break;
	}

        struct nlmsghdr *hdr;

        for (hdr = (struct nlmsghdr*) buf; status >= sizeof(*hdr); ) {
            int len = hdr->nlmsg_len;
            int l = len - sizeof(*hdr);

            if (l < 0 || len > status) {
                if (msg.msg_flags & MSG_TRUNC) {
                    do_log(LOG_ERR, "Truncated message");
                    return 1;
                }
                do_log(LOG_ERR, "Malformed netlink message");
                return 1;
            }

            if (callback) {
                int err;

                if ((err = callback(hdr, arg)) == -1) {
                    do_log(LOG_ERR, "Callback failed");
                    goto outer;
                }
            }

            status -= NLMSG_ALIGN(len);
            hdr = (struct nlmsghdr *) ((char *) hdr + NLMSG_ALIGN(len));
        }
        if (msg.msg_flags & MSG_TRUNC) {
            do_log(LOG_ERR, "Message truncated");
            continue;
        }
        if (status) {
            do_log(LOG_ERR, "!!!Remnant of size %d", status);
            return 1;
        }
    outer:
        /* do nothing */;
    }
}


void
netlink_receive_dump(int fd, netlink_callback callback, void *arg)
{
    char buf[8192];
    struct sockaddr_nl addr;
    struct iovec iov = { buf, sizeof(buf) };
    struct msghdr msg = {
        .msg_name        = (void *) &addr,
        .msg_namelen = sizeof(addr),
        .msg_iov     = &iov,
        .msg_iovlen  = 1,
    };

    while (1) {
	int status;
	
	switch (receive(fd, &msg, &status)) {
	case bail:
	case done:
	    exit(1);
	case user:
	case skip:
	    continue;
	case ok:
	    break;
	}

        struct nlmsghdr *hdr = (struct nlmsghdr *) buf;

        while (NLMSG_OK(hdr, status)) {
            if (hdr->nlmsg_seq != dump) {
                do_log(LOG_ERR, "Skipping junk");
                goto skip_it;
            }

            if (hdr->nlmsg_type == NLMSG_DONE) {
                return;
            }
            else if (hdr->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(hdr);

                if (hdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    do_log(LOG_ERR, "Netlink message truncated");
                } else {
                    errno = -err->error;
                    do_log(LOG_ERR, "Error from rtnetlink: %m");
                }
                exit(1);
            }

            if (callback) {
		int err;

                if ((err = callback(hdr, arg)) == -1) {
                    do_log(LOG_ERR, "Callback failed");
                    exit(1);
                }
            }

        skip_it:
            hdr = NLMSG_NEXT(hdr, status);
        }
        if (msg.msg_flags & MSG_TRUNC) {
            do_log(LOG_ERR, "Message truncated");
            exit(1);
        }
        if (status) {
            do_log(LOG_ERR, "Dangling remnant of size %d!", status);
            exit(1);
        }
    }
}


int
netlink_open(void)
{
    int fd;

    if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
        do_log(LOG_ERR, "Could not create netlink socket: %m");
        exit(1);
    }

    close_on_exec(fd);

    struct sockaddr_nl addr;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK;

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        do_log(LOG_ERR, "Could not bind netlink socket: %m");
        exit(1);
    }

    socklen_t addr_len = sizeof(addr);

    if (getsockname(fd, (struct sockaddr *) &addr, &addr_len) == -1) {
        do_log(LOG_ERR, "Could not get socket details: %m");
        exit(1);
    }

    if (addr_len != sizeof(addr)) {
        do_log(LOG_ERR, "Our netlink socket size does not match the kernel's!");
        exit(1);
    }

    if (addr.nl_family != AF_NETLINK) {
        do_log(LOG_ERR, "The kernel has given us an insane address family!");
        exit(1);
    }

    return fd;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

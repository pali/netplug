#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

    req.hdr.nlmsg_len = sizeof(req);
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
    req.hdr.nlmsg_pid = 0;
    req.hdr.nlmsg_seq = dump = ++seq;
    req.msg.rtgen_family = AF_UNSPEC;

    if (sendto(fd, (void*) &req, sizeof(req), 0,
	       (struct sockaddr *) &addr, sizeof(addr)) == -1) {
	perror("Could not request interface dump");
	exit(1);
    }
}


void netlink_listen(int fd, netlink_callback callback, void *arg)
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
	int status = recvmsg(fd, &msg, 0);

	if (status == -1) {
	    if (errno == EINTR)
		continue;
	    perror("OVERRUN");
	    continue;
	}
	else if (status == 0) {
	    fprintf(stderr, "Unexpected EOF on netlink\n");
	    exit(1);
	}

	if (msg.msg_namelen != sizeof(addr)) {
	    fprintf(stderr, "Unexpected sender address length\n");
	    exit(1);
	}

	struct nlmsghdr *hdr;
	
	for (hdr = (struct nlmsghdr*)buf; status >= sizeof(*hdr); ) {
	    int len = hdr->nlmsg_len;
	    int l = len - sizeof(*hdr);

	    if (l < 0 || len > status) {
		if (msg.msg_flags & MSG_TRUNC) {
		    fprintf(stderr, "Truncated message\n");
		    exit(1);
		}
		fprintf(stderr, "Malformed netlink message\n");
		exit(1);
	    }

	    if (callback) {
		int err;
		
		if ((err = callback(hdr, arg)) == -1) {
		    return;
		}
	    }

	    status -= NLMSG_ALIGN(len);
	    hdr = (struct nlmsghdr *) ((char *) hdr + NLMSG_ALIGN(len));
	}
	if (msg.msg_flags & MSG_TRUNC) {
	    fprintf(stderr, "Message truncated\n");
	    continue;
	}
	if (status) {
	    fprintf(stderr, "!!!Remnant of size %d\n", status);
	    exit(1);
	}
    }
}


void netlink_receive_dump(int fd, netlink_callback callback, void *arg)
{
    char buf[8192];
    struct sockaddr_nl addr;
    struct iovec iov = { buf, sizeof(buf) };
    struct msghdr msg = {
	.msg_name	 = (void *) &addr,
	.msg_namelen = sizeof(addr),
	.msg_iov     = &iov,
	.msg_iovlen  = 1,
    };

    while (1) {
	int status = recvmsg(fd, &msg, 0);

	if (status == -1) {
	    if (errno == EINTR) {
		continue;
	    }
	    perror("Netlink overrun");
	    continue;
	}
	else if (status == 0) {
	    fprintf(stderr, "Unexpected EOF on netlink\n");
	    exit(1);
	}

	if (msg.msg_namelen != sizeof(addr)) {
	    fprintf(stderr, "Unexpected sender address length\n");
	    exit(1);
	}

	struct nlmsghdr *hdr = (struct nlmsghdr *) buf;

	while (NLMSG_OK(hdr, status)) {
	    int err;

	    if (hdr->nlmsg_seq != dump) {
		fprintf(stderr, "Skipping junk\n");
		goto skip_it;
	    }

	    if (hdr->nlmsg_type == NLMSG_DONE) {
		return;
	    }
	    else if (hdr->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(hdr);
		
		if (hdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
		    fprintf(stderr, "Netlink message truncated\n");
		} else {
		    errno = -err->error;
		    perror("Error from rtnetlink");
		}
		exit(1);
	    }

	    if (callback) {
		if ((err = callback(hdr, arg)) == -1) {
		    return;
		}
	    }

	skip_it:
	    hdr = NLMSG_NEXT(hdr, status);
	}
	if (msg.msg_flags & MSG_TRUNC) {
	    fprintf(stderr, "Message truncated\n");
	    continue;
	}
	if (status) {
	    fprintf(stderr, "Dangling remnant of size %d!\n", status);
	    exit(1);
	}
    }
}


int
netlink_open(void)
{
    int fd;

    if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
	perror("Could not create netlink socket");
	exit(1);
    }

    struct sockaddr_nl addr;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK;

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
	perror("Could not bind netlink socket");
	exit(1);
    }
    
    int addr_len = sizeof(addr);
    
    if (getsockname(fd, (struct sockaddr *) &addr, &addr_len) == -1) {
	perror("Could not get socket details");
	exit(1);
    }
	
    if (addr_len != sizeof(addr)) {
	fprintf(stderr, "Our netlink socket size does not match the kernel's!\n");
	exit(1);
    }

    if (addr.nl_family != AF_NETLINK) {
	fprintf(stderr, "The kernel has given us an insane address family!\n");
	exit(1);
    }

    return fd;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

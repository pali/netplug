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
    struct sockaddr_nl nladdr;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    req.hdr.nlmsg_len = sizeof(req);
    req.hdr.nlmsg_type = RTM_GETLINK;
    req.hdr.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
    req.hdr.nlmsg_pid = 0;
    req.hdr.nlmsg_seq = dump = ++seq;
    req.msg.rtgen_family = AF_UNSPEC;

    if (sendto(fd, (void*) &req, sizeof(req), 0,
	       (struct sockaddr *) &nladdr, sizeof(nladdr)) == -1) {
	perror("Could not request interface dump");
	exit(1);
    }
}


int netlink_listen(int fd, 
		   int (*handler)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
		   void *jarg)
{
    int status;
    struct nlmsghdr *h;
    struct sockaddr_nl nladdr;
    struct iovec iov;
    char   buf[8192];
    struct msghdr msg = {
	(void*)&nladdr, sizeof(nladdr),
	&iov,	1,
	NULL,	0,
	0
    };

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = 0;
    nladdr.nl_groups = 0;


    iov.iov_base = buf;

    while (1) {
	iov.iov_len = sizeof(buf);
	status = recvmsg(fd, &msg, 0);

	if (status < 0) {
	    if (errno == EINTR)
		continue;
	    perror("OVERRUN");
	    continue;
	}
	if (status == 0) {
	    fprintf(stderr, "EOF on netlink\n");
	    return -1;
	}
	if (msg.msg_namelen != sizeof(nladdr)) {
	    fprintf(stderr, "Sender address length == %d\n", msg.msg_namelen);
	    exit(1);
	}
	for (h = (struct nlmsghdr*)buf; status >= sizeof(*h); ) {
	    int err;
	    int len = h->nlmsg_len;
	    int l = len - sizeof(*h);

	    if (l<0 || len>status) {
		if (msg.msg_flags & MSG_TRUNC) {
		    fprintf(stderr, "Truncated message\n");
		    return -1;
		}
		fprintf(stderr, "!!!malformed message: len=%d\n", len);
		exit(1);
	    }

	    err = handler(&nladdr, h, jarg);
	    if (err < 0)
		return err;

	    status -= NLMSG_ALIGN(len);
	    h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
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


void netlink_receive_dump(int fd, dump_filter filter, void *arg)
{
    char buf[8192];
    struct sockaddr_nl nladdr;
    struct iovec iov = { buf, sizeof(buf) };

    while (1) {
	struct msghdr msg = {
	    .msg_name	 = (void *) &nladdr,
	    .msg_namelen = sizeof(nladdr),
	    .msg_iov     = &iov,
	    .msg_iovlen  = 1,
	};

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

	if (msg.msg_namelen != sizeof(nladdr)) {
	    fprintf(stderr, "Unexpected sender address length\n");
	    exit(1);
	}

	struct nlmsghdr *h = (struct nlmsghdr *) buf;

	while (NLMSG_OK(h, status)) {
	    int err;

	    if (h->nlmsg_seq != dump) {
		fprintf(stderr, "Skipping junk\n");
		goto skip_it;
	    }

	    if (h->nlmsg_type == NLMSG_DONE) {
		return;
	    }
	    else if (h->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr *) NLMSG_DATA(h);
		
		if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
		    fprintf(stderr, "Netlink message truncated\n");
		} else {
		    errno = -err->error;
		    perror("Error from rtnetlink");
		}
		exit(1);
	    }

	    if (filter) {
		if ((err = filter(h, arg)) == -1) {
		    return;
		}
	    }

	skip_it:
	    h = NLMSG_NEXT(h, status);
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

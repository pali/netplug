#include <asm/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


int netlink_open(void)
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


int main(int argc, char *argv[])
{
    int fd = netlink_open();
    return fd ? 0 : 0;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

#include <stdio.h>
#include <stdlib.h>

#include "netplug.h"


int handle_interface(struct nlmsghdr *hdr, void *arg)
{
    if (hdr->nlmsg_type != RTM_NEWLINK && hdr->nlmsg_type != RTM_DELLINK) {
	return 0;
    }
    
    struct ifinfomsg *info = NLMSG_DATA(hdr);
    int len = hdr->nlmsg_len - NLMSG_LENGTH(sizeof(*info));

    if (len < 0) {
	return -1;
    }
    
    struct rtattr *attrs[IFLA_MAX + 1];

    parse_rtattrs(attrs, IFLA_MAX, IFLA_RTA(info), len);

    if (attrs[IFLA_IFNAME] == NULL) {
	fprintf(stderr, "No interface name\n");
	exit(1);
    }
    
    struct if_info *i;
    
    if ((i = if_info_get_interface(hdr, attrs)) == NULL) {
	return -1;
    }

    char *name = RTA_DATA(attrs[IFLA_IFNAME]);

    if (!if_match(name)) {
	return 0;
    }
    
    printf("%s: flags 0x%08x -> 0x%08x\n", name, i->flags, info->ifi_flags);

    if_info_update_interface(hdr, attrs);
    
    return 0;
}


int
main(int argc, char *argv[])
{
    read_config("-");
    
    int fd = netlink_open();

    netlink_request_dump(fd);
    netlink_receive_dump(fd, if_info_save_interface, NULL);
    netlink_listen(fd, handle_interface, NULL);

    return fd ? 0 : 0;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

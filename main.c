#define _GNU_SOURCE
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "netplug.h"


#define flag_was_set(flag) \
	(!(i->flags & (flag)) && (info->ifi_flags & (flag)))
#define flag_was_unset(flag) \
	((i->flags & (flag)) && !(info->ifi_flags & (flag)))

static int
handle_interface(struct nlmsghdr *hdr, void *arg)
{
    if (hdr->nlmsg_type != RTM_NEWLINK && hdr->nlmsg_type != RTM_DELLINK) {
	return 0;
    }
    
    struct ifinfomsg *info = NLMSG_DATA(hdr);
    int len = hdr->nlmsg_len - NLMSG_LENGTH(sizeof(*info));

    if (info->ifi_flags & IFF_LOOPBACK) {
	goto done;
    }
    
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

    if (i->flags == info->ifi_flags) {
	goto done;
    }
    
    char *name = RTA_DATA(attrs[IFLA_IFNAME]);

    if (!if_match(name)) {
	goto done;
    }
    
    printf("%s: flags 0x%08x -> 0x%08x\n", name, i->flags, info->ifi_flags);

    if (flag_was_set(IFF_RUNNING)) {
	run_netplug_bg(name, "in");
    }
    if (flag_was_unset(IFF_RUNNING)) {
	run_netplug_bg(name, "out");
    }
    if (flag_was_unset(IFF_UP)) {
	if (try_probe(name) == 0) {
	    fprintf(stderr, "Warning: Could not bring %s back up\n", name);
	}
    }

 done:
    if_info_update_interface(hdr, attrs);
    
    return 0;
}


static void
usage(int exitcode)
{
    fprintf(stderr, "Usage: netplug [-FP] [-c config_file] [-i interface]\n");

    fprintf(stderr, "\t-F\t\t"
	    "run in foreground (don't become a daemon)\n");
    fprintf(stderr, "\t-F\t\t"
	    "do not autoprobe for interfaces (use with care)\n");
    fprintf(stderr, "\t-c config_file\t"
	    "read interface patterns from this config file\n");
    fprintf(stderr, "\t-i interface\t"
	    "only handle interfaces matching this pattern\n");

    exit(exitcode);
}


int
main(int argc, char *argv[])
{
    int c;
    int foreground = 0;
    int cfg_read = 0;
    int probe = 1;

    while ((c = getopt(argc, argv, "FPc:hi:")) != EOF) {
	switch (c) {
	case 'F':
	    foreground = 1;
	    break;
	case 'P':
	    probe = 0;
	    break;
	case 'c':
	    read_config(optarg);
	    cfg_read = 1;
	    break;
	case 'h':
	    usage(0);
	    break;
	case 'i':
	    if (save_pattern(optarg) == -1) {
		fprintf(stderr, "Bad pattern for '-i %s'\n", optarg);
		exit(1);
	    }
	    break;
	case '?':
	    usage(1);
	}
    }
    
    if (!cfg_read) {
	read_config(NP_ETC_DIR "/netplugd.conf");
    }
    
    if (getuid() != 0) {
	fprintf(stderr, "Warning: This command will not work properly unless "
		"run by root\n");
    }
    
    if (probe) {
	probe_interfaces();
    }
    
    int fd = netlink_open();

    netlink_request_dump(fd);
    netlink_receive_dump(fd, if_info_save_interface, NULL);

    if (!foreground && daemon(0, 0) == -1) {
	perror("daemon");
	exit(1);
    }
    
    netlink_listen(fd, handle_interface, NULL);

    return fd ? 0 : 0;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

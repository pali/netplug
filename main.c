#define _GNU_SOURCE
#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "netplug.h"


static int use_syslog;


void
do_log(int pri, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

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
	case LOG_ERR:
	    fputs("Error: ", fp);
	    break;
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
	do_log(LOG_ERR, "No interface name");
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
    
    do_log(LOG_INFO, "%s: flags 0x%08x -> 0x%08x", name, i->flags,
	   info->ifi_flags);

    if (flag_was_set(IFF_RUNNING)) {
	run_netplug_bg(name, "in");
    }
    if (flag_was_unset(IFF_RUNNING)) {
	run_netplug_bg(name, "out");
    }
    if (flag_was_unset(IFF_UP)) {
	if (try_probe(name) == 0) {
	    do_log(LOG_WARNING, "Could not bring %s back up", name);
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
    int cfg_read = 0;
    int foreground = 0;
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
	do_log(LOG_WARNING, "This command will not work properly unless "
	       "run by root");
    }
    
    if (probe) {
	probe_interfaces();
    }
    
    int fd = netlink_open();

    netlink_request_dump(fd);
    netlink_receive_dump(fd, if_info_save_interface, NULL);

    if (!foreground) {
	if (daemon(0, 0) == -1) {
	    do_log(LOG_ERR, "daemon: %m");
	    exit(1);
	}
	use_syslog = 1;
	openlog("netplugd", LOG_PID, LOG_DAEMON);
    }
    
    netlink_listen(fd, handle_interface, NULL);

    return fd ? 0 : 0;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

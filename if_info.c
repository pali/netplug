#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "netplug.h"


void
parse_rtattrs(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
    memset(tb, 0, sizeof(tb) * (max + 1));

    while (RTA_OK(rta, len)) {
	if (rta->rta_type <= max)
	    tb[rta->rta_type] = rta;
	rta = RTA_NEXT(rta,len);
    }
    if (len) {
	do_log(LOG_ERR, "Badness! Deficit %d, rta_len=%d", len, rta->rta_len);
	abort();
    }
}


static struct if_info *if_info[16];


int if_info_save_interface(struct nlmsghdr *hdr, void *arg)
{
    struct rtattr *attrs[IFLA_MAX + 1];
    struct ifinfomsg *info = NLMSG_DATA(hdr);

    parse_rtattrs(attrs, IFLA_MAX, IFLA_RTA(info), IFLA_PAYLOAD(hdr));

    return if_info_update_interface(hdr, attrs) ? 0 : -1;
}


struct if_info *
if_info_get_interface(struct nlmsghdr *hdr, struct rtattr *attrs[])
{
    if (hdr->nlmsg_type != RTM_NEWLINK) {
	return NULL;
    }

    struct ifinfomsg *info = NLMSG_DATA(hdr);

    if (hdr->nlmsg_len < NLMSG_LENGTH(sizeof(info))) {
	return NULL;
    }

    if (attrs[IFLA_IFNAME] == NULL) {
	return NULL;
    }
    
    int x = info->ifi_index & 0xf;
    struct if_info *i, **ip;

    for (ip = &if_info[x]; (i = *ip) != NULL; ip = &i->next) {
	if (i->index == info->ifi_index) {
	    break;
	}
    }
    
    if (i == NULL) {
	i = xmalloc(sizeof(*i));
	i->next = *ip;
	i->index = info->ifi_index;
	*ip = i;
    }
    return i;
}


struct if_info *
if_info_update_interface(struct nlmsghdr *hdr, struct rtattr *attrs[])
{
    struct ifinfomsg *info = NLMSG_DATA(hdr);
    struct if_info *i;

    if ((i = if_info_get_interface(hdr, attrs)) == NULL) {
	return NULL;
    }
    
    i->type = info->ifi_type;
    i->flags = info->ifi_flags;

    if (attrs[IFLA_ADDRESS]) {
	int alen;
	i->addr_len = alen = RTA_PAYLOAD(attrs[IFLA_ADDRESS]);
	if (alen > sizeof(i->addr))
	    alen = sizeof(i->addr);
	memcpy(i->addr, RTA_DATA(attrs[IFLA_ADDRESS]), alen);
    } else {
	i->addr_len = 0;
	memset(i->addr, 0, sizeof(i->addr));
    }

    strcpy(i->name, RTA_DATA(attrs[IFLA_IFNAME]));
    
    return i;
}


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

#ifndef __netplug_h
#define __netplug_h


#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>


/* configuration */

void read_config(char *filename);
int if_match(char *name);


/* netlink interfacing */

typedef int (*netlink_callback)(struct nlmsghdr *hdr, void *arg);

int netlink_open(void);
void netlink_request_dump(int fd);
void netlink_receive_dump(int fd, netlink_callback callback, void *arg);
void netlink_listen(int fd, netlink_callback callback, void *arg);


/* network interface info management */

struct if_info {
    struct if_info *next;
    int index;
    int type;
    unsigned flags;
    int addr_len;
    unsigned char addr[8];
    char name[16];
};

struct if_info *if_info_get_interface(struct nlmsghdr *hdr,
				      struct rtattr *attrs[]);
struct if_info *if_info_update_interface(struct nlmsghdr *hdr,
					 struct rtattr *attrs[]);
int if_info_save_interface(struct nlmsghdr *hdr, void *arg);
void parse_rtattrs(struct rtattr *tb[], int max, struct rtattr *rta, int len);


/* generic utilities */

void *xmalloc(size_t n);


#endif /* __netplug_h */


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

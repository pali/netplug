/*
 * netplug.h - common include file
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
 */

#ifndef __netplug_h
#define __netplug_h


#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define NP_SCRIPT NP_SCRIPT_DIR "/netplug"

/* configuration */

void read_config(char *filename);
int save_pattern(char *pat);
int if_match(char *iface);
int try_probe(char *iface);
void probe_interfaces(void);
void close_on_exec(int fd);

extern int debug;

/* netlink interfacing */

typedef int (*netlink_callback)(struct nlmsghdr *hdr, void *arg);

int netlink_open(void);
void netlink_request_dump(int fd);
void netlink_receive_dump(int fd, netlink_callback callback, void *arg);
int  netlink_listen(int fd, netlink_callback callback, void *arg);


/* network interface info management */

struct if_info {
    struct if_info *next;
    int index;
    int type;
    unsigned flags;
    int addr_len;
    unsigned char addr[8];
    char name[16];

    enum ifstate {
        ST_DOWN,                /* uninitialized */
        ST_DOWNANDOUT,          /* went down while running out script */
        ST_PROBING,             /* running probe script */
        ST_PROBING_UP,          /* running probe, and interface went UP */
        ST_INACTIVE,            /* interface inactive */
        ST_INNING,              /* plugin script is running */
        ST_WAIT_IN,             /* wait until plugin script is done */
        ST_ACTIVE,              /* interface active */
        ST_OUTING,              /* plugout script is running */
        ST_INSANE,              /* interface seems to be flapping */
    }           state;

    pid_t       worker;         /* pid of current in/out script */
    time_t      lastchange;     /* timestamp of last state change */
};

struct if_info *if_info_get_interface(struct nlmsghdr *hdr,
                                      struct rtattr *attrs[]);
struct if_info *if_info_update_interface(struct nlmsghdr *hdr,
                                         struct rtattr *attrs[]);
int if_info_save_interface(struct nlmsghdr *hdr, void *arg);
void parse_rtattrs(struct rtattr *tb[], int max, struct rtattr *rta, int len);
void for_each_iface(int (*func)(struct if_info *));

void ifsm_flagpoll(struct if_info *info);
void ifsm_flagchange(struct if_info *info, unsigned int newflags);
void ifsm_scriptdone(pid_t pid, int exitstatus);

/* utilities */

void do_log(int pri, const char *fmt, ...)
    __attribute__ ((format (printf, 2, 3)));
pid_t run_netplug_bg(char *ifname, char *action);
int run_netplug(char *ifname, char *action);
void kill_script(pid_t pid);
void *xmalloc(size_t n);


#endif /* __netplug_h */


/*
 * Local variables:
 * c-file-style: "stroustrup"
 * End:
 */

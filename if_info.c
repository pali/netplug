/*
 * if_info.c - track network interface information
 *
 * Copyright 2003 PathScale, Inc.
 * Copyright 2003, 2004, 2005 Bryan O'Sullivan
 * Copyright 2003 Jeremy Fitzhardinge
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <time.h>
#include <wait.h>
#include <net/if.h>

#include "netplug.h"

#define INFOHASHSZ      16      /* must be a power of 2 */
static struct if_info *if_info[INFOHASHSZ];

static const char *
statename(enum ifstate s)
{
    switch(s) {
#define S(x)    case ST_##x: return #x
        S(DOWN);
        S(DOWNANDOUT);
        S(PROBING);
        S(PROBING_UP);
        S(INACTIVE);
        S(INNING);
        S(WAIT_IN);
        S(ACTIVE);
        S(OUTING);
        S(INSANE);
#undef S
    default: return "???";
    }
}

static const char *
flags_str(char *buf, unsigned int fl)
{
    static struct flag {
        const char *name;
        unsigned int flag;
    } flags[] = {
#define  F(x)   { #x, IFF_##x }
        F(UP),
        F(BROADCAST),
        F(DEBUG),
        F(LOOPBACK),
        F(POINTOPOINT),
        F(NOTRAILERS),
        F(RUNNING),
        F(NOARP),
        F(PROMISC),
        F(ALLMULTI),
        F(MASTER),
        F(SLAVE),
        F(MULTICAST),
#undef F
    };
    char *cp = buf;

    *cp = '\0';

    for(int i = 0; i < sizeof(flags)/sizeof(*flags); i++) {
        if (fl & flags[i].flag) {
            fl &= ~flags[i].flag;
            cp += sprintf(cp, "%s,", flags[i].name);
        }
    }

    if (fl != 0)
        cp += sprintf(cp, "%x,", fl);

    if (cp != buf)
        cp[-1] = '\0';

    return buf;
}

void
for_each_iface(int (*func)(struct if_info *))
{
    for(int i = 0; i < INFOHASHSZ; i++) {
        for(struct if_info *info = if_info[i]; info != NULL; info = info->next) {
            if ((*func)(info))
                return;
        }
    }
}

/* Reevaluate the state machine based on the current state and flag settings */
void
ifsm_flagpoll(struct if_info *info)
{
    enum ifstate state = info->state;

    switch(info->state) {
    case ST_DOWN:
        if ((info->flags & (IFF_UP|IFF_RUNNING)) == 0)
            break;
        /* FALLTHROUGH */
    case ST_INACTIVE:
        if (!(info->flags & IFF_UP)) {
            assert(info->worker == -1);
            info->worker = run_netplug_bg(info->name, "probe");
            info->state = ST_PROBING;
        } else if (info->flags & IFF_RUNNING) {
            assert(info->worker == -1);
            info->worker = run_netplug_bg(info->name, "in");
            info->state = ST_INNING;
        }
        break;

    case ST_PROBING:
    case ST_PROBING_UP:
    case ST_WAIT_IN:
    case ST_DOWNANDOUT:
        break;

    case ST_INNING:
        if (!(info->flags & IFF_RUNNING))
            info->state = ST_WAIT_IN;
        break;

    case ST_ACTIVE:
        if (!(info->flags & IFF_RUNNING)) {
            assert(info->worker == -1);
            info->worker = run_netplug_bg(info->name, "out");
            info->state = ST_OUTING;
        }
        break;

    case ST_OUTING:
        if (!(info->flags & IFF_UP))
            info->state = ST_DOWNANDOUT;
	break;

    case ST_INSANE:
        break;
    }

    if (info->state != state)
        do_log(LOG_DEBUG, "ifsm_flagpoll %s: moved from state %s to %s",
               info->name, statename(state), statename(info->state));
}

/* if_info state machine transitions caused by interface flag changes (edge triggered) */
void
ifsm_flagchange(struct if_info *info, unsigned int newflags)
{
    unsigned int changed = (info->flags ^ newflags) & (IFF_RUNNING | IFF_UP);

    if (changed == 0)
        return;

    char buf1[512], buf2[512];
    do_log(LOG_INFO, "%s: state %s flags 0x%08x %s -> 0x%08x %s", info->name,
           statename(info->state),
           info->flags, flags_str(buf1, info->flags),
           newflags, flags_str(buf2, newflags));

    /* XXX put interface state-change rate limiting here */
    if (0 /* flapping */) {
        info->state = ST_INSANE;
    }

    if (changed & IFF_UP) {
        if (newflags & IFF_UP) {
            switch(info->state) {
            case ST_DOWN:
                info->state = ST_INACTIVE;
                break;

            case ST_PROBING:
                info->state = ST_PROBING_UP;
                break;

            default:
                do_log(LOG_ERR, "%s: unexpected state %s for UP", info->name, statename(info->state));
                exit(1);
            }
        } else {
            /* interface went down */
            switch(info->state) {
            case ST_OUTING:
                /* went down during an OUT script - OK */
                info->state = ST_DOWNANDOUT;
                break;

            case ST_DOWN:
                /* already down */
                break;

	    case ST_PROBING:
		/* already probing - don't do anything rash */
		break;

	    case ST_PROBING_UP:
		/* well, we were up, but now we're not */
		info->state = ST_PROBING;
		break;

            default:
                /* All other states: kill off any scripts currently
                   running, and go into the PROBING state, attempting
                   to bring it up */
                kill_script(info->worker);
                info->state = ST_PROBING;
                info->worker = run_netplug_bg(info->name, "probe");
            }
        }
    }

    if (changed & IFF_RUNNING) {
        switch(info->state) {
        case ST_INACTIVE:
            assert(!(info->flags & IFF_RUNNING));
            assert(info->worker == -1);

            info->worker = run_netplug_bg(info->name, "in");
            info->state = ST_INNING;
            break;

        case ST_INNING:
            assert(info->flags & IFF_RUNNING);
            info->state = ST_WAIT_IN;
            break;

        case ST_WAIT_IN:
            /* unaffected by interface flag changing */
            break;

        case ST_ACTIVE:
            assert(info->flags & IFF_RUNNING);
            assert(info->worker == -1);

            info->worker = run_netplug_bg(info->name, "out");
            info->state = ST_OUTING;
            break;

        case ST_OUTING:
            /* always go to INACTIVE regardless of flag state */
            break;

        case ST_PROBING:
        case ST_PROBING_UP:
            /* ignore running state */
            break;

        case ST_INSANE:
            /* stay insane until there's been quiet for a while, then
               down interface and switch to ST_DOWN */
            break;

        case ST_DOWN:
        case ST_DOWNANDOUT:
            /* badness: somehow interface became RUNNING without being
               UP - ignore it */
            break;
        }
    }

    do_log(LOG_DEBUG, "%s: moved to state %s; worker %d",
           info->name, statename(info->state), info->worker);
    info->flags = newflags;
    info->lastchange = time(0);
}

/* handle a script termination and update the state accordingly */
void ifsm_scriptdone(pid_t pid, int exitstatus)
{
    int exitok = WIFEXITED(exitstatus) && WEXITSTATUS(exitstatus) == 0;
    struct if_info *info;
    assert(WIFEXITED(exitstatus) || WIFSIGNALED(exitstatus));

    int find_pid(struct if_info *i) {
        if (i->worker == pid) {
            info = i;
            return 1;
        }
        return 0;
    }

    info = NULL;
    for_each_iface(find_pid);

    if (info == NULL) {
        do_log(LOG_INFO, "Unexpected child %d exited with status %d",
               pid, exitstatus);
        return;
    }

    do_log(LOG_INFO, "%s: state %s pid %d exited status %d",
           info->name, statename(info->state), pid, exitstatus);

    info->worker = -1;

    switch(info->state) {
    case ST_PROBING:
        /* If we're still in PROBING state, then it means that the
           interface flags have not come up, even though the script
           finished.  Go back to DOWN and wait for the UP flag
           setting. */
        if (!exitok)
            do_log(LOG_WARNING, "Could not bring %s back up", info->name);

        info->state = ST_DOWN;
        break;

    case ST_PROBING_UP:
        /* regardless of script's exit status, the interface is
           actually up now, so just make it inactive */
        info->state = ST_INACTIVE;
        break;

    case ST_DOWNANDOUT:
        /* we were just waiting for the out script to finish - start a
           probe script for this interface */
        info->state = ST_PROBING;
        assert(info->worker == -1);
        info->worker = run_netplug_bg(info->name, "probe");
        break;

    case ST_INNING:
        if (exitok)
            info->state = ST_ACTIVE;
        else
            info->state = ST_INSANE; /* ??? */
        break;

    case ST_OUTING:
        /* What if !exitok?  What if interface is still active? ->ST_INSANE? */
        info->state = ST_INACTIVE;
        break;

    case ST_WAIT_IN:
        assert(info->worker == -1);

        info->worker = run_netplug_bg(info->name, "out");
        info->state = ST_OUTING;
        break;

    case ST_INACTIVE:
    case ST_ACTIVE:
    case ST_INSANE:
    case ST_DOWN:
        do_log(LOG_ERR, "ifsm_scriptdone: %s: bad state %s for script termination",
               info->name, statename(info->state));
        exit(1);
    }

    do_log(LOG_DEBUG, "%s: moved to state %s", info->name, statename(info->state));
}

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

    int x = info->ifi_index & (INFOHASHSZ-1);
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

        /* initialize state machine fields */
        i->state = ST_DOWN;
        i->lastchange = 0;
        i->worker = -1;
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

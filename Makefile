prefix ?=
bindir ?= $(prefix)/sbin
etcdir ?= $(prefix)/etc/netplug
scriptdir ?= $(prefix)/etc/netplug.d

install_opts := -o root -g root

CFLAGS += -Wall -Werror -std=gnu99 -g -DNP_ETC_DIR='"$(etcdir)"' \
	-DNP_SCRIPT_DIR='"$(scriptdir)"'

netplugd: config.o netlink.o lib.o if_info.o main.o
	$(CC) -o $@ $^

clean:
	-rm -f netplugd *.o

install:
	install -d $(install_opts) -m 755 $(bindir) $(etcdir) $(scriptdir)
	install $(install_opts) -m 755 netplugd $(bindir)
	install -C $(install_opts) -m 644 etc/netplugd.conf $(etcdir)
	install -C $(install_opts) -m 755 scripts/netplug $(scriptdir)

version := $(shell awk '/define version/{print $$3}' netplug.spec)

prefix ?=
bindir ?= $(prefix)/sbin
etcdir ?= $(prefix)/etc/netplug
initdir ?= $(prefix)/etc/rc.d/init.d
scriptdir ?= $(prefix)/etc/netplug.d
mandir ?= $(prefix)/usr/share/man

install_opts :=

CFLAGS += -Wall -Werror -std=gnu99 -DNP_ETC_DIR='"$(etcdir)"' \
	-DNP_SCRIPT_DIR='"$(scriptdir)"' -ggdb3 -O3 -DNP_VERSION='"$(version)"'

netplugd: config.o netlink.o lib.o if_info.o main.o
	$(CC) $(LDFLAGS) -o $@ $^

install:
	install -d $(install_opts) -m 755 $(bindir) $(etcdir) $(scriptdir) \
		$(initdir) $(mandir)/man8
	install $(install_opts) -m 755 netplugd $(bindir)
	install $(install_opts) -m 644 etc/netplugd.conf $(etcdir)
	install $(install_opts) -m 755 scripts/netplug $(scriptdir)
	install $(install_opts) -m 755 scripts/rc.netplugd $(initdir)/netplugd
	install $(install_opts) -m 444 man/man8/netplugd.8 $(mandir)/man8

hg_root := $(shell hg root)
tar_root := netplug-$(version)
tar_file := $(hg_root)/$(tar_root).tar.bz2
files := $(shell hg manifest)

tarball: $(tar_file)

$(tar_file): $(files)
	mkdir -p $(hg_root)/$(tar_root)
	echo $(files) | tr ' ' '\n' | \
	  xargs -i cp -a --parents {} $(hg_root)/$(tar_root)
	tar -C $(hg_root) -c -f - $(tar_root) | bzip2 -9 > $(tar_file)
	rm -rf $(hg_root)/$(tar_root)

clean:
	-rm -f netplugd *.o *.tar.bz2

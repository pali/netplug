version := 1.0

prefix ?=
bindir ?= $(prefix)/sbin
etcdir ?= $(prefix)/etc/netplug
initdir ?= $(prefix)/etc/rc.d/init.d
scriptdir ?= $(prefix)/etc/netplug.d

install_opts := -o root -g root

CFLAGS += -Wall -Werror -std=gnu99 -g -DNP_ETC_DIR='"$(etcdir)"' \
	-DNP_SCRIPT_DIR='"$(scriptdir)"'

netplugd: config.o netlink.o lib.o if_info.o main.o
	$(CC) -o $@ $^

install:
	install -d $(install_opts) -m 755 $(bindir) $(etcdir) $(scriptdir) \
		$(initdir)
	install $(install_opts) -m 755 netplugd $(bindir)
	install -C $(install_opts) -m 644 etc/netplugd.conf $(etcdir)
	install -C $(install_opts) -m 755 scripts/netplug $(scriptdir)
	install $(install_opts) -m 755 scripts/rc.netplugd $(initdir)/netplugd
	/sbin/chkconfig --add netplugd

bk_root := $(shell bk root)
tar_root := netplug-$(version)
tar_file := $(bk_root)/$(tar_root).tar.bz2

tarball: $(tar_file)

$(tar_file):
	bk export -tplain $(bk_root)/$(tar_root)
	tar -C $(bk_root) -c -f - $(tar_root) | bzip2 -9 > $(tar_file)
	rm -rf $(bk_root)/$(tar_root)

rpm: $(tar_file)
	rpmbuild -ta $(tar_file)

clean:
	-rm -f netplugd *.o *.tar.bz2

prefix ?= /
bindir ?= $(prefix)/sbin
etcdir ?= $(prefix)/etc/netplug

CFLAGS += -Wall -Werror -std=gnu99 -g -DNP_ETC_DIR="$(etcdir)"

netplug: config.o netlink.o lib.o if_info.o main.o
	$(CC) -o $@ $^

clean:
	-rm -f netplug *.o

install: $(bindir)/netplug $(etcdir)/netplug.conf
	@:

$(bindir)/netplug: netplug
	install -o root -g root -m 755 $^ $@

$(etcdir)/netplug.conf: $(etcdir)
	echo 'eth*' > $@
	chown root.root $@
	chmod 644 $@

$(etcdir):
	install -d -o root -g root -m 755 $@

CFLAGS += -Wall -Werror -std=gnu99 -g -O2

netplug: netlink.o lib.o if_info.o main.o
	$(CC) -o $@ $^

clean:
	-rm -f netplug *.o

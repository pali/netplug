CFLAGS += -Wall -Werror -std=gnu99 -g -O2

netplug: netplug.o
	$(CC) -o $@ $<

clean:
	-rm -f netplug *.o

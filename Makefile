CFLAGS += -Wall -Werror -std=gnu99 -g

netplug: netplug.o
	$(CC) -o $@ $<

clean:
	-rm -f netplug *.o

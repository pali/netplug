CFLAGS += -Wall -Werror -std=gnu99

netplug: netplug.o
	$(CC) -o $@ $<

clean:
	-rm -f netplug *.o

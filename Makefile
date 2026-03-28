CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

.PHONY: all clean

all: sender receiver

sender: sender.c protocol.h
	$(CC) $(CFLAGS) -o $@ sender.c

receiver: receiver.c protocol.h
	$(CC) $(CFLAGS) -o $@ receiver.c

clean:
	rm -f sender receiver

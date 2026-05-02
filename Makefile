CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -pthread
LDFLAGS = -lssl -lcrypto -lncursesw -pthread

.PHONY: all clean

SRCS = chat.c sender.c receiver.c crypto.c ui.c

all: chat

chat: $(SRCS) sender.h receiver.h crypto.h ui.h protocol.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f chat

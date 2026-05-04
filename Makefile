CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O3 -pthread
# CFLAGS  = -Wall -Wextra -std=c11 -O0 -pthread -g
LDFLAGS = -lssl -lcrypto -lncursesw -pthread

.PHONY: all clean

SRCS = chat.c sender.c receiver.c crypto.c ui.c

all: chat

chat: $(SRCS) sender.h receiver.h crypto.h ui.h protocol.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f chat

CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS = -lssl -lcrypto -lreadline
.PHONY: all clean

all: chat

chat: chat.c sender.c receiver.c crypto.c
	$(CC) $(CFLAGS) -o $@ chat.c sender.c receiver.c crypto.c $(LDFLAGS)

clean:
	rm -f chat sender receiver
CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

.PHONY: all clean

# all: chat sender receiver
all: chat

chat: chat.c sender.c receiver.c
	$(CC) $(CFLAGS) -o $@ chat.c sender.c receiver.c

# sender: sender.c protocol.h sender.h
# 	$(CC) $(CFLAGS) -o $@ sender.c

# receiver: receiver.c protocol.h receiver.h
# 	$(CC) $(CFLAGS) -o $@ receiver.c

clean:
	rm -f chat sender receiver

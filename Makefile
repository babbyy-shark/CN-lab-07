CC=gcc
CFLAGS=-Wall -Wextra -g

all: server client

server: server.c common.c db.c handler.c protocol.h
	$(CC) $(CFLAGS) -o server server.c common.c db.c handler.c

client: client.c common.c protocol.h
	$(CC) $(CFLAGS) -o client client.c common.c

clean:
	rm -f server client
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lzmq -lpthread

all: server client

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client

run_server:
	./server

run_client:
	./client user1
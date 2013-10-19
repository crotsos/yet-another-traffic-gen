CC_OPTS = -Wall -L/usr/lib/ -g -I./ 

all: server client

clean:
	rm -f *.o server client

server: server.c debug.c
	$(CC) $(CC_OPTS) -o $@ server.c debug.c -lev

client: client.c debug.c
	$(CC) $(CC_OPTS) -o $@ client.c debug.c -lev

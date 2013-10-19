CC_OPTS = -Wall -L/usr/lib/ -g -I./ 

all: server client

clean:
	rm -f *.o server client

server: server.c
	$(CC) $(CC_OPTS) -o $@ $< -lev

client: client.c
	$(CC) $(CC_OPTS) -o $@ $< -lev

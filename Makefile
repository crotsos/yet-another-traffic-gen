CC_OPTS = -Wall -L/usr/lib/ -g -I./ `pkg-config --cflags libev` 
CC_LIBS = -lev -lgsl -lgslcblas -lm -lconfig


all: server client

clean:
	rm -f *.o server client

server: server.c debug.c config.c debug.h traff_gen.h
	$(CC) $(CC_OPTS) -o $@ server.c debug.c config.c $(CC_LIBS)

client: client.c debug.c config.c debug.h traff_gen.h
	$(CC) $(CC_OPTS) -o $@ client.c debug.c config.c $(CC_LIBS) 

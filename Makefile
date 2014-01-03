CC_OPTS = -Wall -L/usr/lib/ -g -I./ `pkg-config --cflags libev` 
CC_LIBS = -lev -lgsl -lgslcblas -lm -lconfig


all: server client

clean:
	rm -f *.o server client

server: server.c debug.c debug.h traff_gen.h tpl.c tpl.h util.c 
	$(CC) $(CC_OPTS) -g -o $@ util.c server.c debug.c tpl.c $(CC_LIBS)

client: client.c debug.c debug.h traff_gen.h tpl.c tpl.h util.c
	$(CC) $(CC_OPTS) -g -o $@ util.c client.c debug.c tpl.c $(CC_LIBS) 

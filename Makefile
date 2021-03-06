CC_OPTS = -Wall -L/usr/lib/ -g -I./ `pkg-config --cflags libev` -DHTTP_PARSER_STRICT=0 
CC_LIBS = -lev -lgsl -lgslcblas -lm -lconfig 

all: server client

clean:
	rm -f *.o server client

server: server.c debug.c debug.h traff_gen.h tpl.c tpl.h util.c http_parser.c http_parser.h 
	$(CC) $(CC_OPTS) -g -o $@ util.c server.c debug.c tpl.c http_parser.c $(CC_LIBS)

client: client.c debug.c debug.h traff_gen.h tpl.c tpl.h util.c http_parser.c http_parser.h
	$(CC) $(CC_OPTS) -g -o $@ util.c client.c debug.c tpl.c http_parser.c $(CC_LIBS) 

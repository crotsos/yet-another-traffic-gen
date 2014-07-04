CC_OPTS = -Wall -L/usr/lib/ -g -I./ -DHTTP_PARSER_STRICT=0 
CC_LIBS = -lev -lgsl -lgslcblas -lm -lconfig 

TRAFF_GEN_FILES = debug.c debug.h traff_gen.h tpl.c tpl.h util.c http_parser.c http_parser.h udp_util.c udp_util.h tcp_util.c tcp_util.h ctrl_server.c 

all: server client

clean:
	rm -f *.o server client

server: server.c $(TRAFF_GEN_FILES) 
	$(CC) $(CC_OPTS) -g -o $@ util.c server.c debug.c tpl.c http_parser.c udp_util.c tcp_util.c $(CC_LIBS)

client: client.c $(TRAFF_GEN_FILES)
	$(CC) $(CC_OPTS) -g -o $@ util.c client.c debug.c tpl.c http_parser.c udp_util.c tcp_util.c ctrl_server.c $(CC_LIBS) 

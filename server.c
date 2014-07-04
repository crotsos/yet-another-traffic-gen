/*
 * =====================================================================================
 *
 *       Filename:  server.c
 *
 *    Description:  simple libev server. receives a request of bytes and
 *    transits it over the socket. 
 *
 *        Version:  1.0
 *        Created:  18/10/13 12:43:43
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  CHARALAMPOS ROTSOS (cr409@cl.cam.ac.uk), 
 *   Organization: Univeristy of Cambridge, Computer lab
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <ev.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "tpl.h"

#include "debug.h"
#include "traff_gen.h"

static void
stats_cb (struct ev_loop *loop, struct ev_timer *t, int revents) {
  struct timeval st;
  struct traffic_model *model = (struct traffic_model *)t->data;
//  debug_enter("starts_cb");
  gettimeofday(&st, NULL);
  LOG("stat:%ld.%06ld:%lu:%lu:%u", st.tv_sec, st.tv_usec, 
      model->serv.tcp_period_bytes, model->serv.period_finished, model->serv.conns);
  model->serv.tcp_period_bytes = 0;
  model->serv.period_finished = 0;
}

int main(int argc, char **argv) {
  struct ev_loop *loop = ev_default_loop( EVBACKEND_EPOLL | EVFLAG_NOENV);
  int sd;
  struct sockaddr_in addr;
  struct ev_io w_tcp_accept, w_udp_accept;
  struct ev_timer *timer;
  int optval = 1;
  struct traffic_model *t;

  if (loop == NULL) {
    perror("ev_default_loop");
    exit(1);
  }

  t = (struct traffic_model *)malloc(sizeof(struct traffic_model));
  bzero(t, sizeof(struct traffic_model));

  // disable annoying signal 
  signal(SIGPIPE, SIG_IGN);
  

   // load the configuration
  if (argc > 1)
    init_traffic_model(t, argv[1]);
  else
    init_traffic_model(t, "server.cfg");

  // init my logging 
  cl_debug_init(t->logfile);

 // Create server socket
  if( (sd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(t->port);
  addr.sin_addr.s_addr = inet_addr(t->host);

  // Bind socket to address
  if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  // set SO_REUSEADDR on a socket to true (1):
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

  // Start listening on the socket
  if (listen(sd, 1024) < 0) {
    perror("listen error");
    return -1;
  }
 
  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_tcp_accept, tcp_accept_cb, sd, EV_READ);
  w_tcp_accept.data = t;
  ev_io_start(loop, &w_tcp_accept);

  // Create udp server socket
  if( (sd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(t->port);
  addr.sin_addr.s_addr = INADDR_ANY;

  // Bind socket to address
  if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  // set SO_REUSEADDR on a socket to true (1):
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
 
  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_udp_accept, udp_accept_cb, sd, EV_READ);
  w_udp_accept.data = t;
  ev_io_start(loop, &w_udp_accept);

  // Initialize and start a timer watcher to 
  timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
  ev_timer_init(timer, stats_cb, 1.0, 1.0);
  timer->data = t;
  ev_timer_start(loop, timer);

  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  return 0;
}

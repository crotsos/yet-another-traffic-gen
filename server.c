/*
 * =====================================================================================
 *
 *       Filename:  server.c
 *
 *    Description:  isimple libev server. receives a request of bytes and
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
#include <netinet/in.h>
#include <ev.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "debug.h"
#include "traff_gen.h"

#define BUFFER_SIZE 6400

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void stats_cb (struct ev_loop *, struct ev_timer *, int); 

char buffer[BUFFER_SIZE];
const char *kOurProductName = "server";

struct server_stats serv;
struct traffic_model *t;

double 
time_diff (struct timeval *start, struct timeval *end) {
  return ( (double)end->tv_sec - (double)start->tv_sec + 
      (((double)end->tv_usec - (double)start->tv_usec)/(double)1000000)); }


int 
main(int argc, char **argv) {
  struct ev_loop *loop = ev_default_loop(0);
  int sd;
  struct sockaddr_in addr;
  struct ev_io w_accept;
  struct ev_timer *timer;

  //init struct 
  serv.tot_bytes = 0;
  serv.tot_conn = 0;
  serv.period_bytes = 0;
  serv.conns = 0;
  serv.period_finished = 0;

  // disable annoying signal 
  signal(SIGPIPE, SIG_IGN);
  
  t = (struct traffic_model *)malloc(sizeof(struct traffic_model));
  bzero(t, sizeof(struct traffic_model));
 
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
  int optval = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
 
  // Start listing on the socket
  if (listen(sd, 100) < 0) {
    perror("listen error");
    return -1;
  }

  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_accept, accept_cb, sd, EV_READ);
  ev_io_start(loop, &w_accept);

  // Initialize and start a timer watcher to 
  timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
  ev_timer_init(timer, stats_cb, 1.0, 1.0);
  ev_timer_start(loop, timer);

  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  return 0;
}

/* Accept client requests */
void 
accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_sd;
  struct flow_stats* fl;
  struct ev_io *w_client;

  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  // Accept client request
  client_sd = accept(w->fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_sd < 0) {
    perror("accept error");
    return;
  }
  serv.conns++;
  serv.tot_conn++;

  // Initialize and start watcher to read client requests
  w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w_client, read_cb, client_sd, EV_READ);
  fl = malloc(sizeof(struct flow_stats));
  bzero(fl, sizeof(struct flow_stats));
  gettimeofday(&fl->st, NULL);
  fl->id = serv.tot_conn;
  w_client->data = (void *)fl;
  ev_io_start(loop, w_client);
}

/* Read client message */
void read_cb(struct ev_loop *loop, struct ev_io *w, int revents){
  ssize_t read, rcv;
  uint64_t req_data;
  struct flow_stats* fl = (struct flow_stats *)w->data;

  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  if (EV_READ & revents) {
    // Receive message from client socket
    read = recv(w->fd, &req_data, sizeof(req_data), 0);
    if(read <= 0) {
      // Stop and free watchet if client socket is closing
      close(w->fd);
      ev_io_stop(loop,w);
      free(w);
      free(fl);
      serv.conns--;
      serv.period_finished++;
      LOG("- %ld.%06ld:%d:0.000000:0", fl->end.tv_sec, fl->end.tv_usec, 
          fl->id);
    }
    else if (read == sizeof(req_data)) {
      fl->request = req_data;
      LOG("+ %ld.%06ld:%u:%lu", fl->st.tv_sec, fl->st.tv_usec, 
          fl->id, fl->request);
      ev_io_stop(loop,w);
      ev_io_set(w, w->fd, EV_WRITE);
      ev_io_start(loop,w);
    }
  }

  if (EV_WRITE & revents) {
    // Receive message from client socket

    read = BUFFER_SIZE;
    if ((fl->request - fl->send) < BUFFER_SIZE) 
      read = fl->request - fl->send;

    rcv = send(w->fd, buffer, read, 0);
    if((rcv <= 0) || ((fl->send = fl->send + rcv) >= fl->request)) {
      // Stop and free watchet if client socket is closing
      ev_io_stop(loop,w);
      free(w);
      close(w->fd);
      serv.conns--;
      serv.period_finished++;
      gettimeofday(&fl->end, NULL);
      LOG("- %ld.%06ld:%d:%.06f:%lu",  
          fl->end.tv_sec, fl->end.tv_usec, fl->id, 
          time_diff(&fl->st, &fl->end), fl->send);
      free(fl);
    } else {
      serv.tot_bytes += rcv;
      serv.period_bytes += rcv;
    }
  }
}

static void
stats_cb (struct ev_loop *loop, struct ev_timer *t, int revents) {
  struct timeval st;
//  debug_enter("starts_cb");
  gettimeofday(&st, NULL);
 LOG("stat:%ld.%06ld:%lu:%lu:%u", 
     st.tv_sec, st.tv_usec, serv.period_bytes, serv.period_finished, 
     serv.conns);
 serv.period_bytes = 0;
 serv.period_finished = 0;
}

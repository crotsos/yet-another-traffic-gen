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

#include "traff_gen.h"

#define BUFFER_SIZE 6400

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void stats_cb (struct ev_loop *, struct ev_timer *, int); 

char buffer[BUFFER_SIZE];

struct server_stats serv;

float 
time_diff (struct timeval *start, struct timeval *end) {
  return ( (float)end->tv_sec - (float)start->tv_sec + 
      ((float)(end->tv_usec - start->tv_usec)/(float)1000000)); }


int 
main() {
  struct ev_loop *loop = ev_default_loop(0);
  int sd;
  struct sockaddr_in addr;
  struct ev_io w_accept;
  struct ev_timer *t;

  //init struct 
  serv.tot_bytes = 0;
  serv.tot_conn = 0;
  serv.period_bytes = 0;
  serv.conns = 0;
  serv.period_finished = 0;

  signal(SIGPIPE, SIG_IGN);
  // Create server socket
  if( (sd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT_NO);
  addr.sin_addr.s_addr = INADDR_ANY;

  // Bind socket to address
  if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  // set SO_REUSEADDR on a socket to true (1):
  int optval = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
 
  // Start listing on the socket
  if (listen(sd, 2) < 0) {
    perror("listen error");
    return -1;
  }

  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_accept, accept_cb, sd, EV_READ);
  ev_io_start(loop, &w_accept);

  // Initialize and start a timer watcher to 
  t = (struct ev_timer*) malloc (sizeof(struct ev_timer));
  ev_timer_init(t, stats_cb, 1.0, 1.0);
  ev_timer_start(loop, t);

  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  printf("evio loop returned\n");
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
  printf("+ (%u client(s))\n", serv.conns);

  // Initialize and start watcher to read client requests
  w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w_client, read_cb, client_sd, EV_READ);
  fl = malloc(sizeof(struct flow_stats));
  bzero(fl, sizeof(struct flow_stats));
  gettimeofday(&fl->st, NULL);
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
      serv.tot_conn++;
      printf("- send 0 bytes (%u client(s))\n", serv.conns);
      return;
    }
    else if (read == sizeof(req_data)) {
      fl->request = req_data;
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
    if(rcv <= 0) {
      // Stop and free watchet if client socket is closing
      ev_io_stop(loop,w);
      free(w);
      close(w->fd);
      serv.conns--;
      serv.period_finished++;
      serv.tot_conn++;
      gettimeofday(&fl->end, NULL); 
      printf("- send %lu bytes %f secs (%u client(s))\n", fl->send, 
          time_diff(&fl->st, &fl->end), serv.conns);
    } else {
      fl->send += rcv;
      serv.tot_bytes += rcv;
      serv.period_bytes += rcv;
      if (fl->send >= fl->request) {
        close(w->fd);
        ev_io_stop(loop,w);
        gettimeofday(&fl->end, NULL); 
        printf("- send %lu bytes in %f secs (%u client(s))\n", fl->send, 
            time_diff(&fl->st, &fl->end), serv.conns);
        free(w);
        free(fl);
         serv.conns--;
        serv.period_finished++;
        serv.tot_conn++;
      }
      return;
    }
  }
}

static void
stats_cb (struct ev_loop *loop, struct ev_timer *t, int revents) {
  struct timeval st;
  gettimeofday(&st, NULL);
 printf("%u.%06u: rate %lu bytes/sec, completed %lu, remaining %u\n", 
     st.tv_sec, st.tv_usec, serv.period_bytes, serv.period_finished, 
     serv.conns);
 serv.period_bytes = 0;
 serv.period_finished = 0;
}

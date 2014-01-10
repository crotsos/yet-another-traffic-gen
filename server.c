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

#define BUFFER_SIZE 6400

void tcp_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void udp_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void udp_write_cb (struct ev_loop *, struct ev_timer *, int); 
static void stats_cb (struct ev_loop *, struct ev_timer *, int); 

char buffer[BUFFER_SIZE];
const char *kOurProductName = "server";

struct server_stats serv;
struct traffic_model *t;


int 
main(int argc, char **argv) {
  struct ev_loop *loop = ev_default_loop( EVBACKEND_KQUEUE | EVFLAG_NOENV);
  int sd;
  struct sockaddr_in addr;
  struct ev_io w_tcp_accept, w_udp_accept;
  struct ev_timer *timer;
  int optval = 1;

  if (loop == NULL) {
    perror("ev_default_loop");
    exit(1);
  }

  //init struct 
  bzero(&serv, sizeof(struct server_stats));

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
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

  // Start listening on the socket
  if (listen(sd, 1024) < 0) {
    perror("listen error");
    return -1;
  }
 
  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_tcp_accept, tcp_accept_cb, sd, EV_READ);
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
  ev_io_start(loop, &w_udp_accept);

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
tcp_accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_sd;
  struct tcp_flow_stats* fl;
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
  serv.tcp_tot_conn++;
  int flags = fcntl(client_sd, F_GETFL, 0);
  fcntl(client_sd, F_SETFL, flags | O_NONBLOCK);

  // Initialize and start watcher to read client requests
  w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w_client, read_cb, client_sd, EV_READ);
  fl = malloc(sizeof(struct tcp_flow_stats));
  bzero(fl, sizeof(struct tcp_flow_stats));
  gettimeofday(&fl->st, NULL);
  fl->id = serv.tcp_tot_conn;
  w_client->data = (void *)fl;
  ev_io_start(loop, w_client);
}

int udp_flow_count = 0;
/* Accept client requests */
void 
udp_accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
  struct sockaddr_in client_addr;
  socklen_t fromlen = sizeof(struct sockaddr_in);
  ssize_t len;
  struct ev_timer *timer;
  tpl_node *tn;
  struct udp_request req;
  struct udp_flow *f;

  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  // Accept client request
  len = recvfrom(w->fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &fromlen);
  if (len < 0) {
    perror("accept error");
    return;
  }

  tn = tpl_map("S($(uff)$(uff)$(uff))", &req);
  if (tpl_load(tn, TPL_MEM, buffer, len) != 0 ) {
    printf("failed to parse tpl\n");
    return;
  }
  tpl_unpack(tn, 0);
  tpl_free(tn);

  printf("flow arrival %s\n", print_model(&req.request_delay));
  f = (struct udp_flow *)malloc(sizeof(struct udp_flow));
  udp_init_flow(&req, ++udp_flow_count, w->fd, client_addr, f);

  // Initialize and start a timer watcher to 
  timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
  timer->data = f;
  ev_timer_init(timer, udp_write_cb, f->request_delay[f->curr_request], 0.0);
  ev_timer_start(loop, timer);


//  w_client->data = (void *)fl;
//  ev_io_start(loop, w_client);
}


/* Read client message */
void 
read_cb(struct ev_loop *loop, struct ev_io *w, int revents){
  ssize_t read, rcv;
  uint64_t req_data;
  struct tcp_flow_stats* fl = (struct tcp_flow_stats *)w->data;

  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  if (EV_READ & revents) {
    // Receive message from client socket
    read = recv(w->fd, &req_data, sizeof(req_data), 0);
    if(read < sizeof(req_data)) {
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
      LOG("- %ld.%.06ld:%ld.%.06ld:%d:%.06f:%lu",   
          fl->st.tv_sec, fl->st.tv_usec,  
          fl->end.tv_sec, fl->end.tv_usec, fl->id, 
          time_diff(&fl->st, &fl->end), fl->send);
      free(fl);
    } else {
      serv.tcp_tot_bytes += rcv;
      serv.tcp_period_bytes += rcv;
    }
  }
}

static void
stats_cb (struct ev_loop *loop, struct ev_timer *t, int revents) {
  struct timeval st;
//  debug_enter("starts_cb");
  gettimeofday(&st, NULL);
 LOG("stat:%ld.%06ld:%lu:%lu:%u", 
     st.tv_sec, st.tv_usec, serv.tcp_period_bytes, serv.period_finished, 
     serv.conns);
 serv.tcp_period_bytes = 0;
 serv.period_finished = 0;
}

static void 
udp_write_cb (struct ev_loop *loop, struct ev_timer *e, int t) {
  struct udp_flow *f = e->data;
  struct pkt_header *hdr = (struct pkt_header *) buffer;

  hdr->flow_id = f->id; 
  hdr->pkt_id = f->curr_request;
  gettimeofday(&hdr->send, NULL);

  printf("send to %s:%d\n",inet_ntoa(f->addr.sin_addr), ntohs(f->addr.sin_port) );

  int len = sendto(f->fd, buffer, f->size[f->curr_request], 0, 
      (struct sockaddr *)&f->addr, sizeof(struct sockaddr_in));

  if (len < 0) {
    perror("sendto");
  }
  printf("sending %d bytes\n", len);

  f->curr_request++;
  if (f->curr_request < f->requests) {
    e->repeat = f->request_delay[f->curr_request];
  } else {
    printf("finished udp\n");
    e->repeat = 0;
    free(e->data);
  }
  ev_timer_again(loop, e);
  
}

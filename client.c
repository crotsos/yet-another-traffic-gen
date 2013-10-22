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
#include <sys/types.h>
#include <sys/socket.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <arpa/inet.h>

#include <netdb.h>

#include "traff_gen.h"
#include "debug.h"

#define BUFFER_SIZE 3000000
#define WINDOW_SIZE 32000

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void flow_cb (struct ev_loop *, struct ev_timer *, int); 
static void request_cb (struct ev_loop *, struct ev_timer *, int); 

gsl_rng * r;
struct traffic_model *t;
const char *kOurProductName = "client";

uint32_t flow_count = 0;

int
flow_request (struct ev_loop *loop, uint16_t port, uint64_t len, struct flow *f) {
  int sd;
  struct sockaddr_in addr;
  struct ev_io *w;
  ssize_t write;

  // Create server socket
  if( (sd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  
//  printf("dst host %s:%d\n", t->host, t->port);
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(t->port);
  addr.sin_addr.s_addr = inet_addr(t->host);

  // Bind socket to address
  if (connect(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  write = send(sd, &len, sizeof(len), 0);
  if (write < sizeof(len)) {
    perror("send request failed");
    return -1;
  }
  // Initialize and start a watcher to accepts client requests
  w = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w, read_cb, sd, EV_READ);
  w->data = f;
  ev_io_start(loop, w);
  return 0;
}

static void
get_sample(struct model *m, double *r, int len) {
  int ix;
  for (ix=0; ix < len; ix ++) 
    switch(m->type) {
      case CONSTANT:
        r[ix] = m->mean;
        break;
      case EXPONENTIAL:
        r[ix] = gsl_ran_exponential((const gsl_rng *) r, m->mean);
        break;
      case PARETO:
        r[ix] = gsl_ran_pareto((const gsl_rng*)r, m->mean, m->alpha);
        break;
      default:
        printf("Invalid traffic model\n");
        exit(1);
    }
}

void 
init_flow(struct flow *f) {

  // define how many requests we want
  f->curr_request = 0;
  get_sample(&t->request_num, &f->requests, 1);

  // define for reqest inter request delay and size
  f->size = (double *)malloc(f->requests * sizeof(double));
  f->request_delay = (double *)malloc(f->requests * sizeof(double));
  get_sample(&t->request_size, f->size, f->requests);
  get_sample(&t->request_delay, f->request_delay, f->requests);
  f->id = (++flow_count);
}

int 
main(int argc, char **argv) {
  const gsl_rng_type * T; 
  struct ev_loop *loop = ev_default_loop(0);
  int i;
  struct ev_timer *timer;
  double delay;

  //ifnore sigpipe signal
  signal(SIGPIPE, SIG_IGN);

  t = (struct traffic_model *)malloc(sizeof(struct traffic_model));
  bzero(t, sizeof(struct traffic_model));
 
  if (argc > 1)
    init_traffic_model(t, argv[1]);
  else
    init_traffic_model(t, "client.cfg");

  // init my logging 
  cl_debug_init(t->logfile);

  // initiliaze the random number generator
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);
  gsl_rng_set(r, (unsigned long int)t->seed);


  // start a request
  if (t->mode == INDEPENDENT) 
    t->flows = 1;
  for (i = 0; i < t->flows; i++) {
      // in pipeline mode we setup a series of well defined flows who run 
      // in parallel.
      get_sample(&t->flow_arrival, &delay, 1);
      timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      ev_timer_init(timer, flow_cb, delay, 0.0);
      printf ("got a delay of %f, (mean %f)\n", delay, t->flow_arrival.mean);
      ev_timer_start(loop, timer);
  }
  
  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  printf("evio loop returned\n");
  return 0;
}

/* Accept client requests */
void 
read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  ssize_t rcv;
  struct timeval tv;
  char buffer[WINDOW_SIZE];
  struct flow *f = w->data;
  struct ev_timer *request_timer;
  double delay;

  if(EV_ERROR & revents) {
    perror("got invalid event");
  }

  if (EV_READ & revents) {
    // Receive message from client socket
    rcv = recv(w->fd, buffer, WINDOW_SIZE, 0);
    if(rcv <= 0) {
      gettimeofday(&tv, NULL);
      // Stop and free watcher if client socket is closing
      close(w->fd);
      ev_io_stop(l,w);
      free(w);
      LOG("-request:%ld.%06ld:%u:%u",  
          tv.tv_sec, tv.tv_usec, f->id, f->curr_request);

      f->curr_request++;
      if(f->curr_request >= f->requests) {
        // schedule next request
        LOG("-flow:%ld.%06ld:%u",  
            tv.tv_sec, tv.tv_usec, f->id);

        free(f->size);
        free(f->request_delay);
        free(f);
        if (t->mode == PIPELINE) {
          request_timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
          get_sample(&t->flow_arrival, &delay, 1);
          ev_timer_init(request_timer, flow_cb, delay, 0.0);
          ev_timer_start(l, request_timer);
        }
      } else {
        // schedule next request since we still have requests 
       if (f->request_delay[f->curr_request] > 0) {
         request_timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
         request_timer->data = f;
         ev_timer_init(request_timer, request_cb, 
             f->request_delay[f->curr_request], 0.0);
         ev_timer_start(l, request_timer);
        } else { 
          LOG("+request:%ld.%06ld:%u:%u:%f:%f",  
              tv.tv_sec, tv.tv_usec, f->id, f->curr_request, f->size[f->curr_request], 
              f->request_delay[f->curr_request]);

          flow_request(l, PORT_NO, f->size[f->curr_request], f);
        }
      }
    }
  }
}

static void 
flow_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct timeval tv;
  struct flow *f;
  double delay;

  gettimeofday(&tv, NULL);

  // create a new flow definition
  f = (struct flow *)malloc(sizeof(struct flow));
  init_flow(f);
  LOG("+flow:%ld.%06ld:%u:%f",  
      tv.tv_sec, tv.tv_usec, f->id, f->requests);

  LOG("+request:%ld.%06ld:%u:%u:%f:%f",  
      tv.tv_sec, tv.tv_usec, f->id, f->curr_request, f->size[f->curr_request], 
      f->request_delay[f->curr_request]);
  flow_request(l, PORT_NO, f->size[f->curr_request], f);

  if (t->mode == INDEPENDENT) {
      get_sample(&t->flow_arrival, &delay, 1);
      timer->repeat = delay;
      ev_timer_again(l, timer);
  } else {
    free(timer);
  }
  return;
}

static void request_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct timeval tv;
  struct flow *f = timer->data;

  gettimeofday(&tv, NULL);
  LOG("+request:%ld.%06ld:%u:%u:%f:%f",  
      tv.tv_sec, tv.tv_usec, f->id, f->curr_request, f->size[f->curr_request], 
      f->request_delay[f->curr_request]);

    flow_request(l, PORT_NO, f->size[f->curr_request], f);
  free(timer);

  return;
}


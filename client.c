/*
 * =====================================================================================
 *
 *       Filename:  server.c
 *
 *    Description:  a simple libev server. receives a request of bytes and
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
#include "traff_gen.h"
#include "debug.h"
#include "tpl.h"
#include "udp_util.h"
#include "tcp_util.h"

static void end_cb (struct ev_loop *, struct ev_timer *, int); 

static void 
end_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct traffic_model *t = timer->data;
  printf("finishing experiment...\nwaiting for %d events\n", 
      t->requests_running);
  if (!t->requests_running) exit(0); 
  t->running = 0;
}


int 
main(int argc, char **argv) {
//  struct ev_loop *loop = ev_default_loop(EVBACKEND_EPOLL | EVFLAG_NOENV); 
  struct ev_loop *loop = ev_default_loop(0); 
  int i;
  struct ev_timer *timer, end;
  double delay;
  struct traffic_model *t;

//    struct rlimit limit;
//    getrlimit(RLIMIT_NOFILE,&limit);
//    printf("cur: %d, max: %d\n",limit.rlim_cur,limit.rlim_max);

  //ifnore sigpipe signal
  signal(SIGPIPE, SIG_IGN);

  t = (struct traffic_model *)malloc(sizeof(struct traffic_model));
  bzero(t, sizeof(struct traffic_model));
 
  if (argc > 1)
    init_traffic_model(t, argv[1]);
  else
    init_traffic_model(t, "client.cfg");

  // init my logging
  unlink(t->logfile);
  cl_debug_init(t->logfile);

  // init random generator 
  init_rand(t); 

  // start a request
  if (t->mode == PACKET) {
    udp_flow_request (loop, t->port, t); 
  } else if( t->mode == INDEPENDENT || t->mode == PIPELINE ) {
    if (t->mode == INDEPENDENT) 
      t->flows = 1;
    for (i = 0; i < t->flows; i++) {
      // in pipeline mode we setup a series of well defined flows who run 
      // in parallel.
      get_sample(&t->flow_arrival, &delay, 1);
      timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      timer->data = t;
      ev_timer_init(timer, tcp_flow_cb, delay, 0.0);
      printf ("got a delay of %f, (mean %f)\n", delay, t->flow_arrival.mean);
      ev_timer_start(loop, timer);
    }
  }
  
  ev_timer_init(&end, end_cb, t->duration, 0.0);
  end.data = t;
  ev_timer_start(loop, &end);

  start_ctrl_service(loop, t);
  
  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  printf("evio loop returned\n");
  return 0;
}

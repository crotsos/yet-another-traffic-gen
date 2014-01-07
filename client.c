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
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <ev.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#include "traff_gen.h"
#include "debug.h"
#include "tpl.h"

#define BUFFER_SIZE 3000000
#define WINDOW_SIZE 32000

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void udp_read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
static void tcp_flow_cb (struct ev_loop *, struct ev_timer *, int); 
static void request_cb (struct ev_loop *, struct ev_timer *, int); 
static void end_cb (struct ev_loop *, struct ev_timer *, int); 

// static void udp_flow_cb (struct ev_loop *, struct ev_timer *, int); 

struct traffic_model *t;
const char *kOurProductName = "client";

uint32_t requests_running = 0;
int running = 1; 

int
tcp_flow_request (struct ev_loop *loop, uint16_t port, uint64_t len, struct tcp_flow *f) {
  int sd;
  struct sockaddr_in addr;
  struct ev_io *w;

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
  
  int flags = fcntl(sd, F_GETFL, 0);
  fcntl(sd, F_SETFL, flags | O_NONBLOCK);

   // Bind socket to address
  if ((connect(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) && (errno != EINPROGRESS)) {
    perror("bind error");
  }
  
  // Initialize and start a watcher to accepts client requests
  w = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w, read_cb, sd, EV_READ | EV_WRITE);
  w->data = f;
  ev_io_start(loop, w);
  requests_running++;
  return 0;
}

void
init_tcp_request(struct ev_loop *l, struct tcp_flow *f, struct timeval *tv) {
  struct ev_timer *request_timer;
  double delay;
  f->curr_request++;
  if(f->curr_request >= f->requests) {
    // schedule next request
    LOG("-flow:%ld.%06d:%u",  
        tv->tv_sec, tv->tv_usec, f->id);

    //free(f->size);
    //free(f->request_delay);
    //free(f);
    if (t->mode == PIPELINE) {
      request_timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      get_sample(&t->flow_arrival, &delay, 1);
      ev_timer_init(request_timer, tcp_flow_cb, delay, 0.0);
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
      memcpy(&f->start[f->curr_request], &tv, sizeof(struct timeval)); 
      LOG("+request:%ld.%06d:%u:%u:%f:%f",  
          tv->tv_sec, tv->tv_usec, f->id, f->curr_request, f->size[f->curr_request], 
          f->request_delay[f->curr_request]);

      tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f);
    }
  }
}


int
udp_flow_request (struct ev_loop *loop, uint16_t port) {
  int sd, optval=1;
  struct sockaddr_in addr;
  struct ev_io *w;
  struct udp_request req;
  tpl_node *tn;
  size_t len;
  uint8_t *buf;

  // Create server socket
  if( (sd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(20000); //t->port);
  addr.sin_addr.s_addr = INADDR_ANY;

  // Bind socket to address
  if (bind(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
    exit(1);
  }
  
  // set SO_REUSEADDR on a socket to true (1):
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
 
//  printf("dst host %s:%d\n", t->host, t->port);
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(t->port);
  addr.sin_addr.s_addr = inet_addr(t->host);

  int flags = fcntl(sd, F_GETFL, 0);
  fcntl(sd, F_SETFL, flags | O_NONBLOCK);

  memcpy(&req.request_num, &t->request_num, sizeof(struct model));
  memcpy(&req.request_delay, &t->request_delay, sizeof(struct model));
  memcpy(&req.request_size, &t->request_size, sizeof(struct model));

  tn = tpl_map("S($(uff)$(uff)$(uff))", &req);
  tpl_pack(tn, 0);
  if(tpl_dump(tn, TPL_MEM, &buf, &len) < 0) {
    printf("error dumping tpl data\n");
    exit(1);
  }

  printf("sending model using %zu bytes\n", len);
  if (sendto(sd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr)) < len) {
    perror("udp request fail");
    exit(1);
  }

  free(buf);
  tpl_free(tn);

  // Initialize and start a watcher to accepts client requests
  w = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w, udp_read_cb, sd, EV_READ);
//  w->data = f;
  ev_io_start(loop, w);
  requests_running++;
  return 0;
}


int 
main(int argc, char **argv) {
//  struct ev_loop *loop = ev_default_loop(EVBACKEND_EPOLL | EVFLAG_NOENV); 
  struct ev_loop *loop = ev_default_loop(0); 
  int i;
  struct ev_timer *timer, end;
  double delay;

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
    udp_flow_request (loop, t->port); 
  } else if( t->mode == INDEPENDENT || t->mode == PIPELINE ) {
    if (t->mode == INDEPENDENT) 
      t->flows = 1;
    for (i = 0; i < t->flows; i++) {
      // in pipeline mode we setup a series of well defined flows who run 
      // in parallel.
      get_sample(&t->flow_arrival, &delay, 1);
      timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      ev_timer_init(timer, tcp_flow_cb, delay, 0.0);
      printf ("got a delay of %f, (mean %f)\n", delay, t->flow_arrival.mean);
      ev_timer_start(loop, timer);
    }
  }
  
  ev_timer_init(&end, end_cb, t->duration, 0.0);
  ev_timer_start(loop, &end);
  
  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  printf("evio loop returned\n");
  return 0;
}

void 
udp_read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  struct sockaddr_in addr;
  socklen_t size = sizeof(addr);
  ssize_t len = WINDOW_SIZE;
  char buffer[WINDOW_SIZE];
  struct pkt_header *hdr;
  struct timeval rcv;
  
  printf("reading udp data.\n");
  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  len = recvfrom(w->fd, buffer, len, 0, (struct sockaddr *)&addr, &size);
  if (len < 0) {
    perror("recvfrom error:");
    return;
  }
  hdr = (struct pkt_header *) buffer;
  gettimeofday(&rcv, NULL);
  printf("%lu.%06u:%u:%u:%f\n", rcv.tv_sec, rcv.tv_usec, hdr->flow_id, hdr->pkt_id, 
      time_diff(&hdr->send, &rcv));

}

/* Accept client requests */
void 
read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  ssize_t rcv;
  struct timeval tv;
  char buffer[WINDOW_SIZE];
  struct tcp_flow *f = w->data;
  uint64_t len = f->size[f->curr_request];
  ssize_t write;
  regex_t r;
  regmatch_t m[2];

  if(EV_ERROR & revents) {
    perror("got invalid event");
  }

  if ((!f->send_req[f->curr_request]) && (EV_WRITE & revents)) {
    if (f->pages == NULL) {
      write = send(w->fd, &len, sizeof(len), 0);
      if (write < sizeof(len)) {
        perror("send request failed");
        exit(1);
      }
    } else {

      //constrcut http request
      if (t->domain) {
        len = sprintf(buffer, 
            "GET %s HTTP/1.1\r\nUser-Agent: curl/7.30.0\r\nHost: %s\r\nAccept: */*\r\n\r\n", 
            t->urls[f->pages[f->curr_request]], t->domain);
      } else {
        len = printf(buffer, 
            "GET %s HTTP/1.1\r\nUser-Agent: curl/7.30.0\r\nAccept: */*\r\n\r\n", 
            t->urls[f->pages[f->curr_request]]);
      }
      write = send(w->fd, buffer, len, 0);
    }
    f->send_req[f->curr_request] = 1;
    ev_io_stop(l,w);
    ev_io_set(w, w->fd, EV_READ);
    ev_io_start(l,w);
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
      requests_running--;

      LOG("-request:%ld.%06d:%ld.%06d:%u:%u:%f:%f",  
          f->start[f->curr_request].tv_sec,
          f->start[f->curr_request].tv_usec,
	  tv.tv_sec, tv.tv_usec, 
          f->id, f->curr_request, f->size[f->curr_request], 
          f->request_delay[f->curr_request]);
      
      if (!running && !requests_running) exit(0); 
      init_tcp_request(l, f, &tv);

    } else {
//\\([0-9]+\\)
      printf("[%d - %d]: got data\n", f->id, f->curr_request);
      if (f->size[f->curr_request] == 0) {
        if (regcomp(&r, "Content-Length\\: \\([0-9][0-9]*\\)\r\n", 0) != 0)
          perror("regcomp");
        if (regexec(&r, buffer, 2, m, 0) !=0 ) {
          printf("[%d - %d]: Match not found\n", f->id, f->curr_request);
        } else {
          buffer[m[1].rm_eo] = '\0';
          f->size[f->curr_request] = (double)strtol(buffer + m[1].rm_so, NULL, 10);
          printf("[%d - %d]: Match found %f\n", f->id, f->curr_request, f->size[f->curr_request]);
        }
        regfree(&r);
      }

      if (t->urls && f->size[f->curr_request] > 0 && f->body[f->curr_request] == 0) {
        if (regcomp(&r, "\r\n\r\n", 0) != 0) {
          perror("regcomp");
        }
        if (regexec(&r, buffer, 2, m, 0) !=0 ) {
          printf("[%d - %d]: newline not found\n", f->id, f->curr_request);
        } else {
          printf("[%d - %d]: len:%d, found:%d\n", f->id, f->curr_request , rcv, m[0].rm_eo);
          f->recved[f->curr_request] = rcv - m[0].rm_eo;
          printf("[%d - %d]: received %d bytes\n", f->id, f->curr_request,  rcv - m[0].rm_eo);
          f->body[f->curr_request] = 1;
          printf("[%d - %d]: newline found\n", f->id, f->curr_request);
        }
        regfree(&r);

      } else if(!t->urls || (t->urls && f->size[f->curr_request] && f->body[f->curr_request]) ) {
        printf("[%d - %d]: received %d bytes(looking for %f)\n", 
            f->id, f->curr_request, len, f->size[f->curr_request]);
        f->recved[f->curr_request] += (uint32_t)rcv;
      }
      gettimeofday(&tv, NULL);
      // Stop and free watcher if client socket is closing
      if ( f->size[f->curr_request] > 0.0 && 
          f->recved[f->curr_request] >= (uint32_t)f->size[f->curr_request]) {
        printf("[%d - %d]: completed trnasmission %u - %f\n", f->id, 
            f->curr_request, f->recved[f->curr_request], f->size[f->curr_request]);
        ev_io_stop(l,w);
        close(w->fd);
        //free(w);
        requests_running--;
        init_tcp_request(l, f, &tv);
      }
    }
  }
}

static void 
tcp_flow_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct timeval tv;
  struct tcp_flow *f;
  double delay;

  if(!running) return;

  // create a new flow definition
  f = (struct tcp_flow *)malloc(sizeof(struct tcp_flow));
  tcp_init_flow(t, f);
  gettimeofday(&f->start[f->curr_request], NULL);
  LOG("+flow:%ld.%06d:%u:%f",  
      tv.tv_sec, tv.tv_usec, f->id, f->requests);

  LOG("+request:%ld.%06d:%u:%u:%f:%f",  
      f->start[f->curr_request].tv_sec, 
      f->start[f->curr_request].tv_usec, 
      f->id, f->curr_request, f->size[f->curr_request], 
      f->request_delay[f->curr_request]);
  tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f);

  if (t->mode == INDEPENDENT) {
      get_sample(&t->flow_arrival, &delay, 1);
      timer->repeat = delay;
      ev_timer_again(l, timer);
  } else {
    free(timer);
  }
  return;
}

static void 
end_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  printf("finishing experiment...\nwaiting for %d events\n", requests_running);
  running = 0;
}

static void 
request_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct tcp_flow *f = timer->data;

  gettimeofday(&f->start[f->curr_request], NULL);
  LOG("+request:%ld.%06d:%u:%u:%f:%f",  
      f->start[f->curr_request].tv_sec, 
      f->start[f->curr_request].tv_usec, 
      f->id, f->curr_request, f->size[f->curr_request], 
      f->request_delay[f->curr_request]);

    tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f);
  free(timer);

  return;
}


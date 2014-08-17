/*
 * =====================================================================================
 *
 *       Filename:  tcp_util.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  04/07/14 14:13:49
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */
#include "traff_gen.h"

/* ************************************
 *
 * CLIENT CB
 *
 * ************************************/
void tcp_flow_cb (struct ev_loop *l, struct ev_timer *timer,
    int repi) {
  struct timeval tv;
  struct tcp_flow *f;
  double delay;
  struct traffic_model *t = timer->data;
  if(!t->running) return;

  // create a new flow definition
  f = (struct tcp_flow *)malloc(sizeof(struct tcp_flow));
  tcp_init_flow(t, f);
  gettimeofday(&f->start[f->curr_request], NULL);
  LOG("+flow:%ld.%06ld:%u:%f",
      tv.tv_sec, tv.tv_usec, f->id, f->requests);

  LOG("+request:%ld.%06ld:%u:%u:%f:%f",
      f->start[f->curr_request].tv_sec,
      f->start[f->curr_request].tv_usec,
      f->id, f->curr_request, f->size[f->curr_request],
      f->request_delay[f->curr_request]);
  tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f, t);

  if (t->mode == INDEPENDENT) {
      get_sample(&t->flow_arrival, &delay, 1);
      timer->repeat = delay;
      ev_timer_again(l, timer);
  } else {
    free(timer);
  }
  return;
}

void  request_cb (struct ev_loop *l, struct ev_timer *timer, int rep) {
  struct tcp_flow *f = timer->data;
  struct traffic_model *t = f->t;

  gettimeofday(&f->start[f->curr_request], NULL);
  LOG("+request:%ld.%06ld:%u:%u:%f:%f",
      f->start[f->curr_request].tv_sec,
      f->start[f->curr_request].tv_usec,
      f->id, f->curr_request, f->size[f->curr_request],
      f->request_delay[f->curr_request]);

    tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f, t);
  free(timer);
  return;
}

void init_tcp_request(struct ev_loop *l, struct tcp_flow *f,
    struct timeval *tv) {
  struct ev_timer *request_timer;
  double delay;
  struct traffic_model *t = f->t;
  f->curr_request++;
  if(f->curr_request >= f->requests) {
    // schedule next request
    LOG("-flow:%ld.%06ld:%u",
        tv->tv_sec, tv->tv_usec, f->id);
    TAILQ_INSERT_TAIL(&t->stats, f, entry);

    // free(f->size);
    // free(f->request_delay);
    // free(f);
    if (t->mode == PIPELINE) {
      request_timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      get_sample(&t->flow_arrival, &delay, 1);
      ev_timer_init(request_timer, tcp_flow_cb, delay, 0.0);
      request_timer->data = t;
      ev_timer_start(l, request_timer);
    }
  } else {
    // schedule next request since we still have requests
    http_parser_init(&f->parser, HTTP_RESPONSE);
    f->parser.data = f;
    if (f->request_delay[f->curr_request] > 0) {
      request_timer = (struct ev_timer*) malloc (sizeof(struct ev_timer));
      request_timer->data = f;
      ev_timer_init(request_timer, request_cb,
          f->request_delay[f->curr_request], 0.0);
      ev_timer_start(l, request_timer);
    } else {
      memcpy(&f->start[f->curr_request], &tv, sizeof(struct timeval));
      LOG("+request:%ld.%06ld:%u:%u:%f:%f",
          tv->tv_sec, tv->tv_usec, f->id, f->curr_request, f->size[f->curr_request],
          f->request_delay[f->curr_request]);

      tcp_flow_request(l, PORT_NO, f->size[f->curr_request], f, t);
    }
  }
}



int
tcp_flow_request (struct ev_loop *loop, uint16_t port, uint64_t len,
    struct tcp_flow *f, struct traffic_model *t) {
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
  ev_io_init(w, client_read_cb, sd, EV_READ | EV_WRITE);
  w->data = f;

  ev_io_start(loop, w);
  t->requests_running++;
  return 0;
}

int
http_data(http_parser *p, const char *d, size_t len) {
  struct tcp_flow *f = (struct tcp_flow *)p->data;
//  printf("received %lu bytes\n", len);
  f->recved[f->curr_request] += len;
  return 0;
}

/* Accept client requests */
void
client_read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  ssize_t rcv;
  struct timeval tv;
  char buffer[WINDOW_SIZE];
  struct tcp_flow *f = w->data;
  struct traffic_model *t = f->t;
  uint64_t len = f->size[f->curr_request];
  ssize_t write;
//  regex_t r;
//  regmatch_t m[2];

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
            "GET %s HTTP/1.1\r\nUser-Agent: curl/7.30.0\r\nHost: %s\r\nConnection: close\r\nAccept: */*\r\n\r\n",
            t->urls[f->pages[f->curr_request]], t->domain);
      } else {
        len = printf(buffer,
            "GET %s HTTP/1.1\r\nUser-Agent: curl/7.30.0\r\nConnection: close\r\nAccept: */*\r\n\r\n",
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
      gettimeofday(&f->end[f->curr_request], NULL);
      // Stop and free watcher if client socket is closing
      close(w->fd);
      ev_io_stop(l,w);
      free(w);
      t->requests_running--;

      if(f->size[f->curr_request]) {
      LOG("-request:%ld.%06ld:%ld.%06ld:%u:%u:%f:%f",
          f->start[f->curr_request].tv_sec,
          f->start[f->curr_request].tv_usec,
          f->end[f->curr_request].tv_sec, f->end[f->curr_request].tv_usec,
          f->id, f->curr_request, f->size[f->curr_request],
          f->request_delay[f->curr_request]);
      } else {
        LOG("-request:%ld.%06ld:%ld.%06ld:%u:%u:%u:%f:%d",
            f->start[f->curr_request].tv_sec,
            f->start[f->curr_request].tv_usec,
            f->end[f->curr_request].tv_sec, f->end[f->curr_request].tv_usec,
            f->id, f->curr_request, f->recved[f->curr_request],
            f->request_delay[f->curr_request], f->pages[f->curr_request]);
      }

      if (!t->running && !t->requests_running) exit(0);
      init_tcp_request(l, f, &tv);

    } else {
//\\([0-9]+\\)
      if (f->size[f->curr_request] == 0) {
        if(t->debug) printf("[%d - %d]: got %lu data\n", f->id, f->curr_request, rcv);
        http_parser_settings settings = {NULL, NULL, NULL, NULL, NULL, NULL, &http_data, NULL};
        http_parser_execute(&f->parser, &settings, buffer, rcv);
      } else {
        f->recved[f->curr_request] += (uint32_t)rcv;
      }

      // Stop and free watcher if client socket is closing
      if (t->urls != NULL && http_body_is_final(&f->parser ) ) {
        gettimeofday(&f->end[f->curr_request], NULL);
        if(t->debug)
          printf("[%d - %d]: completed trnasmission %u\n",
              f->id, f->curr_request, f->recved[f->curr_request]);
        ev_io_stop(l,w);
        close(w->fd);
        //free(w);
        t->requests_running--;
        LOG("-request:%ld.%06ld:%ld.%06ld:%u:%u:%u:%f:%d",
            f->start[f->curr_request].tv_sec,
            f->start[f->curr_request].tv_usec,
            f->end[f->curr_request].tv_sec, f->end[f->curr_request].tv_usec,
            f->id, f->curr_request, f->recved[f->curr_request],
            f->request_delay[f->curr_request], f->pages[f->curr_request]);

        if (!t->running && !t->requests_running) exit(0);
        init_tcp_request(l, f, &tv);
      }
    }
  }
}

/* ************************************
 *
 * SERVER CB
 *
 * ************************************/
char buffer[BUFFER_SIZE];

/* Read client message */
void
srv_read_cb(struct ev_loop *loop, struct ev_io *w, int revents){
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
      fl->t->serv.conns--;
      fl->t->serv.period_finished++;
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
      fl->t->serv.conns--;
      fl->t->serv.period_finished++;
      gettimeofday(&fl->end, NULL);
      LOG("- %ld.%.06ld:%ld.%.06ld:%d:%.06f:%lu",
          fl->st.tv_sec, fl->st.tv_usec,
          fl->end.tv_sec, fl->end.tv_usec, fl->id,
          time_diff(&fl->st, &fl->end), fl->send);
      free(fl);
    } else {
      fl->t->serv.tcp_tot_bytes += rcv;
      fl->t->serv.tcp_period_bytes += rcv;
    }
  }
}

/* Accept client requests */
void tcp_accept_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_sd;
  struct tcp_flow_stats* fl;
  struct ev_io *w_client;
  struct traffic_model *t = (struct traffic_model *)w->data;

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
  t->serv.conns++;
  t->serv.tcp_tot_conn++;
  int flags = fcntl(client_sd, F_GETFL, 0);
  fcntl(client_sd, F_SETFL, flags | O_NONBLOCK);

  // Initialize and start watcher to read client requests
  w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w_client, srv_read_cb, client_sd, EV_READ);
  fl = malloc(sizeof(struct tcp_flow_stats));
  bzero(fl, sizeof(struct tcp_flow_stats));
  fl->t = t;
  gettimeofday(&fl->st, NULL);
  fl->id = t->serv.tcp_tot_conn;
  w_client->data = (void *)fl;
  ev_io_start(loop, w_client);
}

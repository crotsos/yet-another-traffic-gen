/*
 * =====================================================================================
 *
 *       Filename:  udp_util.c
 *
 *    Description:  <Down>
 *
 *        Version:  1.0
 *        Created:  04/07/14 13:18:32
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "traff_gen.h"
/* *************************************
 *
 * CLIENT CB
 * */
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
  printf("%lu.%06ld:%u:%u:%f\n", rcv.tv_sec, rcv.tv_usec, hdr->flow_id, hdr->pkt_id, 
      time_diff(&hdr->send, &rcv));

}

int
udp_flow_request (struct ev_loop *loop, uint16_t port, struct traffic_model *t) {
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
  t->requests_running++;
  return 0;
}

/* *************************************
 *
 * SERVER CB
 *
 * */
void udp_write_cb (struct ev_loop *loop, struct ev_timer *e, int t) {
  struct udp_flow *f = e->data;
  char buffer[BUFFER_SIZE];
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
  char buffer[BUFFER_SIZE];

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



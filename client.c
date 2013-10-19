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

void read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);

int 
main() {
  struct ev_loop *loop = ev_default_loop(0);
  int sd;
  struct sockaddr_in addr;
  struct ev_io w;
  uint32_t len = BUFFER_SIZE;
  ssize_t write;

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
  if (connect(sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  write = send(sd, &len, sizeof(len), 0);
  if (write < sizeof(len)) {
    perror("send request failed");
    return -1;
  }
  // Initialize and start a watcher to accepts client requests
  ev_io_init(&w_accept, read_cb, sd, EV_READ);
  ev_io_start(loop, &w_accept);

  // Start infinite loop
  while (1) 
    ev_loop(loop, 0);
  printf("evio loop returned\n");
  return 0;
}

/* Accept client requests */
void 
read_cb(struct ev_loop *loop, struct ev_io *w, int revents) {
  ssize_t rcv;
  char buffer[BUFFER_SIZE];

  if(EV_ERROR & revents) {
    perror("got invalid event");
  }

  if (EV_READ & revents) {
    // Receive message from client socket
    rcv = recv(w->fd, buffer, BUFFER_SIZE, 0);
    if(rcv <= 0) {
      // Stop and free watcher if client socket is closing
      close(w->fd);
      ev_io_stop(loop,w);
      perror("peer might closing");
    }
  }
}

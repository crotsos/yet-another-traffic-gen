/*
 * =====================================================================================
 *
 *       Filename:  ctrl_server.c
 *
 *    Description:  A control http server for the client.
 *
 *        Version:  1.0
 *        Created:  04/07/14 11:35:44
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <traff_gen.h>

/**
 * A function to handle new http connections. 
 * TODO: avoid HTTP pipeline.
 * */
int start_service(struct ev_loop *loop, uint16_t port, uint64_t len, 
    struct traffic_model *t) {
 int ctrl_fd, optval = 1;
 struct ev_io *ev_ctl_server;
 struct sockaddr_in addr;

 // Create the control service
  if( (ctrl_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) {
    perror("socket error");
    return -1;
  }

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(t->ctrl_port);
  addr.sin_addr.s_addr = inet_addr(t->host);

  // Bind socket to address
  if (bind(ctrl_fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
    perror("bind error");
  }
  
  // set SO_REUSEADDR on a socket to true (1):
  setsockopt(ctrl_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

  // Start listening on the socket
  if (listen(ctrl_fd, 5) < 0) {
    perror("listen error");
    return -1;
  }

  // Initialize and start a watcher to accepts client requests
  ev_ctl_server = (struct ev_io*) malloc (sizeof(struct ev_io));
//   ev_io_init(ev_ctl_server, ctrl_connect_cb, ctrl_fd, EV_READ | EV_WRITE);
//   w->data = f;

  ev_io_start(loop, ev_ctl_server);

}


/**
 * Read data from a socket.
 * TODO: write a response to the query.
 * */
// TODO write data and close the FD. logically the size of the resposne will be
// small.


/** 
 * Check how to access the global state of the system. 
 * */

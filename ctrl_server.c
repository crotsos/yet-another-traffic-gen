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



int on_url(http_parser *p, const char *at, size_t length) {
  char *tmp = malloc(length + 1);
  strncpy(tmp, at, length);
  tmp[length]='\0';
  printf("got a request for %s (%d)\n", tmp, length);
  return 0;
}

/* Accept client requests */
void ctrl_read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
  ssize_t rcv;
  char buffer[WINDOW_SIZE];
  http_parser parser;
  char *response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nServer: TrafficGen_1.0\r\nContent-Length: 5\r\n\r\naaaaa";

  http_parser_init(&parser, HTTP_REQUEST);

  if(EV_ERROR & revents) {
    perror("got invalid event");
  }

  if (EV_READ & revents) {
    // Receive message from client socket
    rcv = recv(w->fd, buffer, WINDOW_SIZE, 0);
    if(rcv <= 0) {
      close(w->fd);
      ev_io_stop(l,w);
      free(w);
    } else {
//\\([0-9]+\\)
      http_parser_settings settings = {NULL, on_url, NULL, NULL, NULL, NULL, NULL, NULL};
      http_parser_execute(&parser, &settings, buffer, rcv);

      // Assume the request fits in a single packet.
      // Stop and free watcher if client socket is closing
      ev_io_stop(l,w);
      send(w->fd, response, strlen(response), 0);
      close(w->fd);
    }
  }
}


/**
 * Remote control and stats service.
 * */
void ctrl_connect_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  int client_sd;
  struct ev_io *w_client;

  printf("got an http request\n");
  if(EV_ERROR & revents) {
    perror("got invalid event");
    return;
  }

  // Accept client request
  client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);
  if (client_sd < 0) {
    perror("accept error");
    return;
  }
  int flags = fcntl(client_sd, F_GETFL, 0);
  fcntl(client_sd, F_SETFL, flags | O_NONBLOCK);

  // Initialize and start watcher to read client requests
  w_client = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(w_client, ctrl_read_cb, client_sd, EV_READ);
  ev_io_start(loop, w_client);

  return;
}

/**
 * A function to handle new http connections. 
 * TODO: avoid HTTP pipeline.
 * */
int start_ctrl_service(struct ev_loop *loop,  struct traffic_model *t) {
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

  // Start listening on the socket
  if (listen(ctrl_fd, 5) < 0) {
    perror("listen error");
    return -1;
  }  
  // set SO_REUSEADDR on a socket to true (1):
  setsockopt(ctrl_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);


  // Initialize and start a watcher to accepts client requests
  ev_ctl_server = (struct ev_io*) malloc (sizeof(struct ev_io));
  ev_io_init(ev_ctl_server, ctrl_connect_cb, ctrl_fd, EV_READ | EV_WRITE);
//  w->data = f;

  ev_io_start(loop, ev_ctl_server);
  return 0;
}

/**
 * Read data from a socket.
 * TODO: write a response to the query.
 * */
// TODO write data and close the FD. logically the size of the resposne will be
// small.

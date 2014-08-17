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
    p->data = malloc(length + 1);
    strncpy(p->data, at, length);
    ((char *)p->data)[length]='\0';
    printf("got a request for %s (%zu)\n", (char *)p->data, length);
    return 0;
}

inline char *ctrl_generate_response(char *response, const char *data) {
    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nServer: TrafficGen_1.0\r\nContent-Length: %zu\r\n\r\n%s",
            strlen(data), data);
    return response;
}

/* Accept client requests */
void ctrl_read_cb(struct ev_loop *l, struct ev_io *w, int revents) {
    ssize_t rcv;
    char buffer[WINDOW_SIZE];
    http_parser parser;
    struct traffic_model *t = (struct traffic_model *)w->data;

    char data[2048], response[2048];
    char *error_response = "HTTP/1.1 404 OK\r\nContent-Type: application/json\r\nServer: TrafficGen_1.0\r\nContent-Length: 0\r\n\r\n";
    parser.data = NULL;

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
            http_parser_settings settings = {NULL, on_url, NULL, NULL, NULL, NULL, NULL, NULL};
            http_parser_execute(&parser, &settings, buffer, rcv);

            // Assume the request fits in a single packet.
            // Stop and free watcher if client socket is closing

            // start looking up for the appropriate function call
            ev_io_stop(l,w);
            if (strcmp(parser.data, "/model/flow_arrival/mean/get") == 0) {
                sprintf(data, "{status:\"success\", data:{mean:\"%f\"}}", ((struct traffic_model *)w->data)->flow_arrival.mean);
                ctrl_generate_response(response, data);
                send(w->fd, response, strlen(response), 0);
            } else if (strncmp(parser.data, "/model/flow_arrival/mean/set", strlen("/model/flow_arrival/mean/set")) == 0) {
                float median;
                if (sscanf(parser.data, "/model/flow_arrival/mean/set/%f", &median)) {
                    ((struct traffic_model *)w->data)->flow_arrival.mean = median;
                    sprintf(data, "{status:\"success\", data:{mean:\"%f\"}}", ((struct traffic_model *)w->data)->flow_arrival.mean);
                } else
                    sprintf(data, "{status:\"failed\", data:{}}");
                ctrl_generate_response(response, data);
                send(w->fd, response, strlen(response), 0);
            } else if (strncmp(parser.data, "/result/latency/median/get", strlen("/result/latency/median/get")) == 0) {
                struct tcp_flow *p;
                struct timeval now;
                int i, samples_array_count = 100, samples_count = 0;
                double sample = 0.0, *samples = malloc(samples_array_count * sizeof(double));

                gettimeofday(&now, NULL);

                for (p = TAILQ_LAST(&t->stats, tcp_stats); p != NULL; p = TAILQ_PREV(p, tcp_stats, entry)) {
                    if (p->requests == 0) continue;
                    if (time_diff(&p->start[0], &now) > 10) break;
                    for (i=0;i<p->requests;i++) {
                        if (time_diff(&p->start[0], &now) >= 10) break;
                        sample = time_diff(&p->start[i], &p->end[i]);
                        samples_count++;
                        if (samples_array_count < samples_count) {
                            samples_array_count += samples_array_count;
                            samples = realloc(samples, samples_array_count * sizeof(double));
                        }
                        samples[samples_count - 1] = sample;
                        printf("start %f\n", sample);
                    }
                }

                gsl_sort(samples, 1, samples_count);
                sprintf(data, "{status:\"success\", data:{mean:\"%f\"}, median:\"%f\"}}", gsl_stats_mean(samples, 1, samples_count),
                        gsl_stats_median_from_sorted_data(samples, 1, samples_count));
                free(samples);
                ctrl_generate_response(response, data);
                send(w->fd, response, strlen(response), 0);
            } else {
                send(w->fd, error_response, strlen(error_response), 0);
            }

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
    w_client->data = watcher->data;
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
    ev_ctl_server->data = t;

    ev_io_start(loop, ev_ctl_server);
    return 0;
}

/**
 * Read data from a socket.
 * TODO: write a response to the query.
 * */
// TODO write data and close the FD. logically the size of the resposne will be
// small.

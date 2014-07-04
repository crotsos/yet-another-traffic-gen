/*
 * =====================================================================================
 *
 *       Filename:  tcp_util.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  04/07/14 14:14:04
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
void client_read_cb(struct ev_loop *loop, struct ev_io *watcher, 
    int revents);
void srv_read_cb(struct ev_loop *loop, struct ev_io *watcher, 
    int revents);

int tcp_flow_request (struct ev_loop *, uint16_t, uint64_t, 
    struct tcp_flow *, struct traffic_model *);
void init_tcp_request(struct ev_loop *l, struct tcp_flow *f, 
    struct timeval *tv);
void request_cb (struct ev_loop *, struct ev_timer *, int); 
void tcp_flow_cb (struct ev_loop *, struct ev_timer *, int);

void tcp_accept_cb(struct ev_loop *, struct ev_io *, int);

/*
 * =====================================================================================
 *
 *       Filename:  udp_util.h
 *
 *    Description: UDP traffic generation methods 
 *
 *        Version:  1.0
 *        Created:  04/07/14 13:21:06
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Charalampos Rotsos (cr409@cl.cam.ac.uk), 
 *   Organization:  Computer Laboratory, University of Cambridge 
 *
 * =====================================================================================
 */
void udp_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
int udp_flow_request(struct ev_loop *, uint16_t, struct traffic_model *);

/*
 * =====================================================================================
 *
 *       Filename:  traff_gen.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  18/10/13 16:32:12
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (),
 *   Organization:
 *
 * =====================================================================================
 */

#ifndef __TRAFF_GEN_H__

#define __TRAFF_GEN_H__ 1

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
#include <sys/queue.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <regex.h>
#include <string.h>
#include "http_parser.h"

#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>

#define PORT_NO 3033
#define CTRL_PORT 8080
#define WINDOW_SIZE 32000
#define BUFFER_SIZE 6400

#define kOurProductName "traff_gen"
#include "debug.h"
#include "tpl.h"

struct server_stats {
  uint64_t tcp_tot_bytes;
  uint32_t tcp_tot_conn;
  uint32_t udp_tot_conn;
  uint32_t conns;
  uint64_t tcp_period_bytes;
  uint64_t period_finished;
};
/* we need a simple UDP &TCP request model
 * */
// TODO add on-off model and normal
enum model_type {
  CONSTANT=1,
  EXPONENTIAL,
  PARETO,
  WEIBULL,
  LOGNORMAL,
};

struct model {
  uint16_t type;
  double alpha;
  double mean;
};

struct tcp_flow_stats {
  uint64_t request;
  uint64_t send;
  uint32_t id;
  struct timeval st, end;
  struct traffic_model *t;
};

struct udp_flow_stats {
  uint32_t pkt_count;
  uint16_t pkt_size;
  struct model delay;
  uint64_t send;
  uint32_t id;
  struct timeval st, end;
};

enum traffic_mode {
  PIPELINE=1,
  INDEPENDENT,
  PACKET,
};

struct tcp_flow;



struct traffic_model {
  char host[1024];
  char logfile[1024];
  long long int seed;
  long long int duration;
  uint16_t port, ctrl_port;
  enum traffic_mode mode;
  uint16_t flows;
  uint32_t flow_count;
  char *domain;
  char **urls;
  uint32_t url_count;
  int debug;
  struct model flow_arrival;
  struct model request_num;
  struct model request_delay;
  struct model request_size;
  uint32_t requests_running;
  int running;
  struct server_stats serv;
  TAILQ_HEAD(tcp_stats, tcp_flow) stats;
};

struct tcp_request {
  uint64_t size;
};

struct udp_request {
  struct model request_num;
  struct model request_delay;
  struct model request_size;
};

struct tcp_flow {
  uint32_t id;
  double requests;
  uint16_t curr_request;
  http_parser parser;
  uint8_t *send_req;
  double *request_delay;
  double *size;
  uint32_t *recved;
  uint8_t *body;
  struct timeval *start;
  struct timeval *end;
  uint32_t *pages;
  struct traffic_model *t;
  TAILQ_ENTRY(tcp_flow) entry;
};

struct udp_flow {
  int fd;
  uint32_t id;
  struct sockaddr_in addr;
  double requests;
  uint16_t curr_request;
  uint8_t *send_req;
  double *request_delay;
  double *size;
  struct timeval *start;
  struct traffic_model *t;
};

struct pkt_header {
  uint32_t flow_id;
  uint32_t pkt_id;
  struct timeval send;
};

void init_traffic_model( struct traffic_model*, const char *);
void tcp_init_flow(struct traffic_model *, struct tcp_flow *);
void udp_init_flow(struct udp_request *, int, int, struct sockaddr_in, struct udp_flow *);

void init_rand(struct traffic_model *);
void get_sample(struct model *, double *, int);
char *print_model(struct model *);
double time_diff (struct timeval *, struct timeval *);

int start_ctrl_service(struct ev_loop *,  struct traffic_model *);

#include "tcp_util.h"
#include "udp_util.h"


#endif //__TRAFF_GEN_H__

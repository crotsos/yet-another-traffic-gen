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

#define PORT_NO 3033

struct server_stats {
  uint64_t tot_bytes;
  uint32_t tot_conn;
  uint32_t conns;
  uint64_t period_bytes;
  uint64_t period_finished;
};

struct flow_stats {
  uint64_t request;
  uint64_t send;
  uint32_t id;
  struct timeval st, end;
};

enum model_type {
  CONSTANT=1,
  EXPONENTIAL,
  PARETO,
};

struct model {
  int type; 
  double alpha;
  double mean;
};

enum traffic_mode {
  PIPELINE=1,
  INDEPENDENT,
};

struct traffic_model {
  char host[1024];
  char logfile[1024];
  long long int seed;
  long long int duration;
  uint16_t port;
  uint16_t port_num;
  enum traffic_mode mode;
  uint16_t flows;
  struct model flow_arrival;
  struct model request_num;
  struct model request_delay;
  struct model request_size;
};

struct flow {
  uint32_t id;
  double requests;
  uint16_t curr_request;
  uint8_t *send_req; 
  double *request_delay;
  double *size;
  struct timeval *start;
};

void init_traffic_model( struct traffic_model*, const char *);

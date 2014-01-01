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
  uint64_t tcp_tot_bytes;
  uint32_t tcp_tot_conn;
  uint32_t udp_tot_conn;
  uint32_t conns;
  uint64_t tcp_period_bytes;
  uint64_t period_finished;
};
/* we need a simple UDP &TCP request model 
 * */
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

struct tcp_flow_stats {
  uint64_t request;
  uint64_t send;
  uint32_t id;
  struct timeval st, end;
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

void init_rand(struct traffic_model *);
void get_sample(struct model *, double *, int); 

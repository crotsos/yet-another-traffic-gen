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
  uint64_t period_bytes;
  uint64_t conns;
  uint64_t period_finished;
};

struct flow_stats {
  uint32_t request;
  uint32_t send;
  struct timeval st, end;
};




/*
 * =====================================================================================
 *
 *       Filename:  config.c
 *
 *    Description:t
 *
 *        Version:  1.0
 *        Created:  21/10/13 00:43:32
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <libconfig.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <ev.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "traff_gen.h"
#include "debug.h"

void 
init_traffic_model (struct traffic_model *t, const char *file) {
  config_t cfg;
  const char *name;

  config_init(&cfg);
  if (! config_read_file(&cfg, file)) {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
        config_error_line(&cfg), config_error_text(&cfg));
    exit(1);
  }

  if(config_lookup_string(&cfg, "log", &name) == CONFIG_FALSE)
    strncpy(t->logfile, "output.log", 1024);
  else 
    strncpy(t->logfile, name, 1024);

  if(config_lookup_int64(&cfg, "seed", &t->seed) == CONFIG_FALSE)
    t->seed = 10000;

  if(config_lookup_int64(&cfg, "duration", &t->duration) == CONFIG_FALSE)
    t->duration = 60;
  printf("got a duration of %lld\n", t->duration);

  if(config_lookup_string(&cfg, "service.host", &name) == CONFIG_FALSE)
    strncpy(t->host, "127.0.0.1", 1024);
  else 
    strncpy(t->host, name, 1024);
  if(config_lookup_int(&cfg, "service.port_start", (int *)&t->port) == CONFIG_FALSE)
    t->port = PORT_NO;
  if(config_lookup_int(&cfg, "service.port_range", (int *)&t->port_num) == CONFIG_FALSE)
    t->port = 1;

  if(config_lookup_int(&cfg, "traffic.flows", (int *)&t->flows) == CONFIG_FALSE)
    t->flows = 1;

  if (config_lookup_string(&cfg, "traffic.type", &name) == CONFIG_FALSE) {
    perror("undefined traffic type");
    exit(1);
  } else {
    if (!strcasecmp(name, "pipeline")) 
      t->mode = PIPELINE;
    else if (!strcasecmp(name, "independent"))
      t->mode = INDEPENDENT;
    else {
      printf("invalid type value %s\n", name);
    }
  }

  if(config_lookup_string(&cfg, "traffic.flow_arrival.type", &name) == CONFIG_TRUE) {
    if (!strcasecmp(name, "constant"))
      t->flow_arrival.type = CONSTANT;
    else if (!strcasecmp(name, "exponential")) {
      t->flow_arrival.type = EXPONENTIAL;
    } else if (!strcasecmp(name, "pareto")) { 
      t->flow_arrival.type = PARETO;
    } else { 
      printf("invalid flow_arrival type value %s", name);
      exit(1);
    }
  } else { 
    printf("invalid flow_arrival type value %s", name);
    exit(1);
  }

  if(config_lookup_float(&cfg, "traffic.flow_arrival.mean", &t->flow_arrival.mean) == CONFIG_FALSE) {
    printf("undefined traffic.flow_arrival.mean\n");
    exit(1);
  }
  printf("got a delay of %f\n", t->flow_arrival.mean);
  
  if(config_lookup_float(&cfg, "traffic.flow_arrival.alpha", &t->flow_arrival.alpha) == CONFIG_FALSE)
    printf("undefined traffic.flow_arrival.aplha\n");

  if(config_lookup_string(&cfg, "traffic.request_number.type", &name) == CONFIG_TRUE) {
      if (!strcasecmp(name, "constant"))
        t->request_num.type = CONSTANT;
      else if (!strcasecmp(name, "exponential")) 
        t->request_num.type = EXPONENTIAL;
      else if (!strcasecmp(name, "pareto")) 
        t->request_num.type = PARETO;
      else { 
        printf("invalid request_number type value %s", name);
        exit(1);
      }
    } else { 
      printf("invalid request_number type value %s", name);
      exit(1);
    }

    if(config_lookup_float(&cfg, "traffic.request_number.mean", &t->request_num.mean) == CONFIG_FALSE) {
      printf("undefined traffic.request_num.mean\n");
      exit(1);
    }
    if(config_lookup_float(&cfg, "traffic.request_number.alpha", &t->request_num.alpha) == CONFIG_FALSE) {
      printf("undefined traffic.request_num.aplha\n");
    }

    if(config_lookup_string(&cfg, "traffic.request_delay.type", &name) == CONFIG_TRUE) {
      if (!strcasecmp(name, "constant"))
        t->request_delay.type = CONSTANT;
      else if (!strcasecmp(name, "exponential")) 
        t->request_delay.type = EXPONENTIAL;
      else if (!strcasecmp(name, "pareto")) 
        t->request_delay.type = PARETO;
      else { 
        printf("invalid request_delay type value %s", name);
        exit(1);
      }
    }
    if(config_lookup_float(&cfg, "traffic.request_delay.mean", &t->request_delay.mean) == CONFIG_FALSE)
      printf("undefined traffic.request_delay.mean\n");
    if(config_lookup_float(&cfg, "traffic.request_delay.alpha", &t->request_delay.alpha) == CONFIG_FALSE)
      printf("undefined traffic.request_delay.alpha\n");

  if(config_lookup_string(&cfg, "traffic.request_size.type", &name) == CONFIG_TRUE) {
      if (!strcasecmp(name, "constant"))
        t->request_size.type = CONSTANT;
      else if (!strcasecmp(name, "exponential")) 
        t->request_size.type = EXPONENTIAL;
      else if (!strcasecmp(name, "pareto")) 
        t->request_size.type = PARETO;
      else { 
        printf("invalid request_size type value %s", name);
        exit(1);
      }
    } else { 
      printf("invalid request_size type value %s", name);
      exit(1);
    }

    if(config_lookup_float(&cfg, "traffic.request_size.mean", &t->request_size.mean) == CONFIG_FALSE)
      printf("undefined traffic.request_size.mean\n");
    if(config_lookup_float(&cfg, "traffic.request_size.alpha", &t->request_size.alpha) == CONFIG_FALSE)
      printf("undefined traffic.request_size.apha\n");

    config_destroy(&cfg);
  }




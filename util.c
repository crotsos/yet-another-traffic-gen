/*
 * =====================================================================================
 *
 *       Filename:  util.c
 *
 *    Description:  source file with generic util functions.  
 *
 *        Version:  1.0
 *        Created:  01/01/2014 18:41:39
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  crotsos (cr409@cl.cam.ac.uk), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include <libconfig.h>
#include <stdlib.h>
#include <strings.h>

#include "traff_gen.h"
#include "debug.h"

gsl_rng * r;

void 
init_rand(struct traffic_model *t) {
  const gsl_rng_type * T;

  // initiliaze the random number generator
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);
  gsl_rng_set(r, (unsigned long int)t->seed);
  return;
}

/**
 * generate random sample acording to the specification of the distribution
 * model.
 *
 * @param m generation model.
 * @param ret a pointer to a double array where the samples will be stored.
 * @param len the return array size. 
 * */
void
get_sample(struct model *m, double *ret, int len) {
  int ix;
  for (ix=0; ix < len; ix ++) 
    switch(m->type) {
      case CONSTANT:
        ret[ix] = m->mean;
        break;
      case EXPONENTIAL:

        ret[ix] = gsl_ran_exponential((const gsl_rng *) r, m->mean);
        //printf("exponential sample %f (mean %f)\n", ret[ix], m->mean);
        break;
      case PARETO:
        ret[ix] = gsl_ran_pareto((const gsl_rng*)r, m->alpha, m->mean);
        //printf("pareto sample %f (alpha %f mean %f)\n", ret[ix], m->alpha, m->mean);
        break;
      default:
        printf("Invalid traffic model\n");
        exit(1);
    }
}

/**
 * Get a random sample from a uniform integer distribution.
 *
 * @param ret array to store return samples.
 * @param len the length of the return array.
 * @param max The upper bound of the integer distribution
 * */
void
get_ix_sample(uint32_t *ret, int len, uint32_t max) {
  int ix;
  for (ix=0; ix < len; ix ++)
    ret[ix] = gsl_rng_uniform_int ((const gsl_rng *) r, max);
}

double 
time_diff (struct timeval *start, struct timeval *end) {
  return ( (double)end->tv_sec - (double)start->tv_sec + 
      (((double)end->tv_usec - (double)start->tv_usec)/(double)1000000)); }

void *
xmalloc(ssize_t len) {
  void *ret = malloc(len);
  if (!ret) {
    perror("malloc");
    exit(1);
  }
  return ret;
}

void 
tcp_init_flow(struct traffic_model *t, struct tcp_flow *f) {

  // store a local pointer to the traffic model 
  f->t = t;

  // generate a request number sample
  f->curr_request = 0;
  get_sample(&t->request_num, &f->requests, 1);

  // define for each reqest the inter-request delay and size
  f->send_req = (uint8_t *)xmalloc(f->requests * sizeof(uint8_t));
  bzero(f->send_req, f->requests);
  f->size = (double *)xmalloc(f->requests * sizeof(double));
  f->request_delay = (double *)xmalloc(f->requests * sizeof(double));

  // clear the flow statistics 
  f->start = (struct timeval *)xmalloc(f->requests * sizeof(struct timeval));
  bzero(f->start, f->requests * sizeof(struct timeval));
  f->recved = (uint32_t *)xmalloc(f->requests * sizeof(uint32_t));
  bzero(f->recved, f->requests * sizeof(uint32_t));
  f->body = (uint8_t *)xmalloc(f->requests * sizeof(uint8_t));
  bzero(f->body, f->requests * sizeof(uint8_t));


  get_sample(&t->request_delay, f->request_delay, f->requests);

  if (t->urls == NULL) {
    f->pages = NULL;
    get_sample(&t->request_size, f->size, f->requests);
  } else {
    http_parser_init(&f->parser, HTTP_RESPONSE);
    f->parser.data = f;
    f->pages = (uint32_t *) xmalloc(f->requests * sizeof(uint32_t));
    get_ix_sample(f->pages, f->requests, t->url_count);
    bzero(f->size, f->requests * sizeof(double));
  } 
  f->id = (++t->flow_count);
}

void 
udp_init_flow(struct udp_request *t, int id, int fd, struct sockaddr_in a, struct udp_flow *f) {
  int i;
  f->curr_request = 0;

  get_sample(&t->request_num, &f->requests, 1);
  f->send_req = (uint8_t *)malloc(f->requests * sizeof(uint8_t));
  bzero(f->send_req, f->requests);
  memcpy(&f->addr, &a, sizeof(struct sockaddr_in));

  f->request_delay = (double *)malloc(f->requests * sizeof(double));
  get_sample(&t->request_delay, f->request_delay, f->requests);
  f->size = (double *)malloc(f->requests * sizeof(double));
  get_sample(&t->request_size, f->size, f->requests);
  for (i = 0; i < f->requests; i++) {
    if (f->size[i] > 1500) {
      f->size[i] = 1500;
    }
  }
  f->id = id;
  f->fd = fd;
}

char str_model[2000];
char *print_model(struct model *m) {
  switch(m->type) {
    case CONSTANT:
      sprintf(str_model, "CONSTANT(%f)", m->mean);
      break;
    case EXPONENTIAL:
      sprintf(str_model, "EXPONENTIAL(%f)", m->mean);
      break;
     case PARETO:
      sprintf(str_model, "PARETO(%f, %f)", m->mean, m->alpha);
      break;
    default:
      printf("wrong type %d\n", m->type);
      bzero(str_model, 2000);
  }
  return str_model;
}

static void 
parse_model(config_t *cfg, struct model *m, const char *path) {
  const char *name;
  char field_name[100];

  sprintf(field_name, "%s.type", path); 
  if(config_lookup_string(cfg, field_name, &name) != CONFIG_TRUE) {
    printf("undefined %s.type value\n", path);
    exit(1);
  }
  
  if (!strcasecmp(name, "constant")) {
    m->type = CONSTANT;
    sprintf(field_name, "%s.mean", path); 
    if(config_lookup_float(cfg, field_name, &m->mean) == CONFIG_FALSE) {
      printf("undefined %s.mean\n", path);
      exit(1);
    }
  }
  else if (!strcasecmp(name, "exponential")) {
    m->type = EXPONENTIAL;
    sprintf(field_name, "%s.mean", path); 
    if(config_lookup_float(cfg, field_name, &m->mean) == CONFIG_FALSE) {
      printf("undefined %s.mean\n", path);
      exit(1);
    }
  } else if (!strcasecmp(name, "pareto")) { 
    m->type = PARETO;
    sprintf(field_name, "%s.mean", path); 
    if(config_lookup_float(cfg, field_name, &m->mean) == CONFIG_FALSE) {
      printf("undefined %s.mean\n", path);
      exit(1);
    }
    sprintf(field_name, "%s.alpha", path); 
    if(config_lookup_float(cfg, field_name, &m->alpha) == CONFIG_FALSE) {
      printf("undefined %s.alpha\n", path);
      exit(1);
    }
  } else { 
    printf("invalid %s.type=%s", path, name);
    exit(1);
  }
}

void 
init_traffic_model (struct traffic_model *t, const char *file) {
  config_t cfg;
  const char *name;
  char buffer[4086];

  t->requests_running = 0;
  t->running = 1;

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

  if(config_lookup_bool(&cfg, "debug", &t->debug) == CONFIG_FALSE) {
    printf("didn't find debug\n");
    t->debug = 0;
  } else 
    printf("debug=%d\n", t->debug);

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
  if(config_lookup_int(&cfg, "service.ctrl_port", (int *)&t->port) == CONFIG_FALSE)
    t->ctrl_port = CTRL_PORT;


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
    else if (!strcasecmp(name, "packet"))
      t->mode = PACKET;
    else {
      printf("invalid type value %s\n", name);
    }
  }

  parse_model(&cfg, &t->flow_arrival, "traffic.flow_arrival"); 
  parse_model(&cfg, &t->request_num, "traffic.request_number"); 
  parse_model(&cfg, &t->request_delay, "traffic.request_delay"); 
  parse_model(&cfg, &t->request_size, "traffic.request_size"); 

  if (config_lookup_string(&cfg, "traffic.request_pages.pages", &name) == CONFIG_FALSE) {
    printf("pure tcp traffic\n");
    t->urls = NULL;
  } else {
    printf("loading webpages\n");
    FILE *urls = fopen(name, "r");
    t->url_count = 0;

    if (urls == NULL) {
      perror("url fopen");
    } else {
      t->urls = NULL;
      while (fgets(buffer, 4089, urls)) {
        t->url_count++;
        t->urls = (char **)realloc(t->urls, (t->url_count + 1)*sizeof(char *));
        t->urls[t->url_count] = NULL;
        buffer[strlen(buffer) - 1] = '\0';
        t->urls[t->url_count-1] = malloc(strlen(buffer) + 1);
        strcpy(t->urls[t->url_count-1], buffer);
      }
      printf("found %d pages\n", t->url_count);
      fclose(urls);
    }
  }

  if (config_lookup_string(&cfg, "traffic.request_pages.host", &name) == CONFIG_FALSE) {
    printf("host not found\n");
    t->domain = NULL;
  } else {
    printf("domain: %s\n", name);
    t->domain = malloc(strlen(name) + 1);
    strcpy(t->domain, name);
  }


  config_destroy(&cfg);
}

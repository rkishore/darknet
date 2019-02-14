#ifndef CLASSIFYAPP_API
#define CLASSIFYAPP_API
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "option_list.h"
#include "demo.h"
#include "network.h"

struct prep_network_info {
  list *options;
  char *name_list;
  int names_array_len;
  char **names;
  
  image **alphabet;
  network net;
  int classes;
};

#ifdef __cplusplus
}
#endif
#endif

#pragma once

#define HSIGNATURE_SIZE 1024
#define SMALL_FIXED_STRING_SIZE 8
#define MEDIUM_FIXED_STRING_SIZE 40
#define LARGE_FIXED_STRING_SIZE 255

struct Config 
{
  
  char           *debug_level;
  
  char           unique_signature[HSIGNATURE_SIZE];
  char           hostname[HSIGNATURE_SIZE];
  float          detection_thresh;

  char           interface_name[MEDIUM_FIXED_STRING_SIZE];
  char           dnn_config_file[LARGE_FIXED_STRING_SIZE];
  char           dnn_weights_file[LARGE_FIXED_STRING_SIZE];

};

const struct Config* get_config();
struct Config* mod_config();
int create_config();
void free_config();
void fill_default_config();
int fill_unique_node_signature(const char *iname);

void print_usage(const char *app_name);
void free_mem(void *ptr_to_free);

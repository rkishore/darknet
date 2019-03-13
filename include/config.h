#pragma once

#include <stdbool.h>

#define HSIGNATURE_SIZE 1024
#define SMALL_FIXED_STRING_SIZE 8
#define MEDIUM_FIXED_STRING_SIZE 40
#define LARGE_FIXED_STRING_SIZE 255

struct Config 
{
  
  char           *debug_level;
  
  char           unique_signature[HSIGNATURE_SIZE];
  char           hostname[HSIGNATURE_SIZE];
  float          detection_threshold;

  char           interface_name[MEDIUM_FIXED_STRING_SIZE];
  char           dnn_data_file[LARGE_FIXED_STRING_SIZE];
  char           dnn_config_file[LARGE_FIXED_STRING_SIZE];
  char           dnn_weights_file[LARGE_FIXED_STRING_SIZE];
  char           data_folder_path[LARGE_FIXED_STRING_SIZE];

  unsigned int   daemon_port;
  char           image_url[LARGE_FIXED_STRING_SIZE];
  char           output_directory[LARGE_FIXED_STRING_SIZE];
  char           output_filepath[LARGE_FIXED_STRING_SIZE];
  char           output_json_filepath[LARGE_FIXED_STRING_SIZE];
  char           input_type[SMALL_FIXED_STRING_SIZE];
  char           input_mode[SMALL_FIXED_STRING_SIZE];

  int            gpu_idx;
  char           *tmp_output_path;
  
  bool           fastmode; // avoid checking the input params if this is set to true
  int            max_queue_length; // max input request queue length
  bool           dont_overwrite_old_results_until_read; // don't allow new POSTs to over-write old results until they are read by GET

};

const struct Config* get_config();
struct Config* mod_config();
int create_config();
void free_config();
void fill_default_config();
int fill_unique_node_signature(const char *iname);

void print_usage(const char *app_name);
void free_mem(void *ptr_to_free);

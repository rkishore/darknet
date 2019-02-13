/*************************************************************************
 * config.c - routines to handle per-instance init. for AUDIOAPP
 *            Most of this state is obtained from cmdline args.
 *
 * Copyright (C) 2018 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: Kishore Ramachandran (kishore.ramachandran@zipreel.com)
 *          
 **************************************************************************/

#include <unistd.h>

#include "common.h"
#include "config.h"
#include "serialnum.h"

struct Config *config=0;

void free_mem(void *ptr_to_free) 
{
  
  if (ptr_to_free != NULL)
    free(ptr_to_free);
  ptr_to_free = NULL;
  return;

}

int create_config()
{
  config = (struct Config *)malloc(sizeof(struct Config));
  if (config == NULL) {
    return -1;
  }
  return 0;
}

void print_usage(const char *app_name)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s (params) (input_url) (output_directory)\n", app_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "params (mandatory):\n");
  fprintf(stderr, "-a, --interface-name | Primary network interface name (used for licensing purposes) | Default: %s \n", get_config()->interface_name);
  fprintf(stderr, "-g, --data-folder-path | Location of data folder path | Default: %s \n", get_config()->data_folder_path);
  fprintf(stderr, "\nparams (optional): \n");
  fprintf(stderr, "-b, --dnn-config-file | Filesystem location of the dnn config file | Default: %s\n", get_config()->dnn_config_file);
  fprintf(stderr, "-c, --dnn-weights-file | Filesystem location of the dnn weights file | Default: %s\n", get_config()->dnn_weights_file);
  fprintf(stderr, "-d, --detection-thresh | Detection threshold below which objects detected are not reported | Default: %0.1f\n", get_config()->detection_threshold);
  fprintf(stderr, "-e, --dnn-data-file | Filesystem location of the dnn data file | Default: %s\n", get_config()->dnn_data_file);
  fprintf(stderr, "-f, --port [port number] | Port daemon is listening on for REST-ful API | Default = %d (if unspecified)\n", get_config()->daemon_port);
  fprintf(stderr, "-g, --data-folder-path | Filesystem location of the dnn data folder | Default: %s\n", get_config()->data_folder_path);
  fprintf(stderr, "-i, --gpu-idx | In a multi-GPU system, selecting the GPU to run on | Default: %d\n", get_config()->gpu_idx);
  fprintf(stderr, "-L, --log-level | Set the syslog LOG LEVEL | Default: LOG_INFO (%s) \n", get_config()->debug_level);
  
  fprintf(stderr, "\n");

  return;

}

int get_unique_node_signature(char *unique_signature, const char *iname)
{
  int retval;
  char serial_number[80];
  unsigned char mac_addr[6];
  char *message = NULL;
  int i;
  int xval;
        
  // Build unique signature for this machine            
  memset(unique_signature, 0, HSIGNATURE_SIZE);
  message = &unique_signature[0];
  
  retval = get_serial_number((char *)&serial_number[0], 0);
  if (retval < 0) {
    return -1;   
  }
  retval = get_mac_address(iname, (unsigned char *)&mac_addr[0]);
  if (retval < 0) {
    return -1;
  }
  for (i = 0; i < 6; i++) {    
    xval = (i + 72) & 0xff;
    mac_addr[i] = mac_addr[i] ^ xval;
  }
  
  // this is the message that was originally signed - we're just cross-checking it.
  sprintf(message,"%s:%d.%d.%d.%d.%d.%d.%d.%d", 
	  serial_number,
	  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
	  mac_addr[4], mac_addr[5], 0xfa, 0xce);
  
  syslog(LOG_DEBUG, "= Unique node signature: %s\n", message);

  return 0;
  
}

int fill_unique_node_signature(const char *iname)
{
  if (get_unique_node_signature(config->unique_signature, iname) < 0)
    {
      syslog(LOG_ERR, "= Could not fill unique node signature\n");
    }
  
  return 0;
}

void fill_default_config()
{

  int tmp_dbg_level=LOG_INFO;

  memset(config, 0, sizeof(struct Config));

  if(!get_config()->debug_level) {
    mod_config()->debug_level = (char *)malloc(sizeof(char *));
    if(get_config()->debug_level != NULL)
      sprintf(mod_config()->debug_level, "%u", tmp_dbg_level);
  }

  memset(&config->interface_name[0], 0, SMALL_FIXED_STRING_SIZE);
  strcpy(&config->interface_name[0], (const char *)"eth0");

  if (gethostname(config->hostname, HSIGNATURE_SIZE) < 0)
    {
      syslog(LOG_ERR, "= Could not get current hostname\n");
      exit(-1);
    }

  memset(&config->dnn_data_file[0], 0, LARGE_FIXED_STRING_SIZE);
  strcpy(&config->dnn_data_file[0], (const char *)"/usr/local/share/classifyapp/cfg/coco.data");

  memset(&config->dnn_config_file[0], 0, LARGE_FIXED_STRING_SIZE);
  strcpy(&config->dnn_config_file[0], (const char *)"/usr/local/share/classifyapp/cfg/classifyapp_v0.cfg");

  memset(&config->dnn_weights_file[0], 0, LARGE_FIXED_STRING_SIZE);
  strcpy(&config->dnn_weights_file[0], (const char *)"/usr/local/share/classifyapp/cfg/classifyapp_v0.weights");

  memset(&config->data_folder_path[0], 0, LARGE_FIXED_STRING_SIZE);
  strcpy(&config->data_folder_path[0], (const char *)"/usr/local/share/classifyapp/data/");

  config->detection_threshold = 0.5;
  config->daemon_port = 55555;
  config->gpu_idx = -1;
  
  return;
  
}

const struct Config* get_config()
{
  return config;
}

struct Config* mod_config()
{
  return config;
}

void free_config()
{
  free(config->debug_level);
  free(config);
}

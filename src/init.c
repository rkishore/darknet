/*******************************************************************
 * init.c - initialization routines for AUDIOAPP
 *          
 * Copyright (C) 2018 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: Kishore Ramachandran (kishore.ramachandran@zipreel.com)
 *          
 *******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <curl/curl.h>

#include <stdbool.h>
#include <getopt.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "config.h"
#include "serialnum.h"
//#include "speech_proc.h"
#include "common.h"

//#include "darknet.h"
#include "cuda.h"

static void 
verify_license()
{

  int retval = verify_file("/usr/bin/igolgiapp.key", 0, 1);
  if (retval == VERIFY_FAILED) {
    fprintf(stderr,"= ERROR:  application unable to verify license!\n");       
    exit(-1);       
  }

  return;

}

static void
get_info_from_cmdline_args(int *argc, char **argv)
{

  int retval = create_config();
  if (retval < 0) {
    fprintf(stdout, "= ERROR: Could not allocate memory for config\n");
    exit(-1);
  }

  fill_default_config();

  if (*argc == 1) {
    print_usage(argv[0]);
    free_config();
    exit(0);
  }

  while (1) {

    static struct option long_options[] = {
      {"interface-name", required_argument, 0, 'a'},
      {"dnn-config-file", required_argument, 0, 'b'},
      {"dnn-weights-file", required_argument, 0, 'c'},
      {"detection-thresh", required_argument, 0, 'd'},
      {"dnn-data-file", required_argument, 0, 'e'},
      {"daemon-port", required_argument, 0, 'f'},
      {"data-folder-path", required_argument, 0, 'g'},
      {"gpu-idx", required_argument, 0, 'i'},
      {"log-level", required_argument, 0, 'L'},
      {"help", no_argument, 0, 'h' },
      {"version", no_argument, 0, 'v' },
      {0, 0, 0, 0 } // last element has to be filled with zeroes as per the man page
    };

    int option_index = 0;
    int c = getopt_long(*argc, argv, "a:b:c:d:e:f:g:i:L:hv", 
			long_options,
                        &option_index);
    int sizeof_char_ptr = sizeof(char *);
    
    // If there are no more option characters, getopt() returns -1
    if (c == -1) {
      break;
    }
    
    switch(c) {
    case 'a':
      memset(mod_config()->interface_name, 0, SMALL_FIXED_STRING_SIZE);
      memcpy(mod_config()->interface_name, optarg, strlen(optarg));      
      break;
    case 'b':
      memset(mod_config()->dnn_config_file, 0, LARGE_FIXED_STRING_SIZE);
      memcpy(mod_config()->dnn_config_file, optarg, strlen(optarg));      
      break;
    case 'c':
      memset(mod_config()->dnn_weights_file, 0, LARGE_FIXED_STRING_SIZE);
      memcpy(mod_config()->dnn_weights_file, optarg, strlen(optarg));      
      break;
    case 'd':
      mod_config()->detection_threshold = atof(optarg);
      break;
    case 'e':
      memset(mod_config()->dnn_data_file, 0, LARGE_FIXED_STRING_SIZE);
      memcpy(mod_config()->dnn_data_file, optarg, strlen(optarg));      
      break;
    case 'f':
      mod_config()->daemon_port = atoi(optarg);
      break;
    case 'g':
      memset(mod_config()->data_folder_path, 0, LARGE_FIXED_STRING_SIZE);
      memcpy(mod_config()->data_folder_path, optarg, strlen(optarg));      
      break;
    case 'i':
      mod_config()->gpu_idx = atoi(optarg);
      break;
    case 'L' : 
      //      mod_config()->debug_level = optarg;
      memcpy(mod_config()->debug_level, optarg, sizeof_char_ptr);  
      break; 
    case 'h' :
    default :
      print_usage(argv[0]);
      free_config();
      exit(0);
      break;
      
    }
    
  }

  if (fill_unique_node_signature(get_config()->interface_name) < 0)
    {
      syslog(LOG_ERR, "= Could not get unique node signature\n");
      free_config();
      exit(EXIT_FAILURE);
    }

  fprintf(stderr, "= CMDLINE args | dnn_data_file: %s | dnn_config_file: %s | dnn_weights_file: %s | detection_thresh: %0.1f | iface_name: %s | data_folder_path: %s\n",
	  get_config()->dnn_data_file,
	  get_config()->dnn_config_file,
	  get_config()->dnn_weights_file,
	  get_config()->detection_threshold,
	  get_config()->interface_name,
	  get_config()->data_folder_path);

  return;
}

static int 
syslog_init(const char *debug_level) 
{

  long syslog_pri=0;

  /* Syslog-specific init */
  syslog_pri = strtol(debug_level, NULL, 10);
  if ((syslog_pri < 1) || (syslog_pri > 8)) {
    fprintf(stderr, "= Syslog level should be an integer between 1 and 8 (both inclusive). Exiting!\n");
    return -1;
  } else {

    if ((syslog_pri >= LOG_EMERG) && (syslog_pri <= LOG_DEBUG)) {

      setlogmask(LOG_UPTO(syslog_pri));

    } else {

      fprintf(stderr, "= Not using a valid syslog priority: %ld. Will use LOG_WARNING.\n", syslog_pri);
      setlogmask(LOG_UPTO(LOG_WARNING));

    }

  }
  openlog("classifyapp", LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
  syslog(LOG_DEBUG, "= Syslog level set to: %ld\n", syslog_pri);

  return 0;
}


void
cleanup_and_exit()
{

  free_config();
  closelog();
  curl_global_cleanup();
  return;

}

/*
static int 
job_control_init() 
{
  
  int shell_terminal;
  int shell_is_interactive;
  
  pid_t shell_pgid;

  // See if we are running interactively.  
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty (shell_terminal);

  syslog(LOG_INFO, "pgrp=%d pid=%d interactive_shell=%d\n", getpgrp(), getpid(), shell_is_interactive);

  if (shell_is_interactive) 
    {

      // Loop until we are in the foreground.  
      while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
	kill (- shell_pgid, SIGTTIN);
     
      // Ignore interactive and job-control signals.  
      //signal (SIGINT, SIG_IGN);
      signal (SIGQUIT, SIG_IGN);
      signal (SIGTSTP, SIG_IGN);
      signal (SIGTTIN, SIG_IGN);
      signal (SIGTTOU, SIG_IGN);
      signal (SIGCHLD, SIG_IGN);
     
      // Put ourselves in our own process group.  
      shell_pgid = getpid ();
      if (setpgid (shell_pgid, shell_pgid) < 0)
	{
	  syslog (LOG_ERR, "Couldn't put the shell in its own process group");
	  return -1;
	}
     
      // Grab control of the terminal.  
      if (tcsetpgrp (shell_terminal, shell_pgid) < 0) 
	{
	  syslog (LOG_ERR, "Could not grab control of the terminal: %s\n", strerror(errno));
	  return -1;
	}
            
    }
  
  return 0;
}
*/

void
check_if_key_files_and_paths_exist()
{

  // Check if directory exists 
  if ( ( access(get_config()->dnn_data_file, F_OK ) == -1 ) ||
       ( access(get_config()->dnn_config_file, F_OK ) == -1 ) ||
       ( access(get_config()->dnn_weights_file, F_OK ) == -1 ) ||
       ( access(get_config()->data_folder_path, F_OK ) == -1 ) ) {
    
    if ( access(get_config()->dnn_data_file, F_OK ) == -1 )
      {
	fprintf(stderr, "= DNN data file %s does not exist. Exiting.\n", get_config()->dnn_data_file);
	syslog(LOG_ERR, "= DNN data file %s does not exist. Exiting.", get_config()->dnn_data_file);
      }
    
    if ( access(get_config()->dnn_config_file, F_OK ) == -1 )
      {
	fprintf(stderr, "= DNN config file %s does not exist. Exiting.\n", get_config()->dnn_config_file);
	syslog(LOG_ERR, "= DNN config file %s does not exist. Exiting.", get_config()->dnn_config_file);
      }
    
    if ( access(get_config()->dnn_weights_file, F_OK ) == -1 )
      {
	fprintf(stderr, "= DNN weights file %s does not exist. Exiting.\n", get_config()->dnn_weights_file);
	syslog(LOG_ERR, "= DNN weights file %s does not exist. Exiting.", get_config()->dnn_weights_file);
      }
    
    if ( access(get_config()->data_folder_path, F_OK ) == -1 )
      {
	fprintf(stderr, "= Data folder path %s does not exist. Exiting.\n", get_config()->data_folder_path);
	syslog(LOG_ERR, "= Data folder path %s does not exist. Exiting.", get_config()->data_folder_path);
      }

    exit(-1);
  }

}

void 
basic_initialization(int *argc, char **argv, char *identity)
{

  /* getopt args to parse command linearguments */
  get_info_from_cmdline_args(argc, argv);

  /* Verify if path to cfg, weights, data and data_folder_path exist */
  check_if_key_files_and_paths_exist();
  
  /* Check if license key exists */
  // verify_license();

  /* Curl-specific init
     
     Must initialize libcurl before any threads are started 
     AND before syslog initialization (code does not work as 
     expected otherwise).
  */ 
  curl_global_init(CURL_GLOBAL_NOTHING);

  // Syslog-specific init 
  if (syslog_init(get_config()->debug_level) < 0) 
    {
      syslog(LOG_ERR, "Error initializing syslog. Exiting.\n");
      free_config();
      curl_global_cleanup();
      exit(-1);
    }

  // Set GPU to one selected
  if (get_config()->gpu_idx >= 0)
    {
      syslog(LOG_INFO, "= Setting device to idx %d, %s:%d", get_config()->gpu_idx, __FILE__, __LINE__);
      gpu_index = get_config()->gpu_idx;
      cuda_set_device(get_config()->gpu_idx);
      // check_error(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
      syslog(LOG_INFO, "= Getting device idx %d, %s:%d", cuda_get_device(), __FILE__, __LINE__);
    }
  return;
}


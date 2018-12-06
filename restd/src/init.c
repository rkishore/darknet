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
      {"log-level", required_argument, 0, 'L'},
      {"help", no_argument, 0, 'h' },
      {"version", no_argument, 0, 'v' },
      {0, 0, 0, 0 } // last element has to be filled with zeroes as per the man page
    };

    int option_index = 0;
    int c = getopt_long(*argc, argv, "a:b::L:hv", 
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
      mod_config()->detection_thresh = atof(optarg);
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

  fprintf(stderr, "= CMDLINE args | dnn_config_file: %s | dnn_weights_file: %s | detection_thresh: %0.1f | iface_name: %s\n",
	  get_config()->dnn_config_file,
	  get_config()->dnn_weights_file,
	  get_config()->detection_thresh,
	  get_config()->interface_name);
  
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
  openlog("audioapp", LOG_PERROR | LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
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
basic_initialization(int *argc, char **argv, char *identity)
{

  /* getopt args to parse command linearguments */
  get_info_from_cmdline_args(argc, argv);

  /* Check if license key exists */
  verify_license();

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

  /*  
  if (config_curl_hdl(&audio_url_ptr->curl_data) < 0)
    {
      syslog(LOG_ERR, "= Could not initialize curl_hdl_for_src. Exiting.\n");
      cleanup_and_exit();
      exit(-1);
    }

  if ((thread_info_inst->data_ipaddr = malloc(NI_MAXHOST * sizeof(char))) == NULL)
    {
      syslog(LOG_ERR, "= Error assigning memory for data_ipaddr\n");      
      cleanup_and_exit();
      exit(-1);
    }

  // Get the IP address of the eth0 interface of this Node 
  if (get_iface_IP(identity, thread_info_inst->data_ipaddr) < 0) 
    {
      syslog(LOG_ERR, "Error accessing IP address of eth0 interface. Exiting.\n");
      cleanup_and_exit();
      free_mem(thread_info_inst->data_ipaddr);
      exit(-1);
    }
  */


  /* Initialize CURL control connection to DB for monitoring */
  /* if (!get_config()->cmdline_mode)
    {
      if (config_curl_hdl(&thread_info_inst->curl_hdl_for_monit) < 0)
	{
	  syslog(LOG_ERR, "= Could not initialize curl_hdl_for_monit. Exiting.\n");
	  cleanup_and_exit();
	  free_mem(thread_info_inst->data_ipaddr);
	  exit(-1);
	}

      if (!get_config()->partial_exec_mode)
	{
	  // Initialize CURL control connection to DB for phase1 
	  if (config_curl_hdl(&thread_info_inst->curl_hdl_for_p1) < 0)
	    {
	      syslog(LOG_ERR, "= Could not initialize curl_hdl_for_p1. Exiting.\n");
	      curl_easy_cleanup(thread_info_inst->curl_hdl_for_monit);
	      cleanup_and_exit();
	      free_mem(thread_info_inst->data_ipaddr);
	      exit(-1);
	    }

	  // Initialize CURL control connection to DB for phase2 
	  if (config_curl_hdl(&thread_info_inst->curl_hdl_for_p2) < 0)
	    {
	      syslog(LOG_ERR, "= Could not initialize curl_hdl_for_p2. Exiting.\n");
	      curl_easy_cleanup(thread_info_inst->curl_hdl_for_monit);
	      curl_easy_cleanup(thread_info_inst->curl_hdl_for_p1);
	      cleanup_and_exit();
	      free_mem(thread_info_inst->data_ipaddr);
	      exit(-1);
	    }
	}
      else
	{
	  thread_info_inst->curl_hdl_for_p1 = NULL;
	  thread_info_inst->curl_hdl_for_p2 = NULL;
	}

      // Initialize CURL control connection to DB for phase3 
      if (config_curl_hdl(&thread_info_inst->curl_hdl_for_p3) < 0)
	{
	  syslog(LOG_ERR, "= Could not initialize curl_hdl_for_p3. Exiting.\n");
	  curl_easy_cleanup(thread_info_inst->curl_hdl_for_monit);
	  if (thread_info_inst->curl_hdl_for_p1)
	    curl_easy_cleanup(thread_info_inst->curl_hdl_for_p1);
	  if (thread_info_inst->curl_hdl_for_p2)
	    curl_easy_cleanup(thread_info_inst->curl_hdl_for_p2);
	  cleanup_and_exit();
	  free_mem(thread_info_inst->data_ipaddr);
	  exit(-1);
	}

    }
  */
  /*
  if (job_control_init() < 0) {
    syslog(LOG_ERR, "= Error initializing items related to job_control. Exiting.\n");
    if (!get_config()->cmdline_mode)
      {
	curl_easy_cleanup(thread_info_inst->curl_hdl_for_monit);
	if (thread_info_inst->curl_hdl_for_p1)
	  curl_easy_cleanup(thread_info_inst->curl_hdl_for_p1);
	if (thread_info_inst->curl_hdl_for_p2)
	  curl_easy_cleanup(thread_info_inst->curl_hdl_for_p2);
	curl_easy_cleanup(thread_info_inst->curl_hdl_for_p3);
      }
    cleanup_and_exit();
    free_mem(thread_info_inst->data_ipaddr);
    exit(-1);
  }
  */

  return;
}


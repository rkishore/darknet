/* 
   Proprietary and confidential source code.
   
   Copyright igolgi Inc. 2018. All rights reserved.
   
   Authors: kishore.ramachandran@igolgi.com
*/

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <stdio.h>
#include "init.h"
#include "config.h"
#include "fastpool.h"
#include "threadqueue.h"
#include "puller.h"

#include "pool.h"
#include "udpcomm.h"
#include "comm.h"
#include "restful.h"

#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0

// #include "detector.h"

static void create_restful_service_and_classify_threads()
{

  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);

  // Thread to communicate REST-fully with clients 
  // restful_comm_thread_start(&audioapp_inst, &restful_struct, &restful_dispatch_queue, (int *)&get_config()->daemon_port);

  // Thread to transcribe files when requested by clients
  // restful_classify_thread_start(restful_struct);

  return;

}

int main(int argc, char **argv)
{

  bool all_done = false, proc_thread_done = false;
  bool input_thread_done = false;

  fprintf(stdout, "Classify REST daemon (C) Copyright 2012-2018 igolgi Inc.\n");
  fprintf(stdout, "\nRELEASE: JAN 2019\n\n");

  basic_initialization(&argc, argv, "audioapp");

  /*
    Create a thread that waits to receive input JPG files to classify.
    This thread is configured based on cmdline parameters.
    
    This thread runs the test_detector function but keeps going 
    instead of giving up after 1 image detection
  */
  
  /* 
     Create a second thread that waits for REST-ful requests to come in.
     If the request is a valid one, then send it via msg. queue to the 
     1st classifier thread
  */
  
  return 0;
}

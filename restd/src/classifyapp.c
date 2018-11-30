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
static restful_comm_struct *restful_struct = NULL;
static void *restful_dispatch_queue = NULL;
static int restful_done_handling_req = 0;

classifyapp_struct classifyapp_inst;

static int start_classifyapp_config(restful_comm_struct *restful_data)
{
  int retval, sval1, sval2;
  classifyapp_struct *classifyapp_data = restful_data->classifyapp_data;
  
  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
  memset(classifyapp_data, 0, sizeof(classifyapp_struct));

  classifyapp_data->proc_thread_is_active = 1;
  classifyapp_data->input_thread_is_active = 1;

  // Message queue and packet semaphores between http_puller thread and audio_decoder thread
  classifyapp_data->input_queue_sem = (sem_t*)malloc(sizeof(sem_t));
  classifyapp_data->input_packet_sem = (sem_t*)malloc(sizeof(sem_t));
  sem_init(classifyapp_data->input_queue_sem, 0, 0);
  sem_init(classifyapp_data->input_packet_sem, 0, MAX_PACKETS);

  // Message queue and packet semaphores between audio_decoder thread and speech_processing thread
  /*classifyapp_data->speech_proc_queue_sem = (sem_t*)malloc(sizeof(sem_t));
  classifyapp_data->speech_proc_packet_sem = (sem_t*)malloc(sizeof(sem_t));
  sem_init(classifyapp_data->speech_proc_queue_sem, 0, 0);
  sem_init(classifyapp_data->speech_proc_packet_sem, 0, MAX_PACKETS);
  */
  
#ifdef SELF_PULL
  // Message queue and packet buffer pool between http_puller thread and audio_decoder thread
  classifyapp_data->input_packet_queue = (void*)message_queue_create();
  classifyapp_data->input_packet_buffer_pool = (void*)fast_buffer_pool_create(MAX_PACKETS, MAX_PACKET_SIZE);
#endif

  /*
  // Message queue and packet buffer pool between audio_decoder thread and speech_processing thread
  classifyapp_data->speech_proc_packet_queue = (void*)message_queue_create();
  classifyapp_data->speech_proc_packet_buffer_pool = (void*)fast_buffer_pool_create(MAX_PACKETS, MAX_WAV_PACKET_SIZE);
  */
  
  // Common buffer pool for http_puller thread & audio_decoder thread
  classifyapp_data->system_message_pool = (void*)fast_buffer_pool_create(MAX_PACKETS * NUM_PROC_THREADS, sizeof(igolgi_message_struct));

#ifdef SELF_PULL
  classifyapp_data->audio_decoder_buffer_pool = (void*)fast_buffer_pool_create(1, MAX_PACKET_SIZE*2);
  pthread_mutex_init(&classifyapp_data->http_input_thread_complete_mutex, NULL);
  classifyapp_data->http_input_thread_complete = false;
#endif

  /*
  pthread_mutex_init(&classifyapp_data->audio_decoder_thread_complete_mutex, NULL);
  pthread_mutex_init(&classifyapp_data->speech_proc_thread_complete_mutex, NULL);
  classifyapp_data->audio_decoder_thread_complete = false;
  classifyapp_data->speech_proc_thread_complete = false;
  */
  
  classifyapp_data->event_message_queue = (void*)message_queue_create();
  classifyapp_data->event_message_pool = (void*)buffer_pool_create(MAX_EVENT_BUFFERS, MAX_EVENT_SIZE, 1);  
  
  // Thread that pulls from URL
  /* retval = pthread_create(&classifyapp_data->input_thread_id, NULL, http_input_thread_func, (void*)classifyapp_data);
  if (retval < 0) {

    classifyapp_data->proc_thread_is_active = 0;
    classifyapp_data->input_thread_is_active = 0;

    // sem post?                                                                                                                                                 
    fprintf(stderr,"Unable to start http input thread!\n");
    return -1;

  } 
  */
  
  return 0;
}

static void *restful_classify_thread_func(void *context)
{
  restful_comm_struct *restful = (restful_comm_struct *)context;
  classifyapp_struct *classifyapp_info = restful->classifyapp_data;
  int done_handling_req = 0, cur_classify_status = -1;
  bool all_done = false, proc_thread_done = false, audio_decoder_thread_done = false, continue_after_params_parsing = false;
  char config_filename[LARGE_FIXED_STRING_SIZE];       
  uint16_t wait_counter = 0;
  bool input_thread_done = false;

  while(restful->is_classify_thread_active) {
    
    pthread_mutex_lock(&restful->thread_status_lock);
    restful->classify_thread_status = CLASSIFY_THREAD_STATUS_IDLE;
    pthread_mutex_unlock(&restful->thread_status_lock);	      
    
    done_handling_req = 0;
    continue_after_params_parsing = false;
    while (!done_handling_req) {

      igolgi_message_struct *dispatch_msg = (igolgi_message_struct*)message_queue_pop_back(restful->dispatch_queue);
      if (!dispatch_msg) {
	  
	if (!restful->is_classify_thread_active)
	  break;

	usleep(10000);
	continue;

      }
      
      //fprintf(stderr, "RECEIVED DISPATCH_MSG: %d\n", dispatch_msg->buffer_flags);	  	    
      free(dispatch_msg);
      dispatch_msg = NULL;
      
      syslog(LOG_INFO, "= RCVD DISPATCH_MSG | image_url: %s | output_directory: %s | output_fileprefix: %s\n",
	     classifyapp_info->appconfig.input_url, 
	     classifyapp_info->appconfig.output_directory,
	     classifyapp_info->appconfig.output_fileprefix);

      /* 
      // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
      if (check_input_params_for_sanity(restful) == 0)
	{
	  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
	  copy_to_recovery_configfile(restful);
	  continue_after_params_parsing = true;
	  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
	}
      else
	{
	  continue_after_params_parsing = false;
	  pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
	  restful->cur_classify_info.classify_status = CLASSIFY_STATUS_ERROR;
	  pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);
	  clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.end_timestamp);
	} 
      */
      
      done_handling_req = 1;
	
    }
    
    if (!restful->is_classify_thread_active)
      break;
  
    syslog(LOG_INFO, "continue? %d | %d, %s", continue_after_params_parsing, __LINE__, __FILE__);
    if (continue_after_params_parsing == false)
      continue;      

    // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
    start_classifyapp_config(restful);
    // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);

    //pthread_mutex_lock(&restful->classifyapp_init_lock);
    //restful->classifyapp_init = true;
    //pthread_mutex_unlock(&restful->classifyapp_init_lock);

    while (1) {

      pthread_mutex_lock(&classifyapp_info->http_input_thread_complete_mutex);
      input_thread_done = classifyapp_info->http_input_thread_complete;
      pthread_mutex_unlock(&classifyapp_info->http_input_thread_complete_mutex);

      /*
      pthread_mutex_lock(&classifyapp_info->speech_proc_thread_complete_mutex);
      proc_thread_done = classifyapp_info->speech_proc_thread_complete;
      pthread_mutex_unlock(&classifyapp_info->speech_proc_thread_complete_mutex);
      */
      
      if ( (input_thread_done) && (audio_decoder_thread_done) && (proc_thread_done) )
	break;

      /* pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
      cur_classify_status = restful->cur_classify_info.classify_status;
      pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);
      if (cur_classify_status == CLASSIFY_STATUS_STOPPED) 
	break;
      */
      
      usleep(100000);

      /*
      wait_counter += 1;
      
      // Every 300 100ms intervals or 30s, print out debug info (likely useful when 
      // process hangs
      if (wait_counter >= NUM_MAIN_INTERVALS) {
	
	int sval1 = -1, sval2 = -1;
	
	sem_getvalue(classifyapp_inst.speech_proc_packet_sem, &sval1);
	sem_getvalue(classifyapp_inst.speech_proc_queue_sem, &sval2);
	syslog(LOG_INFO, 
	       "= MAIN (localhost:%d) | SPEECH_PROC_PKT_BUFPOOL COUNT: %d | SYS_MSGPOOL_COUNT: %d | PKT_SEM: %d, QUEUE_SEM: %d | decode/speech thread done? %d/%d", 
	       get_config()->daemon_port,
	       fast_buffer_pool_count(classifyapp_inst.speech_proc_packet_buffer_pool), 
	       fast_buffer_pool_count(classifyapp_inst.system_message_pool), sval1, sval2,
	       audio_decoder_thread_done, proc_thread_done);
	
	wait_counter = 0;
	
      }
      */
      
    }

    /* DEBUG_PRINTF("= URL: %s | Transfer DONE: %0.0f bytes @ %0.1f Kbps | Bytes_pushed: %0.0f bytes | processed: %0.0f bytes | cur_classify_status: %d\n", 
		 classifyapp_info->appconfig.input_url,
		 classifyapp_info->bytes_pulled, 
		 classifyapp_info->input_mux_rate,
		 classifyapp_info->bytes_pushed,
		 classifyapp_info->bytes_processed,
		 cur_classify_status);
    */

    /*
    // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
    stop_classifyapp_config(classifyapp_info);
    */
    
    syslog(LOG_INFO, "= Setting end_timestamp | restful_ptr: %p | %s:%d", restful, __FILE__, __LINE__);
    clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.end_timestamp);
      
  }
  
  syslog(LOG_INFO, "= Leaving restful_classify_thread, line:%d, %s", __LINE__, __FILE__);

  return NULL;
}


int restful_classify_thread_start(restful_comm_struct *restful)
{
  if (restful) {
    restful->is_classify_thread_active = 1;
    pthread_create(&restful->classify_thread_id, NULL, restful_classify_thread_func, (void*)restful);
  }
  return 0;
}

int restful_classify_thread_stop(restful_comm_struct *restful)
{
  if (restful) {
    restful->is_classify_thread_active = 0;
    message_queue_push_front(restful->dispatch_queue, NULL);
    pthread_join(restful->classify_thread_id, NULL);
  }
  return 0;
}


int main(int argc, char **argv)
{

  bool all_done = false, proc_thread_done = false;
  bool input_thread_done = false;

  fprintf(stdout, "Classify REST daemon (C) Copyright 2018-2019 igolgi Inc.\n");
  fprintf(stdout, "\nRELEASE: JAN 2019\n\n");

  basic_initialization(&argc, argv, "classifyapp");

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
  restful_comm_thread_start(&classifyapp_inst, &restful_struct, &restful_dispatch_queue, (int *)&get_config()->daemon_port);

  // Thread to pull and classify files when requested by clients
  restful_classify_thread_start(restful_struct);
  
  /*
  if (start_classifyapp_config(restful_struct) < 0) {
    DEBUG_PRINTF("Could not configure classifyapp! Exiting!\n");
    exit(-1);
  }
  */

 wait_for_exit:
  while(1)
    {
      pthread_mutex_lock(&classifyapp_inst.http_input_thread_complete_mutex);
      input_thread_done = classifyapp_inst.http_input_thread_complete;
      pthread_mutex_unlock(&classifyapp_inst.http_input_thread_complete_mutex);

      if (input_thread_done)
	break;

      usleep(100000);
    }

  syslog(LOG_INFO, "= CLASSIFYAPP DONE FOR URL: %s | processed: %0.0f bytes\n",
	 get_config()->image_url, classifyapp_inst.bytes_processed);
  
  return 0;
}

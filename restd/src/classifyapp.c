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
#include "common.h"
#include "darknet.h"

#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0

// #include "detector.h"
static restful_comm_struct *restful_struct = NULL;
static void *restful_dispatch_queue = NULL;

extern void test_detector(char *datacfg, char *cfgfile, char *weightfile, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen);
extern void prepare_detector_custom(struct prep_network_info *prep_netinfo, char *datacfg, char *cfgfile, char *weightfile);
extern void run_detector_custom(struct prep_network_info *prep_netinfo, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen, struct detection_results *results_info);
extern void free_detector_internal_datastructures(struct prep_network_info *prep_netinfo);

classifyapp_struct classifyapp_inst;
struct sigaction sa;

void handle_signal(int signal)
{
  const char *signal_name = NULL;
  // sigset_t pending;

  // Find out which signal we're handling
  switch (signal) {
  case SIGHUP:
    signal_name = "SIGHUP";
    break;
  case SIGUSR1:
    signal_name = "SIGUSR1";
    break;
  case SIGINT:
    signal_name = "SIGINT";
    syslog(LOG_INFO, "= Caught SIGINT, restful_struct: %p", restful_struct);
    break;
    //exit(0);
  default:
    fprintf(stderr, "Caught wrong signal: %d\n", signal);
    return;
  }

  if (restful_struct)
    {
      restful_struct->is_classify_thread_active = 0;
      message_queue_push_front(restful_struct->dispatch_queue, NULL);
      restful_struct->is_restful_thread_active = 0;
    }

  if (signal_name != NULL)
    syslog(LOG_INFO, "= Done handling %s - wait for app to quit", signal_name);
  return;
  
}


/* 
static int stop_classifyapp_config(classifyapp_struct *classifyapp_data)
{
  // bool audio_thread_complete = false, speech_thread_complete = false;

  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);

  classifyapp_data->proc_thread_is_active = 0;
  classifyapp_data->input_thread_is_active = 0;

  // pthread_join(classifyapp_data->proc_thread_id, NULL);

  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);

  pthread_join(classifyapp_data->input_thread_id, NULL);


  //fast_buffer_pool_destroy(classifyapp_data->system_message_pool);
  //classifyapp_data->system_message_pool = NULL;

  pthread_mutex_destroy(&classifyapp_data->http_input_thread_complete_mutex);
  
  //if (classifyapp_data->event_message_queue) {
  //  message_queue_destroy(classifyapp_data->event_message_queue);
  //  classifyapp_data->event_message_queue = NULL;
  //}
  //if (classifyapp_data->event_message_pool) {
  //  buffer_pool_destroy(classifyapp_data->event_message_pool);
  //  classifyapp_data->event_message_pool = NULL;
  //}
  
  // remove_files_used_to_recover_from_crash();  

  return 0;
}

static int start_classifyapp_config(restful_comm_struct *restful_data)
{
  int retval; //, sval1, sval2;
  classifyapp_struct *classifyapp_data = restful_data->classifyapp_data;
  
  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
  memset(classifyapp_data, 0, sizeof(classifyapp_struct));

  classifyapp_data->proc_thread_is_active = 1;
  classifyapp_data->input_thread_is_active = 1;

  // Message queue and packet semaphores between audio_decoder thread and speech_processing thread
  //classifyapp_data->speech_proc_queue_sem = (sem_t*)malloc(sizeof(sem_t));
  //classifyapp_data->speech_proc_packet_sem = (sem_t*)malloc(sizeof(sem_t));
  //sem_init(classifyapp_data->speech_proc_queue_sem, 0, 0);
  //sem_init(classifyapp_data->speech_proc_packet_sem, 0, MAX_PACKETS);
    
  
  // Message queue and packet buffer pool between audio_decoder thread and speech_processing thread
  //classifyapp_data->speech_proc_packet_queue = (void*)message_queue_create();
  //classifyapp_data->speech_proc_packet_buffer_pool = (void*)fast_buffer_pool_create(MAX_PACKETS, MAX_WAV_PACKET_SIZE);
  
  // Common buffer pool for http_puller thread & audio_decoder thread
  // classifyapp_data->system_message_pool = (void*)fast_buffer_pool_create(MAX_PACKETS * NUM_PROC_THREADS, sizeof(igolgi_message_struct));


#ifdef SELF_PULL
  // classifyapp_data->audio_decoder_buffer_pool = (void*)fast_buffer_pool_create(1, MAX_PACKET_SIZE*2);
  pthread_mutex_init(&classifyapp_data->http_input_thread_complete_mutex, NULL);
  classifyapp_data->http_input_thread_complete = false;
#endif

  //pthread_mutex_init(&classifyapp_data->audio_decoder_thread_complete_mutex, NULL);
  //pthread_mutex_init(&classifyapp_data->speech_proc_thread_complete_mutex, NULL);
  //classifyapp_data->audio_decoder_thread_complete = false;
  //classifyapp_data->speech_proc_thread_complete = false;
  
  //classifyapp_data->event_message_queue = (void*)message_queue_create();
  //classifyapp_data->event_message_pool = (void*)buffer_pool_create(MAX_EVENT_BUFFERS, MAX_EVENT_SIZE, 1);  


  // Thread that pulls from URL
  retval = pthread_create(&classifyapp_data->input_thread_id, NULL, http_input_thread_func, (void*)classifyapp_data);
  if (retval < 0) {

    classifyapp_data->proc_thread_is_active = 0;
    classifyapp_data->input_thread_is_active = 0;

    // sem post?                                                                                                                                                 
    fprintf(stderr,"Unable to start http input thread!\n");
    return -1;

  } 
    
  return 0;
}
*/

static int check_input_url(restful_comm_struct *restful_ptr) 
{

  int txtcheck = -1, retval = 0;
  char *samplefilename = NULL, *filecmd = NULL;
  
  if (config_curl_and_pull_file_sample(restful_ptr->classifyapp_data) < 0)
    {
      syslog(LOG_ERR, "= Could not check file for file type from input_url, %s:%d", __FILE__, __LINE__);
      retval = -1;
      goto exit_check_input_url;
    }    

  if (asprintf(&samplefilename, "%s/%s", restful_ptr->classifyapp_data->appconfig.output_directory, basename(get_config()->image_url)) < 0)
    {
      syslog(LOG_ERR, "= Could not asprintf samplefilename, %s:%d", __FILE__, __LINE__);
      retval = -1;
      goto exit_check_input_url;
    }
  
  if (asprintf(&filecmd, "/usr/bin/file %s | grep text", samplefilename) < 0)
    {
      syslog(LOG_ERR, "= Could not asprintf filecmd, %s:%d", __FILE__, __LINE__);
      free(samplefilename);
      samplefilename = NULL;
      retval = -1;
      goto exit_check_input_url;
    }

  txtcheck = system(filecmd);  

  free(samplefilename);
  samplefilename = NULL;
  
  free(filecmd);
  filecmd = NULL;
  
  if (!txtcheck)
    {
      retval = -1;
      goto exit_check_input_url;
    }
  
  if ( !( (ends_with("png", get_config()->image_url) == true) || (ends_with("jpg", get_config()->image_url) == true) ) )
    {
      syslog(LOG_ERR, "= Cannot support URLs that don't end in .jpg or .png yet, current_url: %s %s:%d", get_config()->image_url, __FILE__, __LINE__);
      retval = -1;
    }

 exit_check_input_url:
  return retval;

}

static int check_output_dir(restful_comm_struct *restful_ptr) 
{

  // Check if directory exists 
  if ( access(get_config()->output_directory, F_OK ) == -1 ) {
    syslog(LOG_ERR, "= Output directory %s does not exist", get_config()->output_directory);
    pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
    restful_ptr->cur_classify_info.classify_status = CLASSIFY_STATUS_OUTPUT_ERROR;
    pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);
    return -1;	  	
  }

  return 0;
}

static int check_input_file(restful_comm_struct *restful_ptr) 
{

  // Check if directory exists 
  if ( access(get_config()->image_url, F_OK ) == -1 ) {
    syslog(LOG_ERR, "= Input file %s does not exist", get_config()->image_url);
    pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
    restful_ptr->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
    pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);    
    return -1;	  	
  }

  return 0;
}

/*
static int check_config(restful_comm_struct *restful_ptr) 
{

  // Check if the file exists on local disk
  if ( access(get_config()->acoustic_model, F_OK ) == -1 ) {    
    syslog(LOG_ERR, "= Acoustic model %s not found at specified location", get_config()->acoustic_model);
    pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
    restful_ptr->cur_classify_info.classify_status = HTTP_400;	
    pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);
    return -1;	  	
  }

  // Check if the file exists on local disk
  if ( access(get_config()->dictionary, F_OK ) == -1 ) {
    syslog(LOG_ERR, "= Dictionary %s not found at specified location", get_config()->dictionary);
    pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
    restful_ptr->cur_classify_info.classify_status = HTTP_400;	
    pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);    
    return -1;	  	
  }

  // Check if the file exists on local disk
  if ( access(get_config()->language_model, F_OK ) == -1 ) {	
    syslog(LOG_ERR, "= Language_Model %s not found at specified location", get_config()->language_model);
    pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
    restful_ptr->cur_classify_info.classify_status = HTTP_400;	
    pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);    
    return -1;	  	
  }

  return 0;

}
*/

static int check_input_params_for_sanity(restful_comm_struct *restful_ptr) 
{
  classifyapp_struct *cur_classifyapp_data = restful_ptr->classifyapp_data;
  
  if ((!strcmp(cur_classifyapp_data->appconfig.input_type, "stream")) && (check_input_url(restful_ptr) < 0))
    {
      syslog(LOG_ERR, "= Input URL invalid | line:%d, %s", __LINE__, __FILE__);
      return -1;
    }

  if ((!strcmp(cur_classifyapp_data->appconfig.input_type, "file")) && (check_input_file(restful_ptr) < 0))
    {
      syslog(LOG_ERR, "= Input FILE invalid | line:%d, %s", __LINE__, __FILE__);
      return -1;
    }

  if (check_output_dir(restful_ptr) < 0)
    {
      syslog(LOG_ERR, "= Output directory does not exist | line:%d, %s", __LINE__, __FILE__);
      return -1;
    }

  /* if (check_config(restful_ptr) < 0)
    {
      syslog(LOG_ERR, "= acoustic_model/dictionary/language_model does not exist | line:%d, %s", __LINE__, __FILE__);
      return -1;
    }
  */
  
  return 0;

}

static void *restful_classify_thread_func(void *context)
{
  restful_comm_struct *restful = (restful_comm_struct *)context;
  classifyapp_struct *classifyapp_info = restful->classifyapp_data;
  int done_handling_req = 0, i = 0;
  // bool all_done = false, proc_thread_done = false, audio_decoder_thread_done = false, continue_after_params_parsing = false;
  bool continue_after_params_parsing = false;
  // char config_filename[LARGE_FIXED_STRING_SIZE];       
  // uint16_t wait_counter = 0;
  // bool input_thread_done = false;
  struct prep_network_info prep_netinfo_inst;

  memset(&restful->cur_classify_info.results_info, 0, sizeof(struct detection_results));
  prepare_detector_custom(&prep_netinfo_inst,
			  (char *)get_config()->dnn_data_file,
			  (char *)get_config()->dnn_config_file,
			  (char *)get_config()->dnn_weights_file);
  
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
	  {
	    syslog(LOG_INFO, "= About to leave restful_classify_thread, line:%d, %s", __LINE__, __FILE__);
	    break;
	  }
	
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

      // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
      if (check_input_params_for_sanity(restful) == 0)
	{
	  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
	  // copy_to_recovery_configfile(restful);
	  continue_after_params_parsing = true;
	  // syslog(LOG_INFO, "HERE %d, %s", __LINE__, __FILE__);
	}
      else
	{
	  continue_after_params_parsing = false;
	  pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
	  restful->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
	  pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);
	  clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.end_timestamp);
	} 
      
      done_handling_req = 1;
	
    }
    
    if (!restful->is_classify_thread_active)
      break;
  
    syslog(LOG_DEBUG, "= Continue? %d | %d, %s", continue_after_params_parsing, __LINE__, __FILE__);
    if (continue_after_params_parsing == false)
      continue;      

    if (!strcmp(restful->classifyapp_data->appconfig.input_type, "stream"))
      {
	if (config_curl_and_pull_file(classifyapp_info) < 0)
	  {
	    syslog(LOG_ERR, "= Could not config. curl and pull file, %s:%d", __FILE__, __LINE__);
	    pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
	    restful->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
	    pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);
	    clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.end_timestamp);
	    continue;
	  }
      }
    else
      {
	memset(restful->classifyapp_data->appconfig.input_filename, 0, LARGE_FIXED_STRING_SIZE);
	snprintf(restful->classifyapp_data->appconfig.input_filename, LARGE_FIXED_STRING_SIZE-1, "%s", get_config()->image_url);
      }

    syslog(LOG_INFO, "= About to run the detect+classify, %s:%d", __FILE__, __LINE__);
  
    run_detector_custom(&prep_netinfo_inst,
			restful->classifyapp_data->appconfig.input_filename,
			restful->classifyapp_data->appconfig.detection_threshold,
			.5,
			"/tmp/predictions.png",
			0,
			&restful->cur_classify_info.results_info);

    syslog(LOG_INFO, "= Done with detect+classify, %s:%d", __FILE__, __LINE__);
    
    /* 
    test_detector("/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/coco.data",
		  "/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/yolov3-tiny.cfg",
		  "/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/yolov3-tiny.weights",
		  restful->classifyapp_data->appconfig.input_filename,
		  0.5,
		  .5,
		  "/tmp/predictions.png",
		  0);
    */
    
    syslog(LOG_DEBUG, "= Setting end_timestamp | restful_ptr: %p | %s:%d", restful, __FILE__, __LINE__);
    clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.end_timestamp);
    pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
    restful->cur_classify_info.classify_status = CLASSIFY_STATUS_COMPLETED;
    pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);

    syslog(LOG_INFO, "= Num. labels detected: %d in time: %0.2f milliseconds",
	   restful->cur_classify_info.results_info.num_labels_detected,
	   restful->cur_classify_info.results_info.processing_time_in_seconds * 1000.0);
    for (i = 0; i<restful->cur_classify_info.results_info.num_labels_detected; i++)
      syslog(LOG_INFO, "= Label #: %d | name: %s | confidence: %0.2f%% | tl: (%d,%d), tr: (%d,%d), bl: (%d,%d), br: (%d,%d)",
	     i,
	     restful->cur_classify_info.results_info.labels[i],
	     restful->cur_classify_info.results_info.confidence[i],
	     restful->cur_classify_info.results_info.top_left_x[i],
	     restful->cur_classify_info.results_info.top_left_y[i],
	     restful->cur_classify_info.results_info.top_right_x[i],
	     restful->cur_classify_info.results_info.top_right_y[i],
	     restful->cur_classify_info.results_info.bottom_left_x[i],
	     restful->cur_classify_info.results_info.bottom_left_y[i],
	     restful->cur_classify_info.results_info.bottom_right_x[i],
	     restful->cur_classify_info.results_info.bottom_right_y[i]);
    
  }

  syslog(LOG_INFO, "= About to leave restful_classify_thread, line:%d, %s", __LINE__, __FILE__);
  free_detector_internal_datastructures(&prep_netinfo_inst);
  syslog(LOG_INFO, "= Leaving restful_classify_thread, line:%d, %s", __LINE__, __FILE__);
  
  return NULL;
}


int restful_classify_thread_start(restful_comm_struct *restful)
{
  if (restful) {
    restful->is_classify_thread_active = 1;
    syslog(LOG_INFO, "= Creating classify_thread");
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

  // Setup the sighub handler
  sa.sa_handler = &handle_signal;

  // Restart the system call, if at all possible
  sa.sa_flags = SA_RESTART;

  // Block every signal during the handler
  sigfillset(&sa.sa_mask);

  // Intercept SIGHUP and SIGINT
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    perror("Error: cannot handle SIGHUP"); // Should not happen
  }

  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("Error: cannot handle SIGUSR1"); // Should not happen
  }

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Error: cannot handle SIGINT"); // Should not happen
  }

  /*
  if (start_classifyapp_config(restful_struct) < 0) {
    DEBUG_PRINTF("Could not configure classifyapp! Exiting!\n");
    exit(-1);
  }
  */

  pthread_join(restful_struct->classify_thread_id, NULL);
  pthread_join(restful_struct->restful_server_thread_id, NULL);
  message_queue_destroy(restful_struct->dispatch_queue);
  
  syslog(LOG_INFO, "= CLASSIFYAPP DONE FOR URL: %s | processed: %0.0f bytes\n",
	 get_config()->image_url, classifyapp_inst.bytes_processed);
  
  return 0;
}

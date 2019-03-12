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
#include "classifyapp.h"
#include "darknet.h"

#define APP_VERSION_MAJOR    1
#define APP_VERSION_MINOR    0

// #include "detector.h"
static restful_comm_struct *restful_struct = NULL;
static void *restful_dispatch_queue = NULL;

extern void prepare_detector_custom(struct prep_network_info *prep_netinfo, char *datacfg, char *cfgfile, char *weightfile);
extern void run_detector_custom(struct prep_network_info *prep_netinfo, char *filename, float thresh, float hier_thresh, char *outfile, int fullscreen,
				struct detection_results *results_info);
extern void run_detector_custom_video(struct prep_network_info *prep_netinfo,
				      char *filename,
				      float thresh,
				      float hier_thresh,
				      char *outfile,
				      char *outjson,
				      struct detection_results *results_info);
extern void free_detector_internal_datastructures(struct prep_network_info *prep_netinfo);

// classifyapp_struct classifyapp_inst[MAX_MESSAGES_PER_WINDOW];
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

static int check_file_type(char *filepath, char *filetype_to_check)
{
  char *filecmd = NULL;
  int typecheck = -1;
  
  if (asprintf(&filecmd, "/usr/bin/file %s | grep %s", filepath, filetype_to_check) < 0)
    {
      syslog(LOG_ERR, "= Could not asprintf filecmd, %s:%d", __FILE__, __LINE__);
      return -1;
    }
  
  typecheck = system(filecmd);  
  
  free(filecmd);
  filecmd = NULL;

  return typecheck;
  
}

static int fill_input_mode_based_on_file_type(classifyapp_struct *classifyapp_data, char *filename_to_check)
{
  int retval = 0, typecheck = -1;
  
  // Check file type
  typecheck = check_file_type(filename_to_check, "image");
  if (typecheck < 0)
    retval = -1;
  else if (typecheck == 0)
    snprintf(classifyapp_data->appconfig.input_mode, SMALL_FIXED_STRING_SIZE-1, "%s", "image");
  else
    {
      typecheck = check_file_type(filename_to_check, "Media");
      if (typecheck < 0)
	retval = -1;
      else if (typecheck == 0)
	snprintf(classifyapp_data->appconfig.input_mode, SMALL_FIXED_STRING_SIZE-1, "%s", "video");
      else
	{
	  syslog(LOG_ERR, "= Could not find input mode, %s:%d", __FILE__, __LINE__);
	  retval = -1;
	}
    }   

  if (typecheck == 0)
    {
      memset(mod_config()->input_mode, 0, SMALL_FIXED_STRING_SIZE);
      if (strlen(classifyapp_data->appconfig.input_mode) > 0)
	memcpy(mod_config()->input_mode, classifyapp_data->appconfig.input_mode, strlen(classifyapp_data->appconfig.input_mode));
    }
  return retval;
  
}

static int check_input_url(restful_comm_struct *restful_ptr) 
{

  int typecheck = -1, retval = 0, next_post_id = -1;
  char *samplefilename = NULL;
  classifyapp_struct *cur_classifyapp_data = NULL;

  pthread_mutex_lock(&restful_ptr->next_post_id_lock);
  next_post_id = restful_ptr->next_post_classify_id;
  pthread_mutex_unlock(&restful_ptr->next_post_id_lock);

  if (next_post_id >= 0)
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data); 
  else
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + (next_post_id-1)); 

  if (get_config()->fastmode == false)
    {
      if (config_curl_and_pull_file_sample(cur_classifyapp_data) < 0)
	{
	  syslog(LOG_ERR, "= Could not check file for file type from input_url, %s:%d", __FILE__, __LINE__);
	  retval = -1;
	  goto exit_check_input_url;
	}    

      if (asprintf(&samplefilename, "%s/%s", cur_classifyapp_data->appconfig.output_directory, basename(get_config()->image_url)) < 0)
	{
	  syslog(LOG_ERR, "= Could not asprintf samplefilename, %s:%d", __FILE__, __LINE__);
	  retval = -1;
	  goto exit_check_input_url;
	}
  
      // Check if this is a text file to reject it
      typecheck = check_file_type(samplefilename, "text");
      if (typecheck <= 0) // if its an error (typecheck = -1) or a text file (typecheck = 0)
	{

	  free(samplefilename);
	  samplefilename = NULL;
      
	  retval = -1;
	  goto exit_check_input_url;
      
	}

      if (strlen(cur_classifyapp_data->appconfig.input_mode) == 0)
	{

	  if (fill_input_mode_based_on_file_type(cur_classifyapp_data, samplefilename) < 0)
	    {

	      free(samplefilename);
	      samplefilename = NULL;
	  
	      retval = -1;
	      goto exit_check_input_url;
	  
	    }

	  syslog(LOG_INFO, "= INPUT MODE: %s, %s:%d", cur_classifyapp_data->appconfig.input_mode, __FILE__, __LINE__);
      
	}

      free(samplefilename);
      samplefilename = NULL;
    
      if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "image"))
	{
      
	  if ( !( (ends_with("png", get_config()->image_url) == true) || (ends_with("jpg", get_config()->image_url) == true) ) )
	    {
	      syslog(LOG_ERR, "= Cannot support URLs that don't end in .jpg or .png yet, current_url: %s %s:%d", get_config()->image_url, __FILE__, __LINE__);
	      retval = -1;
	    }
      
	}
      else if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "video"))
	{
      
	  if ( !( (ends_with("mp4", get_config()->image_url) == true) || (ends_with("ts", get_config()->image_url) == true) ) )
	    {
	      syslog(LOG_ERR, "= Cannot support URLs that don't end in .ts or .mp4 yet, current_url: %s %s:%d", get_config()->image_url, __FILE__, __LINE__);
	      retval = -1;
	    }

	  memset(mod_config()->output_json_filepath, 0, LARGE_FIXED_STRING_SIZE);
	  memcpy(mod_config()->output_json_filepath, cur_classifyapp_data->appconfig.output_json_filepath, strlen(cur_classifyapp_data->appconfig.output_json_filepath));
      
	}
    }
  else
    {
      if (strlen(cur_classifyapp_data->appconfig.input_mode) == 0)
	{
	  if ( (ends_with("png", get_config()->image_url) == true) || (ends_with("jpg", get_config()->image_url) == true) )
	    snprintf(cur_classifyapp_data->appconfig.input_mode, SMALL_FIXED_STRING_SIZE-1, "%s", "image");
	  else if ( (ends_with("mp4", get_config()->image_url) == true) || (ends_with("ts", get_config()->image_url) == true) )
	    snprintf(cur_classifyapp_data->appconfig.input_mode, SMALL_FIXED_STRING_SIZE-1, "%s", "video");
	  else
	    {
	      syslog(LOG_ERR, "= Cannot support URLs that don't end in .jpg or .png or .mp4 or .ts yet, current_url: %s %s:%d", get_config()->image_url, __FILE__, __LINE__);
	      retval = -1;
	    }

	  memset(mod_config()->input_mode, 0, SMALL_FIXED_STRING_SIZE);
	  if (strlen(cur_classifyapp_data->appconfig.input_mode) > 0)
	    memcpy(mod_config()->input_mode, cur_classifyapp_data->appconfig.input_mode, strlen(cur_classifyapp_data->appconfig.input_mode));
	  
	}
    }
 exit_check_input_url:
  return retval;

}

static int check_output_dir(restful_comm_struct *restful_ptr) 
{

  int next_post_id = -1;
  classifyapp_struct *cur_classifyapp_data = NULL;
  
  pthread_mutex_lock(&restful_ptr->next_post_id_lock);
  next_post_id = restful_ptr->next_post_classify_id;
  pthread_mutex_unlock(&restful_ptr->next_post_id_lock);

  if (next_post_id >= 0)
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data); 
  else
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + (next_post_id-1)); 

  // Check if directory exists 
  if ( access(get_config()->output_directory, F_OK ) == -1 ) {
    syslog(LOG_ERR, "= Output directory %s does not exist", get_config()->output_directory);
    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_OUTPUT_ERROR;
    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    return -1;	  	
  }

  return 0;
}

static int check_input_file(restful_comm_struct *restful_ptr) 
{

  int next_post_id = -1;
  classifyapp_struct *cur_classifyapp_data = NULL;
  
  pthread_mutex_lock(&restful_ptr->next_post_id_lock);
  next_post_id = restful_ptr->next_post_classify_id;
  pthread_mutex_unlock(&restful_ptr->next_post_id_lock);

  if (next_post_id >= 0)
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data); 
  else
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + (next_post_id-1)); 
    
  // Check if input file exists 
  if ( access(get_config()->image_url, F_OK ) == -1 ) {
    syslog(LOG_ERR, "= Input file %s does not exist", get_config()->image_url);
    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);    
    return -1;	  	
  }

  if (strlen(cur_classifyapp_data->appconfig.input_mode) == 0)
    {
      
      if (fill_input_mode_based_on_file_type(cur_classifyapp_data, (char *)get_config()->image_url) < 0)
	{

	  syslog(LOG_ERR, "= Input's %s mode could not be determined, %s: %d", get_config()->image_url, __FILE__, __LINE__);
	  pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	  cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
	  pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);    
	  return -1;
	  
	}

      syslog(LOG_INFO, "= INPUT MODE: %s, %s:%d", cur_classifyapp_data->appconfig.input_mode, __FILE__, __LINE__);
      
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
  int next_post_id = -1;
  classifyapp_struct *cur_classifyapp_data = NULL;
  
  pthread_mutex_lock(&restful_ptr->next_post_id_lock);
  next_post_id = restful_ptr->next_post_classify_id;
  pthread_mutex_unlock(&restful_ptr->next_post_id_lock);

  if (next_post_id >= 0)
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data); 
  else
    cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + (next_post_id-1)); 
  
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
  int done_handling_req = 0, i = 0, next_post_id = -1;
  bool continue_after_params_parsing = false;
  struct prep_network_info prep_netinfo_inst;
  classifyapp_struct *cur_classifyapp_data = NULL;
  int cur_dispatch_queue_size = -1, cur_classify_thread_status;
  igolgi_message_struct *dispatch_msg = NULL;
  
  // Initialization
  for (i = 0; i<MAX_MESSAGES_PER_WINDOW; i++)
    {
      cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + i);
      memset(&cur_classifyapp_data->cur_classify_info.results_info, 0, sizeof(struct detection_results));
    }

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

      // Check here? or rely on process thread to mark BUSY signal?
      cur_dispatch_queue_size = message_queue_count(restful->dispatch_queue);
      pthread_mutex_lock(&restful->thread_status_lock);
      cur_classify_thread_status = restful->classify_thread_status;
      pthread_mutex_unlock(&restful->thread_status_lock);	      

      if ( (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_IDLE) && (cur_dispatch_queue_size > (MAX_MESSAGES_PER_WINDOW-1) ) )
	{
	  syslog(LOG_INFO, "= SETTING THREAD_STATUS TO BUSY, Dispatch message queue size: %d, %s:%d", cur_dispatch_queue_size, __FILE__, __LINE__);
	  pthread_mutex_lock(&restful->thread_status_lock);
	  restful->classify_thread_status = CLASSIFY_THREAD_STATUS_BUSY;
	  cur_classify_thread_status = restful->classify_thread_status;
	  pthread_mutex_unlock(&restful->thread_status_lock);	      
	}

      if ( (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_BUSY) && (cur_dispatch_queue_size < MAX_MESSAGES_PER_WINDOW) )
	{
	  syslog(LOG_INFO, "= SETTING THREAD_STATUS TO IDLE, Dispatch message queue size: %d, %s:%d", cur_dispatch_queue_size, __FILE__, __LINE__);
	  pthread_mutex_lock(&restful->thread_status_lock);
	  restful->classify_thread_status = CLASSIFY_THREAD_STATUS_IDLE;
	  cur_classify_thread_status = restful->classify_thread_status;
	  pthread_mutex_unlock(&restful->thread_status_lock);	      
	}

      // dispatch_msg = (igolgi_message_struct*)message_queue_pop_back(restful->dispatch_queue);
      
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

      pthread_mutex_lock(&restful->next_post_id_lock);
      next_post_id = restful->next_post_classify_id;
      pthread_mutex_unlock(&restful->next_post_id_lock);

      if (next_post_id >= 0)
	cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data); 
      else
	cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + (next_post_id-1)); 

      syslog(LOG_DEBUG, "= RCVD DISPATCH_MSG | input_url: %s | output_directory: %s | output_filepath: %s",
	     cur_classifyapp_data->appconfig.input_url, 
	     cur_classifyapp_data->appconfig.output_directory,
	     cur_classifyapp_data->appconfig.output_filepath);

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
	  pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	  cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
	  pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	  clock_gettime(CLOCK_REALTIME, &cur_classifyapp_data->cur_classify_info.end_timestamp);
	} 
      
      done_handling_req = 1;
	
    }
    
    if (!restful->is_classify_thread_active)
      break;
  
    syslog(LOG_DEBUG, "= Continue? %d | %d, %s", continue_after_params_parsing, __LINE__, __FILE__);
    if (continue_after_params_parsing == false)
      continue;      

    if (!strcmp(cur_classifyapp_data->appconfig.input_type, "stream"))
      {
	if (config_curl_and_pull_file(cur_classifyapp_data) < 0)
	  {
	    syslog(LOG_ERR, "= Could not config. curl and pull file, %s:%d", __FILE__, __LINE__);
	    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	    cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_INPUT_ERROR;
	    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	    clock_gettime(CLOCK_REALTIME, &cur_classifyapp_data->cur_classify_info.end_timestamp);
	    continue;
	  }
      }
    else
      {
	memset(cur_classifyapp_data->appconfig.input_filename, 0, LARGE_FIXED_STRING_SIZE);
	snprintf(cur_classifyapp_data->appconfig.input_filename, LARGE_FIXED_STRING_SIZE-1, "%s", cur_classifyapp_data->appconfig.input_url);
      }

    if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "image"))
      {
	syslog(LOG_DEBUG, "= About to run the detect+classify for %s: %s, %s:%d", cur_classifyapp_data->appconfig.input_mode,
	       cur_classifyapp_data->appconfig.input_filename,
	       __FILE__, __LINE__);
  
	run_detector_custom(&prep_netinfo_inst,
			    cur_classifyapp_data->appconfig.input_filename,
			    cur_classifyapp_data->appconfig.detection_threshold,
			    .5,
			    cur_classifyapp_data->appconfig.output_filepath,
			    0,
			    &cur_classifyapp_data->cur_classify_info.results_info);
	
	syslog(LOG_INFO, "= Done with detect+classify, %s:%d", __FILE__, __LINE__);
	
	/* 
	   test_detector("/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/coco.data",
	   "/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/yolov3-tiny.cfg",
	   "/home/igolgi/cnn/yolo/rkishore/darknet/restd/cfg/yolov3-tiny.weights",
	   cur_classifyapp_data->appconfig.input_filename,
	   0.5,
	   .5,
	   "/tmp/predictions.png",
	   0);
	*/
	
	syslog(LOG_DEBUG, "= Setting end_timestamp | restful_ptr: %p | %s:%d", restful, __FILE__, __LINE__);
	clock_gettime(CLOCK_REALTIME, &cur_classifyapp_data->cur_classify_info.end_timestamp);
	pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_COMPLETED;
	pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);

	syslog(LOG_INFO, "= Num. labels detected: %d in time: %0.2f milliseconds",
	       cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected,
	       cur_classifyapp_data->cur_classify_info.results_info.processing_time_in_seconds * 1000.0);

	for (i = 0; i<cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected; i++)
	  syslog(LOG_INFO, "= Label #: %d | name: %s | confidence: %0.2f%% | tl: (%d,%d), tr: (%d,%d), bl: (%d,%d), br: (%d,%d)",
		 i,
		 cur_classifyapp_data->cur_classify_info.results_info.labels[i],
		 cur_classifyapp_data->cur_classify_info.results_info.confidence[i],
		 cur_classifyapp_data->cur_classify_info.results_info.top_left_x[i],
		 cur_classifyapp_data->cur_classify_info.results_info.top_left_y[i],
		 cur_classifyapp_data->cur_classify_info.results_info.top_right_x[i],
		 cur_classifyapp_data->cur_classify_info.results_info.top_right_y[i],
		 cur_classifyapp_data->cur_classify_info.results_info.bottom_left_x[i],
		 cur_classifyapp_data->cur_classify_info.results_info.bottom_left_y[i],
		 cur_classifyapp_data->cur_classify_info.results_info.bottom_right_x[i],
		 cur_classifyapp_data->cur_classify_info.results_info.bottom_right_y[i]);
	
      }
    else
      {

	syslog(LOG_INFO, "= Detect+classify for %s: %s, %s:%d",
	       cur_classifyapp_data->appconfig.input_mode,
	       cur_classifyapp_data->appconfig.input_filename,
	       __FILE__, __LINE__);

	run_detector_custom_video(&prep_netinfo_inst,
				  cur_classifyapp_data->appconfig.input_filename,
				  cur_classifyapp_data->appconfig.detection_threshold,
				  .5,
				  cur_classifyapp_data->appconfig.output_filepath,
				  cur_classifyapp_data->appconfig.output_json_filepath,
				  &cur_classifyapp_data->cur_classify_info.results_info);

	syslog(LOG_DEBUG, "= Setting end_timestamp | restful_ptr: %p | %s:%d", restful, __FILE__, __LINE__);
	clock_gettime(CLOCK_REALTIME, &cur_classifyapp_data->cur_classify_info.end_timestamp);
	pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_COMPLETED;
	pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	
      }
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
  fprintf(stdout, "\nRELEASE: MAR 2019\n\n");

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
  restful_comm_thread_start(&restful_struct, &restful_dispatch_queue, (int *)&get_config()->daemon_port);

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
  
  syslog(LOG_INFO, "= CLASSIFYAPP EXITING");
  
  return 0;
}

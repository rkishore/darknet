/*******************************************************************
 * puller.c - routines to pull from a URL for CLASSIFYAPP
 *          
 * Copyright (C) 2015 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: Kishore Ramachandran (kishore.ramachandran@zipreel.com)
 *          
 *******************************************************************/

#include <pthread.h>
#include "common.h"
#include "config.h"
#include "fastpool.h"
#include "threadqueue.h"

#define REPORTING_THRESHOLD 32*1024 // 32 KB
#define SAMPLE_FILE_THRESHOLD_BYTES 2048 // 2 KB

static char *get_byte_range_str(char *byte_range_suffix) 
{

  char *byte_range_prefix = "Range: bytes=";
  char *byte_range_str = NULL;
 
  byte_range_str = (char *)malloc(strlen(byte_range_prefix) + strlen(byte_range_suffix) + 1);
  if (byte_range_str == NULL) {

    syslog(LOG_ERR, "Could not allocate memory for byte-range\n");
    
  } else {

    memset(byte_range_str, 0, strlen(byte_range_prefix) + strlen(byte_range_suffix) + 1);
    memcpy(byte_range_str, byte_range_prefix, strlen(byte_range_prefix) + 1);
    strcat(byte_range_str, byte_range_suffix);

  }

  return byte_range_str;
  
}

static int get_new_input_packet_buf(classifyapp_struct *classifyapp_inst)
{
  sem_wait(classifyapp_inst->input_packet_sem);
  if (!classifyapp_inst->input_thread_is_active) {
    DEBUG_PRINTF("HTTP_INPUT_THREAD_FUNC:  leaving thread\n");
    return -1;
  }
    
  if ((fast_buffer_pool_take(classifyapp_inst->input_packet_buffer_pool, &classifyapp_inst->cur_input_packet) < 0) ||
      (!classifyapp_inst->cur_input_packet)) {
    DEBUG_PRINTF("HTTP_INPUT_THREAD_FUNC: output of input_packet_buffers\n");
    sem_post(classifyapp_inst->input_packet_sem); // Added by KR. Remove if prev. line is uncommented later.
    return -1;
  }
  memset(classifyapp_inst->cur_input_packet, 0, MAX_PACKET_SIZE);
  classifyapp_inst->cur_input_packet_fill = 0;
  return 0;
}

static int push_input_packet_to_msg_queue(classifyapp_struct *classifyapp_inst, bool last_packet_flag)
{
  igolgi_message_struct *message_wrapper = NULL;
  struct timespec packet_timestamp;
  double incoming_rate = 0, cur_start_ts = 0, cur_end_ts = 0;

  curl_easy_getinfo(classifyapp_inst->curl_data, CURLINFO_SIZE_DOWNLOAD, &classifyapp_inst->bytes_pulled);      
  curl_easy_getinfo(classifyapp_inst->curl_data, CURLINFO_SPEED_DOWNLOAD, &incoming_rate);
  classifyapp_inst->input_mux_rate = (incoming_rate * 8) / (1024);
      
  /*fprintf(stderr, "= URL: %s | Transfer detail: %0.0f bytes @ %0.1f Kbps\n", 
	  get_config()->image_url,
	  classifyapp_inst->bytes_pulled, classifyapp_inst->input_mux_rate);
  */
  syslog(LOG_INFO, "= URL: %s | Transfer detail: %0.0f bytes @ %0.1f Kbps\n", 
	 get_config()->image_url,
	 classifyapp_inst->bytes_pulled, classifyapp_inst->input_mux_rate);    
  
  // Create message and send to mp3decoder thread	  
  clock_gettime(CLOCK_REALTIME, &packet_timestamp);
  if ((fast_buffer_pool_take(classifyapp_inst->system_message_pool, (uint8_t **)&message_wrapper) < 0) ||
      (!message_wrapper)) {	    
    DEBUG_PRINTF("HTTP_INPUT_THREAD_FUNC:  out of system messages\n");
    //syslog(LOG_ERR,"[CH=%d]  HDRELAY:  RESYNC - OUT OF SYSTEM MESSAGES?\n", classifyapp_inst->appconfig.channel);
    //force_hard_resync(hdrelay_app_data, SYSTEM_EVENT_NO_SIGNAL);
    sem_post(classifyapp_inst->input_packet_sem); // Added by KR. Remove if prev. line is uncommented later.
    return -1;	    
  }

  message_wrapper->buffer = classifyapp_inst->cur_input_packet;
  message_wrapper->buffer_size = classifyapp_inst->cur_input_packet_fill;
  message_wrapper->pull_end_timestamp_low = packet_timestamp.tv_nsec;
  message_wrapper->pull_end_timestamp_high = packet_timestamp.tv_sec;
  message_wrapper->pull_start_timestamp_low = classifyapp_inst->pull_start_timestamp.tv_nsec;
  message_wrapper->pull_start_timestamp_high = classifyapp_inst->pull_start_timestamp.tv_sec;
  message_queue_push_front(classifyapp_inst->input_packet_queue, message_wrapper);
  sem_post(classifyapp_inst->input_queue_sem);

  /*
    cur_start_ts = (double)message_wrapper->pull_start_timestamp_high + (double)(message_wrapper->pull_start_timestamp_low)/NANOSEC;
    cur_end_ts = (double)message_wrapper->pull_end_timestamp_high + (double)(message_wrapper->pull_end_timestamp_low)/NANOSEC;
    fprintf(stderr, "= PULLER_THREAD | Timestamps: %0.3f-%0.3f | diff: %0.3f\n", 
	  cur_start_ts, cur_end_ts, cur_end_ts - cur_start_ts);
  */

  classifyapp_inst->bytes_pushed += message_wrapper->buffer_size;

  if (last_packet_flag) {

    igolgi_message_struct *message_wrapper_final = NULL;

    clock_gettime(CLOCK_REALTIME, &packet_timestamp);
    if ((fast_buffer_pool_take(classifyapp_inst->system_message_pool, (uint8_t **)&message_wrapper_final) < 0) ||
	(!message_wrapper_final)) {	    
      DEBUG_PRINTF("HTTP_INPUT_THREAD_FUNC:  out of system messages\n");
      //syslog(LOG_ERR,"[CH=%d]  HDRELAY:  RESYNC - OUT OF SYSTEM MESSAGES?\n", classifyapp_inst->appconfig.channel);
      //force_hard_resync(hdrelay_app_data, SYSTEM_EVENT_NO_SIGNAL);
      sem_post(classifyapp_inst->input_packet_sem); // Added by KR. Remove if prev. line is uncommented later.
      return -1;	    
    }
    
    message_wrapper_final->buffer = NULL;
    message_wrapper_final->buffer_size = -1;
    message_wrapper_final->pull_end_timestamp_low = packet_timestamp.tv_nsec;
    message_wrapper_final->pull_end_timestamp_high = packet_timestamp.tv_sec;
    message_wrapper_final->pull_start_timestamp_low = classifyapp_inst->pull_start_timestamp.tv_nsec;
    message_wrapper_final->pull_start_timestamp_high = classifyapp_inst->pull_start_timestamp.tv_sec;
    message_queue_push_front(classifyapp_inst->input_packet_queue, message_wrapper_final);
    sem_post(classifyapp_inst->input_queue_sem);

  }

  return 0;
}

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  CURL *curl_hdl = (CURL *)userp;
  classifyapp_struct *classifyapp_data = (classifyapp_struct *)userp;
  bool cur_contents_used = false, push_condition1 = false, push_condition2 = false;

  classifyapp_data->bytes_pulled += (double)realsize;
  classifyapp_data->bytes_pulled_report += (double)realsize;
  if (classifyapp_data->bytes_pulled_report >= REPORTING_THRESHOLD) {
    //fprintf(stderr, "= size of incoming data: %lu | total_pulled: %0.0f\n", (long unsigned)realsize, classifyapp_data->bytes_pulled);
    classifyapp_data->bytes_pulled_report = 0;
  }

  // Get new buffer if one has not been allocated before
  // Should come here only once at the beginning
  if (!classifyapp_data->cur_input_packet) {
    if (get_new_input_packet_buf(classifyapp_data) < 0)
      return -1;
  }
  
  // Curl will at most write 16K per call to this function 
  // http://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
  if ((classifyapp_data->cur_input_packet_fill + realsize) <= MAX_PACKET_SIZE) {    
    memcpy(classifyapp_data->cur_input_packet + classifyapp_data->cur_input_packet_fill, contents, realsize);    
    classifyapp_data->cur_input_packet_fill += realsize;
    cur_contents_used = true;
  } 

  // Send msg to mp3decoder thread if the current buffer is fully filled 
  push_condition1 = (cur_contents_used == true) && (classifyapp_data->cur_input_packet_fill == MAX_PACKET_SIZE);
  // or if it does not have enough space to accomodate the contents in this call
  push_condition2 = (cur_contents_used == false) && ((classifyapp_data->cur_input_packet_fill + realsize) > MAX_PACKET_SIZE);
 
  if ( push_condition1 || push_condition2 ) {

    if (push_input_packet_to_msg_queue(classifyapp_data, false) < 0)
      return -1;

    classifyapp_data->cur_input_packet = NULL;
    if (!classifyapp_data->cur_input_packet) {
      if (get_new_input_packet_buf(classifyapp_data) < 0)
	return -1;
    }
  
    if ( (push_condition2) && ((classifyapp_data->cur_input_packet_fill + realsize) <= MAX_PACKET_SIZE) ) {
      memcpy(classifyapp_data->cur_input_packet + classifyapp_data->cur_input_packet_fill, contents, realsize);    
      classifyapp_data->cur_input_packet_fill += realsize;      
    }

    // Reinit for next buf to be sent to mp3_decoder
    clock_gettime(CLOCK_REALTIME, &classifyapp_data->pull_start_timestamp);

  } 

  return realsize;
}


static int pull_specified_byte_range(classifyapp_struct *classifyapp_inst, char *spec_byte_range) 
{
  
  CURL *curl_hdl = classifyapp_inst->curl_data;
  CURLcode retval;
  char *byte_range_str = NULL;
  struct curl_slist *byte_range = NULL;
  double bytes_pulled = -1.0;

  if (spec_byte_range != NULL)
    {
      byte_range_str = get_byte_range_str(spec_byte_range);
      
      syslog(LOG_INFO, "= Pulling %s | %s\n", byte_range_str, spec_byte_range);

      if (byte_range_str != NULL) 
	{
	  byte_range = curl_slist_append(byte_range, byte_range_str);
	  curl_easy_setopt(curl_hdl, CURLOPT_HTTPHEADER, byte_range);
	  curl_easy_setopt(curl_hdl, CURLOPT_RANGE, spec_byte_range);
	}
      else
	{
	  syslog(LOG_ERR, "= Could not allocate memory for byte-range\n");
	  return -1;
	}
    }
  
  /* get it! */ 
  /* Check for errors */ 
  if ((retval = curl_easy_perform(curl_hdl)) != CURLE_OK) 
    {
      bool print_err = false;
      syslog(LOG_INFO, "= CURL BYTES PULLED (END): %0.0f", classifyapp_inst->bytes_pulled);
      print_err = (retval != CURLE_WRITE_ERROR) ||
	((retval == CURLE_WRITE_ERROR) && (classifyapp_inst->bytes_pulled < SAMPLE_FILE_THRESHOLD_BYTES));
      
      if (print_err)
	syslog(LOG_ERR, "curl_easy_perform() failed: %s/%d @ %s, line %d\n",
	       curl_easy_strerror(retval), retval,
	       __FILE__, __LINE__);
    }
  
  if ((byte_range_str != NULL) && (byte_range != NULL))
    {
      free_mem(byte_range_str);
      curl_slist_free_all(byte_range);
    }
    
  return (int)retval;
    
}

static int config_curl_and_pull_file(classifyapp_struct *classifyapp_data)
{
  
  char *pat_pmt_range_str = NULL;
  int retval = 0;
  u64 end_byte_range = 0;
  char *json_str = NULL;
  curl_off_t max_download_speed = 32000; // 32000 bytes per sec = 256kbps

  // Initialization
  classifyapp_data->bytes_pulled = 0;

  classifyapp_data->curl_data = curl_easy_init();
  
  if (!classifyapp_data->curl_data) {
    fprintf(stderr, "= Could not get CURL data handle\n");
    return -1;
  }

  /* Specify that HTTP error codes (>= 400) should be caught */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_FAILONERROR, true);

  /* Avoid sending signals on abort/timeout */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_NOSIGNAL, 1);

  /* 
     Specify that connection should abort if average transfer speed is below 
     XFER_ABORT_SPEED_THRESHOLD (bytes/sec) for XFER_ABORT_TIME (seconds)

     Motivation: for very large files, we see that sometimes the transfer stalls and 
     the connection should timeout. However, curl's timeout control requires the user 
     to set an upper limit on how long each transfer should take. Since this number 
     is impractical to try to predict, the control below is used instead. 
     
     The idea here is that if the average transfer speed falls below XFER_ABORT_SPEED_THRESHOLD 
     bytes/sec for XFER_ABORT_TIME seconds, then curl should abort. Note that each transfer is 
     retried MAX_PULL_RETRIES times.
  */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_LOW_SPEED_LIMIT, XFER_ABORT_SPEED_THRESHOLD);
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_LOW_SPEED_TIME, XFER_ABORT_TIME);

  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_MAX_RECV_SPEED_LARGE, max_download_speed);

  /* specify URL to get */ 
  syslog(LOG_INFO, "= About to start pulling from URL (cmdline): %s\n", get_config()->image_url);
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_URL, get_config()->image_url); 

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  
  /* send all data to this function  */ 
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEFUNCTION, write_memory_callback);
  
  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEDATA, (void *)classifyapp_data);

  clock_gettime(CLOCK_REALTIME, &classifyapp_data->pull_start_timestamp);

  retval = pull_specified_byte_range(classifyapp_data, NULL);
  
  if (push_input_packet_to_msg_queue(classifyapp_data, true) < 0)
    return -1;
    
  pthread_mutex_lock(&classifyapp_data->http_input_thread_complete_mutex);
  classifyapp_data->http_input_thread_complete = true;
  pthread_mutex_unlock(&classifyapp_data->http_input_thread_complete_mutex);

  return retval;
  
}

static size_t write_file(void *ptr, size_t size, size_t nmemb, void *userp)
{
  classifyapp_struct *classifyapp_data = (classifyapp_struct *)userp;
  size_t written = fwrite(ptr, size, nmemb, (FILE *)classifyapp_data->samplefptr);
  curl_easy_getinfo(classifyapp_data->curl_data, CURLINFO_SIZE_DOWNLOAD, &classifyapp_data->bytes_pulled);      
  // syslog(LOG_INFO, "= CURL BYTES PULLED: %0.0f", classifyapp_data->bytes_pulled);
  if (written > SAMPLE_FILE_THRESHOLD_BYTES)
    return 0;
  return written;
}

int config_curl_and_pull_file_sample(classifyapp_struct *classifyapp_data)
{
  
  char *pat_pmt_range_str = NULL;
  int retval = 0;
  u64 end_byte_range = 0;
  char *json_str = NULL;
  static const char *samplefilename = "/tmp/page.out";

  // Initialization
  classifyapp_data->bytes_pulled = 0;

  classifyapp_data->curl_data = curl_easy_init();
  
  if (!classifyapp_data->curl_data) {
    fprintf(stderr, "= Could not get CURL data handle\n");
    return -1;
  }

  /* Specify that HTTP error codes (>= 400) should be caught */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_FAILONERROR, true);

  /* Avoid sending signals on abort/timeout */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_NOSIGNAL, 1);

  /* 
     Specify that connection should abort if average transfer speed is below 
     XFER_ABORT_SPEED_THRESHOLD (bytes/sec) for XFER_ABORT_TIME (seconds)

     Motivation: for very large files, we see that sometimes the transfer stalls and 
     the connection should timeout. However, curl's timeout control requires the user 
     to set an upper limit on how long each transfer should take. Since this number 
     is impractical to try to predict, the control below is used instead. 
     
     The idea here is that if the average transfer speed falls below XFER_ABORT_SPEED_THRESHOLD 
     bytes/sec for XFER_ABORT_TIME seconds, then curl should abort. Note that each transfer is 
     retried MAX_PULL_RETRIES times.
  */
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_LOW_SPEED_LIMIT, XFER_ABORT_SPEED_THRESHOLD);
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_LOW_SPEED_TIME, XFER_ABORT_TIME);

  /* specify URL to get */ 
  syslog(LOG_INFO, "= About to start pulling sample file from URL: %s", get_config()->image_url);
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_URL, get_config()->image_url); 

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  
  /* send all data to this function  */ 
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEFUNCTION, write_file);

  /* open the file */ 
  classifyapp_data->samplefptr = fopen(samplefilename, "wb");
  if(classifyapp_data->samplefptr) {
  
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEDATA, (void *)classifyapp_data);

    retval = pull_specified_byte_range(classifyapp_data, "0-2048"); // SHOULD BE SAME AS SAMPLE_FILE_THRESHOLD_BYTES

    fclose(classifyapp_data->samplefptr);

    syslog(LOG_INFO, "= Done pulling from URL and writing sample file: %s", get_config()->image_url);

  } else {
    syslog(LOG_ERR, "= Could not open %s and pull data to it, line:%d, %s", samplefilename, __LINE__, __FILE__);
  }

  curl_easy_cleanup(classifyapp_data->curl_data);

  return retval;
  
}

void *http_input_thread_func(void *arg)
{
  int retval = -1;
  classifyapp_struct *classifyapp_data = (classifyapp_struct *)arg;

  if (config_curl_and_pull_file(classifyapp_data) < 0)
    {
      syslog(LOG_ERR, "= Could not initialize curl_hdl_for_src. Exiting.\n");
      classifyapp_data->input_thread_is_active = 0;
      return NULL;
    }    

  curl_easy_cleanup(classifyapp_data->curl_data);

  syslog(LOG_INFO, "= Leaving puller_thread, line %d in %s", __LINE__, __FILE__);
  return NULL;

}

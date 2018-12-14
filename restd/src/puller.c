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

static int pull_specified_byte_range(classifyapp_struct *classifyapp_inst, char *spec_byte_range) 
{
  
  CURL *curl_hdl = classifyapp_inst->curl_data;
  CURLcode retval;
  char *byte_range_str = NULL;
  struct curl_slist *byte_range = NULL;
  // double bytes_pulled = -1.0;

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
	{
	  syslog(LOG_ERR, "curl_easy_perform() failed: %s/%d @ %s, line %d\n",
		 curl_easy_strerror(retval), retval,
		 __FILE__, __LINE__);
	  retval = -1;
	}
    }
  
  if ((byte_range_str != NULL) && (byte_range != NULL))
    {
      free_mem(byte_range_str);
      curl_slist_free_all(byte_range);
    }
    
  return (int)retval;
    
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

static size_t write_full_file(void *ptr, size_t size, size_t nmemb, void *userp)
{
  classifyapp_struct *classifyapp_data = (classifyapp_struct *)userp;
  size_t written = fwrite(ptr, size, nmemb, (FILE *)classifyapp_data->samplefptr);
  curl_easy_getinfo(classifyapp_data->curl_data, CURLINFO_SIZE_DOWNLOAD, &classifyapp_data->bytes_pulled);      
  // syslog(LOG_INFO, "= CURL BYTES PULLED: %0.0f", classifyapp_data->bytes_pulled);
  return written;
}

int config_curl_and_pull_file(classifyapp_struct *classifyapp_data)
{
  
  int retval = 0;

  memset(classifyapp_data->appconfig.input_filename, 0, LARGE_FIXED_STRING_SIZE);
  snprintf(classifyapp_data->appconfig.input_filename, LARGE_FIXED_STRING_SIZE-1, "%s/%s",
	   classifyapp_data->appconfig.output_directory,
	   basename(get_config()->image_url));

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
  curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEFUNCTION, write_full_file);

  /* open the file */
  classifyapp_data->samplefptr = fopen((const char *)classifyapp_data->appconfig.input_filename, "wb");
  if(classifyapp_data->samplefptr) {
  
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEDATA, (void *)classifyapp_data);

    retval = pull_specified_byte_range(classifyapp_data, NULL); // SHOULD BE SAME AS SAMPLE_FILE_THRESHOLD_BYTES

    fclose(classifyapp_data->samplefptr);

    syslog(LOG_INFO, "= Done pulling from URL and writing sample file: %s", get_config()->image_url);

  } else {
    syslog(LOG_ERR, "= Could not open %s and pull data to it, line:%d, %s", classifyapp_data->appconfig.input_filename, __LINE__, __FILE__);
  }

  curl_easy_cleanup(classifyapp_data->curl_data);

  pthread_mutex_lock(&classifyapp_data->http_input_thread_complete_mutex);
  classifyapp_data->http_input_thread_complete = true;
  pthread_mutex_unlock(&classifyapp_data->http_input_thread_complete_mutex);

  return retval;
  
}

int config_curl_and_pull_file_sample(classifyapp_struct *classifyapp_data)
{
  
  // char *pat_pmt_range_str = NULL;
  int retval = 0;
  // u64 end_byte_range = 0;
  // char *json_str = NULL;
  char *samplefilename = NULL;

  if (asprintf(&samplefilename, "%s/%s", classifyapp_data->appconfig.output_directory, basename(get_config()->image_url)) < 0)
    {
      syslog(LOG_ERR, "= Could not asprintf samplefilename, %s:%d", __FILE__, __LINE__);
      return -1;
    }
  
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
  classifyapp_data->samplefptr = fopen((const char *)samplefilename, "wb");
  if(classifyapp_data->samplefptr) {
  
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(classifyapp_data->curl_data, CURLOPT_WRITEDATA, (void *)classifyapp_data);

    retval = pull_specified_byte_range(classifyapp_data, "0-2048"); // SHOULD BE SAME AS SAMPLE_FILE_THRESHOLD_BYTES

    fclose(classifyapp_data->samplefptr);

    syslog(LOG_INFO, "= Done pulling from URL and writing sample file: %s", get_config()->image_url);

  } else {
    syslog(LOG_ERR, "= Could not open %s and pull data to it, line:%d, %s", samplefilename, __LINE__, __FILE__);
  }

  free(samplefilename);
  samplefilename = NULL;
  
  curl_easy_cleanup(classifyapp_data->curl_data);

  return retval;
  
}

void *http_input_thread_func(void *arg)
{
  // int retval = -1;
  classifyapp_struct *classifyapp_data = (classifyapp_struct *)arg;

  if (config_curl_and_pull_file(classifyapp_data) < 0)
    {
      syslog(LOG_ERR, "= Could not initialize curl_hdl_for_src. Exiting.\n");
      classifyapp_data->input_thread_is_active = 0;
      return NULL;
    }    

  syslog(LOG_INFO, "= Leaving puller_thread, line %d in %s", __LINE__, __FILE__);
  return NULL;

}

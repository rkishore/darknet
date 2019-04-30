#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include "cJSON.h"
#include "restful.h"
#include "threadqueue.h"
#include "fastpool.h"
#include "common.h"
#include "config.h"
#include "utils.h"

#define RESTFUL_MAX_MESSAGE_SIZE      1024
#define MAX_LISTEN 10
#define GET_CUR_CLASSIFY_INFO 0
#define MIN_RUNNING_TIME_BEFORE_STOP 30

static int cur_classify_id = START_CLASSIFY_ID;

static int open_tcp_server_socket(int port)
{
    int sock;                        
    struct sockaddr_in serveraddr; 
    int reuse_flag = 1;
    int retval;
    int nodelay_flag = 1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr,"Could not create TCP socket\n");
        return -1;
    }       
   
    memset(&serveraddr, 0, sizeof(serveraddr));   /* Zero out structure */
    serveraddr.sin_family = AF_INET;                /* Internet address family */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    serveraddr.sin_port = htons(port);              /* Local port */
    
    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_flag,
                        sizeof(reuse_flag));
    if (retval < 0)
    {
        fprintf(stderr, "setsockopt() failed - REUSEADDR\n");
        close(sock);
        return -1;
    }     

    retval = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay_flag, sizeof(nodelay_flag));

    if (bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stderr,"Unable to bind to server interface\n");
        return -1;
    }

    if (listen(sock, MAX_LISTEN) < 0) {
        fprintf(stderr,"Unable to listen to tcp socket\n");
        return -1;
    }

    return sock;
}

/*
static int open_tcp_socket(const char *address, int src_port, int dest_port)
{
    int sock;
    int reuse_flag = 1;
    int retval;
    struct sockaddr_in serv_addr;
    struct in_addr ip_addr;
    struct timeval timeout;      
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    int nodelay_flag = 1;

    if (inet_aton(address, &ip_addr) == 0)
    {
        fprintf(stderr, "Invalid IP address: %s\n", address);
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "Could not create TCP socket\n");
        return -1;
    }

    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_flag,
                        sizeof(reuse_flag));
    if (retval < 0)
    {
        fprintf(stderr, "setsockopt() failed - REUSEADDR\n");
        close(sock);
        return -1;
    } 

    retval = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    if (retval < 0) {
        fprintf(stderr,"setsockopt() failed - SNDTIMEO\n");
        close(sock);
        return -1;
    }

    retval = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay_flag, sizeof(nodelay_flag));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(dest_port);
    serv_addr.sin_addr.s_addr = inet_addr(address);

    retval = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (retval < 0)
    {
        fprintf(stderr, "connect() failed - addr: %s, port: %d\n", address,
                dest_port);

        syslog(LOG_ERR,"TCP CONNECT() FAILED - ADDR: %s PORT: %d  IS SERVER UP?\n",
               address,
               dest_port);

        close(sock);
        return -1;
    }

    return sock;
}
*/

int close_tcp_socket(int sock)
{
    return close(sock);
}

int write_tcp_socket(int sock, const unsigned char *buf, int count)
{
    struct timeval t;
    fd_set writeset;
    int retval;

    t.tv_usec = 500000;
    t.tv_sec = 0;
 
    FD_ZERO(&writeset);
    FD_SET(sock,&writeset);

    retval = select(sock+1,NULL,&writeset,NULL,&t);
    if (retval < 0) {     
        return -1;
    }

    if (retval) {
        retval = send(sock, buf, count, MSG_NOSIGNAL);
        if (retval < 0) {
            return -1;
        }
        return retval;
    }
    return -1;
}

/*
static int read_tcp_socket(int sock, unsigned char *buf, int count)
{ 
    int retval;    
    fd_set readset;
    struct timeval t;

    t.tv_usec = 500000;
    t.tv_sec = 0;

    FD_ZERO(&readset);
    FD_SET(sock,&readset);

    retval = select(sock+1,&readset,NULL,NULL,&t);
    if (retval < 0) { 
        return 0;
    }

    if (retval) {   
        retval = recv(sock, buf, count, 0);
        if (retval < 0) {
            return -1;
        }
        return retval;
    }
    return -1;
}
*/

/*
static int read_tcp_socket_complete(int sock, unsigned char *buf, int count)
{
    int bytes_read = 0;
    int bytes_needed = count;
    int retval = 0;
    
    while (bytes_read < count) {
        retval = read_tcp_socket(sock, &buf[bytes_read], bytes_needed);
        if (retval < 0) {   
            return -1;
        }
        bytes_read += retval;
        bytes_needed -= retval;
    } 
    return bytes_read;
}
*/

char *file_extension(char *filename) {
    char *ext = NULL;
    ext = strrchr(filename, '.');
    return ext;
}

/*
static void handle_get_all_request(cJSON **resp_json, restful_comm_struct *restful)
{
  int resp_json_array_size = 0;

  *resp_json = cJSON_CreateArray();

  resp_json_array_size = cJSON_GetArraySize(*resp_json);
  syslog(LOG_INFO, "= RESPONSE ARRAY SIZE: %d at line %d, %s", resp_json_array_size, __LINE__, __FILE__);

  if (restful->cur_transcribe_info.transcribe_id != -1) {
    cJSON *cur_transcribe_json = cJSON_CreateObject();
    cJSON *cur_config_json = cJSON_CreateObject();
    int cur_transcribe_status = -1;
    float cur_perc_finished = -1.0;
    
    cJSON_AddNumberToObject(cur_transcribe_json, "id", restful->cur_transcribe_info.transcribe_id);
    cJSON_AddStringToObject(cur_transcribe_json, "input", restful->cur_transcribe_info.audio_url);
    cJSON_AddStringToObject(cur_transcribe_json, "output_dir", restful->cur_transcribe_info.output_directory);

    pthread_mutex_lock(&restful->cur_transcribe_info.job_status_lock);
    cur_transcribe_status = restful->cur_transcribe_info.transcribe_status;
    pthread_mutex_unlock(&restful->cur_transcribe_info.job_status_lock);

    pthread_mutex_lock(&restful->cur_transcribe_info.job_percent_complete_lock);
    cur_perc_finished = restful->cur_transcribe_info.percentage_finished;
    pthread_mutex_unlock(&restful->cur_transcribe_info.job_percent_complete_lock);

    cJSON_AddNumberToObject(cur_transcribe_json, "percentage_finished", cur_perc_finished);

    if ( (cur_transcribe_status == CLASSIFY_STATUS_COMPLETED) && (cur_perc_finished == 100) ) {
      cJSON_AddStringToObject(cur_transcribe_json, "status", "finished");
      cJSON_AddNullToObject(cur_transcribe_json, "error_msg");
    } else {
      if (cur_transcribe_status == CLASSIFY_STATUS_STOPPED)
	cJSON_AddStringToObject(cur_transcribe_json, "status", "stopped");
      else if (cur_transcribe_status == CLASSIFY_STATUS_ERROR)
	cJSON_AddStringToObject(cur_transcribe_json, "status", "error");
      else
	cJSON_AddStringToObject(cur_transcribe_json, "status", "running");

      if ( (cur_transcribe_status == CLASSIFY_STATUS_STOPPED) || 
	   !(cur_transcribe_status == CLASSIFY_STATUS_ERROR) )
	cJSON_AddNullToObject(cur_transcribe_json, "error_msg");
      else
	cJSON_AddStringToObject(cur_transcribe_json, "error_msg", "transcribe did not complete");
      
      cJSON_AddItemToArray(*resp_json, cur_transcribe_json);
    }
  }

  resp_json_array_size = cJSON_GetArraySize(*resp_json);
  syslog(LOG_INFO, "= RESPONSE ARRAY SIZE: %d at line %d, %s", resp_json_array_size, __LINE__, __FILE__);

  if (resp_json_array_size == 0) {
    cJSON *info_json = cJSON_CreateObject();
    cJSON_AddStringToObject(info_json, "info", "no transcribes scheduled yet");
    cJSON_AddItemToArray(*resp_json, info_json);
  }

  return;
}

static void handle_delete_request(int8_t *return_http_flag, restful_comm_struct *restful)
{
  int cur_classify_status = -1;
  // classifyapp_struct *cur_classifyapp_data = restful->classifyapp_data;
  struct timespec cur_timestamp;
  long running_time_sec = -1;

  pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
  cur_classify_status = restful->cur_classify_info.classify_status;
  pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);

  clock_gettime(CLOCK_REALTIME, &cur_timestamp);
  running_time_sec = (long)difftime(cur_timestamp.tv_sec, restful->cur_classify_info.start_timestamp.tv_sec);

  if (cur_classify_status == CLASSIFY_STATUS_RUNNING) {
  
    //pthread_mutex_lock(&restful->mp4reader_active_lock);
    //restful->is_mp4file_thread_active = 0;
    //pthread_mutex_unlock(&restful->mp4reader_active_lock);

    if (running_time_sec > MIN_RUNNING_TIME_BEFORE_STOP) {

      pthread_mutex_lock(&restful->cur_classify_info.job_status_lock);
      restful->cur_classify_info.classify_status = CLASSIFY_STATUS_STOPPED;
      pthread_mutex_unlock(&restful->cur_classify_info.job_status_lock);
      
      *return_http_flag = HTTP_204;

    } else {

      char *cur_err = "Not exceeded min. running time of";
      syslog(LOG_ERR, "= %s %lds, line:%d, %s", cur_err, running_time_sec, __LINE__, __FILE__);
      *return_http_flag = HTTP_400;	
      
    }

  } else {

    char *cur_err = "No active classify is running!";
    syslog(LOG_ERR, "= %s, line:%d, %s", cur_err, __LINE__, __FILE__);
    *return_http_flag = HTTP_400;	
    
  }
 
  return;
}
*/
	
static int store_input_loc(cJSON **input_file_loc, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{

  cJSON *input_data = *input_file_loc;
  // char *input_file_ext = NULL;
  // int retval = 0;

  if (input_data) {

    snprintf(classifyapp_data->appconfig.input_url, LARGE_FIXED_STRING_SIZE-1, "%s", input_data->valuestring);
    //fprintf(stderr,"input_url: %s\n", classifyapp_data->appconfig.input_url);
    //syslog(LOG_INFO,"= input_url: %s\n", classifyapp_data->appconfig.input_url);

  } else {

    char *cur_err = NULL;
    if (asprintf(&cur_err, "No input url data in incoming request") < 0)
      syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
    else {      
      syslog(LOG_ERR, "= %s", cur_err);
      free_mem(cur_err);
    }
    *response_http_code = HTTP_400;	
    return -1;	  	
            
  }

  return 0;

}


static int store_input_type(cJSON **input_type, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{

  cJSON *input_type_data = *input_type;
  // char *input_file_ext = NULL;

  if (input_type_data) {

    uint8_t is_expected_input_type = (!strcmp(input_type_data->valuestring, "file")) || (!strcmp(input_type_data->valuestring, "stream"));
    
    if (!is_expected_input_type) {
      
      char *cur_err = NULL;
      if (asprintf(&cur_err, "Unexpected input_type: %s (valid values are 'file' or 'stream')", input_type_data->valuestring) < 0)
	syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
      else {
	syslog(LOG_ERR, "= %s", cur_err);
	free_mem(cur_err);
      }
      *response_http_code = HTTP_400;	
      return -1;

    } else {
      snprintf(classifyapp_data->appconfig.input_type, SMALL_FIXED_STRING_SIZE-1, "%s", input_type_data->valuestring);
      //fprintf(stderr, "input_type: %s\n", classifyapp_data->appconfig.input_type);
      //syslog(LOG_INFO,"= input_type: %s\n", classifyapp_data->appconfig.input_type);
    }

  } else {

    char *cur_err = NULL;
    if (asprintf(&cur_err, "No input type data in incoming request") < 0)
      syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
    else {
      syslog(LOG_ERR, "= %s", cur_err);
      free_mem(cur_err);
    }
    *response_http_code = HTTP_400;	

    return -1;	  	
            
  }

  return 0;

}

static int store_input_mode(cJSON **input_mode, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{

  cJSON *input_mode_data = *input_mode;

  if (input_mode_data) {

    uint8_t is_expected_input_mode = (!strcmp(input_mode_data->valuestring, "image")) || (!strcmp(input_mode_data->valuestring, "video"));
    
    if (!is_expected_input_mode) {
      
      char *cur_err = NULL;
      if (asprintf(&cur_err, "Unexpected input_mode: %s (valid values are 'file' or 'stream')", input_mode_data->valuestring) < 0)
	syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
      else {
	syslog(LOG_ERR, "= Unexpected input_mode: %s, %s:%d", cur_err, __FILE__, __LINE__);
	free_mem(cur_err);
      }
      *response_http_code = HTTP_400;	
      return -1;

    } else {
      snprintf(classifyapp_data->appconfig.input_mode, SMALL_FIXED_STRING_SIZE-1, "%s", input_mode_data->valuestring);
      //fprintf(stderr, "input_mode: %s\n", classifyapp_data->appconfig.input_mode);
      syslog(LOG_INFO,"= input_mode: %s, %s:%d", classifyapp_data->appconfig.input_mode, __FILE__, __LINE__);
    }

  } else {

    char *cur_err = NULL;
    if (asprintf(&cur_err, "No input mode data in incoming request") < 0)
      syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
    else {
      syslog(LOG_ERR, "= %s", cur_err);
      free_mem(cur_err);
    }
    *response_http_code = HTTP_400;	

    return -1;	  	
            
  }

  return 0;

}

static int store_output_loc(cJSON **output_file_dir, cJSON **output_file_path, cJSON **output_json_path, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{
  cJSON *output_dir = *output_file_dir, *output_file = *output_file_path, *output_json = *output_json_path;
  // mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
  // int output_fd = -1;

  // if ( (output_dir) && (output_file) ) {
  if (output_dir) {

    snprintf(classifyapp_data->appconfig.output_directory, LARGE_FIXED_STRING_SIZE-1, "%s", output_dir->valuestring);
    if (output_file)
      snprintf(classifyapp_data->appconfig.output_filepath, LARGE_FIXED_STRING_SIZE-1, "%s", output_file->valuestring);
    else
      classifyapp_data->appconfig.output_filepath[0] = '\0';
    
    if (output_json)
      snprintf(classifyapp_data->appconfig.output_json_filepath, LARGE_FIXED_STRING_SIZE-1, "%s", output_json->valuestring);
    //fprintf(stderr,"output_data: %s\n", classifyapp_data->appconfig.output_filename);
    syslog(LOG_DEBUG,"= output_filename: %s, %d, %s:%d", classifyapp_data->appconfig.output_filepath, (int)strlen(classifyapp_data->appconfig.output_filepath), __FILE__, __LINE__);

  } else {
    
    char *cur_err = NULL;
    if (asprintf(&cur_err, "No output file data in incoming request") < 0)
      syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
    else {
      syslog(LOG_ERR, "= %s", cur_err);
      free_mem(cur_err);
    }
    *response_http_code = HTTP_400;	
    return -1;	  	
            
  }

  return 0;
}


static int store_config_info(cJSON **config_info, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{
  cJSON *config_data = *config_info;
  cJSON *detection_threshold = NULL;
  
  if (config_data) {

    detection_threshold = cJSON_GetObjectItem(config_data, "detection_threshold");

    if ( (detection_threshold) && (detection_threshold->valuedouble) ) {
      
      classifyapp_data->appconfig.detection_threshold = detection_threshold->valuedouble;
      syslog(LOG_DEBUG,"= detection_threshold: %0.1f\n", classifyapp_data->appconfig.detection_threshold);

    } else {

      classifyapp_data->appconfig.detection_threshold = get_config()->detection_threshold;
      syslog(LOG_DEBUG,"= detection_threshold: %0.1f\n", classifyapp_data->appconfig.detection_threshold);

    }

  } else {

    char *cur_err = NULL;
    if (asprintf(&cur_err, "No config data in incoming request") < 0)
      syslog(LOG_ERR, "Out of memory? line:%d, %s", __LINE__, __FILE__);
    else {
      syslog(LOG_ERR, "= %s", cur_err);
      free_mem(cur_err);
    }
    *response_http_code = HTTP_400;	
    return -1;	  	
            
  }

  return 0;
}

static int handle_post_request(cJSON **parsedjson, int8_t *return_http_flag, restful_comm_struct *restful, char *unparsed_json_str)
{
  // char *out = NULL;
  cJSON *input_data = NULL, *output_filepath = NULL, *output_dir = NULL;
  cJSON *config_data = NULL, *output_json_filepath = NULL, *input_type_data = NULL;
  cJSON *input_mode = NULL;
  classifyapp_struct *cur_classifyapp_data = NULL; 
  int cur_classify_thread_status = -1, next_post_id = -1;
  bool local_next_post_id_rollover_done = false;
    
  // Check if classify thread is idle
  pthread_mutex_lock(&restful->thread_status_lock);
  cur_classify_thread_status = restful->classify_thread_status;
  pthread_mutex_unlock(&restful->thread_status_lock);

  if (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_IDLE) {
    
    // fprintf(stderr, "= JSON RCVD: %s\n", unparsed_json_str);
    
    pthread_mutex_lock(&restful->next_post_id_lock);
    next_post_id = restful->next_post_classify_id;
    pthread_mutex_unlock(&restful->next_post_id_lock);

    if (next_post_id < 0)
      {
	pthread_mutex_lock(&restful->next_post_id_lock);
	restful->next_post_classify_id = START_CLASSIFY_ID;
	next_post_id = restful->next_post_classify_id;
	pthread_mutex_unlock(&restful->next_post_id_lock);
      }

    // Check if rollover of post_ids has happened
    pthread_mutex_lock(&next_post_id_rollover_done_lock);
    local_next_post_id_rollover_done = next_post_id_rollover_done;
    pthread_mutex_unlock(&next_post_id_rollover_done_lock);
    
    cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + next_post_id);

    if (local_next_post_id_rollover_done == true)
      {
	if ( (get_config()->dont_overwrite_old_results_until_read == true) && (cur_classifyapp_data->cur_classify_info.results_info.results_read == false) )
	  {
	    
	    double time_now = what_time_is_it_now(), processing_time_ms = cur_classifyapp_data->cur_classify_info.results_info.processing_time_in_seconds * 1000.0;
	    double time_passed_ms = (time_now - cur_classifyapp_data->cur_classify_info.results_info.processing_time_end) * 1000.0;
	    bool reset_results_read_flag = false;
	    int classify_status_for_this_buffer = -1;
	    
	    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	    classify_status_for_this_buffer = cur_classifyapp_data->cur_classify_info.classify_status;
	    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
	    
	    reset_results_read_flag = ( ((cur_classifyapp_data->cur_classify_info.results_info.processing_time_end > 0) || (classify_status_for_this_buffer == CLASSIFY_STATUS_INPUT_ERROR)) &&
					( (processing_time_ms >= get_config()->results_read_thresh_ms) || (time_passed_ms >= get_config()->results_read_thresh_ms) ) ) ;

	    syslog(LOG_INFO, "= id: %d | results_read? %d processing_time_ms: %0.2f time_passed_ms: %0.2f processing_time_end: %0.2f classify_status_input_error? %d reset_results_read? %d, %s:%d",
		   next_post_id,
		   (int)cur_classifyapp_data->cur_classify_info.results_info.results_read,
		   processing_time_ms,
		   time_passed_ms,
		   cur_classifyapp_data->cur_classify_info.results_info.processing_time_end,
		   (classify_status_for_this_buffer == CLASSIFY_STATUS_INPUT_ERROR),
		   (int)reset_results_read_flag,
		   __FILE__, __LINE__);
	    
	    if (reset_results_read_flag == false)
	      {
		syslog(LOG_INFO, "= Response to HTTP POST | Returning that service is unavailable (HTTP 503) as older results for ID %d/%d have not yet been read, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id, next_post_id, __FILE__, __LINE__);
		
		*return_http_flag = HTTP_503;

		return -1;
	      }
	    else
	      cur_classifyapp_data->cur_classify_info.results_info.results_read = true;
	    
	  }
      }

    // Re-initialize key values
    memset(&cur_classifyapp_data->appconfig, 0, sizeof(classifyapp_config_struct));
    memset(&cur_classifyapp_data->cur_classify_info, 0, sizeof(classify_job_info));
    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    cur_classifyapp_data->cur_classify_info.classify_status = -1;
    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    cur_classifyapp_data->bytes_pulled = 0;
    cur_classifyapp_data->cur_classify_info.results_info.processing_time_end = -1.0;
	
    //fprintf(stderr,"decoding data:%s\n", lineptr);
    input_data = cJSON_GetObjectItem(*parsedjson, "input");
    input_type_data = cJSON_GetObjectItem(*parsedjson, "type");
    input_mode = cJSON_GetObjectItem(*parsedjson, "mode");
    output_dir = cJSON_GetObjectItem(*parsedjson, "output_dir");
    output_filepath = cJSON_GetObjectItem(*parsedjson, "output_filepath");
    output_json_filepath = cJSON_GetObjectItem(*parsedjson, "output_json_filepath");
    config_data = cJSON_GetObjectItem(*parsedjson, "config");

    if ( (input_data == NULL) || (input_type_data == NULL) ||
	 (output_dir == NULL) || (config_data == NULL) ) {
      syslog(LOG_ERR, "= Not enough input args specified, %s:%d", __FILE__, __LINE__);
      *return_http_flag = HTTP_400;
      return -1;
    }

    if (store_input_loc(&input_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;

    if (store_input_type(&input_type_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;

    if (store_output_loc(&output_dir, &output_filepath, &output_json_filepath, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;

    if (store_config_info(&config_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;    

    if (input_mode == NULL)
      memset(cur_classifyapp_data->appconfig.input_mode, 0, SMALL_FIXED_STRING_SIZE-1);
    else
      {
	if (store_input_mode(&input_mode, cur_classifyapp_data, return_http_flag) < 0)
	  return -1;
      }
    
    *return_http_flag = HTTP_201;

    cur_classifyapp_data->cur_classify_info.classify_id = cur_classify_id;
    cur_classify_id = (cur_classify_id + 1) % get_config()->max_queue_length;

    pthread_mutex_lock(&next_post_id_rollover_done_lock);
    local_next_post_id_rollover_done = next_post_id_rollover_done;
    pthread_mutex_unlock(&next_post_id_rollover_done_lock);
    
    pthread_mutex_lock(&restful->next_post_id_lock);
    restful->next_post_classify_id = cur_classify_id;
    next_post_id = restful->next_post_classify_id;
    pthread_mutex_unlock(&restful->next_post_id_lock);

    if ( (local_next_post_id_rollover_done == false) && (next_post_id == 0) )
      {
	pthread_mutex_lock(&next_post_id_rollover_done_lock);
	next_post_id_rollover_done = true;
	local_next_post_id_rollover_done = next_post_id_rollover_done;
	pthread_mutex_unlock(&next_post_id_rollover_done_lock);
	syslog(LOG_DEBUG, "= next_post_id rollover done? %d, %s:%d", (int)local_next_post_id_rollover_done, __FILE__, __LINE__);
      }

    pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    cur_classifyapp_data->cur_classify_info.classify_status = CLASSIFY_STATUS_RUNNING;
    pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
    clock_gettime(CLOCK_REALTIME, &cur_classifyapp_data->cur_classify_info.start_timestamp);
    
    syslog(LOG_INFO, "= RESTFUL_THREAD_RCV HTTP POST | ID: %d | input_url: %s (%s) | mode: %s | output_directory: %s | output_filepath: %s\n", 
	   cur_classifyapp_data->cur_classify_info.classify_id,
	   cur_classifyapp_data->appconfig.input_url,
	   cur_classifyapp_data->appconfig.input_type,
	   (input_mode == NULL) ? "unspecified" : cur_classifyapp_data->appconfig.input_mode,
	   cur_classifyapp_data->appconfig.output_directory,
	   (output_filepath == NULL) ? "unspecified" : cur_classifyapp_data->appconfig.output_filepath);
    
    cur_classifyapp_data->cur_classify_info.results_info.percentage_completed = 0;

    //cJSON_Delete(json);		
    //fprintf(stderr,"\n\n\n\ndecoded data\n");			    
	    
  } else {
    if (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_NOTSETUP)
      syslog(LOG_INFO, "= Returning that service is unavailable (HTTP 503) as network setup is not yet completed, %s:%d", __FILE__, __LINE__);
    else
      syslog(LOG_INFO, "= Returning that service is unavailable (HTTP 503) as input queue is full, %s:%d", __FILE__, __LINE__);
    *return_http_flag = HTTP_503;
  }
  
  return 0;
}

/*
static void copy_to_global_config(restful_comm_struct *restful_ptr)
{
  classifyapp_struct *classifyapp_info = (classifyapp_struct *)(restful_ptr->classifyapp_data + restful_ptr->cur_classify_info.classify_id);

  memset(mod_config()->image_url, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->image_url, classifyapp_info->appconfig.input_url, strlen(classifyapp_info->appconfig.input_url));

  memset(mod_config()->input_type, 0, SMALL_FIXED_STRING_SIZE);
  memcpy(mod_config()->input_type, classifyapp_info->appconfig.input_type, strlen(classifyapp_info->appconfig.input_type));

  memset(mod_config()->input_mode, 0, SMALL_FIXED_STRING_SIZE);
  if (strlen(classifyapp_info->appconfig.input_mode) > 0)
    memcpy(mod_config()->input_mode, classifyapp_info->appconfig.input_mode, strlen(classifyapp_info->appconfig.input_mode));

  memset(mod_config()->output_directory, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->output_directory, classifyapp_info->appconfig.output_directory, strlen(classifyapp_info->appconfig.output_directory));

  memset(mod_config()->output_filepath, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->output_filepath, classifyapp_info->appconfig.output_filepath, strlen(classifyapp_info->appconfig.output_filepath));

  memset(mod_config()->output_json_filepath, 0, LARGE_FIXED_STRING_SIZE);
  if (strlen(classifyapp_info->appconfig.input_mode) > 0)
    memcpy(mod_config()->output_json_filepath, classifyapp_info->appconfig.output_json_filepath, strlen(classifyapp_info->appconfig.output_json_filepath));

  syslog(LOG_DEBUG, "= COPIED DISPATCH_MSG | image_url: %s (%s) | output_directory: %s | output_filepath: %s | output_json_filepath: %s\n", 
	 get_config()->image_url,
	 get_config()->input_type,
	 get_config()->output_directory,
	 get_config()->output_filepath,
	 get_config()->output_json_filepath);

  return;
}
*/

static int parse_input_request_data(uint32_t **input_request_data, 
				    cJSON **parsed_json, 
				    int8_t *return_http_flag, 
				    char *http_method, 
				    restful_comm_struct *restful_ptr)
{
  
  char uri[384];
  char version[128];
  char *lineptr = NULL;

  *http_method = '\0';
  uri[0] = '\0';
  version[0] = '\0';
  sscanf((const char *)*input_request_data, "%s %s HTTP/%s", http_method, uri, version);

  // Bad request
  if ( !strlen(http_method) || !strlen(uri) || !strlen(version) ) {
    *return_http_flag = HTTP_400;
    return -1;
  }

  //#ifdef IQMEDIA_DEBUG
  syslog(LOG_DEBUG,"= cmd:%s | uri:%s | version:%s", http_method, uri, version);
  //#endif

  // POST for /api/v0/classify/
  if ( !strcmp(uri, "/api/v0/classify/") || !strcmp(uri, "/api/v0/classify") ) {
    
    if ( !strcmp(http_method, "POST") ) {
      
      syslog(LOG_DEBUG, "= %s %s | restful_ptr: %p", http_method, uri, restful_ptr);

      lineptr = strstr((const char *)*input_request_data, "\r\n\r\n");
      if (!lineptr) {      
	lineptr = strstr((const char *)*input_request_data,"\n\n");
	if (lineptr) {
	  lineptr += 2;		    
	}
      } else {		 
	lineptr += 4;		    
      }
	
      // fprintf(stderr,"= POST JSON lineptr: %s\n", lineptr);
      
      if (lineptr) {
	*parsed_json = cJSON_Parse(lineptr);
	if (!*parsed_json) {
	  fprintf(stderr,"unable to parse msg\n");
	  *return_http_flag = HTTP_400; // Bad Request
	} else {
	  handle_post_request(parsed_json, return_http_flag, restful_ptr, lineptr);
	  /* if (*return_http_flag == HTTP_201)
	    copy_to_global_config(restful_ptr); 
	  */
	  
	}
      }

      // parse_and_handle_post_request(input_request_data, parsed_json, return_http_flag, restful_ptr);
      
    } else {

      fprintf(stderr, "Access-Control-Allow-Methods: POST\n");		 
      syslog(LOG_ERR, "Access-Control-Allow-Methods: POST\n");		 
      *return_http_flag = HTTP_405;

    }

  } else if (starts_with("/api/v0/classify/", uri) == true) {
    
    int classify_id = -1, next_post_id = -1;
    bool local_next_post_id_rollover_done = false;
    
    // GET for /api/v0/classify/id/
    if (!strcmp(http_method, "GET")) {

      sscanf((const char *)uri, "/api/v0/classify/%d", &classify_id);
      pthread_mutex_lock(&restful_ptr->next_post_id_lock);
      next_post_id = restful_ptr->next_post_classify_id;
      pthread_mutex_unlock(&restful_ptr->next_post_id_lock);

      pthread_mutex_lock(&next_post_id_rollover_done_lock);
      local_next_post_id_rollover_done = next_post_id_rollover_done;
      pthread_mutex_unlock(&next_post_id_rollover_done_lock);

      if ( (next_post_id == 0) || (local_next_post_id_rollover_done == true) )
	next_post_id = get_config()->max_queue_length; 
      
      syslog(LOG_INFO, "= GET %s | classify_id: %d/%d | restful_ptr: %p", uri, classify_id, next_post_id, restful_ptr);
      
      if ( (classify_id >= 0) && (next_post_id >= 0) && (classify_id < next_post_id) )
	{
	  restful_ptr->get_request_classify_id = classify_id;
	  *return_http_flag = HTTP_200;
	}
      else
	*return_http_flag = HTTP_404;

      //if (classify_id == restful_ptr->cur_classify_info.classify_id) 
      //*return_http_flag = HTTP_200;	
      //else {
      //*return_http_flag = HTTP_404;
      //syslog(LOG_INFO, "GET %s | classify_id: %d | restful_ptr: %p | ID NOT FOUND", uri, classify_id, restful_ptr);
      //}

    /* } else if (!strcmp(http_method, "DELETE")) {

      sscanf((const char *)uri, "/api/v0/classify/%d", &classify_id);
      syslog(LOG_INFO, "= DELETE %s | classify_id: %d", uri, classify_id);

      //if (classify_id == restful_ptr->cur_classify_info.classify_id) {
      handle_delete_request(return_http_flag, restful_ptr);

      if (*return_http_flag == HTTP_204) {
	syslog(LOG_INFO, "= Setting end_timestamp | %p | %d, %s", restful_ptr, __LINE__, __FILE__);
	clock_gettime(CLOCK_REALTIME, &restful_ptr->cur_classify_info.end_timestamp);
      }

      //} else {
      // *return_http_flag = HTTP_404;
      // syslog(LOG_INFO, "DELETE %s | classify_id: %d | ID NOT FOUND", uri, classify_id);
      //} */
    } else {
      
      //fprintf(stderr, "Access-Control-Allow-Methods: GET, DELETE\n");		 
      fprintf(stderr, "Access-Control-Allow-Methods: GET\n");		 
      *return_http_flag = HTTP_405; // Method Not Allowed
    }
      
  } else {
    fprintf(stderr, "= Resource %s not found\n", uri);
    *return_http_flag = HTTP_404; // Not Found
  }
 
  return 0;
}


static void build_response_json_for_one_classify(restful_comm_struct *restful_ptr, 
						  cJSON **response_json)
{

  int i = 0, cur_classify_status = -1;
  cJSON *config_json = NULL, *results_json = NULL, *labels_array_json = NULL, *confidence_array_json = NULL;
  cJSON *left_array_json = NULL, *right_array_json = NULL, *top_array_json = NULL, *bottom_array_json = NULL;
  // char output_resolution[64];
  classifyapp_struct *cur_classifyapp_data = NULL;
  struct tm *ptm;
  char start_time_string[MEDIUM_FIXED_STRING_SIZE];
  
  *response_json = cJSON_CreateObject();

  /* pthread_mutex_lock(&restful_ptr->next_post_id_lock);
  next_post_id = restful_ptr->next_post_classify_id;
  pthread_mutex_unlock(&restful_ptr->next_post_id_lock);
  */

  cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + restful_ptr->get_request_classify_id);

  pthread_mutex_lock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
  cur_classify_status = cur_classifyapp_data->cur_classify_info.classify_status;
  pthread_mutex_unlock(&cur_classifyapp_data->cur_classify_info.job_status_lock);
  
  cJSON_AddNumberToObject(*response_json, "id", restful_ptr->get_request_classify_id);

  cJSON_AddStringToObject(*response_json, "input", cur_classifyapp_data->appconfig.input_url);
  cJSON_AddStringToObject(*response_json, "output_dir", cur_classifyapp_data->appconfig.output_directory);
  cJSON_AddStringToObject(*response_json, "output_filepath", cur_classifyapp_data->appconfig.output_filepath);
  if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "video"))
    cJSON_AddStringToObject(*response_json, "output_json_filepath", cur_classifyapp_data->appconfig.output_json_filepath);
    
  cJSON_AddItemToObject(*response_json, "config", config_json=cJSON_CreateObject());
  cJSON_AddNumberToObject(config_json, "detection_threshold", cur_classifyapp_data->appconfig.detection_threshold);

  if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "video"))
    {
      cJSON_AddNumberToObject(*response_json, "percentage_completed", cur_classifyapp_data->cur_classify_info.results_info.percentage_completed);
    }

  if (cur_classify_status == CLASSIFY_STATUS_COMPLETED) {

    cJSON_AddStringToObject(*response_json, "status", "finished");
    cJSON_AddNullToObject(*response_json, "error_msg");
    if (cur_classifyapp_data->cur_classify_info.results_info.results_read == false)
      {
	syslog(LOG_INFO, "= Setting results_read to 1 for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id, __FILE__, __LINE__);
	cur_classifyapp_data->cur_classify_info.results_info.results_read = true;
      }
    cJSON_AddNumberToObject(*response_json, "results_read", (int)cur_classifyapp_data->cur_classify_info.results_info.results_read);
    
    if (!strcmp(cur_classifyapp_data->appconfig.input_mode, "image"))
      {
	syslog(LOG_DEBUG, "= Creating results_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	cJSON_AddItemToObject(*response_json, "results", results_json=cJSON_CreateObject());
	syslog(LOG_DEBUG, "= Adding results_json->num_labels_detected for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	cJSON_AddNumberToObject(results_json, "num_labels_detected", cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);

	syslog(LOG_DEBUG, "= Creating labels_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	labels_array_json = cJSON_CreateArray();
	for ( i=0; i<cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected; i++)
	  {
	    syslog(LOG_DEBUG, "= Adding labels %d for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id, i, __FILE__, __LINE__);
	    cJSON_AddItemToArray(labels_array_json, cJSON_CreateString((const char *)cur_classifyapp_data->cur_classify_info.results_info.labels[i]));
	  }
	syslog(LOG_DEBUG, "= Adding labels for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	cJSON_AddItemToObject(results_json, "labels", labels_array_json);

	syslog(LOG_DEBUG, "= Creating confidence_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	confidence_array_json = cJSON_CreateFloatArray((const float *)cur_classifyapp_data->cur_classify_info.results_info.confidence, cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);
	syslog(LOG_DEBUG, "= Adding confidence for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	cJSON_AddItemToObject(results_json, "confidence", confidence_array_json);
	syslog(LOG_DEBUG, "= Adding processing_time_in_ms for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	cJSON_AddNumberToObject(results_json, "processing_time_in_ms", cur_classifyapp_data->cur_classify_info.results_info.processing_time_in_seconds*1000.0);

	syslog(LOG_DEBUG, "= Creating left_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	left_array_json = cJSON_CreateIntArray((const int *)cur_classifyapp_data->cur_classify_info.results_info.left, cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);
	cJSON_AddItemToObject(results_json, "left", left_array_json);

	syslog(LOG_DEBUG, "= Creating right_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	right_array_json = cJSON_CreateIntArray((const int *)cur_classifyapp_data->cur_classify_info.results_info.right, cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);
	cJSON_AddItemToObject(results_json, "right", right_array_json);

	syslog(LOG_DEBUG, "= Creating top_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	top_array_json = cJSON_CreateIntArray((const int *)cur_classifyapp_data->cur_classify_info.results_info.top, cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);
	cJSON_AddItemToObject(results_json, "top", top_array_json);

	syslog(LOG_DEBUG, "= Creating bottom_array_json for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
	bottom_array_json = cJSON_CreateIntArray((const int *)cur_classifyapp_data->cur_classify_info.results_info.bottom, cur_classifyapp_data->cur_classify_info.results_info.num_labels_detected);
	cJSON_AddItemToObject(results_json, "bottom", bottom_array_json);
	
      }
    else
      {
	cJSON_AddNumberToObject(results_json, "percentage_completed", cur_classifyapp_data->cur_classify_info.results_info.percentage_completed);
      }
       
  } else {

    if (cur_classify_status == CLASSIFY_STATUS_STOPPED) {

      cJSON_AddStringToObject(*response_json, "status", "stopped");
      cJSON_AddNullToObject(*response_json, "error_msg");

    } else if (cur_classify_status == CLASSIFY_STATUS_ERROR) {

      cJSON_AddStringToObject(*response_json, "status", "error");
      cJSON_AddStringToObject(*response_json, "error_msg", "classify error: check syslog");

    } else if (cur_classify_status == CLASSIFY_STATUS_INPUT_ERROR) {

      cJSON_AddStringToObject(*response_json, "status", "error");
      cJSON_AddStringToObject(*response_json, "error_msg", "Input URL/path invalid");

    } else if (cur_classify_status == CLASSIFY_STATUS_OUTPUT_ERROR) {

      cJSON_AddStringToObject(*response_json, "status", "error");
      cJSON_AddStringToObject(*response_json, "error_msg", "Output folder does not exist");

    } else {

      cJSON_AddStringToObject(*response_json, "status", "running");
      cJSON_AddNullToObject(*response_json, "error_msg");
	
    } 
    
  }

  syslog(LOG_DEBUG, "= Adding time_started for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
  
  // Report time this job started
  ptm = localtime(&cur_classifyapp_data->cur_classify_info.start_timestamp.tv_sec);
  strftime(start_time_string, sizeof(start_time_string), "%Y/%m/%d %H:%M:%S", ptm);
  //sprintf(millisec, "%0.3f", (double)(cur_classifyapp_data->cur_classify_info.start_timestamp.tv_nsec)/NANOSEC);
  //strcat(start_time_string, millisec);
  cJSON_AddStringToObject(*response_json, "time_started", start_time_string);

  /* } else {

    cJSON_AddNumberToObject(*response_json, "id", -1);  
    cJSON_AddStringToObject(*response_json, "status", "idle");
    cJSON_AddStringToObject(*response_json, "input", "None");
    
    } */

  syslog(LOG_DEBUG, "= Returning from build_response_for_one_classify for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
  
  return;
}


static void get_response_for_get_single(restful_comm_struct *restful, char **rendered_json)
{
  cJSON *resp_json = NULL;
  // float cur_perc_finished = -1.0;
  // int cur_classify_status = -1;

  // TODO: should not be doing this during the HTTP REQ-REP phase
  // Should instead get this info beforehand and report the last data read    
  classifyapp_struct *cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + restful->get_request_classify_id);
  
  syslog(LOG_DEBUG, "= Getting end_timestamp | %p | %d, %s", restful, __LINE__, __FILE__);
  build_response_json_for_one_classify(restful, &resp_json);
  syslog(LOG_DEBUG, "= About to cJSON_Print for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
  *rendered_json = cJSON_Print(resp_json);
  syslog(LOG_DEBUG, "= Done with cJSON_Print for ID: %d, %s:%d", cur_classifyapp_data->cur_classify_info.classify_id,__FILE__, __LINE__);
  
  cJSON_Delete(resp_json);		
  resp_json = NULL;

  return;
}

static void get_response_for_post(restful_comm_struct *restful, cJSON **parsedjson, char **rendered_json)
{
  // float cur_perc_finished = -1.0;
  struct tm *ptm;
  char start_time_string[MEDIUM_FIXED_STRING_SIZE];
  int next_post_id = -1;
  classifyapp_struct *cur_classifyapp_data = NULL;
 
  pthread_mutex_lock(&restful->next_post_id_lock);
  next_post_id = restful->next_post_classify_id;
  pthread_mutex_unlock(&restful->next_post_id_lock);

  if (next_post_id == 0)
    next_post_id = get_config()->max_queue_length;
  
  cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + (next_post_id-1));
  
  cJSON_AddNumberToObject(*parsedjson, "id", cur_classifyapp_data->cur_classify_info.classify_id);
  cJSON_AddStringToObject(*parsedjson, "status", "running");
  cJSON_AddNullToObject(*parsedjson, "error_msg");

  ptm = localtime(&cur_classifyapp_data->cur_classify_info.start_timestamp.tv_sec);
  strftime(start_time_string, sizeof(start_time_string), "%Y/%m/%d %H:%M:%S", ptm);
  cJSON_AddStringToObject(*parsedjson, "time_started", start_time_string);

  *rendered_json = cJSON_Print(*parsedjson);

  syslog(LOG_DEBUG, "= POST JSON response: %s, %s:%d\n", *rendered_json, __FILE__, __LINE__);
  
  return;
}

static void send_response_to_client(int8_t *return_http_flag, 
				    cJSON **parsed_json, 
				    int *client_sock_id, 
				    char *http_method, 
				    restful_comm_struct *restful_ptr)
{

  char return_msg[1024], *return_msg2 = NULL;
  char *rendered = NULL;
  int rendered_len = 0, fixed_strlen = 0, total_len = 0;
  bool mem_allocated = false;
  
  /* int next_post_id = -1;
     classifyapp_struct *cur_classifyapp_data = NULL;
 
     pthread_mutex_lock(&restful_ptr->next_post_id_lock);
     next_post_id = restful_ptr->next_post_classify_id;
     pthread_mutex_unlock(&restful_ptr->next_post_id_lock);
     
     if (next_post_id > 0)
     cur_classifyapp_data = (classifyapp_struct *)(restful_ptr->classifyapp_data + (next_post_id-1)); 
  */
  
  memset(return_msg, 0, sizeof(return_msg));		  

  switch(*return_http_flag) {

  case HTTP_200:
    get_response_for_get_single(restful_ptr, &rendered);
    
    rendered_len = (int)(strlen(rendered) + 4);
    fixed_strlen = (int)(strlen("HTTP/1.1 200 OK\r\n"
				"Server: igolgi\r\n"
				"Date: Today\r\n"
				"Content-Type: application/json\r\n"
				"Content-Length:1024\r\n"));
    total_len = rendered_len + fixed_strlen + 8;
    if (total_len >= 1024)
      {

	syslog(LOG_DEBUG, "= JSON RESPONSE: strlen(fixed): %d, strlen(rendered): %d, total: %d, %s:%d",
	       fixed_strlen,
	       rendered_len,
	       total_len,
	       __FILE__, __LINE__);

	return_msg2 = (char *)malloc(total_len * sizeof(char));
	if (!return_msg2)
	  {
	  
	    syslog(LOG_ERR, "= Could not allocate memory, everything ok? %s:%d", __FILE__, __LINE__);
	    sprintf(return_msg,
		    "HTTP/1.1 %d %s\r\n"                       
		    "Server: igolgi\r\n"
		    "Date: Today\r\n"
		    "Access-Control-Allow-Methods: GET, POST\r\n",
		    400, "Could not allocate memory");  
	    
	  }
	else
	  {
	
	    memset(return_msg2, 0, sizeof(total_len));
	    
	    sprintf(return_msg2,
		    "HTTP/1.1 200 OK\r\n"
		    "Server: igolgi\r\n"
		    "Date: Today\r\n"
		    "Content-Type: application/json\r\n"
		    "Content-Length: %d\r\n"
		    "\r\n"
		    "%s\r\n\r\n",
		    (int)(strlen(rendered)+4),  // +4 for the \r\n\r\n
		    rendered);
	    
	    mem_allocated = true;
	    
	  }
	
      }
    else
      {
	sprintf(return_msg,
		"HTTP/1.1 %d %s\r\n"                       
		"Server: igolgi\r\n"
		"Date: Today\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"\r\n"
		"%s\r\n\r\n",
		200, "OK",
		(int)(strlen(rendered)+4),  // +4 for the \r\n\r\n
		rendered);
      }
    
    free(rendered);
    rendered = NULL;
    // syslog(LOG_DEBUG, "= After free, %s:%d", __FILE__, __LINE__);
    break;
    
  case HTTP_201: 

    get_response_for_post(restful_ptr, parsed_json, &rendered);
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Content-Type: application/json\r\n"
	    "Content-Length: %d\r\n"
	    "\r\n"
	    "%s\r\n\r\n",
	    201, "Created",
	    (int)(strlen(rendered)+4),  // +4 for the \r\n\r\n
	    rendered);
		  
    cJSON_Delete(*parsed_json);		
    *parsed_json = NULL;
    free(rendered);
    rendered = NULL;

    break;
  
  case HTTP_204:

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    // "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    204, "No Content");		      
    break;
    
  case HTTP_400:

    /* if (cur_classifyapp_data->restful_err_str) {
      sprintf(return_msg,
	      "HTTP/1.1 %d %s\r\n"                       
	      "Server: zipreel\r\n"
	      "Date: Today\r\n"
	      "Access-Control-Allow-Methods: GET, POST\r\n",
	      400, cur_classifyapp_data->restful_err_str);		      
      free_mem(cur_classifyapp_data->restful_err_str);
    } else
    */
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    400, "Bad Request");		      
    break;

  case HTTP_404:
    
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    404, "Not Found");    
    break;

  case HTTP_405:
    
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    405, "Method Not Allowed");    
    break;

  case HTTP_501:

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    501, "Not Implemented");		  
    break;

  case HTTP_503:

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    503, "Service unavailable");		  

    cJSON_Delete(*parsed_json);		
    *parsed_json = NULL;

    break;    

  default:
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST\r\n",
	    400, "Bad Request");		      
    break;

  }

  if (mem_allocated == false)
    send(*client_sock_id, return_msg, strlen(return_msg), 0);
  else
    {
      send(*client_sock_id, return_msg2, strlen(return_msg2), 0);
      free(return_msg2);
      return_msg2 = NULL;
      mem_allocated =false;
    }
  
  close(*client_sock_id);
  *client_sock_id = 0;

  syslog(LOG_DEBUG, "= About to return from %s:%d", __FILE__, __LINE__);
  return;
}

static void *restful_comm_thread_func(void *context)
{   
  restful_comm_struct *restful = (restful_comm_struct*)context;
  // int child_socket = 0;
  // int ret; 
  // int i;
  int servsock = 0;
  fd_set sockset;
  struct timeval select_timeout;                       
  struct sockaddr_in clientaddr;
  unsigned int clientsize;
  int total_received = 0;
  int message_size = 0;
  int receive_buffersize = 0;
#define MAX_RECEIVE_BUFFER_SIZE 8192   
    
  uint8_t receive_buffer[8192];    
  int8_t http_response_status = 0;
  cJSON *json = NULL;

  clientsize = sizeof(clientaddr);
  servsock = open_tcp_server_socket(restful->restful_server_port);
  if (servsock <= 0) {
    syslog(LOG_ERR, "= Unable to open tcp server socket at port %d for RESTful service. RESTful thread is quitting.", restful->restful_server_port);
    restful->is_restful_thread_active = 0;
    return NULL;
  }
       
  while (restful->is_restful_thread_active) {                                   
    FD_ZERO(&sockset);
    FD_SET(servsock, &sockset);
    select_timeout.tv_sec = 1;
    select_timeout.tv_usec = 100000;
    syslog(LOG_DEBUG, "RESTFUL1: %d %d, %s", restful->is_restful_thread_active, __LINE__, __FILE__);

    if (select(servsock + 1, &sockset, NULL, NULL, &select_timeout) != 0) {

      if (FD_ISSET(servsock, &sockset)) {

	uint32_t *data;             
	int clientsock;
	char cmd[128];
	// char content_type[128];

	clientsock = accept(servsock, (struct sockaddr *)&clientaddr, &clientsize);
	syslog(LOG_INFO,"= Received tcp request on port %d from %s:%d\n", 
	       restful->restful_server_port, 
	       inet_ntoa(clientaddr.sin_addr),
	       clientaddr.sin_port);
	// fprintf(stderr, "handling the request\n");
	total_received = 0;
	memset(receive_buffer, 0, sizeof(receive_buffer));
	receive_buffersize = RESTFUL_MAX_MESSAGE_SIZE - 1;
	//while (total_received < STATMUX_MESSAGE_SIZE) {
	message_size = recv(clientsock, receive_buffer, receive_buffersize, 0);
	total_received += message_size;
	//}

	data = (uint32_t*)&receive_buffer[0];          
	//fprintf(stderr,"received data:\n%s\n", (char *)data);
		
	//============ NEED TO MAKE THIS REALLY ROBUST ==============
	if (data) {
	  parse_input_request_data(&data, &json, &http_response_status, &cmd[0], restful);
	} else {
	  syslog(LOG_ERR, "= No data found in incoming request\n");		 
	  http_response_status = HTTP_400;
	}
		
	syslog(LOG_DEBUG, "= HTTP_REQ_METHOD: %s RESPONSE: %d", cmd, http_response_status);	
	send_response_to_client(&http_response_status, &json, &clientsock, &cmd[0], restful);

	// Tell parent to start classify
	if ((!strcmp(cmd, "POST")) && (http_response_status == HTTP_201)) {

	  igolgi_message_struct *dispatch_msg = (igolgi_message_struct*)malloc(sizeof(igolgi_message_struct));
	  int cur_classify_thread_status = -1, cur_dispatch_queue_size = -1, next_post_id = -1;
	  
	  if (dispatch_msg) {

	    memset(dispatch_msg, 0, sizeof(igolgi_message_struct));

	    pthread_mutex_lock(&restful->next_post_id_lock);
	    next_post_id = restful->next_post_classify_id;
	    pthread_mutex_unlock(&restful->next_post_id_lock);
	    if (next_post_id == 0)
	      next_post_id = get_config()->max_queue_length;
	    dispatch_msg->idx = next_post_id - 1;
	    syslog(LOG_DEBUG, "= Sending dispatch msg: %d", http_response_status);
	    message_queue_push_front(restful->dispatch_queue, dispatch_msg);
	    dispatch_msg = NULL;
	    sem_post(restful->dispatch_queue_sem);
	    syslog(LOG_DEBUG, "= Pushed msg to dispatch_queue and post to sem, %s:%d", __FILE__, __LINE__);

	    // Set thread busy signal where the queue is filled up
	    cur_dispatch_queue_size = message_queue_count(restful->dispatch_queue);
	    pthread_mutex_lock(&restful->thread_status_lock);
	    cur_classify_thread_status = restful->classify_thread_status;
	    pthread_mutex_unlock(&restful->thread_status_lock);	      

	    if ( (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_IDLE) && (cur_dispatch_queue_size > (get_config()->max_queue_length-1) ) )
	      {
		syslog(LOG_INFO, "= cur_busy_status: idle, dispatch message queue size: %d, %s:%d", cur_dispatch_queue_size, __FILE__, __LINE__);
		syslog(LOG_INFO, "= SETTING STATUS TO BUSY, Dispatch message queue size: %d, %s:%d", cur_dispatch_queue_size, __FILE__, __LINE__);
		pthread_mutex_lock(&restful->thread_status_lock);
		restful->classify_thread_status = CLASSIFY_THREAD_STATUS_BUSY;
		cur_classify_thread_status = restful->classify_thread_status;
		pthread_mutex_unlock(&restful->thread_status_lock);	      
	      }
	  } 
	  // syslog(LOG_INFO, "= Should push msg to msg_queue here, %s:%d", __FILE__, __LINE__);
	  
	} 
				
      }
    }
  }
  syslog(LOG_INFO, "= About to leave restful_thread, line:%d, %s", __LINE__, __FILE__);
  close(servsock);
  syslog(LOG_INFO, "= Leaving restful_thread, line:%d, %s", __LINE__, __FILE__);
  return NULL;
}    

restful_comm_struct *restful_comm_create(int *server_port)
{
  restful_comm_struct *restful = (restful_comm_struct*)malloc(sizeof(restful_comm_struct));
  int i = 0;
  classifyapp_struct *cur_classifyapp_data = NULL;
    
  if (!restful) {
    return NULL;
  }

  memset(restful, 0, sizeof(restful_comm_struct));

  restful->dispatch_queue_sem = (sem_t*)malloc(sizeof(sem_t));
  sem_init(restful->dispatch_queue_sem, 0, 0);
  
  pthread_mutex_init(&restful->thread_status_lock, NULL);
  pthread_mutex_init(&restful->next_post_id_lock, NULL);
  pthread_mutex_init(&restful->cur_process_id_lock, NULL);
  
  // pthread_mutex_init(&restful->classifyapp_init_lock, NULL);
  syslog(LOG_INFO, "= SETTING STATUS TO NOTSETUP at initialization, %s:%d", __FILE__, __LINE__);
  restful->classify_thread_status = CLASSIFY_THREAD_STATUS_NOTSETUP;

  //syslog(LOG_DEBUG, "CRASH FLAG: %d/%p | LINE: %d, %s", 
  //(int)classifyapp_data->appconfig.crash_recovery_flag, classifyapp_data, __LINE__, __FILE__);

  restful->classifyapp_data = (classifyapp_struct *)malloc(sizeof(classifyapp_struct) * get_config()->max_queue_length);
  if (restful->classifyapp_data == NULL)
    {
      syslog(LOG_ERR, "= Could not allocate memory for critical struct, out of memory? Quitting");
      return NULL;
    }
  memset(restful->classifyapp_data, 0, sizeof(classifyapp_struct) * get_config()->max_queue_length);

  for (i = 0; i<get_config()->max_queue_length; i++)
    {
      cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + i);
      pthread_mutex_init(&cur_classifyapp_data->cur_classify_info.job_status_lock, NULL);
      cur_classifyapp_data->cur_classify_info.classify_id = START_CLASSIFY_ID;
      cur_classifyapp_data->cur_classify_info.classify_status = -1;
    }
  
  restful->next_post_classify_id = -1;
  restful->cur_process_classify_id = START_CLASSIFY_ID;
  
  restful->restful_server_port = *server_port;
  
  syslog(LOG_INFO, "= Done allocating basic struct needed for operations, %s:%d", __FILE__, __LINE__);
  
  return restful;
}

int restful_comm_destroy(restful_comm_struct *restful)
{
  int i = 0;
  classifyapp_struct *cur_classifyapp_data = NULL;

  if (restful) {
    pthread_mutex_destroy(&restful->thread_status_lock);
    pthread_mutex_destroy(&restful->next_post_id_lock);
    pthread_mutex_destroy(&restful->cur_process_id_lock);

    if (restful->dispatch_queue_sem) {
      sem_destroy(restful->dispatch_queue_sem);
      free(restful->dispatch_queue_sem);
      restful->dispatch_queue_sem = NULL;
    }
    
    for (i = 0; i<get_config()->max_queue_length; i++)
      {
	cur_classifyapp_data = (classifyapp_struct *)(restful->classifyapp_data + i);
	pthread_mutex_destroy(&cur_classifyapp_data->cur_classify_info.job_status_lock);
      }

    free(restful->classifyapp_data);
    restful->classifyapp_data = NULL;

    message_queue_destroy(restful->dispatch_queue);
    restful->dispatch_queue = NULL;
    
    free(restful);
    restful = NULL;
    
  }    

  return 0;
}

int restful_comm_start(restful_comm_struct *restful, void *dispatch_queue)
{
  if (restful) {
    restful->is_restful_thread_active = 1;
    restful->dispatch_queue = dispatch_queue;
    syslog(LOG_INFO, "= Starting restful_thread");
    pthread_create(&restful->restful_server_thread_id, NULL, restful_comm_thread_func, (void*)restful);
  }
  return 0;
}

int restful_comm_stop(restful_comm_struct *restful)
{
  if (restful) {
    restful->is_restful_thread_active = 0;
    syslog(LOG_INFO, "= RESTFUL_COMM_STOP: %d %d, %s", restful->is_restful_thread_active, __LINE__, __FILE__);
    pthread_join(restful->restful_server_thread_id, NULL);
    restful_comm_destroy(restful);
  }
  return 0;
}

int restful_comm_thread_start(restful_comm_struct **restful, void **dispatch_queue, int *server_port)
{

  /* Create thread to listen to incoming HTTP requests to start/stop classifys or get their status */
  *dispatch_queue = (void*)message_queue_create();
  *restful = restful_comm_create(server_port);
  restful_comm_start(*restful, *dispatch_queue);

  return 0;
}


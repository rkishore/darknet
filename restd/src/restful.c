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
*/

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

static int store_output_file_loc(cJSON **output_file_dir, cJSON **output_file_prefix, classifyapp_struct *classifyapp_data, int8_t *response_http_code) 
{
  cJSON *output_dir = *output_file_dir, *output_fileprefix = *output_file_prefix;
  // mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
  // int output_fd = -1;

  if ( (output_dir) && (output_file_prefix) ) {

    snprintf(classifyapp_data->appconfig.output_directory, LARGE_FIXED_STRING_SIZE-1, "%s", output_dir->valuestring);
    snprintf(classifyapp_data->appconfig.output_fileprefix, LARGE_FIXED_STRING_SIZE-1, "%s", output_fileprefix->valuestring);
    //fprintf(stderr,"output_data: %s\n", classifyapp_data->appconfig.output_filename);
    //syslog(LOG_INFO,"= output_filename: %s\n", classifyapp_data->appconfig.output_filename);

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

static int handle_post_request(cJSON **parsedjson, int8_t *return_http_flag, restful_comm_struct *restful)
{
  // char *out = NULL;
  cJSON *input_data = NULL, *output_dir = NULL, *config_data = NULL, *output_fileprefix = NULL, *input_type_data = NULL;
  classifyapp_struct *cur_classifyapp_data = restful->classifyapp_data;
  int cur_classify_thread_status = -1;

  // Check if classify thread is idle
  pthread_mutex_lock(&restful->thread_status_lock);
  cur_classify_thread_status = restful->classify_thread_status;
  pthread_mutex_unlock(&restful->thread_status_lock);

  if (cur_classify_thread_status == CLASSIFY_THREAD_STATUS_IDLE) {

    //fprintf(stderr,"decoding data:%s\n", lineptr);
    input_data = cJSON_GetObjectItem(*parsedjson, "input");
    input_type_data = cJSON_GetObjectItem(*parsedjson, "type");
    output_dir = cJSON_GetObjectItem(*parsedjson, "output_dir");
    output_fileprefix = cJSON_GetObjectItem(*parsedjson, "output_fileprefix");
    config_data = cJSON_GetObjectItem(*parsedjson, "config");

    if ( (input_data == NULL) || (input_type_data == NULL) ||
	 (output_dir == NULL) || (output_fileprefix == NULL) || (config_data == NULL) ) {
      *return_http_flag = HTTP_400;
      return -1;
    }

    if (store_input_loc(&input_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;

    if (store_input_type(&input_type_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;    

    if (store_output_file_loc(&output_dir, &output_fileprefix, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;

    if (store_config_info(&config_data, cur_classifyapp_data, return_http_flag) < 0) 
      return -1;    

    *return_http_flag = HTTP_201;
    
    pthread_mutex_lock(&restful->thread_status_lock);
    restful->classify_thread_status = CLASSIFY_THREAD_STATUS_BUSY;
    pthread_mutex_unlock(&restful->thread_status_lock);	      
    
    restful->cur_classify_info.classify_id = cur_classify_id++;
    restful->cur_classify_info.classify_status = CLASSIFY_STATUS_RUNNING;
    clock_gettime(CLOCK_REALTIME, &restful->cur_classify_info.start_timestamp);

    syslog(LOG_INFO, "= RESTFUL_THREAD_RCV | image_url: %s (%s) | output_directory: %s | output_fileprefix: %s\n", 
	   cur_classifyapp_data->appconfig.input_url,
	   cur_classifyapp_data->appconfig.input_type,
	   cur_classifyapp_data->appconfig.output_directory,
	   cur_classifyapp_data->appconfig.output_fileprefix);
    
    restful->cur_classify_info.percentage_finished = 0;

    //cJSON_Delete(json);		
    //fprintf(stderr,"\n\n\n\ndecoded data\n");			    
	    
  } else {
    *return_http_flag = HTTP_503;
  }
  
  return 0;
}

/*
static void parse_and_handle_post_request(uint32_t **input_request_data, cJSON **parsed_json, 
					  int8_t *return_http_flag, restful_comm_struct *restful_ptr)
{
  char *lineptr = strstr((const char *)*input_request_data, "\r\n\r\n");
  if (!lineptr) {      
    lineptr = strstr((const char *)*input_request_data,"\n\n");
    if (lineptr) {
      lineptr += 2;		    
    }
  } else {		 
    lineptr += 4;		    
  }
		
  fprintf(stderr,"lineptr: %s\n", lineptr);
		
  if (lineptr) {
    *parsed_json = cJSON_Parse(lineptr);
    if (!*parsed_json) {
      fprintf(stderr,"unable to parse msg\n");
      *return_http_flag = HTTP_400; // Bad Request
    } else {
      handle_post_request(parsed_json, return_http_flag, restful_ptr);
    }
  }

  return;
}
*/

static void copy_to_global_config(restful_comm_struct *restful_ptr)
{
  classifyapp_struct *classifyapp_info = restful_ptr->classifyapp_data;

  memset(mod_config()->image_url, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->image_url, classifyapp_info->appconfig.input_url, strlen(classifyapp_info->appconfig.input_url));

  memset(mod_config()->input_type, 0, SMALL_FIXED_STRING_SIZE);
  memcpy(mod_config()->input_type, classifyapp_info->appconfig.input_type, strlen(classifyapp_info->appconfig.input_type));

  memset(mod_config()->output_directory, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->output_directory, classifyapp_info->appconfig.output_directory, strlen(classifyapp_info->appconfig.output_directory));

  memset(mod_config()->output_fileprefix, 0, LARGE_FIXED_STRING_SIZE);
  memcpy(mod_config()->output_fileprefix, classifyapp_info->appconfig.output_fileprefix, strlen(classifyapp_info->appconfig.output_fileprefix));

  syslog(LOG_INFO, "= COPIED DISPATCH_MSG | image_url: %s (%s) | output_directory: %s | output_fileprefix: %s\n", 
	 get_config()->image_url,
	 get_config()->input_type,
	 get_config()->output_directory,
	 get_config()->output_fileprefix);

  return;
}

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
      
      syslog(LOG_INFO, "= %s %s | restful_ptr: %p", http_method, uri, restful_ptr);

      lineptr = strstr((const char *)*input_request_data, "\r\n\r\n");
      if (!lineptr) {      
	lineptr = strstr((const char *)*input_request_data,"\n\n");
	if (lineptr) {
	  lineptr += 2;		    
	}
      } else {		 
	lineptr += 4;		    
      }
	
      // fprintf(stderr,"lineptr: %s\n", lineptr);
	
      if (lineptr) {
	*parsed_json = cJSON_Parse(lineptr);
	if (!*parsed_json) {
	  fprintf(stderr,"unable to parse msg\n");
	  *return_http_flag = HTTP_400; // Bad Request
	} else {
	  handle_post_request(parsed_json, return_http_flag, restful_ptr);
	  if (*return_http_flag == HTTP_201)
	    copy_to_global_config(restful_ptr); 
	}
      }

      // parse_and_handle_post_request(input_request_data, parsed_json, return_http_flag, restful_ptr);
      
    } else {

      fprintf(stderr, "Access-Control-Allow-Methods: POST\n");		 
      syslog(LOG_ERR, "Access-Control-Allow-Methods: POST\n");		 
      *return_http_flag = HTTP_405;

    }

  } else if (starts_with("/api/v0/classify/", uri) == true) {
    
    int classify_id = -1;

    // GET for /api/v0/classify/id/
    if (!strcmp(http_method, "GET")) {

      sscanf((const char *)uri, "/api/v0/classify/%d", &classify_id);
      syslog(LOG_DEBUG, "GET %s | classify_id: %d/%d | restful_ptr: %p", uri, classify_id, restful_ptr->cur_classify_info.classify_id, restful_ptr);
      
      //if (restful_ptr->cur_classify_info.classify_id >= 0)
      *return_http_flag = HTTP_200;	
      //else
      //*return_http_flag = HTTP_404;

      //if (classify_id == restful_ptr->cur_classify_info.classify_id) 
      //*return_http_flag = HTTP_200;	
      //else {
      //*return_http_flag = HTTP_404;
      //syslog(LOG_INFO, "GET %s | classify_id: %d | restful_ptr: %p | ID NOT FOUND", uri, classify_id, restful_ptr);
      //}

    } else if (!strcmp(http_method, "DELETE")) {

      sscanf((const char *)uri, "/api/v0/classify/%d", &classify_id);
      syslog(LOG_INFO, "= DELETE %s | classify_id: %d", uri, classify_id);

      //if (classify_id == restful_ptr->cur_classify_info.classify_id) {
      handle_delete_request(return_http_flag, restful_ptr);

      if (*return_http_flag == HTTP_204) {
	syslog(LOG_INFO, "= Setting end_timestamp | %p | %d, %s", restful_ptr, __LINE__, __FILE__);
	clock_gettime(CLOCK_REALTIME, &restful_ptr->cur_classify_info.end_timestamp);
      }

      //} else {
      //*return_http_flag = HTTP_404;
      //syslog(LOG_INFO, "DELETE %s | classify_id: %d | ID NOT FOUND", uri, classify_id);
      //}
    } else {

      fprintf(stderr, "Access-Control-Allow-Methods: GET, DELETE\n");		 
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

  int cur_classify_status = -1, i = 0;
  cJSON *config_json = NULL, *results_json = NULL, *labels_array_json = NULL, *confidence_array_json = NULL;
  // char output_resolution[64];
  // classifyapp_struct *cur_classifyapp_data = restful_ptr->classifyapp_data;
  struct tm *ptm;
  char start_time_string[MEDIUM_FIXED_STRING_SIZE], running_time_string[MEDIUM_FIXED_STRING_SIZE];
  struct timespec cur_timestamp;
  long running_time_sec = -1, running_time_rem = -1, running_time_hr = -1; //, running_time_min = -1;

  *response_json = cJSON_CreateObject();

  /* pthread_mutex_lock(&restful_ptr->cur_classify_info.job_percent_complete_lock);
  cur_perc_finished = restful_ptr->cur_classify_info.percentage_finished;
  pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_percent_complete_lock);
  */

  pthread_mutex_lock(&restful_ptr->cur_classify_info.job_status_lock);
  cur_classify_status = restful_ptr->cur_classify_info.classify_status;
  pthread_mutex_unlock(&restful_ptr->cur_classify_info.job_status_lock);

  cJSON_AddNumberToObject(*response_json, "id", restful_ptr->cur_classify_info.classify_id);

  if (restful_ptr->cur_classify_info.classify_id >= 0) {

    cJSON_AddStringToObject(*response_json, "input", get_config()->image_url);
    cJSON_AddStringToObject(*response_json, "output_dir", get_config()->output_directory);
    cJSON_AddStringToObject(*response_json, "output_fileprefix", get_config()->output_fileprefix);
    cJSON_AddItemToObject(*response_json, "config", config_json=cJSON_CreateObject());
    cJSON_AddNumberToObject(config_json, "detection_threshold", restful_ptr->classifyapp_data->appconfig.detection_threshold);

    if (cur_classify_status == CLASSIFY_STATUS_COMPLETED) {

      cJSON_AddStringToObject(*response_json, "status", "finished");
      cJSON_AddNullToObject(*response_json, "error_msg");

      cJSON_AddItemToObject(*response_json, "results", results_json=cJSON_CreateObject());
      cJSON_AddNumberToObject(results_json, "num_labels_detected", restful_ptr->cur_classify_info.results_info.num_labels_detected);

      labels_array_json = cJSON_CreateArray();
      for ( i=0; i<restful_ptr->cur_classify_info.results_info.num_labels_detected; i++)
	{
	  cJSON_AddItemToArray(labels_array_json, cJSON_CreateString((const char *)restful_ptr->cur_classify_info.results_info.labels[i]));
	}
      cJSON_AddItemToObject(results_json, "labels", labels_array_json);

      confidence_array_json = cJSON_CreateFloatArray((const float *)restful_ptr->cur_classify_info.results_info.confidence, restful_ptr->cur_classify_info.results_info.num_labels_detected);
      cJSON_AddItemToObject(results_json, "confidence", confidence_array_json);
      cJSON_AddNumberToObject(results_json, "processing_time_in_ms", restful_ptr->cur_classify_info.results_info.processing_time_in_seconds*1000.0);
      
      running_time_sec = (long)difftime(restful_ptr->cur_classify_info.end_timestamp.tv_sec, restful_ptr->cur_classify_info.start_timestamp.tv_sec);
      running_time_hr = running_time_sec / SECONDS_IN_HOUR;
      running_time_rem = running_time_sec % SECONDS_IN_HOUR;
      sprintf(running_time_string, "%02ld:%02ld:%02ld", running_time_hr, running_time_rem / 60, running_time_rem % 60);

    } else {

      if (cur_classify_status == CLASSIFY_STATUS_STOPPED) {

	cJSON_AddStringToObject(*response_json, "status", "stopped");
	cJSON_AddNullToObject(*response_json, "error_msg");

	syslog(LOG_DEBUG, "Getting end_timestamp | %p | %d, %s", restful_ptr, __LINE__, __FILE__);
	running_time_sec = (long)difftime(restful_ptr->cur_classify_info.end_timestamp.tv_sec, restful_ptr->cur_classify_info.start_timestamp.tv_sec);
	running_time_hr = running_time_sec / SECONDS_IN_HOUR;
	running_time_rem = running_time_sec % SECONDS_IN_HOUR;
	sprintf(running_time_string, "%02ld:%02ld:%02ld", running_time_hr, running_time_rem / 60, running_time_rem % 60);

      } else if (cur_classify_status == CLASSIFY_STATUS_ERROR) {

	cJSON_AddStringToObject(*response_json, "status", "error");
	cJSON_AddStringToObject(*response_json, "error_msg", "classify error: check syslog");

	running_time_sec = (long)difftime(restful_ptr->cur_classify_info.end_timestamp.tv_sec, restful_ptr->cur_classify_info.start_timestamp.tv_sec);
	running_time_hr = running_time_sec / SECONDS_IN_HOUR;
	running_time_rem = running_time_sec % SECONDS_IN_HOUR;
	sprintf(running_time_string, "%02ld:%02ld:%02ld", running_time_hr, running_time_rem / 60, running_time_rem % 60);
      
      } else {

	cJSON_AddStringToObject(*response_json, "status", "running");
	cJSON_AddNullToObject(*response_json, "error_msg");

	// Report running time for this job
	clock_gettime(CLOCK_REALTIME, &cur_timestamp);      
      
	running_time_sec = (long)difftime(cur_timestamp.tv_sec, restful_ptr->cur_classify_info.start_timestamp.tv_sec);
	running_time_hr = running_time_sec / SECONDS_IN_HOUR;
	running_time_rem = running_time_sec % SECONDS_IN_HOUR;
	sprintf(running_time_string, "%02ld:%02ld:%02ld", running_time_hr, running_time_rem / 60, running_time_rem % 60);
      
      } 
    
    }
    
    // Report time this job started
    ptm = localtime(&restful_ptr->cur_classify_info.start_timestamp.tv_sec);
    strftime(start_time_string, sizeof(start_time_string), "%Y/%m/%d %H:%M:%S", ptm);
    //sprintf(millisec, "%0.3f", (double)(restful_ptr->cur_classify_info.start_timestamp.tv_nsec)/NANOSEC);
    //strcat(start_time_string, millisec);
    cJSON_AddStringToObject(*response_json, "time_started", start_time_string);

    cJSON_AddStringToObject(*response_json, "time_running", running_time_string);

  } else {

    cJSON_AddStringToObject(*response_json, "status", "idle");

  }

  return;
}


static void get_response_for_get_single(restful_comm_struct *restful, char **rendered_json)
{
  cJSON *resp_json = NULL;
  // float cur_perc_finished = -1.0;
  // int cur_classify_status = -1;

  // TODO: should not be doing this during the HTTP REQ-REP phase
  // Should instead get this info beforehand and report the last data read    

  syslog(LOG_DEBUG, "Getting end_timestamp | %p | %d, %s", restful, __LINE__, __FILE__);
  build_response_json_for_one_classify(restful, &resp_json);
  *rendered_json = cJSON_Print(resp_json);
  
  cJSON_Delete(resp_json);		
  resp_json = NULL;

  return;
}

static void get_response_for_post(restful_comm_struct *restful, cJSON **parsedjson, char **rendered_json)
{
  // float cur_perc_finished = -1.0;
  struct tm *ptm;
  char start_time_string[MEDIUM_FIXED_STRING_SIZE];

  /* pthread_mutex_lock(&restful->cur_classify_info.job_percent_complete_lock);
     cur_perc_finished = restful->cur_classify_info.percentage_finished;
     pthread_mutex_unlock(&restful->cur_classify_info.job_percent_complete_lock);
  */

  cJSON_AddNumberToObject(*parsedjson, "id", restful->cur_classify_info.classify_id);
  cJSON_AddStringToObject(*parsedjson, "status", "running");
  //cJSON_AddNumberToObject(*parsedjson, "percentage_finished", cur_perc_finished);
  cJSON_AddNullToObject(*parsedjson, "error_msg");

  ptm = localtime(&restful->cur_classify_info.start_timestamp.tv_sec);
  strftime(start_time_string, sizeof(start_time_string), "%Y/%m/%d %H:%M:%S", ptm);
  cJSON_AddStringToObject(*parsedjson, "time_started", start_time_string);
  cJSON_AddStringToObject(*parsedjson, "time_running", "00:00:00");

  *rendered_json = cJSON_Print(*parsedjson);

  return;
}

static void send_response_to_client(int8_t *return_http_flag, 
				    cJSON **parsed_json, 
				    int *client_sock_id, 
				    char *http_method, 
				    restful_comm_struct *restful_ptr)
{

  char return_msg[1024];
  char *rendered = NULL;
  // float cur_perc_finished = -1.0;
  // int cur_classify_status = -1;
  // cJSON *last_classify_json = NULL, *cur_classify_json = NULL;
  classifyapp_struct *cur_classifyapp_data = restful_ptr->classifyapp_data; 

  memset(return_msg, 0, sizeof(return_msg));		  

  switch(*return_http_flag) {

  case HTTP_200:
    get_response_for_get_single(restful_ptr, &rendered);

    // syslog(LOG_INFO, "JSON RESPONSE: %s", rendered);

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Content-Type: application/json\r\n"
	    "Content-Length: %d\r\n"
	    "\r\n"
	    "%s\r\n\r\n",
	    200, "OK",
	    (int)(strlen(rendered)+4),  // +4 for the \r\n\r\n
	    rendered);
		  
    free(rendered);
    rendered = NULL;
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
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    204, "No Content");		      
    break;
    
  case HTTP_400:

    if (cur_classifyapp_data->restful_err_str) {
      sprintf(return_msg,
	      "HTTP/1.1 %d %s\r\n"                       
	      "Server: zipreel\r\n"
	      "Date: Today\r\n"
	      "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	      400, cur_classifyapp_data->restful_err_str);		      
      free_mem(cur_classifyapp_data->restful_err_str);
    } else
      sprintf(return_msg,
	      "HTTP/1.1 %d %s\r\n"                       
	      "Server: zipreel\r\n"
	      "Date: Today\r\n"
	      "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	      400, "Bad Request");		      
    break;

  case HTTP_404:
    
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    404, "Not Found");    
    break;

  case HTTP_405:
    
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    405, "Method Not Allowed");    
    break;

  case HTTP_501:

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    501, "Not Implemented");		  
    break;

  case HTTP_503:

    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    503, "Service unavailable");		  

    cJSON_Delete(*parsed_json);		
    *parsed_json = NULL;

    break;    

  default:
    sprintf(return_msg,
	    "HTTP/1.1 %d %s\r\n"                       
	    "Server: zipreel\r\n"
	    "Date: Today\r\n"
	    "Access-Control-Allow-Methods: GET, POST, DELETE\r\n",
	    400, "Bad Request");		      
    break;

  }
  
  send(*client_sock_id, return_msg, strlen(return_msg), 0);		  
  close(*client_sock_id);
  *client_sock_id = 0;

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
		
	// syslog(LOG_INFO, "HTTP_REQ_METHOD: %s RESPONSE: %d", cmd, http_response_status);	
	send_response_to_client(&http_response_status, &json, &clientsock, &cmd[0], restful);

	// Tell parent to start classify
	if ((!strcmp(cmd, "POST")) && (http_response_status == HTTP_201)) {
	  igolgi_message_struct *dispatch_msg = (igolgi_message_struct*)malloc(sizeof(igolgi_message_struct));
	  if (dispatch_msg) {
	    memset(dispatch_msg, 0, sizeof(igolgi_message_struct));
	    dispatch_msg->buffer_flags = http_response_status;
	    syslog(LOG_INFO, "= Sending dispatch msg: %d", http_response_status);
	    message_queue_push_front(restful->dispatch_queue, dispatch_msg);
	    dispatch_msg = NULL;
	  }
	} 
				
      }
    }
  }
  syslog(LOG_INFO, "= About to leave restful_thread, line:%d, %s", __LINE__, __FILE__);
  close(servsock);
  syslog(LOG_INFO, "= Leaving restful_thread, line:%d, %s", __LINE__, __FILE__);
  return NULL;
}    

restful_comm_struct *restful_comm_create(classifyapp_struct *classifyapp_data, int *server_port)
{
  restful_comm_struct *restful = (restful_comm_struct*)malloc(sizeof(restful_comm_struct));

  if (!restful) {
    return NULL;
  }

  memset(restful, 0, sizeof(restful_comm_struct));
  
  pthread_mutex_init(&restful->classify_info_lock, NULL);
  pthread_mutex_init(&restful->thread_status_lock, NULL);
  pthread_mutex_init(&restful->cur_classify_info.job_percent_complete_lock, NULL);
  pthread_mutex_init(&restful->cur_classify_info.job_status_lock, NULL);

  // pthread_mutex_init(&restful->classifyapp_init_lock, NULL);

  restful->classify_thread_status = CLASSIFY_THREAD_STATUS_IDLE;

  //syslog(LOG_DEBUG, "CRASH FLAG: %d/%p | LINE: %d, %s", 
  //(int)classifyapp_data->appconfig.crash_recovery_flag, classifyapp_data, __LINE__, __FILE__);

  restful->classifyapp_data = classifyapp_data;

  //pthread_mutex_lock(&restful->classifyapp_init_lock);
  //restful->classifyapp_init = false;
  //pthread_mutex_unlock(&restful->classifyapp_init_lock);

  restful->cur_classify_info.classify_id = -1;
  restful->cur_classify_info.classify_status = -1;
    
  restful->restful_server_port = *server_port;

  return restful;
}

int restful_comm_destroy(restful_comm_struct *restful)
{
  if (restful) {
    pthread_mutex_destroy(&restful->thread_status_lock);
    pthread_mutex_destroy(&restful->cur_classify_info.job_percent_complete_lock);
    pthread_mutex_destroy(&restful->cur_classify_info.job_status_lock);
    pthread_mutex_destroy(&restful->classify_info_lock);
    //pthread_mutex_destroy(&restful->classifyapp_init_lock);

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
    syslog(LOG_INFO, "= Creating restful_thread");
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
    message_queue_destroy(restful->dispatch_queue);
  }
  return 0;
}

int restful_comm_thread_start(classifyapp_struct *classifyapp_data, restful_comm_struct **restful, void **dispatch_queue, int *server_port)
{

  /* Create thread to listen to incoming HTTP requests to start/stop classifys or get their status */
  *dispatch_queue = (void*)message_queue_create();	
  *restful = restful_comm_create(classifyapp_data, server_port);
  restful_comm_start(*restful, *dispatch_queue);

  return 0;
}


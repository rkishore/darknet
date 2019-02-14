/**************************************************************
 * common.h - common header file for CLASSIFYAPP
 *          
 * Copyright (C) 2018 igolgi Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: Kishore Ramachandran (kishore.ramachandran@zipreel.com)
 *          
 **************************************************************/

#pragma once

#include <curl/curl.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <semaphore.h>

#include "threadqueue.h"

#define WAV_HEADER_SIZE 96
#define XFER_ABORT_TIME 10L // seconds
#define XFER_ABORT_SPEED_THRESHOLD 512L // bytes/sec or 512 Bytes/s (input stream is typically 4 KB/s or 8 KB/s)
#define MAX_PACKETS 10 // to support processing ~10X slower than incoming stream rate/bandwidth
#define MAX_PACKET_SIZE 64*1024 
#define BASIC_PROC_BUFFER 30.1 // how many seconds of data we should process at a time
#define MAX_WAV_PACKET_SIZE 1024*1024 // For 256Kbps compressed input stream rate/bandwidth, 30s of data =~ 1MB of data

#define NUM_PROC_THREADS 2 // the audio_decoder and the speech_processing thread
#define NANOSEC 1000000000
#define MAX_EVENT_BUFFERS      1
#define MAX_EVENT_SIZE         128
#define COMM_TCP_PORT          7777
#define COMM_TCP_HOST          "localhost"
#define POLLING_INTERVAL       2 // Time in second between checking of child processes
#define REPLY_TIMEOUT          5 // Timeout for replies from transcribe process to daemon process
// Timeout for replies from daemon process to the cmdline process
// This should always be greater than REPLY_TIMEOUT
#define DAEMON_REPLY_TIMEOUT   (REPLY_TIMEOUT + 2)
#define MAX_AUDIO_DECODE_READ_FRAME_RETRIES 3
#define AUDIO_DECODE_READ_FRAME_BACKOFF_INIT_US 50000
#define MAX_FULL_PROCESS_RETRIES 3
#define FULL_PROCESS_BACKOFF_INIT_US 500000
#define BACKOFF_1SECOND 1000000
#define SMALL_FIXED_STRING_SIZE 8
#define MEDIUM_FIXED_STRING_SIZE 40
#define LARGE_FIXED_STRING_SIZE 255
#define XLARGE_FIXED_STRING_SIZE 1024
#define FILE_READ_BUFSIZE 4096
#define EXPECTED_ARGS_AFTER_OPTIONS 2
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_MINUTE 60
#define WAVHDR_DEBUG 1
#define MAX_SIMULTANEOUS_JOBS 5

typedef uint64_t u64;
typedef int64_t i64;
typedef unsigned int u32;

typedef struct _classifyapp_config_struct_ {

  char input_url[LARGE_FIXED_STRING_SIZE];
  char input_type[SMALL_FIXED_STRING_SIZE];
  char input_mode[SMALL_FIXED_STRING_SIZE];
  char output_filepath[LARGE_FIXED_STRING_SIZE];
  char output_json_filepath[LARGE_FIXED_STRING_SIZE];
  float detection_threshold;
  //bool crash_recovery_flag;
  char input_filename[LARGE_FIXED_STRING_SIZE];
  
} classifyapp_config_struct;

typedef struct _classifyapp_struct_ {

  classifyapp_config_struct   appconfig;

  volatile int             input_thread_is_active;
  volatile int             proc_thread_is_active;

  void                     *input_packet_queue;
  sem_t                    *input_queue_sem;
    
  void                     *input_packet_buffer_pool;
  sem_t                    *input_packet_sem;
  
  void                     *system_message_pool;

  pthread_t                input_thread_id;
  pthread_t                proc_thread_id;

  CURL                     *curl_data;
  double                   bytes_pulled, bytes_pushed, bytes_pulled_report;
  double                   bytes_processed;
  double                   input_mux_rate;

  u64                      cur_input_packet_fill;
  uint8_t                  *cur_input_packet;
  
  pthread_mutex_t          http_input_thread_complete_mutex;
  bool                     http_input_thread_complete;

  uint64_t                 decoded_frame_count;
  uint64_t                 decoded_total_bytes;

  int64_t                  output_bit_rate;

  struct timespec          pull_start_timestamp;
  int64_t                  pull_start_ts_high, pull_start_ts_low, pull_end_ts_high, pull_end_ts_low;

  void                     *event_message_queue;
  void                     *event_message_pool;
  
  igolgi_message_struct    *cur_message_wrapper;

  char                     *restful_err_str;

  uint64_t                 decoded_audio_pkts;
  uint16_t                 consecutive_audio_decode_failures; // on URL retry, ffmpeg sometimes goes into infinite loop where packets get pulled but not decoded

  FILE                     *samplefptr;

} classifyapp_struct;

#define __PRINTF(fmt, args...) { fprintf(stdout, fmt , ## args); fflush(stdout); }
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)

bool starts_with(const char *pre, const char *str);
bool ends_with(const char *suf, const char *str);
//#define CLASSIFYAPP_DEBUG

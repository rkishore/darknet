/**************************************************************
 * common.h - common header file for CLASSIFYAPP
 *          
 * Copyright (C) 2015 Zipreel Inc.
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
//#define MAX_WAV_PACKET_SIZE (256000/8) * BASIC_PROC_BUFFER // For 256Kbps compressed input stream rate/bandwidth, 30s of data = 256000/8 * 30
#define MAX_WAV_PACKET_SIZE 1024*1024 // For 256Kbps compressed input stream rate/bandwidth, 30s of data =~ 1MB of data

// #define MAX_PROC_BUFSIZE 1024*1024 // For 256Kbps input stream rate/bandwidth and 30s of data
// #define MAX_PROC_BUFS 10 

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

typedef uint64_t u64;
typedef int64_t i64;
typedef unsigned int u32;

typedef struct _classifyapp_config_struct_ {

  //int  channel;
  char input_url[LARGE_FIXED_STRING_SIZE];
  char input_type[SMALL_FIXED_STRING_SIZE];
  char output_directory[LARGE_FIXED_STRING_SIZE];
  char output_fileprefix[LARGE_FIXED_STRING_SIZE];
  //char acoustic_model[LARGE_FIXED_STRING_SIZE];
  //char dictionary[LARGE_FIXED_STRING_SIZE];
  //char language_model[LARGE_FIXED_STRING_SIZE];
  //float writeout_duration;
  //bool crash_recovery_flag;

} classifyapp_config_struct;

/* 

typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;

*/


typedef struct _classifyapp_struct_ {

  classifyapp_config_struct   appconfig;

  volatile int             input_thread_is_active;
  volatile int             proc_thread_is_active;

  void                     *input_packet_queue;
  sem_t                    *input_queue_sem;
    
  void                     *input_packet_buffer_pool;
  sem_t                    *input_packet_sem;

  /* 
  void                     *speech_proc_packet_queue;
  sem_t                    *speech_proc_queue_sem;

  void                     *speech_proc_packet_buffer_pool;
  sem_t                    *speech_proc_packet_sem;
  */
  
  void                     *system_message_pool;

  // void                     *audio_decoder_buffer_pool;

  pthread_t                input_thread_id;

  // pthread_t                audio_decoder_thread_id;
  pthread_t                proc_thread_id;

  CURL                     *curl_data;
  double                   bytes_pulled, bytes_pushed, bytes_pulled_report;
  double                   bytes_processed;
  double                   input_mux_rate;

  u64                      cur_input_packet_fill;
  uint8_t                  *cur_input_packet;

  /*
  u64                      cur_speech_proc_packet_fill;
  uint8_t                  *cur_speech_proc_packet;
  FILE                     *cur_speech_proc_packet_fd;
  */
  
  pthread_mutex_t          http_input_thread_complete_mutex;
  bool                     http_input_thread_complete;

  /* 
  pthread_mutex_t          audio_decoder_thread_complete_mutex;
  bool                     audio_decoder_thread_complete;

  pthread_mutex_t          speech_proc_thread_complete_mutex;
  bool                     speech_proc_thread_complete;

  pthread_mutex_t          input_codec_mutex;
  pthread_mutex_t          input_fmt_ctx_mutex;
  */
  
  uint64_t                 decoded_frame_count;
  uint64_t                 decoded_total_bytes;

  int64_t                  output_bit_rate;

  struct timespec          pull_start_timestamp;
  int64_t                  pull_start_ts_high, pull_start_ts_low, pull_end_ts_high, pull_end_ts_low;

  /* 
  struct timespec          audio_decoder_start_timestamp;
  int64_t                  audio_decode_start_ts_high, audio_decode_start_ts_low, audio_decode_end_ts_high, audio_decode_end_ts_low;
  double                   audio_decode_buf_fill_sofar, num_audio_decode_bufs_sofar, expected_total_audio_decode_buf_fill_sofar;
  double                   ewma_audio_decode_buf_fill;

  pthread_mutex_t          audio_decode_buf_mutex;
  */
  
  void                     *event_message_queue;
  void                     *event_message_pool;

  /* 
  int                      audio_decode_read_frame_retries;
  int                      audio_decode_read_frame_backoff;  
  int                      full_process_retries;
  int                      full_process_retry_backoff;
  */
  
  igolgi_message_struct    *cur_message_wrapper;

  char                     *restful_err_str;

  uint64_t                 decoded_audio_pkts;
  uint16_t                 consecutive_audio_decode_failures; // on URL retry, ffmpeg sometimes goes into infinite loop where packets get pulled but not decoded

  FILE                     *samplefptr;

} classifyapp_struct;

/* 
typedef struct _speech_proc_hyp_struct_ {

  classifyapp_struct          *classifyapp_inst;

  ps_decoder_t             *ps;
  cmd_ln_t                 *sphinx_config_obj;

  char                     waveheader[WAV_HEADER_SIZE];

  uint8_t                  *raw_audio_buf;
  int                      raw_audio_bufsize, this_wav_buf_writesize, next_wav_buf_writesize;
  int                      raw_audio_buf_num, num_blocks_to_skip_get_hyp;
  pthread_mutex_t          speech_proc_condwait_mutex;
  pthread_cond_t           speech_proc_cond_var;
  uint8_t                  speech_proc_cond_boolean_predicate;
  double                   last_out_nspeech, out_nspeech, out_ncpu, out_nwall; // cumulative speech stats reported by pocketsphinx
  double                   utt_nspeech, utt_ncpu, utt_nwall; // speech stats for a particular utterance reported by pocketsphinx
  struct timespec          start_timestamp, ps_decoder_init_timestamp;
  double                   num_bufs_dropped;
  uint8_t                  last_buf_dropped, header_written, footer_written;
  FILE                     *output_xml_fptr, *output_wav_fptr;

#ifdef WAVHDR_DEBUG
  FILE                     *output_wavhdr_fptr;
  bool                     invalid_wav_hdr;
#endif

  struct timespec          output_file_opentime;
  double                   first_pull_start_timestamp_high, first_pull_start_timestamp_low;
  int64_t                  pull_start_timestamp_high, pull_start_timestamp_low, pull_end_timestamp_high, pull_end_timestamp_low;
  //int64_t                  first_decode_start_timestamp_high, first_decode_start_timestamp_low;
  int64_t                  audio_decode_start_timestamp_high, audio_decode_start_timestamp_low, audio_decode_end_timestamp_high, audio_decode_end_timestamp_low;
  char                     time_string[MEDIUM_FIXED_STRING_SIZE];

  double                   first_speech_decode_start_time, total_speech_proc_time, num_speech_blocks_proc;

  char                     input_container[SMALL_FIXED_STRING_SIZE], input_codec[SMALL_FIXED_STRING_SIZE];

  int                      output_file_idx, cur_output_packet_size;
  
  uint64_t                 interval_cnt;
  int64_t                  output_tvsec;

  struct tm                outputfile_ptm;
  double                   cur_block_start_timestamp, exp_block_ts_after_proc;
  bool                     speech_proc_cond_vars_init, retry_not_handled;
  int                      retry_num;

  const char               *hyp;
  const char               *uttid;
  FILE                     *rawfd;
  struct timespec          cur_buf_start_timestamp, prev_timestamp, cur_timestamp;  

} speech_proc_hyp_struct;
*/

#define __PRINTF(fmt, args...) { fprintf(stdout, fmt , ## args); fflush(stdout); }
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
//#define CLASSIFYAPP_DEBUG

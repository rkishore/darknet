/*******************************************************************
 * threadqueue.h - thread safe message queue for AUDIOAPP
 *          
 * Copyright (C) 2015 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 *          
 *******************************************************************/

#pragma once

#include <stdint.h>

typedef struct _igolgi_message_struct {
  void       *buffer;
  int        buffer_size;
  int        buffer_flags;
  int64_t    pull_end_timestamp_low;
  int64_t    pull_end_timestamp_high;
  int64_t    pull_start_timestamp_low;
  int64_t    pull_start_timestamp_high;
  int64_t    audio_decode_end_timestamp_low;
  int64_t    audio_decode_end_timestamp_high;
  int64_t    audio_decode_start_timestamp_low;
  int64_t    audio_decode_start_timestamp_high;
  int64_t    dts;
  int        intra_flag;
  int64_t    pts_diff64;
  int        opi;
  int        idx;
} igolgi_message_struct;

int message_queue_count(void *queue);
void *message_queue_create(void);
int message_queue_destroy(void *queue);
int message_queue_push_back(void *queue, igolgi_message_struct *message);
int message_queue_push_front(void *queue, igolgi_message_struct *message);
igolgi_message_struct *message_queue_pop_back(void *queue);
igolgi_message_struct *message_queue_pop_front(void *queue);
int message_queue_resync(void *queue);

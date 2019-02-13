/**************************************************************                                                                                                         
 * threadqueue.c - thread safe message queue                                                                                                                            
 *                                                                                                                                                                      
 * Copyright (C) 2009, 2010, 2011 Igolgi Inc.                                                                                                                           
 *                                                                                                                                                                      
 * Confidential and Proprietary Source Code                                                                                                                             
 *                                                                                                                                                                      
 * Authors: John Richardson (john.richardson@igolgi.com)                                                                                                               
 *          Jeff Cooper (jeff.cooper@igolgi.com)                                                                                                                        
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <semaphore.h>
#include "threadqueue.h"
#include "fastpool.h"

typedef struct _igolgi_node_struct
{   
  struct _igolgi_node_struct *prev;
  struct _igolgi_node_struct *next;
  igolgi_message_struct      *message;
} igolgi_node_struct;

typedef struct _igolgi_message_queue_struct
{
  igolgi_node_struct         *head;
  igolgi_node_struct         *tail;
  int                        count;
  pthread_mutex_t            *mutex;
  void                       *bufpool;
} igolgi_message_queue_struct;

int message_queue_count(void *queue)
{   
  int count;
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  if (!message_queue)
    return -1;

  pthread_mutex_lock(message_queue->mutex);
  count = message_queue->count;
  pthread_mutex_unlock(message_queue->mutex);

  return count;
}

int message_queue_resync(void *queue)
{
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  if (!message_queue)
    return -1;

  pthread_mutex_lock(message_queue->mutex);
  message_queue->count = 0;
  message_queue->head = NULL;
  message_queue->tail = NULL;
  fast_buffer_pool_resync(message_queue->bufpool);
  pthread_mutex_unlock(message_queue->mutex);

  return 0;
}

void *message_queue_create(void)
{
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)malloc(sizeof(igolgi_message_queue_struct));
  if (!message_queue)
    return NULL;

  memset(message_queue, 0, sizeof(igolgi_message_queue_struct));

  message_queue->bufpool = fast_buffer_pool_create(65535, sizeof(igolgi_node_struct));
  message_queue->count = 0;
  message_queue->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(message_queue->mutex, NULL);

  return message_queue;
}

int message_queue_destroy(void *queue)
{
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  if (!message_queue)
    return -1;

  //  igolgi_message_struct *current_message;                                                                                                                          
  //  while ((current_message = message_queue_pop_back(message_queue)))                                                                                                
  //    free(current_message);                                                                                                                                         

  pthread_mutex_destroy(message_queue->mutex);
  free(message_queue->mutex);
  message_queue->mutex = NULL;
  fast_buffer_pool_destroy(message_queue->bufpool);
  free(message_queue);

  return 0;
}

int message_queue_push_back(void *queue, igolgi_message_struct *message)
{
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  igolgi_node_struct *new_node = NULL;
  
  if (fast_buffer_pool_take(message_queue->bufpool, (uint8_t **)&new_node) < 0)
    return -1;

  if (!message_queue)
    return -1;

  if (!new_node)
    return -1;

  pthread_mutex_lock(message_queue->mutex);
  if (message_queue->tail)
    {
      message_queue->tail->next = new_node;
      new_node->prev = message_queue->tail;
      new_node->next = NULL;
      new_node->message = message;
      message_queue->tail = new_node;
    }
  else
    {
      new_node->next = NULL;
      new_node->prev = NULL;
      new_node->message = message;
      message_queue->head = message_queue->tail = new_node;
    }
  message_queue->count++;
  pthread_mutex_unlock(message_queue->mutex);

  return 0;
}

int message_queue_push_front(void *queue, igolgi_message_struct *message)
{
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  igolgi_node_struct *new_node = NULL;

  if (fast_buffer_pool_take(message_queue->bufpool, (uint8_t **)&new_node) < 0)
    return -1;

  if (!message_queue)
    return -1;

  if (!new_node)
    return -1;

  pthread_mutex_lock(message_queue->mutex);
  if (message_queue->head)
    {
      message_queue->head->prev = new_node;
      new_node->next = message_queue->head;
      new_node->prev = NULL;
      new_node->message = message;
      message_queue->head = new_node;
    }
  else
    {
      new_node->prev = NULL;
      new_node->next = NULL;
      new_node->message = message;
      message_queue->head = message_queue->tail = new_node;
    }
  message_queue->count++;
  pthread_mutex_unlock(message_queue->mutex);

  return 0;
}

igolgi_message_struct *message_queue_pop_back(void *queue)
{   
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  igolgi_node_struct *current_node = NULL;
  igolgi_message_struct *return_message;

  if (!message_queue)
    return NULL;

  pthread_mutex_lock(message_queue->mutex);
  if (message_queue->tail)
    {
      current_node = message_queue->tail;
      message_queue->tail = current_node->prev;
      if (message_queue->tail)
        {   
	  message_queue->tail->next = NULL;
        }
      else
        {
	  message_queue->head = NULL;
        }
      current_node->prev = NULL;
      message_queue->count--;
      return_message = current_node->message;
      fast_buffer_pool_return(message_queue->bufpool, current_node);
      pthread_mutex_unlock(message_queue->mutex);
      return return_message;
    }
  pthread_mutex_unlock(message_queue->mutex);
  return NULL;
}

igolgi_message_struct *message_queue_pop_front(void *queue)
{   
  igolgi_message_queue_struct *message_queue = (igolgi_message_queue_struct *)queue;
  igolgi_node_struct *current_node = NULL;
  igolgi_message_struct *return_message;

  if (!message_queue)
    return NULL;

  pthread_mutex_lock(message_queue->mutex);
  if (message_queue->head)
    {
      current_node = message_queue->head;
      message_queue->head = current_node->next;
      if (message_queue->head)
        {
	  message_queue->head->prev = NULL;
        }
      else
        {
	  message_queue->tail = NULL;
        }
      current_node->next = NULL;
      message_queue->count--;
      return_message = current_node->message;
      fast_buffer_pool_return(message_queue->bufpool, current_node);
      pthread_mutex_unlock(message_queue->mutex);
      return return_message;
    }
  pthread_mutex_unlock(message_queue->mutex);
  return NULL;
}

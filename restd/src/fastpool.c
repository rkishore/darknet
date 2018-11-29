/**************************************************************                                                                                                         
 * pool.c - fixed size memory pools                                                                                                                                     
 *                                                                                                                                                                     
 * Copyright (C) 2009 Igolgi Inc.                                                                                                                                       
 *                                                                                                                                                                      
 * Confidential and Proprietary Source Code                                                                                                                             
 *                                                                                                                                                                      
 * Authors: John Richardson (john.richardson@igolgi.com)                                                                                                                
 *          Jeff Cooper (jeff.cooper@igolgi.com)                                                                                                                        
 **************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include <semaphore.h>
#include <syslog.h>
#include <stdint.h>
#include <string.h>

#define MAX_POOL_SIZE    131070
#define BUFFER_HEAD      4

typedef struct _igolgi_buffer_struct
{
  int                        available;
  int                        sanity_check;
  uint8_t                    *buffer;
} igolgi_buffer_struct;

typedef struct _igolgi_buffer_pool_struct
{   
  igolgi_buffer_struct       buffers[MAX_POOL_SIZE];
  int                        buffer_count;
  int                        size;
  int                        start_loc;
  int                        end_loc;
  pthread_mutex_t            mutex;
  uint8_t                    *fast_buffer;
} igolgi_buffer_pool_struct;

int fast_buffer_pool_count(void *pool)
{
  int i;   
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  if (buffer_pool) {
    int buffer_count = buffer_pool->buffer_count;
    //int buffer_size = buffer_pool->size;
    int available = 0;

    pthread_mutex_lock(&buffer_pool->mutex);
    for (i = 0; i < buffer_count; i++) {
      available += buffer_pool->buffers[i].available;
    }
    pthread_mutex_unlock(&buffer_pool->mutex);
        
    return available;
  } 
  return 0;
}

int fast_buffer_pool_resync(void *pool)
{   
  int i;
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  int buffer_count = buffer_pool->buffer_count;
  int buffer_size = buffer_pool->size;
  uint8_t *buffer_ptr;

  buffer_pool->start_loc = 0;
  buffer_pool->end_loc = 0;
  buffer_ptr = buffer_pool->fast_buffer;

  memset(buffer_ptr, 0, buffer_count * (buffer_size+BUFFER_HEAD));

  for (i = 0; i < buffer_count; i++) {
    uint32_t *tempbuff = (uint32_t*)buffer_ptr;
    *tempbuff = i;
    buffer_pool->buffers[i].buffer = (uint8_t*)buffer_ptr;
    if (i != (buffer_count-1)) {
      buffer_ptr += (buffer_size+BUFFER_HEAD);
    }
    buffer_pool->buffers[i].available = 1;
    buffer_pool->buffers[i].sanity_check = i;
  }
  return 0;
}

void *fast_buffer_pool_create(int buffer_count, int buffer_size)
{
  int i;
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)malloc(sizeof(igolgi_buffer_pool_struct));
  if (!buffer_pool)
    return NULL;

  uint32_t allocated_memory = buffer_count * (buffer_size+BUFFER_HEAD);
  uint8_t *buffer_ptr;

  buffer_pool->buffer_count = buffer_count;
  buffer_pool->start_loc = 0;
  buffer_pool->end_loc = 0;
  buffer_pool->size = buffer_size;

  buffer_pool->fast_buffer = (uint8_t*)malloc(allocated_memory);
  buffer_ptr = buffer_pool->fast_buffer;

  memset(buffer_pool->fast_buffer, 0, allocated_memory);

  pthread_mutex_init(&buffer_pool->mutex, NULL);

  for (i = 0; i < buffer_count; i++) {
    uint32_t *tempbuff = (uint32_t*)buffer_ptr;
    *tempbuff = i;
    buffer_pool->buffers[i].buffer = (uint8_t*)buffer_ptr;
    if (i != (buffer_count-1)) {
      buffer_ptr += (buffer_size+BUFFER_HEAD);
    }
    buffer_pool->buffers[i].available = 1;
    buffer_pool->buffers[i].sanity_check = i;
  }
  return buffer_pool;
}

int fast_buffer_pool_destroy(void *pool)
{
  int i;
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  if (!buffer_pool)
    return -1;

  free(buffer_pool->fast_buffer);
  pthread_mutex_destroy(&buffer_pool->mutex);

  for (i = 0; i < buffer_pool->buffer_count; i++)
    {
      buffer_pool->buffers[i].buffer = NULL;
    }

  free(buffer_pool);
  buffer_pool = NULL;

  return 0;
}

int print_fast_buffer_pool_info(void *pool)
{
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  if (!buffer_pool)
    return -1;
  int next_buffer_pos = buffer_pool->start_loc;
  uint8_t *my_buffer = buffer_pool->buffers[next_buffer_pos].buffer;

  fprintf(stderr, "start: %d | count: %d | available? %d | addr: %p %p\n",
	  buffer_pool->start_loc, buffer_pool->buffer_count,
	  buffer_pool->buffers[next_buffer_pos].available,
	  my_buffer,
	  my_buffer + BUFFER_HEAD);

  return 0;
}

int fast_buffer_pool_take(void *pool, uint8_t **my_buffer)
{
  int next_buffer_pos;
  int tries = 0;

  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  if (!buffer_pool)
    return -1;

#if 0
  fast_buffer_pool_sanity_check(pool);
#endif

  *my_buffer = NULL;
  pthread_mutex_lock(&buffer_pool->mutex);
  //while ((!my_buffer) && (tries < MAX_POOL_SIZE)) {                                                                                                                  
  while ((*my_buffer == NULL) && (tries < buffer_pool->buffer_count)) {
    next_buffer_pos = buffer_pool->start_loc;
    if (buffer_pool->buffers[next_buffer_pos].available) {
      *my_buffer = buffer_pool->buffers[next_buffer_pos].buffer;
      buffer_pool->buffers[next_buffer_pos].available = 0;
      break;
    } else {
      tries++;
    }
    buffer_pool->start_loc = (buffer_pool->start_loc + 1) % buffer_pool->buffer_count;
  }
  pthread_mutex_unlock(&buffer_pool->mutex);
  //if (tries > 30000) {                                                                                                                                               
  //  fprintf(stderr,"TRIES IS BIG!  TRIES=%d\n", tries);                                                                                                              
  //}                                                    
  if (*my_buffer) {
    *my_buffer += BUFFER_HEAD;
    return 0;
  } else {
    return -1;
  }
}

int fast_buffer_pool_return(void *pool, void *buffer)
{
  igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
  if (!buffer_pool)
    return -1;

  uint8_t *internal_buffer = (uint8_t*)buffer;
  internal_buffer -= BUFFER_HEAD;
  uint32_t *tempbuffer = (uint32_t*)internal_buffer;
  if (!tempbuffer) {
    fprintf(stderr,"fast_buffer_pool_return(): buffer=%p\n", tempbuffer);
    return -1;
  }
  uint32_t buffer_index = *tempbuffer;
  pthread_mutex_lock(&buffer_pool->mutex);
  if (buffer_index < 0 || buffer_index > MAX_POOL_SIZE) {
    fprintf(stderr,"PROBLEM RETURNING BUFFER : %d\n", buffer_index);
    syslog(LOG_ERR,"FASTPOOL - PROBLEM RETURNING BUFFER : %d\n", buffer_index);
    tempbuffer = NULL;
    memcpy((void*)tempbuffer, (void*)12, 12);
    // throw a seg fault                                                                                                                                             
  } else {
    if (buffer_pool->buffers[buffer_index].available != 0) {
      fprintf(stderr,"PROBLEM RETURNING BUFFER : %d\n", buffer_index);
      syslog(LOG_ERR,"FASTPOOL - PROBLEM RETURNING BUFFER : %d\n", buffer_index);
      tempbuffer = NULL;
      memcpy((void*)tempbuffer, (void*)12, 12);
      // throw a seg fault                                                                                                                                         
    }
    buffer_pool->buffers[buffer_index].available = 1;
  }
  pthread_mutex_unlock(&buffer_pool->mutex);
  return 0;
}

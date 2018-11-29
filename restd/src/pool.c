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
#include <string.h>
#include "pool.h"
#include "allocator.h"

#if defined(USE_DMALLOC)
#include "dmalloc.h"
#endif

#define MAX_POOL_SIZE    65535

typedef struct _igolgi_buffer_struct
{
    void                       *buffer;
    int                        references;
    int                        available;
} igolgi_buffer_struct;

typedef struct _igolgi_buffer_pool_struct
{
    igolgi_buffer_struct       buffers[MAX_POOL_SIZE];
    int                        buffer_count;
    int                        size;
    int                        loc;
    pthread_mutex_t            mutex;
} igolgi_buffer_pool_struct;

int buffer_pool_resync(void *pool)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;

    if (!buffer_pool)
        return -1;

    buffer_pool->loc = -1;
    for (i = 0; i < buffer_pool->buffer_count; i++)
    {        
        buffer_pool->buffers[i].available = 1;
        buffer_pool->buffers[i].references = 0;
    } 
    return 0;
}

void buffer_pool_sanity_check(void *pool)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;

    if (!buffer_pool)
        return;

    return;
}

void *buffer_pool_create(int buffer_count, int buffer_size, int align128)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)igolgi_malloc(sizeof(igolgi_buffer_pool_struct));
    if (!buffer_pool)
        return NULL;

    buffer_pool->buffer_count = buffer_count;
    buffer_pool->loc = -1;
    buffer_pool->size = buffer_size;
    pthread_mutex_init(&buffer_pool->mutex, NULL);

    for (i = 0; i < buffer_count; i++)
    {
        buffer_pool->buffers[i].buffer = (void *)igolgi_malloc(buffer_size);
        buffer_pool->buffers[i].references = 0;
        buffer_pool->buffers[i].available = 1;
    }
    for (i = buffer_count; i < MAX_POOL_SIZE; i++)
    {
        buffer_pool->buffers[i].buffer = NULL;
        buffer_pool->buffers[i].references = 0;
        buffer_pool->buffers[i].available = 0;
    }
    return buffer_pool;
}

int buffer_pool_destroy(void *pool)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return -1;

    for (i = 0; i < buffer_pool->buffer_count; i++)
    {
        igolgi_free(buffer_pool->buffers[i].buffer);
        buffer_pool->buffers[i].buffer = NULL;
    }

    pthread_mutex_destroy(&buffer_pool->mutex);

    igolgi_free(buffer_pool);
    return 0;
}

void *buffer_pool_take(void *pool)
{
    int i, index;
    void *my_buffer;

    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return NULL;

    pthread_mutex_lock(&buffer_pool->mutex);
    buffer_pool->loc = (buffer_pool->loc + 1) % buffer_pool->buffer_count;
    for (i = 0; i < buffer_pool->buffer_count; i++)
    {
        index = (buffer_pool->loc + i) % buffer_pool->buffer_count;
        if (buffer_pool->buffers[index].available)
        {
            my_buffer = buffer_pool->buffers[index].buffer;
            buffer_pool->buffers[index].references = 1;
            buffer_pool->buffers[index].available = 0;
            pthread_mutex_unlock(&buffer_pool->mutex);
            return my_buffer;
        }
    }
    pthread_mutex_unlock(&buffer_pool->mutex);
    return NULL;
}

int buffer_pool_return_nodie(void *pool, void *buffer)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return -1;

    pthread_mutex_lock(&buffer_pool->mutex);
    for (i = 0; i < buffer_pool->buffer_count; i++)
    {
        if (buffer_pool->buffers[i].buffer == buffer)
        {
            if (buffer_pool->buffers[i].available != 0) {
                fprintf(stderr,"BUFFER_POOL_RETURN: buffer invalid - already returned!\n");
                // syslog(LOG_ERR,"BUFFER_POOL_RETURN: buffer invalid - possible corruption - resync\n");
                //make it crash to find the line we need in the backtrace
                pthread_mutex_unlock(&buffer_pool->mutex);
                return -1;
            }
            buffer_pool->buffers[i].references = 0;
            buffer_pool->buffers[i].available = 1;
            pthread_mutex_unlock(&buffer_pool->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&buffer_pool->mutex);
    fprintf(stderr,"BUFFER_POOL_RETURN: unable to return %p to buffer pool %p\n", buffer, pool);
    //syslog(LOG_ERR,"BUFFER_POOL_RETURN: buffer invalud - possible corruption - resync\n");

    return -1;
}

int buffer_pool_return(void *pool, void *buffer)
{
    int i;
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return -1;

    pthread_mutex_lock(&buffer_pool->mutex);
    for (i = 0; i < buffer_pool->buffer_count; i++)
    {
        if (buffer_pool->buffers[i].buffer == buffer)
        {
            if (buffer_pool->buffers[i].available != 0) {
                fprintf(stderr,"BUFFER_POOL_RETURN: buffer invalid - already returned!\n");
                syslog(LOG_ERR,"BUFFER_POOL_RETURN: buffer invalid - resync\n");               
#if 1
                //make it crash to find the line we need in the backtrace
                if (1) {
                    void *y = NULL;
                    void *w = NULL;          
                    memcpy(y,w,1000);
                }
#endif
            }
            buffer_pool->buffers[i].references = 0;
            buffer_pool->buffers[i].available = 1;
            pthread_mutex_unlock(&buffer_pool->mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&buffer_pool->mutex);
    fprintf(stderr,"BUFFER_POOL_RETURN: unable to return %p to buffer pool %p\n", buffer, pool);
    syslog(LOG_ERR,"BUFFER_POOL_RETURN: buffer invalid - possible corruption - resync\n");
#if 1
    if (1) {
        void *y = NULL;
        void *w = NULL;          
        memcpy(y,w,1000);
    }  
#endif
    return -1;
}

int buffer_pool_count(void *pool)
{
    int i;
    int count = 0;

    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return -1;

    pthread_mutex_lock(&buffer_pool->mutex);
    for (i = 0; i < buffer_pool->buffer_count; i++)
    {
        if (buffer_pool->buffers[i].available)
        {
            count++;
        }
    }
    pthread_mutex_unlock(&buffer_pool->mutex);
    return count; 
}

int buffer_pool_total_count(void *pool)
{
    igolgi_buffer_pool_struct *buffer_pool = (igolgi_buffer_pool_struct *)pool;
    if (!buffer_pool)
        return -1;

    return buffer_pool->buffer_count;
}


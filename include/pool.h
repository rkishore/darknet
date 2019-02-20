/**************************************************************
 * pool.h - fixed size memory pools
 *
 * Copyright (C) 2009 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: John Richardson (john.richardson@igolgi.com)
 *          Jeff Cooper (jeff.cooper@igolgi.com) 
 **************************************************************/

#if !defined(IGOLGI_BUFFER_POOL_H)
#define IGOLGI_BUFFER_POOL_H

#if defined(__cplusplus)
extern "C" {
#endif

        void *buffer_pool_create(int buffer_count, int buffer_size, int align128);
        int buffer_pool_destroy(void *pool);
        void *buffer_pool_take(void *pool);
        int buffer_pool_return(void *pool, void *buffer);
        int buffer_pool_return_nodie(void *pool, void *buffer);
        int buffer_pool_count(void *pool);
        int buffer_pool_resync(void *pool);
        int buffer_pool_total_count(void *pool);

#if defined(__cplusplus)
}
#endif

#endif  // IGOLGI_BUFFER_POOL_H

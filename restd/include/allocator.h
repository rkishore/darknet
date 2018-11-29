/**************************************************************
 * allocator.h - memory allocator for igolgi software
 *
 * Copyright (C) 2009 Igolgi Inc.
 * 
 * Confidential and Proprietary Source Code
 *
 * Authors: John Richardson (john.richardson@igolgi.com)
 *          Jeff Cooper (jeff.cooper@igolgi.com) 
 **************************************************************/

#if !defined(IGOLGI_ALLOCATOR_H)
#define IGOLGI_ALLOCATOR_H

#if defined(__cplusplus)
extern "C" {
#endif

        void *igolgi_malloc(unsigned int size_in_bytes);
        void igolgi_free(void *free_ptr);

#if defined(__cplusplus)
}
#endif

#endif // IGOLGI_ALLOCATOR_H

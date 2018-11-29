/**************************************************************
 * allocator.c - memory allocator for igolgi software
 *
 * Copyright (C) 2009 Igolgi Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: John Richardson (john.richardson@igolgi.com)
 *          Jeff Cooper (jeff.cooper@igolgi.com)
 **************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "allocator.h"

#if defined(USE_DMALLOC)
#include "dmalloc.h"
#endif

#if defined(DEBUG)
static unsigned int total_memory_allocated = 0;
#endif

void *igolgi_malloc(unsigned int size_in_bytes)
{
    void *my_memory = NULL;
    int retval = 0;
    char errbuf[128];

#if defined(USE_DMALLOC)
    my_memory = (void*)malloc(size_in_bytes);
#else
#if 1
    if (size_in_bytes <= 256) {
        my_memory = (void*)malloc(size_in_bytes);
    } else {
      if ((retval = posix_memalign((void **)&my_memory, 16, size_in_bytes)) != 0) {
	fprintf(stderr, "== POSIX_MEMALIGN FAILED: %s\n", strerror_r(retval, errbuf, 128));
	return NULL;
      }
    }
#else
    my_memory = (void*)malloc(size_in_bytes);
#endif
#if 1
    if (my_memory) {
        // TOUCH MEMORY TO PREVENT LAZY ALLOCATION
        memset(my_memory, 0, size_in_bytes);
    }
#endif
#endif  // USE_DMALLOC

#if defined(DEBUG)
    total_memory_allocated += size_in_bytes;
    // add in empty bytes in front and behind to check for out
    // of bound conditions
#endif

    return my_memory;
}

void igolgi_free(void *free_ptr)
{
    free(free_ptr);

#if defined(DEBUG)
    // keep track of memory allocations in a list - good for
    // tracking memory leaks and corruption, etc.
#endif
}

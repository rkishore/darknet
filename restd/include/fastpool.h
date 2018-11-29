/*******************************************************************
 * fastpool.h - fixed size memory pools for AUDIOAPP
 *          
 * Copyright (C) 2015 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 *          
 *******************************************************************/

#pragma once

void *fast_buffer_pool_create(int buffer_count, int buffer_size);
int fast_buffer_pool_destroy(void *pool);
int fast_buffer_pool_take(void *pool, uint8_t **my_buffer);
int fast_buffer_pool_return(void *pool, void *buffer);
int fast_buffer_pool_resync(void *pool);
int print_fast_buffer_pool_info(void *pool);
int fast_buffer_pool_count(void *pool);

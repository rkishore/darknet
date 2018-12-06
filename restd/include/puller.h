/**************************************************************
 * puller.h - pull from URL header file for CLASSIFYAPP
 *          
 * Copyright (C) 2015 Zipreel Inc.
 *
 * Confidential and Proprietary Source Code
 *
 * Authors: Kishore Ramachandran (kishore.ramachandran@zipreel.com)
 *          
 **************************************************************/

#pragma once

void *http_input_thread_func(void *arg);
int config_curl_and_pull_file_sample(classifyapp_struct *classifyapp_data);
int config_curl_and_pull_file(classifyapp_struct *classifyapp_data);

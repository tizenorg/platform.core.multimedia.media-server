/*
 *  Media Utility
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Yong Yeon Kim <yy9875.kim@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/**
 * This file defines api utilities of contents manager engines.
 *
 * @file		media-util-register.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
 #ifndef _MEDIA_UTIL_REGISTER_H_
#define _MEDIA_UTIL_REGISTER_H_

#include <glib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	MEDIA_DIRECTORY_SCAN = 0,
	MEDIA_FILES_REGISTER,
} media_request_type_e;

typedef struct
{
	int pid;
	int result;
	int request_type;
	char *complete_path; /* if the request type is MEDIA_FILES_REGISTER, this value will be NULL. */
}media_request_result_s;

typedef void (*scan_complete_cb)(media_request_result_s *, void *);
typedef void (*insert_complete_cb)(media_request_result_s *, void *);

int media_directory_scanning_async(const char *directory_path, bool recursive_on, scan_complete_cb user_callback, void *user_data);

int media_files_register(const char *list_path, insert_complete_cb user_callback, void *user_data);

int media_burstshot_register(const char *list_path, insert_complete_cb user_callback, void *user_data);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_REGISTER_H_*/

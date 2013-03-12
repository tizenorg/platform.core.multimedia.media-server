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

/**
 * @fn		int ms_media_file_register(const char *file_full_path);
 * @brief 		This function registers multimedia file to media DB
 *  			When you did some file operations such as Create, Copy, Move, Rename, and Delete in phone or mmc storage, media-server registers the result to database automatically by inotify mechanism.
 *  			However, automatic registration will have a little delay because the method is asynchronous.
 *  			If you want to register some files to database immediately, you should use this API.
 *
 * @param 	file_full_path [in]		full path of file for register
 * @return	This function returns zero(MEDIA_INFO_ERROR_NONE) on success, or negative value with error code.
 *			Please refer 'media-info-error.h' to know the exact meaning of the error.
 * @see		None.
 * @pre		None.
 * @post		None.
 * @remark	The database name is "/opt/usr/dbspace/.media.db".
 *                  You have to use this API only for registering multimedia files. If you try to register no multimedia file, this API returns error.
 * @par example
 * @code

#include <media-info.h>

int main()
{
	int result = -1;

	result = ms_media_file_register("/opt/usr/media/test.mp3");
	if( result < 0 )
	{
		printf("FAIL to mediainfo_register_file\n");
		return 0;
	}
	else
	{
		printf("SUCCESS to register file\n");
	}

	return 0;
}

 * 	@endcode
 */
int media_file_register(const char *file_full_path);

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

int media_directory_scanning_async(const char *directory_path, bool recusive_on, scan_complete_cb user_callback, void *user_data);

int media_files_register(const char *list_path, insert_complete_cb user_callback, void *user_data);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_REGISTER_H_*/

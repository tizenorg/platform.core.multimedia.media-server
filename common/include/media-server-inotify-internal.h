/*
 *  Media Server
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
 * @file		media-server-inotify-internal.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief		
 */
#ifndef _MEDIA_SERVER_INOTIFY_INTERNAL_H_
#define _MEDIA_SERVER_INOTIFY_INTERNAL_H_

#include <sys/inotify.h>

#define INOTI_EVENT_SIZE (sizeof(struct inotify_event))
#define INOTI_BUF_LEN (1024*(INOTI_EVENT_SIZE+16))
#define INOTI_FOLDER_COUNT_MAX 1024

typedef struct ms_inoti_dir_data {
	char *name;
	int wd;
	struct ms_inoti_dir_data *next;
} ms_inoti_dir_data;

typedef struct ms_create_file_info {
	char *name;
	int wd;
	struct ms_create_file_info *previous;
	struct ms_create_file_info *next;
} ms_create_file_info;

int _ms_inoti_add_create_file_list(int wd, char *name);

int _ms_inoti_delete_create_file_list(ms_create_file_info *node);

ms_create_file_info *_ms_inoti_find_create_file_list(int wd, char *name);

bool _ms_inoti_full_path(int wd, char *name, char *path, int sizeofpath);

bool _ms_inoti_get_full_path(int wd, char *name, char *path, int sizeofpath);

#endif /*_MEDIA_SERVER_INOTIFY_INTERNAL_H_*/

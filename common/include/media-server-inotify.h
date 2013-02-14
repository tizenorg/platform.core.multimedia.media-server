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
 * @file		media-server-inotify.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_SERVER_INOTI_H_
#define _MEDIA_SERVER_INOTI_H_

#include <glib.h>
#include "media-server-types.h"

#if MS_INOTI_ENABLE

typedef struct ms_ignore_file_info {
	char *path;
	struct ms_ignore_file_info *previous;
	struct ms_ignore_file_info *next;
} ms_ignore_file_info;

int ms_inoti_init(void);

gboolean ms_inoti_thread(gpointer data);

void ms_inoti_add_watch(char *path);

int ms_inoti_add_watch_with_node(ms_dir_scan_info * const current_node, int depth);

void ms_inoti_remove_watch_recursive(char *path);

void ms_inoti_remove_watch(char *path);

void ms_inoti_modify_watch(char *path_from, char *path_to);

int ms_inoti_add_ignore_file(const char *path);

int ms_inoti_delete_ignore_file(ms_ignore_file_info * delete_node);

ms_ignore_file_info *ms_inoti_find_ignore_file(const char *path);

void ms_inoti_delete_mmc_ignore_file(void);

void ms_inoti_add_watch_all_directory(ms_storage_type_t storage_type);

#endif
#endif/* _MEDIA_SERVER_INOTI_H_ */

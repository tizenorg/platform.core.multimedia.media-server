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
 * @file		media-server-inotify-internal.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief23
 */
#include "media-server-global.h"
#include "media-server-common.h"
#include "media-server-inotify-internal.h"

int inoti_fd;
ms_create_file_info *latest_create_file;
extern ms_dir_data *first_inoti_node;

int _ms_inoti_add_create_file_list(int wd, char *name)
{
	ms_create_file_info *new_node;

	new_node = malloc(sizeof(ms_create_file_info));
	new_node->name = strdup(name);
	new_node->wd = wd;

	/*first created file */
	if (latest_create_file == NULL) {
		latest_create_file = malloc(sizeof(ms_create_file_info));
		new_node->previous = NULL;
	} else {
		latest_create_file->next = new_node;
		new_node->previous = latest_create_file;
	}
	new_node->next = NULL;

	latest_create_file = new_node;

	return MS_ERR_NONE;
}

int _ms_inoti_delete_create_file_list(ms_create_file_info *node)
{
	if (node->previous != NULL)
		node->previous->next = node->next;
	if (node->next != NULL)
		node->next->previous = node->previous;

	if (node == latest_create_file) {
		latest_create_file = node->previous;
	}

	free(node->name);
	free(node);

	return MS_ERR_NONE;
}

ms_create_file_info *_ms_inoti_find_create_file_list(int wd, char *name)
{
	ms_create_file_info *node = NULL;
	node = latest_create_file;

	while (node != NULL) {
		if ((node->wd == wd) && (strcmp(node->name, name) == 0)) {
			return node;
		}

		node = node->previous;
	}

	return NULL;
}

int _fex_is_default_path(const char *path)
{
	int ret = false;

	if ((strcmp(path, "Images and videos") == 0) ||
	    (strcmp(path, "Sounds and music") == 0) ||
	    (strcmp(path, "Downloads") == 0) ||
	    (strcmp(path, "Camera shots") == 0) ||
	    (strcmp(path, "Wallpapers") == 0) ||
	    (strcmp(path, "Music") == 0) ||
	    (strcmp(path, "Ringtones and Alerts") == 0) ||
	    (strcmp(path, "FM Radio") == 0) ||
	    (strcmp(path, "Voice recorder") == 0)) {
		ret = true;
	}

	return ret;
}

bool _ms_inoti_get_full_path(int wd, char *name, char *path, int sizeofpath)
{
	int err;
	ms_dir_data *node = NULL;

	if (name == NULL || path == NULL)
		return false;

	if (first_inoti_node != NULL) {
		node = first_inoti_node;
		while (node->next != NULL) {
			if (wd == node->wd) {
				MS_DBG("find parent directory: %s", node->name);
				break;
			}
			node = node->next;
		}
	} else {
		MS_DBG("first_node is NULL");
		return false;
	}

	err = ms_strappend(path, sizeofpath, "%s/%s", node->name, name);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_strappend error : %d", err);
		return false;
	}
	MS_DBG("full path : %s", path);
	return true;
}

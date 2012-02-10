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
 * @file		media-server-utils.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_SERVER_UTILS_H__
#define _MEDIA_SERVER_UTILS_H__

#include <sqlite3.h>
#include "media-server-global.h"

int
ms_set_db_status(ms_db_status_type_t status);

int
ms_db_init(bool need_db_create);

bool
ms_is_mmc_inserted(void);

void
ms_usb_vconf_cb(void *data);

int
ms_start(bool need_db_create);

void
ms_end(void);

int
ms_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path);

ms_store_type_t
ms_get_store_type_by_full(const char *path);

int
ms_strappend(char *res, const int size, const char *pattern,
		 const char *str1, const char *str2);

int
ms_strcopy(char *res, const int size, const char *pattern,
	       const char *str1);

bool
ms_config_get_int(const char *key, int *value);

bool
ms_config_set_int(const char *key, int value);

bool
ms_config_get_str(const char *key, char *value);

bool
ms_config_set_str(const char *key, const char *value);

void
ms_check_db_updating(void);

int
ms_get_category_from_mime(const char *path, int *category);

#ifdef FMS_PERF
void
ms_check_start_time(struct timeval *start_time);

void
ms_check_end_time(struct timeval *end_time);

void
ms_check_time_diff(struct timeval *start_time, struct timeval *end_time);
#endif/*FMS_PERF */

#ifdef PROGRESS
struct quickpanel;

void
ms_create_quickpanel(struct quickpanel *ms_quickpanel);

void
ms_update_progress(struct quickpanel *ms_quickpanel, double progress);

void
ms_delete_quickpanel(struct quickpanel *ms_quickpanel);
#endif /*PROGRSS*/
/**
 * @}
 */
#endif/*_MEDIA_SERVER_UTILS_H__*/


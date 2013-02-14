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

#ifndef _MEDIA_SCANNER_UTILS_H__
#define _MEDIA_SCANNER_UTILS_H__

#include "media-server-types.h"

bool
msc_is_mmc_inserted(void);

int
msc_update_mmc_info(void);

void
msc_mmc_vconf_cb(void *data);

int
msc_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path, int depth);

ms_storage_type_t
msc_get_storage_type_by_full(const char *path);

int
msc_strappend(char *res, const int size, const char *pattern,
		 const char *str1, const char *str2);

int
msc_strcopy(char *res, const int size, const char *pattern,
	       const char *str1);

bool
msc_config_get_int(const char *key, int *value);

bool
msc_config_set_int(const char *key, int value);

bool
msc_config_get_str(const char *key, char *value);

bool
msc_config_set_str(const char *key, const char *value);

#ifdef FMS_PERF
void
msc_check_start_time(struct timeval *start_time);

void
msc_check_end_time(struct timeval *end_time);

void
msc_check_time_diff(struct timeval *start_time, struct timeval *end_time);
#endif/*FMS_PERF */

/**
 * @}
 */
#endif/*_MEDIA_SCANNER_UTILS_H__*/


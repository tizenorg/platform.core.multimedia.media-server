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
 * @file		media-common-utils.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_SERVER_UTILS_H__
#define _MEDIA_SERVER_UTILS_H__

#include "media-common-types.h"

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

ms_storage_type_t
ms_get_storage_type_by_full(const char *path, uid_t uid);

int
ms_get_mime(const char *path, char *mimetype);

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

#ifdef FMS_PERF
void
ms_check_start_time(struct timeval *start_time);

void
ms_check_end_time(struct timeval *end_time);

void
ms_check_time_diff(struct timeval *start_time, struct timeval *end_time);
#endif/*FMS_PERF */


/**
 * @}
 */
#endif/*_MEDIA_SERVER_UTILS_H__*/


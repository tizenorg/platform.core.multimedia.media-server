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
 * @file		media-server-utils.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */

#include <pmapi.h>
#include <vconf.h>

#include "media-server-inotify.h"
#include "media-server-utils.h"
#include "media-server-drm.h"
#include "media-server-dbus.h"

#ifdef FMS_PERF
#include <sys/time.h>
#define MILLION 1000000L
struct timeval g_mmc_start_time;
struct timeval g_mmc_end_time;
#endif

#define MS_DRM_CONTENT_TYPE_LENGTH 100

int ums_mode = 0;
extern int mmc_state;

static int
_ms_set_power_mode(ms_db_status_type_t status)
{
	int res = MS_ERR_NONE;
	int err;

	switch (status) {
	case MS_DB_UPDATING:
		err = pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
		if (err != 0)
			res = MS_ERR_UNKNOWN_ERROR;
		break;
	case MS_DB_UPDATED:
		err = pm_unlock_state(LCD_OFF, STAY_CUR_STATE);
		if (err != 0)
			res = MS_ERR_UNKNOWN_ERROR;
		break;
	default:
		MS_DBG_ERR("Unacceptable type : %d", status);
		break;
	}

	return res;
}

int
ms_set_db_status(ms_db_status_type_t status)
{
	int res = MS_ERR_NONE;
	int err = 0;

	if (status == MS_DB_UPDATING) {
		if (ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATING))
			  res = MS_ERR_VCONF_SET_FAIL;
	} else if (status == MS_DB_UPDATED) {
		if(ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED))
			  res = MS_ERR_VCONF_SET_FAIL;
		/*notify to other application about db updated by DBUS*/
		ms_dbus_send_noti(MS_DBUS_DB_UPDATED);
	}

	err = _ms_set_power_mode(status);
	if (err != MS_ERR_NONE) {
		MS_DBG_ERR("_ms_set_power_mode fail");
		res = err;
	}

	return res;
}

#ifdef FMS_PERF
void
ms_check_start_time(struct timeval *start_time)
{
	gettimeofday(start_time, NULL);
}

void
ms_check_end_time(struct timeval *end_time)
{
	gettimeofday(end_time, NULL);
}

void
ms_check_time_diff(struct timeval *start_time, struct timeval *end_time)
{
	struct timeval time;
	long difftime;

	time.tv_sec = end_time->tv_sec - start_time->tv_sec;
	time.tv_usec = end_time->tv_usec - start_time->tv_usec;
	difftime = MILLION * time.tv_sec + time.tv_usec;
	MS_DBG("The function_to_time took %ld microseconds or %f seconds.",
	       difftime, difftime / (double)MILLION);
}
#endif

bool
ms_is_mmc_inserted(void)
{
	int data = -1;
	ms_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &data);
	if (data != VCONFKEY_SYSMAN_MMC_MOUNTED) {
		return false;
	} else {
		return true;
	}
}

/*CAUTION : Before using this function, Have to allocate static memory of ret_path*/
/*And the array length does not over MS_FILE_PATH_LEN_MAX*/
/*for example : char path[MS_FILE_PATH_LEN_MAX] = {0};*/
int
ms_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path, int depth)
{
	int i = 0;
	int path_length = 0;
	int length = 0;
	ms_dir_scan_info *cur_node;
	char **path_array;

	if (depth < 0) {
		MS_DBG_ERR("depth < 0");
		return MS_ERR_INVALID_DIR_PATH;
	}

	path_array = malloc(sizeof(char*) * (depth + 1));

	cur_node = node;

	while (1) {
		path_array[i] = cur_node->name;
		if (cur_node->parent == NULL)
			break;

		cur_node = cur_node->parent;
		i++;
	}

	for(i = depth ; i >= 0 ; i --) {
		length = strlen(path_array[i]);

		if (path_length + length > MS_FILE_PATH_LEN_MAX) {
			MS_DBG_ERR("This is invalid path, %s, %d", node->name, depth);
			return MS_ERR_INVALID_DIR_PATH;
		}

		strncpy(ret_path+path_length, path_array[i], length);
		path_length += length;

		ret_path[path_length] = '/';
		path_length ++;
	}

	ret_path[-- path_length] = '\0';

	free(path_array);

	return MS_ERR_NONE;
}

ms_storage_type_t
ms_get_storage_type_by_full(const char *path)
{
	if (strncmp(path, MS_ROOT_PATH_INTERNAL, strlen(MS_ROOT_PATH_INTERNAL)) == 0) {
		return MS_STORAGE_INTERNAL;
	} else if (strncmp(path, MS_ROOT_PATH_EXTERNAL, strlen(MS_ROOT_PATH_EXTERNAL)) == 0) {
		return MS_STORATE_EXTERNAL;
	} else
		return MS_ERR_INVALID_FILE_PATH;
}

int
ms_strappend(char *res, const int size, const char *pattern,
	     const char *str1, const char *str2)
{
	int len = 0;
	int real_size = size - 1;

	if (!res ||!pattern || !str1 ||!str2 )
		return MS_ERR_ARG_INVALID;

	if (real_size < (strlen(str1) + strlen(str2)))
		return MS_ERR_OUT_OF_RANGE;

	len = snprintf(res, real_size, pattern, str1, str2);
	if (len < 0) {
		return MS_ERR_ARG_INVALID;
	}

	res[len] = '\0';

	return MS_ERR_NONE;
}

int
ms_strcopy(char *res, const int size, const char *pattern, const char *str1)
{
	int len = 0;
	int real_size = size - 1;

	if (!res || !pattern || !str1)
		return MS_ERR_ARG_INVALID;

	if (real_size < strlen(str1))
		return MS_ERR_OUT_OF_RANGE;

	len = snprintf(res, real_size, pattern, str1);
	if (len < 0) {
		return MS_ERR_ARG_INVALID;
	}

	res[len] = '\0';

	return MS_ERR_NONE;
}

bool
ms_config_get_int(const char *key, int *value)
{
	int err;

	if (!key || !value) {
		MS_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	err = vconf_get_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MS_DBG_ERR("Unexpected error code: %d", err);

	return false;
}

bool
ms_config_set_int(const char *key, int value)
{
	int err;

	if (!key) {
		MS_DBG_ERR("Arguments key is NULL");
		return false;
	}

	err = vconf_set_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MS_DBG_ERR("Unexpected error code: %d", err);

	return false;
}

bool
ms_config_get_str(const char *key, char *value)
{
	char *res;
	if (!key || !value) {
		MS_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	res = vconf_get_str(key);
	if (res) {
		strncpy(value, res, strlen(res) + 1);
		return true;
	}

	return false;
}

bool
ms_config_set_str(const char *key, const char *value)
{
	int err;

	if (!key || !value) {
		MS_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	err = vconf_set_str(key, value);
	if (err == 0)
		return true;
	else
		MS_DBG_ERR("fail to vconf_set_str %d", err);

	return false;
}


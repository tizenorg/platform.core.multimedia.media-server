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

#define _GNU_SOURCE
#include <errno.h>
#include <vconf.h>
#include <aul/aul.h>
#include <grp.h>
#include <pwd.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-dbg.h"
#include "media-common-system.h"
#include "media-common-utils.h"

#ifdef FMS_PERF
#include <sys/time.h>
#define MILLION 1000000L
struct timeval g_mmc_start_time;
struct timeval g_mmc_end_time;
#endif

#define MS_DRM_CONTENT_TYPE_LENGTH 100

#ifdef FMS_PERF
void ms_check_start_time(struct timeval *start_time)
{
	gettimeofday(start_time, NULL);
}

void ms_check_end_time(struct timeval *end_time)
{
	gettimeofday(end_time, NULL);
}

void ms_check_time_diff(struct timeval *start_time, struct timeval *end_time)
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

bool ms_is_mmc_inserted(void)
{
	bool ret = FALSE;
	ms_stg_type_e stg_type = MS_STG_TYPE_MMC;
	GArray *dev_list = NULL;

	ret = ms_sys_get_device_list(stg_type, &dev_list);
	if (ret == MS_MEDIA_ERR_NONE) {
		if (dev_list != NULL) {
			MS_DBG_ERR("MMC FOUND[%d]", dev_list->len);
			ms_sys_release_device_list(&dev_list);
			ret = TRUE;
		} else {
			MS_DBG_ERR("MMC NOT FOUND");
		}
	} else {
		MS_DBG_ERR("ms_sys_get_device_list failed");
	}

	return ret;
}

static char* __media_get_path(uid_t uid)
{
	char *result_psswd = NULL;
	struct group *grpinfo = NULL;
	int err = 0;

	if (uid == getuid()) {
		result_psswd = strndup(MEDIA_ROOT_PATH_INTERNAL, strlen(MEDIA_ROOT_PATH_INTERNAL));
		grpinfo = getgrnam("users");
		if (grpinfo == NULL) {
			MS_DBG_ERR("getgrnam(users) returns NULL !");
			MS_SAFE_FREE(result_psswd);
			return NULL;
		}
	} else {
		struct passwd *userinfo = getpwuid(uid);
		if (userinfo == NULL) {
			MS_DBG_ERR("getpwuid(%d) returns NULL !", uid);
			return NULL;
		}
		grpinfo = getgrnam("users");
		if (grpinfo == NULL) {
			MS_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
		// Compare git_t type and not group name
		if (grpinfo->gr_gid != userinfo->pw_gid) {
			MS_DBG_ERR("UID [%d] does not belong to 'users' group!", uid);
			return NULL;
		}
		result_psswd = strndup(userinfo->pw_dir, strlen(userinfo->pw_dir));
	}

	return result_psswd;
}

ms_storage_type_t ms_get_storage_type_by_full(const char *path, uid_t uid)
{
	int length_path;
	char * user_path = NULL;

	if (path == NULL)
		return false;

	user_path = __media_get_path(uid);
	length_path = strlen(user_path);

	if (strncmp(path, user_path, length_path) == 0) {
		return MS_STORAGE_INTERNAL;
	} else if (strncmp(path, MEDIA_ROOT_PATH_SDCARD, strlen(MEDIA_ROOT_PATH_SDCARD)) == 0) {
		return MS_STORAGE_EXTERNAL;
	} else {
		free(user_path);
		return MS_MEDIA_ERR_INVALID_PATH;
    }
}

int ms_strappend(char *res, const int size, const char *pattern,
	     const char *str1, const char *str2)
{
	int len = 0;
	int real_size = size - 1;

	if (!res ||!pattern || !str1 ||!str2 )
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	if (real_size < (int)(strlen(str1) + strlen(str2)))
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	len = snprintf(res, real_size, pattern, str1, str2);
	if (len < 0) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res[len] = '\0';

	return MS_MEDIA_ERR_NONE;
}

int ms_strcopy(char *res, const int size, const char *pattern, const char *str1)
{
	int len = 0;
	int real_size = size;

	if (!res || !pattern || !str1) {
		MS_DBG_ERR("parameta is invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (real_size < (int)(strlen(str1))) {
		MS_DBG_ERR("size is wrong");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	len = snprintf(res, real_size, pattern, str1);
	if (len < 0) {
		MS_DBG_ERR("snprintf failed");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res[len] = '\0';

	return MS_MEDIA_ERR_NONE;
}

bool ms_config_get_int(const char *key, int *value)
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

bool ms_config_set_int(const char *key, int value)
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

bool ms_config_get_str(const char *key, char **value)
{
	char *res = NULL;

	if (key == NULL || value == NULL) {
		MS_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	res = vconf_get_str(key);
	if (MS_STRING_VALID(res)) {
		*value = strdup(res);
		MS_SAFE_FREE(res);
		return true;
	}

	return false;
}

bool ms_config_set_str(const char *key, const char *value)
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

bool ms_config_get_bool(const char *key, int *value)
{
	int err;

	if (!key || !value) {
		MS_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	err = vconf_get_bool(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MS_DBG_ERR("Unexpected error code: %d", err);

	return false;
}

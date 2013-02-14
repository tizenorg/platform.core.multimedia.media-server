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

#include <dirent.h>
#include <malloc.h>
#include <vconf.h>

#include "media-util.h"
#include "media-scanner-dbg.h"
#include "media-scanner-utils.h"
#include "media-scanner-drm.h"

#ifdef FMS_PERF
#include <sys/time.h>
#define MILLION 1000000L
struct timeval g_mmc_start_time;
struct timeval g_mmc_end_time;
#endif

#define MMC_INFO_SIZE 256

extern int mmc_state;
extern GAsyncQueue *storage_queue;
extern GAsyncQueue *scan_queue;



#ifdef FMS_PERF
void
msc_check_start_time(struct timeval *start_time)
{
	gettimeofday(start_time, NULL);
}

void
msc_check_end_time(struct timeval *end_time)
{
	gettimeofday(end_time, NULL);
}

void
msc_check_time_diff(struct timeval *start_time, struct timeval *end_time)
{
	struct timeval time;
	long difftime;

	time.tv_sec = end_time->tv_sec - start_time->tv_sec;
	time.tv_usec = end_time->tv_usec - start_time->tv_usec;
	difftime = MILLION * time.tv_sec + time.tv_usec;
	MSC_DBG_INFO("The function_to_time took %ld microseconds or %f seconds.",
	       difftime, difftime / (double)MILLION);
}
#endif

bool
msc_is_mmc_inserted(void)
{
	int data = -1;
	msc_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &data);
	if (data != VCONFKEY_SYSMAN_MMC_MOUNTED) {
		return false;
	} else {
		return true;
	}
}

int
_msc_update_mmc_info(const char *cid)
{
	bool res;

	if (cid == NULL) {
		MSC_DBG_ERR("Parameters are invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res = msc_config_set_str(MS_MMC_INFO_KEY, cid);
	if (!res) {
		MSC_DBG_ERR("fail to get MS_MMC_INFO_KEY");
		return MS_MEDIA_ERR_VCONF_SET_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

static int
_msc_get_contents(const char *filename, char *buf)
{
	FILE *fp;

	fp = fopen(filename, "rt");
	if (fp == NULL) {
		MSC_DBG_ERR("fp is NULL. file name : %s", filename);
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	if (fgets(buf, 255, fp) == NULL)
		MSC_DBG_ERR("fgets failed");

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}

/*need optimize*/
int
_msc_get_mmc_info(char *cid)
{
	int i;
	int j;
	int len;
	int err = -1;
	bool getdata = false;
	bool bHasColon = false;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char mmcpath[MS_FILE_PATH_LEN_MAX] = { 0 };

	DIR *dp;
	struct dirent ent;
	struct dirent *res = NULL;

	/* mmcblk0 and mmcblk1 is reserved for movinand */
	for (j = 1; j < 3; j++) {
		len = snprintf(mmcpath, MS_FILE_PATH_LEN_MAX, "/sys/class/mmc_host/mmc%d/", j);
		if (len < 0) {
			MSC_DBG_ERR("FAIL : snprintf");
			return MS_MEDIA_ERR_INTERNAL;
		}
		else {
			mmcpath[len] = '\0';
		}

		dp = opendir(mmcpath);
		if (dp == NULL) {
			MSC_DBG_ERR("dp is NULL");
			return MS_MEDIA_ERR_DIR_OPEN_FAIL;
		}

		while (!readdir_r(dp, &ent, &res)) {
			 /*end of read dir*/
			if (res == NULL)
				break;

			bHasColon = false;
			if (ent.d_name[0] == '.')
				continue;

			if (ent.d_type == DT_DIR) {
				/*ent->d_name is including ':' */
				for (i = 0; i < strlen(ent.d_name); i++) {
					if (ent.d_name[i] == ':') {
						bHasColon = true;
						break;
					}
				}

				if (bHasColon) {
					/*check serial */
					err = msc_strappend(path, sizeof(path), "%s%s/cid", mmcpath, ent.d_name);
					if (err < 0) {
						MSC_DBG_ERR("ms_strappend error : %d", err);
						continue;
					}

					if (_msc_get_contents(path, cid) != MS_MEDIA_ERR_NONE)
						break;
					else
						getdata = true;
				}
			}
		}
		closedir(dp);

		if (getdata == true) {
			break;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_update_mmc_info(void)
{
	int err;
	char cid[MMC_INFO_SIZE] = { 0 };

	err = _msc_get_mmc_info(cid);

	err = _msc_update_mmc_info(cid);

	/*Active flush */
	if (!malloc_trim(0))
		MSC_DBG_ERR("malloc_trim is failed");

	return err;
}

bool
_msc_check_mmc_info(const char *cid)
{
	char pre_mmc_info[MMC_INFO_SIZE] = { 0 };
	bool res = false;

	if (cid == NULL) {
		MSC_DBG_ERR("Parameters are invalid");
		return false;
	}

	res = msc_config_get_str(MS_MMC_INFO_KEY, pre_mmc_info);
	if (!res) {
		MSC_DBG_ERR("fail to get MS_MMC_INFO_KEY");
		return false;
	}

	MSC_DBG_INFO("Last MMC info	= %s", pre_mmc_info);
	MSC_DBG_INFO("Current MMC info = %s", cid);

	if (strcmp(pre_mmc_info, cid) == 0) {
		return true;
	}

	return false;
}


ms_dir_scan_type_t
msc_get_mmc_state(void)
{
	char cid[MMC_INFO_SIZE] = { 0 };
	ms_dir_scan_type_t ret = MS_SCAN_ALL;

	/*get new info */
	_msc_get_mmc_info(cid);

	/*check it's same mmc */
	if (_msc_check_mmc_info(cid)) {
		ret = MS_SCAN_PART;
	}

	return ret;
}

void
msc_mmc_vconf_cb(void *data)
{
	int status = 0;
	ms_comm_msg_s *scan_msg;
	ms_dir_scan_type_t scan_type = MS_SCAN_PART;

	if (!msc_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status)) {
		MSC_DBG_ERR("Get VCONFKEY_SYSMAN_MMC_STATUS failed.");
	}

	MSC_DBG_INFO("VCONFKEY_SYSMAN_MMC_STATUS :%d", status);

	mmc_state = status;

	MS_MALLOC(scan_msg, sizeof(ms_comm_msg_s));

	if (mmc_state == VCONFKEY_SYSMAN_MMC_REMOVED ||
		mmc_state == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {

		if (!msc_drm_extract_ext_memory())
			MSC_DBG_ERR("ms_drm_extract_ext_memory failed");

		scan_type = MS_SCAN_INVALID;
	} else if (mmc_state == VCONFKEY_SYSMAN_MMC_MOUNTED) {

		if (!msc_drm_insert_ext_memory())
			MSC_DBG_ERR("ms_drm_insert_ext_memory failed");

		scan_type = msc_get_mmc_state();
	}

	switch (scan_type) {
		case MS_SCAN_ALL:
			scan_msg->msg_type = MS_MSG_STORAGE_ALL;
			break;
		case MS_SCAN_PART:
			scan_msg->msg_type = MS_MSG_STORAGE_PARTIAL;
			break;
		case MS_SCAN_INVALID:
			scan_msg->msg_type = MS_MSG_STORAGE_INVALID;
			break;
	}

	scan_msg->pid = 0;
	scan_msg->msg_size = strlen(MEDIA_ROOT_PATH_SDCARD);
	msc_strcopy(scan_msg->msg, scan_msg->msg_size+1, "%s", MEDIA_ROOT_PATH_SDCARD);

	MSC_DBG_INFO("ms_get_mmc_state is %d", scan_msg->msg_type);

	g_async_queue_push(storage_queue, GINT_TO_POINTER(scan_msg));

	return;
}

/*CAUTION : Before using this function, Have to allocate static memory of ret_path*/
/*And the array length does not over MS_FILE_PATH_LEN_MAX*/
/*for example : char path[MS_FILE_PATH_LEN_MAX] = {0};*/
int
msc_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path, int depth)
{
	int i = 0;
	int path_length = 0;
	int length = 0;
	ms_dir_scan_info *cur_node;
	char **path_array;

	if (depth < 0) {
		MSC_DBG_ERR("depth < 0");
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	MS_MALLOC(path_array, sizeof(char*) * (depth + 1));

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
			MSC_DBG_ERR("This is invalid path, %s, %d", node->name, depth);
			MS_SAFE_FREE(path_array);
			return MS_MEDIA_ERR_INVALID_PATH;
		}

		strncpy(ret_path+path_length, path_array[i], length);
		path_length += length;

		ret_path[path_length] = '/';
		path_length ++;
	}

	ret_path[-- path_length] = '\0';

	MS_SAFE_FREE(path_array);

	return MS_MEDIA_ERR_NONE;
}

ms_storage_type_t
msc_get_storage_type_by_full(const char *path)
{
	if (strncmp(path, MEDIA_ROOT_PATH_INTERNAL, strlen(MEDIA_ROOT_PATH_INTERNAL)) == 0) {
		return MS_STORAGE_INTERNAL;
	} else if (strncmp(path, MEDIA_ROOT_PATH_SDCARD, strlen(MEDIA_ROOT_PATH_SDCARD)) == 0) {
		return MS_STORAGE_EXTERNAL;
	} else
		return MS_MEDIA_ERR_INVALID_PATH;
}

int
msc_strappend(char *res, const int size, const char *pattern,
	     const char *str1, const char *str2)
{
	int len = 0;
	int real_size = size - 1;

	if (!res ||!pattern || !str1 ||!str2 )
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	if (real_size < (strlen(str1) + strlen(str2)))
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	len = snprintf(res, real_size, pattern, str1, str2);
	if (len < 0) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res[len] = '\0';

	return MS_MEDIA_ERR_NONE;
}

int
msc_strcopy(char *res, const int size, const char *pattern, const char *str1)
{
	int len = 0;
	int real_size = size;

	if (!res || !pattern || !str1)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	if (real_size < strlen(str1))
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	len = snprintf(res, real_size, pattern, str1);
	if (len < 0) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res[len] = '\0';

	return MS_MEDIA_ERR_NONE;
}

bool
msc_config_get_int(const char *key, int *value)
{
	int err;

	if (!key || !value) {
		MSC_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	err = vconf_get_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MSC_DBG_ERR("Unexpected error code: %d", err);

	return false;
}

bool
msc_config_set_int(const char *key, int value)
{
	int err;

	if (!key) {
		MSC_DBG_ERR("Arguments key is NULL");
		return false;
	}

	err = vconf_set_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MSC_DBG_ERR("Unexpected error code: %d", err);

	return false;
}

bool
msc_config_get_str(const char *key, char *value)
{
	char *res;
	if (!key || !value) {
		MSC_DBG_ERR("Arguments key or value is NULL");
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
msc_config_set_str(const char *key, const char *value)
{
	int err;

	if (!key || !value) {
		MSC_DBG_ERR("Arguments key or value is NULL");
		return false;
	}

	err = vconf_set_str(key, value);
	if (err == 0)
		return true;
	else
		MSC_DBG_ERR("fail to vconf_set_str %d", err);

	return false;
}


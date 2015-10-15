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
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <pmapi.h>
#include <vconf.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-scanner-dbg-v2.h"
#include "media-scanner-common-v2.h"
#include "media-scanner-socket-v2.h"
#include "media-scanner-extract-v2.h"

GAsyncQueue *storage_extract_queue;
GAsyncQueue *folder_extract_queue;
GMutex extract_req_mutex;
GMutex extract_blocked_mutex;
char *g_extract_cancel_path;
char *g_extract_blocked_path;
bool g_directory_extract_processing;
int stg_extract_status;

GCond extract_data_cond;
GMutex extract_data_mutex;


#ifdef FMS_PERF
extern struct timeval g_mmc_start_time;
extern struct timeval g_mmc_end_time;
#endif

static int __msc_check_extract_stop_status(int scan_type, ms_storage_type_t storage_type, const char *start_path);
static int __msc_set_storage_extract_status(ms_storage_scan_status_e status);
static int __msc_get_storage_extract_status(ms_storage_scan_status_e *status);
static int __msc_resume_extract();
static int __msc_pause_extract();
static int __msc_extract_set_db_status(ms_db_status_type_t status, ms_storage_type_t storage_type);
static int __msc_extract_set_power_mode(ms_db_status_type_t status);

int msc_init_extract_thread()
{
	if (!storage_extract_queue) storage_extract_queue = g_async_queue_new();
	if (!folder_extract_queue) folder_extract_queue = g_async_queue_new();

	g_mutex_init(&extract_req_mutex);
	g_mutex_init(&extract_blocked_mutex);
	g_mutex_init(&extract_data_mutex);

	g_cond_init(&extract_data_cond);

	return MS_MEDIA_ERR_NONE;
}

int msc_deinit_extract_thread()
{
	if (storage_extract_queue) g_async_queue_unref(storage_extract_queue);
	if (folder_extract_queue) g_async_queue_unref(folder_extract_queue);

	g_mutex_clear(&extract_req_mutex);
	g_mutex_clear(&extract_blocked_mutex);
	g_mutex_clear(&extract_data_mutex);

	g_cond_clear(&extract_data_cond);

	return MS_MEDIA_ERR_NONE;
}

gboolean msc_folder_extract_thread(void *data)
{
	ms_comm_msg_s *extract_data = NULL;
	int err;
	int ret;
	void **handle = NULL;
	int scan_type;
	int storage_type;
	char *storage_id = NULL;
	char *update_path = NULL;
	bool power_off_status = FALSE;

	while (1) {
		extract_data = g_async_queue_pop(folder_extract_queue);
		if (extract_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		MS_DBG_ERR("[No-Error] DIRECTORY EXTRACT START folder_path [%s], scan_type [%d]", extract_data->msg, extract_data->msg_type);

		g_directory_extract_processing = true;

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = ms_connect_db(&handle, extract_data->uid);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		scan_type = extract_data->msg_type;

		storage_id = strdup(extract_data->storage_id);
		if (storage_id == NULL) {
			MS_DBG_ERR("storage_id NULL");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		MS_DBG("path : [%s], storage_id : [%s]", extract_data->msg, storage_id);

		update_path = strndup(extract_data->msg, extract_data->msg_size);

		if (strlen(storage_id) == 0) {
			MS_DBG_ERR("storage_id length is 0. There is no information of your request [%s]", extract_data->msg);
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		if (scan_type != MS_MSG_DIRECTORY_SCANNING
			&& scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			MS_DBG_ERR("Invalid request");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		storage_type = ms_get_storage_type_by_full(extract_data->msg, extract_data->uid);
		ret = __msc_check_extract_stop_status(extract_data->msg_type, storage_type, update_path);

		if (ret == MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			MS_DBG_ERR("MS_MEDIA_ERR_SCANNER_FORCE_STOP");
			goto NEXT;
		}

		/*insert data into media db */
		ret = ms_insert_item_pass2(handle, extract_data->storage_id, update_path, scan_type, extract_data->uid);
		msc_del_extract_cancel_path();

		/*call for bundle commit*/
		//__msc_bacth_commit_disable(handle, TRUE, TRUE, extract_data->storage_id, ret);

NEXT :
		msc_get_power_status(&power_off_status);
		if (power_off_status) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		/*Active flush */
		malloc_trim(0);

		if (extract_data->result) {
			msc_send_result(ret, extract_data);
		}

		MS_SAFE_FREE(update_path);
		MS_SAFE_FREE(extract_data);
		MS_SAFE_FREE(storage_id);

		g_directory_extract_processing = false;

		MS_DBG_ERR("[No-Error] DIRECTORY EXTRACT END [%d]", ret);

		ms_storage_scan_status_e storage_scan_status = MS_STORAGE_SCAN_NONE;
		ret = __msc_get_storage_extract_status(&storage_scan_status);
		if (ret == MS_MEDIA_ERR_NONE) {
			/*get storage list and scan status from media db*/
			if (storage_scan_status != MS_STORAGE_SCAN_COMPLETE) {
				__msc_resume_extract();
				MS_DBG_ERR("extract RESUME OK");
			}
		}

		/*disconnect form media db*/
		if (handle) ms_disconnect_db(&handle);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(update_path);
	MS_SAFE_FREE(extract_data);
	MS_SAFE_FREE(storage_id);

	if (handle) ms_disconnect_db(&handle);

	return false;
}

gboolean msc_storage_extract_thread(void *data)
{
	ms_comm_msg_s *extract_data = NULL;
	int ret;
	int err;
	void **handle = NULL;
	ms_storage_type_t storage_type = MS_STORAGE_INTERNAL;
	int scan_type;
	//bool valid_status = TRUE;
	char *update_path = NULL;
	//GArray *dir_array = NULL;
	bool power_off_status = FALSE;

	while (1) {
		__msc_set_storage_extract_status(MS_STORAGE_SCAN_DONE);
		extract_data = g_async_queue_pop(storage_extract_queue);
		if (extract_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		__msc_set_storage_extract_status(MS_STORAGE_SCAN_META_PROCESSING);
		MS_DBG_ERR("STORAGE extract START ");

		scan_type = extract_data->msg_type;
		if (scan_type != MS_MSG_STORAGE_ALL
			&& scan_type != MS_MSG_STORAGE_PARTIAL
			&& scan_type != MS_MSG_STORAGE_INVALID) {
			MS_DBG_ERR("Invalid request[%d]", scan_type);
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = ms_connect_db(&handle, extract_data->uid);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_connect_db falied!");
			continue;
		}

		update_path = strndup(extract_data->msg, extract_data->msg_size);
		MS_DBG_ERR("extract storage_id is [%s], path [%s]", extract_data->storage_id, update_path);

		ms_set_storage_scan_status(handle, extract_data->storage_id, MEDIA_EXTRACT_PROCESSING, extract_data->uid);
		storage_type = ms_get_storage_type_by_full(extract_data->msg, extract_data->uid);
		__msc_extract_set_db_status(MS_DB_UPDATING, storage_type);

		ret = __msc_check_extract_stop_status(extract_data->msg_type, storage_type, update_path);
		if (ret == MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			MS_DBG_ERR("MS_MEDIA_ERR_SCANNER_FORCE_STOP");
			goto NEXT;
		}

		/*extract meta*/
		ret = ms_insert_item_pass2(handle, extract_data->storage_id, update_path, scan_type, extract_data->uid);

		storage_type = ms_get_storage_type_by_full(extract_data->msg, extract_data->uid);
		//update_path = strndup(extract_data->msg, extract_data->msg_size);

		msc_del_extract_blocked_path();

		MS_DBG_ERR("extract PAUSE");
		__msc_pause_extract();
		MS_DBG_ERR("extract RESUME");

		if (ret == MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			ms_set_storage_scan_status(handle, extract_data->storage_id, MEDIA_EXTRACT_STOP, extract_data->uid);
			__msc_set_storage_extract_status(MS_STORAGE_SCAN_META_STOP);
			/*set vconf key mmc loading for indicator */
			__msc_extract_set_db_status(MS_DB_STOPPED, storage_type);
		} else if (extract_data->result == TRUE) {
			MS_DBG_ERR("extract_data->result == TRUE, MS_STORAGE_SCAN_COMPLETE");
			ms_set_storage_scan_status(handle, extract_data->storage_id, MEDIA_EXTRACT_COMPLETE, extract_data->uid);
			__msc_set_storage_extract_status(MS_STORAGE_SCAN_COMPLETE);
			ms_delete_invalid_items(handle, extract_data->storage_id, storage_type, extract_data->uid);
			/*set vconf key mmc loading for indicator */
			__msc_extract_set_db_status(MS_DB_UPDATED, storage_type);
		}

#if 0
		if (scan_type == MS_MSG_STORAGE_PARTIAL && storage_type == MS_STORAGE_INTERNAL) {
			ms_validaty_change_all_items(handle, storage_id, storage_type, true);

			/* find and compare modified time */
			ret = __msc_compare_with_db(handle, storage_id, update_path, scan_data->msg_type, &dir_array);
			if (ret != MS_MEDIA_ERR_NONE) {
				MS_DBG_WARN("__msc_compare_with_db is falied");
				goto NEXT;
			}

			if (dir_array->len != 0) {
				MSC_DBG_INFO("DB UPDATING IS NEEDED");
				ret = _msc_db_update_partial(handle, storage_id, storage_type, dir_array);
			} else {
				MSC_DBG_INFO("THERE IS NO UPDATE");
			}
		} else {
			if (scan_type == MS_MSG_STORAGE_ALL) {
				/*  Delete all data before full scanning */
				if (!ms_delete_all_items(handle, storage_id, storage_type)) {
					MS_DBG_ERR("msc_delete_all_record fails");
				}
			} else if (scan_type == MS_MSG_STORAGE_PARTIAL) {
					ms_validaty_change_all_items(handle, storage_id, storage_type, false);
			}

			ret = __msc_db_update(handle, storage_id, scan_data);
		}

		/*call for bundle commit*/
		if (extract_data.msg_type != MS_MSG_STORAGE_INVALID)
			__msc_bacth_commit_disable(handle, TRUE, valid_status, scan_data->storage_id, ret);

		if (scan_type == MS_MSG_STORAGE_PARTIAL && ret == MS_MEDIA_ERR_NONE) {
			int del_count = 0;

			/*check delete count*/
			MSC_DBG_INFO("update path : %s", update_path);

			ms_count_delete_items_in_folder(handle, storage_id, update_path, &del_count);

			/*if there is no delete content, do not call delete API*/
			if (del_count != 0) {
				MS_DBG_ERR("storage thread delete count [%d]", del_count);
				ms_delete_invalid_items(handle, storage_id, storage_type);
			}
		}

		/* send notification */
		ms_send_dir_update_noti(handle,  storage_id, update_path);
#endif

NEXT:

		MS_SAFE_FREE(update_path);

		msc_get_power_status(&power_off_status);
		if (power_off_status) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		/*disconnect form media db*/
		if (handle) ms_disconnect_db(&handle);

		/*Active flush */
		malloc_trim(0);

		if (extract_data->result) {
			msc_send_result(ret, extract_data);
		}

		MS_SAFE_FREE(extract_data);

		MS_DBG_ERR("STORAGE EXTRACT END[%d]", ret);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(extract_data);
	if (handle) ms_disconnect_db(&handle);

	return false;
}

void msc_insert_exactor_request(int message_type, bool ins_status, const char *storage_id, const char *path, int pid, uid_t uid)
{
	ms_comm_msg_s *extract_data = NULL;
	MS_MALLOC(extract_data, sizeof(ms_comm_msg_s));
	if (extract_data == NULL) {
		MS_DBG_ERR("MS_MALLOC failed");
		return;
	}

	extract_data->msg_type = message_type;
	extract_data->pid = pid;
	extract_data->uid = uid;
	extract_data->result = ins_status;
	extract_data->msg_size = strlen(path);
	strncpy(extract_data->msg, path, extract_data->msg_size);
	strncpy(extract_data->storage_id, storage_id, MS_UUID_SIZE-1);

	if (message_type == MS_MSG_STORAGE_ALL || message_type == MS_MSG_STORAGE_PARTIAL || message_type == MS_MSG_STORAGE_INVALID) {
		g_async_queue_push(storage_extract_queue, GINT_TO_POINTER(extract_data));
		MS_DBG("insert to storage exactor queue. msg_type [%d]", ins_status);
	} else if (message_type == MS_MSG_DIRECTORY_SCANNING || message_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
		g_async_queue_push(folder_extract_queue, GINT_TO_POINTER(extract_data));
		MS_DBG("insert to dir exactor queue. msg_type [%d]", ins_status);
	} else {
		MS_DBG_ERR("try to insert to exactor scan with msg_type [%d]", message_type);
		MS_SAFE_FREE(extract_data);
	}

	return;
}

int msc_remove_extract_request(const ms_comm_msg_s *recv_msg)
{
	if (recv_msg == NULL) {
		MS_DBG_ERR("recv_msg is null");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}
	const char *storageid = recv_msg->storage_id;
	int len = g_async_queue_length(storage_extract_queue);
	ms_comm_msg_s *msg = NULL;
	GAsyncQueue *temp_queue = NULL;

	MS_DBG_WARN("exactor_req_mutex is LOCKED");
	g_mutex_lock(&extract_req_mutex);

	if (len == 0) {
		MS_DBG_ERR("Request is not stacked");
		goto END_REMOVE_REQUEST;
	}

	temp_queue = g_async_queue_new();
	int i = 0;
	for (; i < len; i++) {
		/*create new queue to compare request*/
		msg = g_async_queue_pop(storage_extract_queue);
		if ((strcmp(msg->storage_id, storageid) == 0)) {
			MS_SAFE_FREE(msg);
		} else {
			g_async_queue_push(temp_queue, GINT_TO_POINTER(msg));
		}
	}
	g_async_queue_unref(storage_extract_queue);
	storage_extract_queue = temp_queue;

END_REMOVE_REQUEST:
	g_mutex_unlock(&extract_req_mutex);
	MS_DBG_WARN("exactor_req_mutex is UNLOCKED");

	return MS_MEDIA_ERR_NONE;
}

int msc_set_extract_cancel_path(const char *cancel_path)
{
	if (g_extract_cancel_path != NULL) {
		MS_DBG_WARN("g_extract_cancel_path is not NULL");
		free(g_extract_cancel_path);
		g_extract_cancel_path = NULL;
	}

	g_extract_cancel_path = strdup(cancel_path);

	return MS_MEDIA_ERR_NONE;
}

int msc_del_extract_cancel_path(void)
{
	if (g_extract_cancel_path != NULL) {
		MS_DBG_WARN("g_extract_cancel_path is not NULL");
		free(g_extract_cancel_path);
		g_extract_cancel_path = NULL;
	}

	return MS_MEDIA_ERR_NONE;
}

int msc_set_extract_blocked_path(const char *blocked_path)
{
	MS_DBG_FENTER();

	g_mutex_lock(&extract_blocked_mutex);

	if (g_extract_blocked_path != NULL) {
		MS_DBG_ERR("g_extract_blocked_path is not NULL [%s]", g_extract_blocked_path);
		free(g_extract_blocked_path);
		g_extract_blocked_path = NULL;
	}

	g_extract_blocked_path = strdup(blocked_path);

	g_mutex_unlock(&extract_blocked_mutex);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;
}

int msc_del_extract_blocked_path(void)
{
	MS_DBG_FENTER();

	g_mutex_lock(&extract_blocked_mutex);

	if (g_extract_blocked_path != NULL) {
		MS_DBG_ERR("g_extract_blocked_path is not NULL [%s]", g_extract_blocked_path);
		free(g_extract_blocked_path);
		g_extract_blocked_path = NULL;
	}

	g_mutex_unlock(&extract_blocked_mutex);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;
}

static int __msc_check_extract_stop_status(int scan_type, ms_storage_type_t storage_type, const char *start_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	bool power_off_status = FALSE;

	/*check poweroff status*/
	msc_get_power_status(&power_off_status);
	if (power_off_status) {
		MS_DBG_ERR("Power off");
		ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
	}

	if (scan_type == MS_MSG_DIRECTORY_SCANNING || scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
		g_mutex_lock(&extract_req_mutex);
		/* check cancel path */
		if (g_extract_cancel_path != NULL) {
			MS_DBG_ERR("check cancel storage [%s][%s]", g_extract_cancel_path, start_path);
			if (strncmp(g_extract_cancel_path, start_path, strlen(g_extract_cancel_path)) == 0) {
				MS_DBG_ERR("Receive cancel request [%s][%s]. STOP extract!!", g_extract_cancel_path, start_path);
				unsigned int path_len = strlen(g_extract_cancel_path);

				if (strlen(start_path) > path_len) {
					if (start_path[path_len] == '/') {
						MS_DBG_ERR("start path is same as cancel path");
						ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
					} else {
						MS_DBG_ERR("start path is not same as cancel path");
					}
				} else {
					ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
				}
			}

			MS_SAFE_FREE(g_extract_cancel_path);
		}

		g_mutex_unlock(&extract_req_mutex);
	}

	if (scan_type == MS_MSG_STORAGE_ALL || scan_type == MS_MSG_STORAGE_PARTIAL) {
		if (g_directory_extract_processing == true) {
			MS_DBG_ERR("Now directory extract is start. So, storage extract is stopped.");
			__msc_pause_extract();
			MS_DBG_ERR("Now directory extract is end. So, storage extract is resumed.");
		}

		g_mutex_lock(&extract_blocked_mutex);
		/* check block path */
		if (g_extract_blocked_path != NULL) {
			MS_DBG_ERR("check blocked storage [%s][%s]", g_extract_blocked_path, start_path);
			if (strncmp(start_path, g_extract_blocked_path, strlen(start_path)) == 0) {
				MS_DBG_ERR("Receive blocked message[%s][%s]. STOP extract!!", g_extract_blocked_path, start_path);
				ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
			}

			MS_SAFE_FREE(g_extract_blocked_path);
		}

		g_mutex_unlock(&extract_blocked_mutex);
	}

	return ret;
}

static int __msc_set_storage_extract_status(ms_storage_scan_status_e status)
{
	int res = MS_MEDIA_ERR_NONE;

	stg_extract_status = status;

	return res;
}

static int __msc_get_storage_extract_status(ms_storage_scan_status_e *status)
{
	int res = MS_MEDIA_ERR_NONE;

	*status = stg_extract_status;

	return res;
}

static int __msc_resume_extract()
{
	g_mutex_lock(&extract_data_mutex);

	g_cond_signal(&extract_data_cond);

	g_mutex_unlock(&extract_data_mutex);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_pause_extract()
{
	g_mutex_lock(&extract_data_mutex);

	while (g_directory_extract_processing)
		g_cond_wait(&extract_data_cond, &extract_data_mutex);

	g_mutex_unlock(&extract_data_mutex);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_extract_set_db_status(ms_db_status_type_t status, ms_storage_type_t storage_type)
{
	int res = MS_MEDIA_ERR_NONE;
	int err = 0;

	if (status == MS_DB_UPDATING) {
		if (!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATING)) {
			res = MS_MEDIA_ERR_VCONF_SET_FAIL;
			MS_DBG_ERR("ms_config_set_int failed");
		}

		if (storage_type == MS_STORAGE_EXTERNAL) {
			if (!ms_config_set_int(VCONFKEY_FILEMANAGER_MMC_STATUS, VCONFKEY_FILEMANAGER_MMC_LOADING)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MS_DBG_ERR("ms_config_set_int failed");
			}
		}
	} else if (status == MS_DB_UPDATED) {
		if (!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED)) {
			res = MS_MEDIA_ERR_VCONF_SET_FAIL;
			MS_DBG_ERR("ms_config_set_int failed");
		}

		if (storage_type == MS_STORAGE_EXTERNAL) {
			if (!ms_config_set_int(VCONFKEY_FILEMANAGER_MMC_STATUS, VCONFKEY_FILEMANAGER_MMC_LOADED)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MS_DBG_ERR("ms_config_set_int failed");
			}
		}
	} else {
		if (!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED)) {
			res = MS_MEDIA_ERR_VCONF_SET_FAIL;
			MS_DBG_ERR("ms_config_set_int failed");
		}

		if (storage_type == MS_STORAGE_EXTERNAL) {
			if (!ms_config_set_int(VCONFKEY_FILEMANAGER_MMC_STATUS, VCONFKEY_FILEMANAGER_MMC_LOADED)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MS_DBG_ERR("ms_config_set_int failed");
			}
		}
	}

	err = __msc_extract_set_power_mode(status);
	if (err != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("__msc_tv_set_power_mode fail");
		res = err;
	}

	return res;
}

static int __msc_extract_set_power_mode(ms_db_status_type_t status)
{
	int res = MS_MEDIA_ERR_NONE;
	int err;

	switch (status) {
	case MS_DB_UPDATING:
		err = pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
		if (err != 0)
			res = MS_MEDIA_ERR_INTERNAL;
		break;
	case MS_DB_UPDATED:
		err = pm_unlock_state(LCD_OFF, STAY_CUR_STATE);
		if (err != 0)
			res = MS_MEDIA_ERR_INTERNAL;
		break;
	default:
		MS_DBG_ERR("Unacceptable type : %d", status);
		break;
	}

	return res;
}

int msc_push_extract_request(ms_extract_type_e scan_type, ms_comm_msg_s *recv_msg)
{
	int ret = MS_MEDIA_ERR_NONE;

	switch (scan_type) {
		case MS_EXTRACT_STORAGE:
			g_async_queue_push(storage_extract_queue, GINT_TO_POINTER(recv_msg));
			break;
		case MS_EXTRACT_DIRECTORY:
			g_async_queue_push(folder_extract_queue, GINT_TO_POINTER(recv_msg));
			break;
		default:
			MS_DBG_ERR("invalid parameter");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			break;
	}

	return ret;
}

int msc_stop_extract_thread(void)
{
	ms_comm_msg_s *data = NULL;

	if (storage_extract_queue) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		data->pid = POWEROFF;
		msc_push_extract_request(MS_EXTRACT_STORAGE, data);
	}

	if (folder_extract_queue) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		data->pid = POWEROFF;
		msc_push_extract_request(MS_EXTRACT_DIRECTORY, data);
	}

	return MS_MEDIA_ERR_NONE;
}

int msc_get_remain_extract_request(ms_extract_type_e scan_type, int *remain_request)
{
	int ret = MS_MEDIA_ERR_NONE;

	switch (scan_type) {
		case MS_EXTRACT_STORAGE:
			*remain_request = g_async_queue_length(storage_extract_queue);
			break;
		case MS_EXTRACT_DIRECTORY:
			*remain_request = g_async_queue_length(folder_extract_queue);
			break;
		default:
			MS_DBG_ERR("invalid parameter");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			break;
	}

	return ret;
}

int msc_get_dir_extract_status(bool *extract_status)
{
	*extract_status = g_directory_extract_processing;

	return MS_MEDIA_ERR_NONE;
}


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
 * @file		media-server-scan.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <pmapi.h>
#include <vconf.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-common-external-storage.h"
#include "media-scanner-dbg.h"
#include "media-scanner-db-svc.h"
#include "media-scanner-socket.h"
#include "media-scanner-scan.h"

typedef struct msc_scan_data {
	char *name;
	struct msc_scan_data *next;
} msc_scan_data;

int mmc_state = 0;
bool power_off;
GAsyncQueue * storage_queue;
GAsyncQueue *scan_queue;
GAsyncQueue *reg_queue;

#ifdef FMS_PERF
extern struct timeval g_mmc_start_time;
extern struct timeval g_mmc_end_time;
#endif

static int
_msc_set_power_mode(ms_db_status_type_t status)
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
		MSC_DBG_ERR("Unacceptable type : %d", status);
		break;
	}

	return res;
}

static int
_msc_set_db_status(ms_db_status_type_t status)
{
	int res = MS_MEDIA_ERR_NONE;
	int err = 0;

	if (status == MS_DB_UPDATING) {
		if (!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATING)) {
			res = MS_MEDIA_ERR_VCONF_SET_FAIL;
			MSC_DBG_ERR("ms_config_set_int failed");
		}
	} else if (status == MS_DB_UPDATED) {
		if(!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED)) {
			res = MS_MEDIA_ERR_VCONF_SET_FAIL;
			MSC_DBG_ERR("ms_config_set_int failed");
		}
	}

	err = _msc_set_power_mode(status);
	if (err != MS_MEDIA_ERR_NONE) {
		MSC_DBG_ERR("_msc_set_power_mode fail");
		res = err;
	}

	return res;
}

static bool _msc_check_scan_ignore(char * path)
{
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result;
	char *ignore_path = ".scan_ignore";

	dp = opendir(path);
	if (dp == NULL) {
		MSC_DBG_ERR("%s folder opendir fails", path);
		return true;
	}

	while (!readdir_r(dp, &entry, &result)) {
		if (result == NULL)
			break;

		if (strcmp(entry.d_name, ignore_path) == 0) {
			closedir(dp);
			return true;
		}
	}

	closedir(dp);
	return false;
}

static int _ms_check_stop_status(ms_storage_type_t storage_type)
{
	int ret = MS_MEDIA_ERR_NONE;

	/*check poweroff status*/
	if (power_off) {
		MSC_DBG_INFO("Power off");
		ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
	}

	/*check SD card in out */
	if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORAGE_EXTERNAL)) {
	    	MSC_DBG_INFO("Directory scanning is stopped");
		ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
	}

	return ret;
}

static void _msc_insert_array(GArray *garray, ms_comm_msg_s *insert_data)
{
	MSC_DBG_INFO("path : %s", insert_data->msg);
	MSC_DBG_INFO("scan_type : %d", insert_data->msg_type);

	if (insert_data->pid == POWEROFF) {
		g_array_prepend_val(garray, insert_data);
	} else {
		g_array_append_val(garray, insert_data);
	}
}

void _msc_check_dir_path(char *dir_path)
{
	/* need implementation */
	/* if dir_path is not NULL terminated, this function will occure crash */
	int len = strlen(dir_path);

	if (dir_path[len -1] == '/')
		dir_path[len -1] = '\0';
}

static int _msc_dir_scan(void **handle, const char*start_path, ms_storage_type_t storage_type, int scan_type)
{
	DIR *dp = NULL;
	GArray *dir_array = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int i;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int (*scan_function)(void **, const char*) = NULL;

	/* make new array for storing directory */
	dir_array = g_array_new (FALSE, FALSE, sizeof (char*));
	if (dir_array == NULL){
		MSC_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
	}
	/* add first direcotiry to directory array */
	g_array_append_val (dir_array, start_path);

	if (scan_type == MS_MSG_STORAGE_ALL)
		scan_function = msc_insert_item_batch;
	else
		scan_function = msc_validate_item;

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = _ms_check_stop_status(storage_type);
		if (ret != MS_MEDIA_ERR_NONE) {
			goto STOP_SCAN;
		}
		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index (dir_array, 0);
		MSC_DBG_INFO("%d", dir_array->len);

		if (_msc_check_scan_ignore(current_path)) {
			MSC_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = _ms_check_stop_status(storage_type);
				if (ret != MS_MEDIA_ERR_NONE) {
					goto STOP_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.')
					continue;

				if (entry.d_type & DT_REG) {
					 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
					 	MSC_DBG_ERR("ms_strappend failed");
						continue;
					}
					/* insert into media DB */
					if (scan_function(handle,path) != MS_MEDIA_ERR_NONE) {
						MSC_DBG_ERR("failed to update db : %d\n", scan_type);
						continue;
					}
				} else if (entry.d_type & DT_DIR) {
					if  (scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
						/* this request is recursive scanning */
						 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
						 	MSC_DBG_ERR("ms_strappend failed");
							continue;
						}
						/* add new directory to dir_array */
						new_path = strdup(path);
						g_array_append_val (dir_array, new_path);
					} else {
						/* this request is recursive scanning */
						/* don't add new directory to dir_array */
					}
				}
			}
		} else {
			MSC_DBG_ERR("%s folder opendir fails", current_path);
		}
		if (dp) closedir(dp);
		dp = NULL;
		MS_SAFE_FREE(current_path);
	}		/*db update while */
STOP_SCAN:
	if (dp) closedir(dp);

	/*delete all node*/
	if(dir_array != NULL) {
		for (i =0; i < dir_array->len; i++) {
			char *data = NULL;
			data = g_array_index(dir_array , char*, 0);
			g_array_remove_index (dir_array, 0);
			MS_SAFE_FREE(data);
		}
		g_array_free (dir_array, TRUE);
		dir_array = NULL;
	}

	MSC_DBG_INFO("ret : %d", ret);

	return ret;
}

static int _msc_db_update(void **handle, const ms_comm_msg_s * scan_data)
{
	int scan_type;
	int err = MS_MEDIA_ERR_NONE;
	char *start_path = NULL;
	char *noti_path = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(scan_data->msg);
	scan_type = scan_data->msg_type;

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_type != MS_MSG_STORAGE_INVALID) {
		MSC_DBG_INFO("INSERT");
		start_path = strndup(scan_data->msg, scan_data->msg_size);
		scan_type = scan_data->msg_type;

		err = _msc_dir_scan(handle, start_path, storage_type, scan_type);
		if (err != MS_MEDIA_ERR_NONE) {
			MSC_DBG_ERR("error : %d", err);
		} else {
			noti_path = strndup(scan_data->msg, scan_data->msg_size);
			msc_send_dir_update_noti(handle, noti_path);
			MS_SAFE_FREE(noti_path);
		}
	} else if ( scan_type == MS_MSG_STORAGE_INVALID) {
		MSC_DBG_INFO("INVALID");
		/*In this case, update just validation record*/
		/*update just valid type*/
		err = msc_invalidate_all_items(handle, storage_type);
		if (err != MS_MEDIA_ERR_NONE) {
			MSC_DBG_ERR("error : %d", err);
		} else {
			if (storage_type == MS_STORAGE_INTERNAL) {
				msc_send_dir_update_noti(handle, MEDIA_ROOT_PATH_INTERNAL);
			} else {
				msc_send_dir_update_noti(handle, MEDIA_ROOT_PATH_SDCARD);
			}
		}
	}

	sync();

	MSC_DBG_INFO("ret : %d", err);

	return err;
}

gboolean msc_directory_scan_thread(void *data)
{
	ms_comm_msg_s *scan_data = NULL;
	ms_comm_msg_s *insert_data = NULL;
	GArray *garray = NULL;
	int length;
	int err;
	int ret;
	void **handle = NULL;
	ms_storage_type_t storage_type;
	int scan_type;

	/*create array for processing overlay data*/
	garray = g_array_new (FALSE, FALSE, sizeof (ms_comm_msg_s *));
	if (garray == NULL) {
		MSC_DBG_ERR("g_array_new error");
		return false;
	}

	while (1) {
		length  = g_async_queue_length(scan_queue);

		/*updating requests remain*/
		if (garray->len != 0 && length == 0) {
			scan_data = g_array_index(garray, ms_comm_msg_s*, 0);
			g_array_remove_index (garray, 0);
			if (scan_data->pid == POWEROFF) {
				MSC_DBG_INFO("power off");
				goto _POWEROFF;
			}
		} else if (length != 0) {
			insert_data = g_async_queue_pop(scan_queue);
			_msc_insert_array(garray, insert_data);
			continue;
		} else if (garray->len == 0 && length == 0) {
			/*Threre is no request, Wait until pushung new request*/
			insert_data = g_async_queue_pop(scan_queue);
			_msc_insert_array(garray, insert_data);
			continue;
		}

		MSC_DBG_INFO("DIRECTORY SCAN START");

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = msc_connect_db(&handle);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		storage_type = ms_get_storage_type_by_full(scan_data->msg);
		scan_type = scan_data->msg_type;

		if (scan_type != MS_MSG_DIRECTORY_SCANNING
			&& scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			MSC_DBG_ERR("Invalid request");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		_msc_check_dir_path(scan_data->msg);

		/*change validity before scanning*/
		if (scan_type == MS_MSG_DIRECTORY_SCANNING)
			err = msc_set_folder_validity(handle, scan_data->msg, MS_INVALID, MS_RECURSIVE);
		else
			err = msc_set_folder_validity(handle, scan_data->msg, MS_INVALID, MS_NON_RECURSIVE);
		if (err != MS_MEDIA_ERR_NONE)
			MSC_DBG_ERR("error : %d", err);

		/*call for bundle commit*/
		msc_register_start(handle, MS_NOTI_DISABLE, 0);
		msc_validate_start(handle);

		/*insert data into media db */
		ret = _msc_db_update(handle, scan_data);

		/*call for bundle commit*/
		msc_register_end(handle);
		msc_validate_end(handle);

		if (ret == MS_MEDIA_ERR_NONE) {
			MSC_DBG_INFO("working normally");
			msc_delete_invalid_items_in_folder(handle, scan_data->msg);
		}

		if (power_off) {
			MSC_DBG_INFO("power off");
			goto _POWEROFF;
		}

		/*disconnect form media db*/
		if (handle) msc_disconnect_db(&handle);
NEXT:
		/*Active flush */
		malloc_trim(0);

		msc_send_scan_result(ret, scan_data);

		MS_SAFE_FREE(scan_data);

		MSC_DBG_INFO("DIRECTORY SCAN END");
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (garray) g_array_free (garray, TRUE);
	if (handle) msc_disconnect_db(&handle);

	return false;
}

/* this thread process only the request of media-server */
gboolean msc_storage_scan_thread(void *data)
{
	ms_comm_msg_s *scan_data = NULL;
	ms_comm_msg_s *insert_data = NULL;
	GArray *garray = NULL;
	bool res;
	int ret;
	int length;
	int err;
	void **handle = NULL;
	ms_storage_type_t storage_type;
	int scan_type;

	/*create array for processing overlay data*/
	garray = g_array_new (FALSE, FALSE, sizeof (ms_comm_msg_s *));
	if (garray == NULL) {
		MSC_DBG_ERR("g_array_new error");
		return false;
	}

	while (1) {
		length  = g_async_queue_length(storage_queue);

		/*updating requests remain*/
		if (garray->len != 0 && length == 0) {
			scan_data = g_array_index(garray, ms_comm_msg_s*, 0);
			g_array_remove_index (garray, 0);
			if (scan_data->pid == POWEROFF) {
				MSC_DBG_INFO("power off");
				goto _POWEROFF;
			}
		} else if (length != 0) {
			insert_data = g_async_queue_pop(storage_queue);
			_msc_insert_array(garray, insert_data);
			continue;
		} else if (garray->len == 0 && length == 0) {
			/*Threre is no request, Wait until pushing new request*/
			insert_data = g_async_queue_pop(storage_queue);
			_msc_insert_array(garray, insert_data);
			continue;
		}

		MSC_DBG_INFO("STORAGE SCAN START");

		scan_type = scan_data->msg_type;
		if (scan_type != MS_MSG_STORAGE_ALL
			&& scan_type != MS_MSG_STORAGE_PARTIAL
			&& scan_type != MS_MSG_STORAGE_INVALID) {
			MSC_DBG_ERR("Invalid request");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		storage_type = ms_get_storage_type_by_full(scan_data->msg);
		MSC_DBG_INFO("%d", storage_type);

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = msc_connect_db(&handle);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		/*start db updating */
		_msc_set_db_status(MS_DB_UPDATING);

		/*Delete all data before full scanning*/
		if (scan_type == MS_MSG_STORAGE_ALL) {
			res = msc_delete_all_items(handle, storage_type);
			if (res != true) {
				MSC_DBG_ERR("msc_delete_all_record fails");
			}
		} else if (scan_type == MS_MSG_STORAGE_PARTIAL) {
			err = msc_invalidate_all_items(handle, storage_type);
			if (err != MS_MEDIA_ERR_NONE)
				MSC_DBG_ERR("error : %d", err);
		}

		if (storage_type == MS_STORAGE_EXTERNAL && scan_type == MS_MSG_STORAGE_ALL) {
			ms_update_mmc_info();
		}

#ifdef FMS_PERF
		if (storage_type == MS_STORAGE_EXTERNAL) {
			ms_check_start_time(&g_mmc_start_time);
		}
#endif
		/*call for bundle commit*/
		msc_register_start(handle, MS_NOTI_DISABLE, 0);
		if (scan_type == MS_MSG_STORAGE_PARTIAL) {
			/*enable bundle commit*/
			msc_validate_start(handle);
		}

		/*add inotify watch and insert data into media db */
		ret = _msc_db_update(handle, scan_data);

		/*call for bundle commit*/
		msc_register_end(handle);
		if (scan_type == MS_MSG_STORAGE_PARTIAL) {
			/*disable bundle commit*/
			msc_validate_end(handle);
			if (ret == MS_MEDIA_ERR_NONE) {
				MSC_DBG_INFO("working normally");
				msc_delete_invalid_items(handle, storage_type);
			}
		}

#ifdef FMS_PERF
		if (storage_type == MS_STORAGE_EXTERNAL) {
			ms_check_end_time(&g_mmc_end_time);
			ms_check_time_diff(&g_mmc_start_time, &g_mmc_end_time);
		}
#endif

		/*set vconf key mmc loading for indicator */
		_msc_set_db_status(MS_DB_UPDATED);

		if (power_off) {
			MSC_DBG_INFO("power off");
			goto _POWEROFF;
		}

		/*disconnect form media db*/
		if (handle) msc_disconnect_db(&handle);

NEXT:
		/*Active flush */
		malloc_trim(0);

		msc_send_scan_result(ret, scan_data);

		MS_SAFE_FREE(scan_data);

		MSC_DBG_INFO("STORAGE SCAN END");
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (garray) g_array_free (garray, TRUE);
	if (handle) msc_disconnect_db(&handle);

	return false;
}

static void _msc_insert_register_request(GArray *register_array, ms_comm_msg_s *insert_data)
{
	MSC_DBG_INFO("path : %s", insert_data->msg);

	if (insert_data->pid == POWEROFF) {
		g_array_prepend_val(register_array, insert_data);
	} else if (insert_data->msg_type == MS_MSG_BURSTSHOT_INSERT) {
		g_array_prepend_val(register_array, insert_data);
	} else {
		g_array_append_val(register_array, insert_data);
	}
}

static bool _is_valid_path(const char *path)
{
	if (path == NULL)
		return false;

	if (strncmp(path, MEDIA_ROOT_PATH_INTERNAL, strlen(MEDIA_ROOT_PATH_INTERNAL)) == 0) {
		return true;
	} else if (strncmp(path, MEDIA_ROOT_PATH_SDCARD, strlen(MEDIA_ROOT_PATH_SDCARD)) == 0) {
		return true;
	} else
		return false;

	return true;
}

static int _check_file_path(const char *file_path)
{
	int exist;
	struct stat file_st;

	/* check location of file */
	/* file must exists under "/opt/usr/media" or "/opt/storage/sdcard" */
	if(!_is_valid_path(file_path)) {
		MSC_DBG_ERR("Invalid path : %s", file_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	/* check the file exits actually */
	exist = open(file_path, O_RDONLY);
	if(exist < 0) {
		MSC_DBG_ERR("error [%s, %s]", file_path, strerror(errno));
		return MS_MEDIA_ERR_INVALID_PATH;
	}
	close(exist);

	/* check type of the path */
	/* It must be a regular file */
	memset(&file_st, 0, sizeof(struct stat));
	if(stat(file_path, &file_st) == 0) {
		if(!S_ISREG(file_st.st_mode)) {
			/* In this case, it is not a regula file */
			MSC_DBG_ERR("this path is not a file");
			return MS_MEDIA_ERR_INVALID_PATH;
		}
	} else {
		MSC_DBG_ERR("stat failed [%s]", strerror(errno));
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	return MS_MEDIA_ERR_NONE;
}

gboolean msc_register_thread(void *data)
{
	ms_comm_msg_s *register_data = NULL;
	ms_comm_msg_s *insert_data = NULL;
	GArray *register_array = NULL;
	GArray *path_array = NULL;
	FILE *fp = NULL;
	char buf[MS_FILE_PATH_LEN_MAX];
	char *insert_path = NULL;
	char *path = NULL;
	char *file_path = NULL;
	int path_size;
	int length;
	int err;
	int i;
	int ret;
	void **handle = NULL;
	ms_msg_type_e current_msg = MS_MSG_MAX;
	int (*insert_function)(void **, const char*) = NULL;

	/*create array for processing overlay data*/
	register_array = g_array_new (FALSE, FALSE, sizeof (ms_comm_msg_s *));
	if (register_array == NULL) {
		MSC_DBG_ERR("g_array_new error");
		return false;
	}

	while (1) {
		length  = g_async_queue_length(reg_queue);

		/*updating requests remain*/
		if (register_array->len != 0 && length == 0) {
			register_data = g_array_index(register_array, ms_comm_msg_s*, 0);
			g_array_remove_index (register_array, 0);
			if (register_data->pid == POWEROFF) {
				MSC_DBG_INFO("power off");
				goto _POWEROFF;
			}
		} else if (length != 0) {
			insert_data = g_async_queue_pop(reg_queue);
			_msc_insert_register_request(register_array, insert_data);
			continue;
		} else if (register_array->len == 0 && length == 0) {
			/*Threre is no request, Wait until pushung new request*/
			insert_data = g_async_queue_pop(reg_queue);
			_msc_insert_register_request(register_array, insert_data);
			continue;
		}

		if((register_data->msg_size <= 0) ||(register_data->msg_size > MS_FILE_PATH_LEN_MAX)) {
			MSC_DBG_ERR("message size[%d] is wrong", register_data->msg_size);
			goto FREE_RESOURCE;
		} else {
			path_size = register_data->msg_size + 1;
		}

		file_path = strndup(register_data->msg, register_data->msg_size);

		/* load the file list from file */
		fp = fopen(file_path, "rt");
		if (fp == NULL) {
			MSC_DBG_ERR("fopen failed [%s]", strerror(errno));
			goto FREE_RESOURCE;
		}

		memset(buf, 0x0, MS_FILE_PATH_LEN_MAX);
		/* This is an array for storing the path of insert datas*/
		path_array = g_array_new (FALSE, FALSE, sizeof (char *));
		if (path_array == NULL) {
			MSC_DBG_ERR("g_array_new failed");
			goto FREE_RESOURCE;
		}

		/* read registering file path from stored file */
		while(fgets(buf, MS_FILE_PATH_LEN_MAX, fp) != NULL) {
			length = strlen(buf); /*the return value of function, strlen(), includes "\n" */
			path = strndup(buf, length - 1); /*copying except "\n" and strndup fuction adds "\0" at the end of the copying string */
			MSC_DBG_INFO("insert path : %s [%d]", path, strlen(path));
			/* insert getted path to the list */
			if (g_array_append_val(path_array, path)  == NULL) {
				MSC_DBG_ERR("g_array_append_val failed");
				goto FREE_RESOURCE;
			}
		}

		/* connect to media db, if conneting is failed, db updating is stopped */
		err = msc_connect_db(&handle);
		if (err != MS_MEDIA_ERR_NONE)
			goto FREE_RESOURCE;

		/*start db updating */
		/*call for bundle commit*/
		msc_register_start(handle, MS_NOTI_ENABLE, register_data->pid);

		/* check current request */
		current_msg = register_data->msg_type;
		if (current_msg == MS_MSG_BULK_INSERT) {
			insert_function = msc_insert_item_batch;
		} else if (current_msg == MS_MSG_BURSTSHOT_INSERT) {
			insert_function = msc_insert_burst_item;
		} else {
			MSC_DBG_ERR("wrong message type");
			goto FREE_RESOURCE;
		}

		/* get the inserting file path from array  and insert to db */
		for (i = 0; i < path_array->len; i++) {

			insert_path =  g_array_index(path_array, char*, i);
			/* check the file */
			/* it is really existing */
			/* it is in /opt/usr/media or /opt/storage/sdcard */
			/* it is really regular file */
			ret = _check_file_path(insert_path);
			if (ret != MS_MEDIA_ERR_NONE) {
				MSC_DBG_ERR("Can't insert the meta of the path");
				continue;
			}

			/* insert to db */
			err = insert_function(handle, insert_path);

			if (power_off) {
				MSC_DBG_INFO("power off");
				/*call for bundle commit*/
				msc_register_end(handle);
				goto _POWEROFF;
			}
		}

		/*call for bundle commit*/
		msc_register_end(handle);

		/*disconnect form media db*/
		if (handle) msc_disconnect_db(&handle);

		/*Active flush */
		malloc_trim(0);

		/* If register_files operation is stopped, there is no necessrty for sending result. */
		msc_send_register_result(MS_MEDIA_ERR_NONE, register_data);
FREE_RESOURCE:
		if (path_array) {
			g_array_free(path_array, TRUE);
			path_array = NULL;
		}

		MS_SAFE_FREE(file_path);
		MS_SAFE_FREE(register_data);

		if(fp) fclose(fp);
		fp = NULL;

		/* reset function pointer */
		insert_function = NULL;
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(file_path);
	MS_SAFE_FREE(register_data);
	if (register_array) g_array_free (register_array, TRUE);
	if (path_array) g_array_free (path_array, TRUE);
	if (handle) msc_disconnect_db(&handle);

	if(fp) fclose(fp);

	return false;
}

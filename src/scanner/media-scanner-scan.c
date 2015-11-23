/*
 * Media Server
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

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <pmapi.h>
#include <vconf.h>
#include <grp.h>
#include <pwd.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-common-external-storage.h"
#include "media-common-db-svc.h"
#include "media-scanner-dbg.h"
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
GMutex scan_req_mutex;
GMutex blocked_mutex;
char *g_cancel_path;
char *g_blocked_path;
bool g_directory_scan_processing;
GArray *blocked_stg_list;

int stg_scan_status;

#ifdef FMS_PERF
extern struct timeval g_mmc_start_time;
extern struct timeval g_mmc_end_time;
#endif

static int __msc_set_power_mode(ms_db_status_type_t status);
static int __msc_set_db_status(ms_db_status_type_t status, ms_storage_type_t storage_type);
static int __msc_check_stop_status(ms_storage_type_t storage_type);
static int __msc_stg_scan(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, uid_t uid);
static int __msc_dir_scan(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, bool is_recursive, uid_t uid);
static int __msc_db_update(void **handle, const char *storage_id, const ms_comm_msg_s * scan_data);
static int __msc_check_file_path(const char *file_path, uid_t uid);
static bool __msc_check_folder_path(const char *folder_path);
static int __msc_make_file_list(char *file_path, GArray **path_array, uid_t uid);
static int __msc_batch_insert(ms_msg_type_e current_msg, int pid, GArray *path_array, uid_t uid);
static int __msc_pop_register_request(GArray *register_array, ms_comm_msg_s **register_data);
static int __msc_clear_file_list(GArray *path_array);
static bool __msc_check_scan_ignore(char * path);
static bool __msc_is_valid_path(const char *path, uid_t uid);
static void __msc_trim_dir_path(char *dir_path);
static void __msc_insert_register_request(GArray *register_array, ms_comm_msg_s *insert_data);
static void __msc_bacth_commit_enable(void* handle, bool ins_status, bool valid_status, bool noti_enable, int pid);
static void __msc_bacth_commit_disable(void* handle, bool ins_status, bool valid_status, const char *path, uid_t uid);
static int __msc_set_storage_scan_status(ms_storage_scan_status_e status);
//static int __msc_get_storage_scan_status(ms_storage_scan_status_e *status, uid_t uid);
//static bool __msc_storage_mount_status(const char* start_path);
static char* __msc_get_path(uid_t uid);

static char* __msc_get_path(uid_t uid)
{
	int result_size = 0;
	char *result_passwd = NULL;
	struct group *grpinfo = NULL;
	if (uid == getuid()) {
		grpinfo = getgrnam("users");
		if (grpinfo == NULL) {
			MS_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
		if (MS_STRING_VALID(MEDIA_ROOT_PATH_INTERNAL))
			result_passwd = strndup(MEDIA_ROOT_PATH_INTERNAL, strlen(MEDIA_ROOT_PATH_INTERNAL));
	} else {
		char passwd_str[MAX_FILEPATH_LEN] = {0, };
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
		result_size = strlen(userinfo->pw_dir) + strlen(MEDIA_CONTENT_PATH) + 2;

		snprintf(passwd_str, sizeof(passwd_str), "%s/%s", userinfo->pw_dir, MEDIA_CONTENT_PATH);
		result_passwd = g_strdup(passwd_str);
	}

	return result_passwd;
}

static int __msc_set_power_mode(ms_db_status_type_t status)
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

static int __msc_set_db_status(ms_db_status_type_t status, ms_storage_type_t storage_type)
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
		if (!ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATED)) {
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

	err = __msc_set_power_mode(status);
	if (err != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("__msc_set_power_mode fail");
		res = err;
	}

	return res;
}

static bool __msc_check_scan_ignore(char * path)
{
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result;
	const char *ignore_path = ".scan_ignore";

	if (strstr(path, "/.")) {
		MS_DBG_ERR("hidden path");
		return true;
	}

	dp = opendir(path);
	if (dp == NULL) {
		MS_DBG_ERR("%s folder opendir fails", path);
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

#if 0
GCond data_cond;
GMutex data_mutex;
gpointer current_data = NULL;

static int __msc_resume_scan()
{
	g_mutex_lock(&data_mutex);

//	current_data = GINT_TO_POINTER(g_directory_scan_processing);
	g_cond_signal(&data_cond);

	g_mutex_unlock(&data_mutex);

	return MS_MEDIA_ERR_NONE;
}
static int __msc_pause_scan()
{
	g_mutex_lock(&data_mutex);

//	while (current_data)
	while (g_directory_scan_processing)
		g_cond_wait(&data_cond, &data_mutex);

//	current_data = GPOINTER_TO_INT(g_directory_scan_processing);

	g_mutex_unlock(&data_mutex);

	return MS_MEDIA_ERR_NONE;
}
#endif

static int __msc_check_stop_status(ms_storage_type_t storage_type)
{
	int ret = MS_MEDIA_ERR_NONE;

	/*check poweroff status*/
	if (power_off) {
		MS_DBG_ERR("Power off");
		ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
	}

	/*check SD card in out */
#if 0
	if ((mmc_state != MS_STG_INSERTED) && (storage_type == MS_STORAGE_EXTERNAL)) {
		MS_DBG_ERR("Directory scanning is stopped");
		ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
	}
#endif
	return ret;
}

static void __msc_trim_dir_path(char *dir_path)
{
	/* need implementation */
	/* if dir_path is not NULL terminated, this function will occure crash */
	int len = strlen(dir_path);

	if (dir_path[len -1] == '/')
		dir_path[len -1] = '\0';
}

static bool __msc_check_mount_storage(const char* start_path)
{
	bool ret = FALSE;
	DIR *dp = NULL;

	dp = opendir(start_path);

	if (dp != NULL) {
		ret = TRUE;
		closedir(dp);
		MS_DBG_ERR("STORAGE IS MOUNTED[%s]", start_path);
	}

	return ret;
}

static int __msc_read_dir(void **handle, GArray *dir_array, const char *start_path, const char *storage_id, ms_storage_type_t storage_type, int scan_type, uid_t uid)
{
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int (*scan_function)(void **, const char*, const char*, uid_t) = NULL;

	if (dir_array != NULL) {
		MS_DBG_ERR("dir_array is NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	/* make new array for storing directory */
	dir_array = g_array_new(FALSE, FALSE, sizeof(char*));
	if (dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_array_append_val(dir_array, start_path);

	scan_function = (scan_type == MS_MSG_STORAGE_ALL) ? ms_insert_item_batch : ms_validate_item;

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = __msc_check_stop_status(storage_type);
		if (ret != MS_MEDIA_ERR_NONE) {
			goto STOP_DIR_SCAN;
		}
		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index(dir_array, 0);
		MS_DBG_SLOG("%d", dir_array->len);

		if (__msc_check_scan_ignore(current_path)) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		ms_insert_folder_start(handle);

		dp = opendir(current_path);
		if (dp != NULL) {
			/*insert current directory*/
			if (ms_insert_folder(handle, storage_id, current_path, uid) != MS_MEDIA_ERR_NONE) {
				MS_DBG_ERR("insert folder failed");
			}

			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = __msc_check_stop_status(storage_type);
				if (ret != MS_MEDIA_ERR_NONE) {
					ms_insert_folder_end(handle, uid);
					goto STOP_DIR_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.')
					continue;

				if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strappend failed");
					continue;
				}

				if (entry.d_type & DT_REG) {
					/* insert into media DB */
					if (scan_function(handle, storage_id, path, uid) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("failed to update db : %d", scan_type);
						continue;
					}
				} else if (entry.d_type & DT_DIR) {
					if	(scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
						/* this request is recursive scanning */
						/* add new directory to dir_array */
						new_path = strdup(path);
						if (new_path != NULL) {
							g_array_append_val(dir_array, new_path);
						} else {
							MS_DBG_ERR("strdup failed");
							continue;
						}
					} else {
						/* this request is recursive scanning */
						/* don't add new directory to dir_array */
						if (ms_insert_folder(handle, storage_id, path, uid) != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("insert folder failed");
						}
					}
				}
			}
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
		}
		if (dp) closedir(dp);
		dp = NULL;

		ms_insert_folder_end(handle, uid);

		MS_SAFE_FREE(current_path);
	}

STOP_DIR_SCAN:
	if (dp) closedir(dp);
	__msc_clear_file_list(dir_array);

	return ret;

}

static int __msc_dir_scan(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, bool is_recursive, uid_t uid)
{
	GArray *dir_array = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_start_path = NULL;

	new_start_path = strdup(start_path);
	if (new_start_path == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	MS_DBG_ERR("[No-Error] start path [%s]", new_start_path);

	/*call for bundle commit*/
	__msc_bacth_commit_enable(handle, TRUE, TRUE, MS_NOTI_SWITCH_OFF, 0);

	__msc_read_dir(handle, dir_array, start_path, storage_id, storage_type, scan_type, uid);

	/*call for bundle commit*/
	__msc_bacth_commit_disable(handle, TRUE, TRUE, new_start_path, uid);

	MS_SAFE_FREE(new_start_path);

	MS_DBG_INFO("ret : %d", ret);

	return ret;
}


static int __msc_stg_scan(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, uid_t uid)
{
	DIR *dp = NULL;
	GArray *dir_array = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	bool is_recursive = true;
	char *new_start_path = NULL;
	int (*scan_function)(void **, const char*, const char*, uid_t) = NULL;

	/* make new array for storing directory */
	dir_array = g_array_new(FALSE, FALSE, sizeof(char*));
	if (dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	/* add first direcotiry to directory array */
	new_start_path = strdup(start_path);
	if (new_start_path == NULL) {
		MS_DBG_ERR("g_array_new failed");
		g_array_free(dir_array, FALSE);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_array_append_val(dir_array, start_path);

	MS_DBG_ERR("[No-Error] start path [%s]", new_start_path);

	scan_function = (scan_type == MS_MSG_STORAGE_ALL) ? ms_insert_item_batch : ms_validate_item;
	is_recursive = (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) ? false : true;

	/* folder validity set 0 under the start_path in folder table*/
	if (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE || scan_type == MS_MSG_DIRECTORY_SCANNING) {
		if (ms_set_folder_validity(handle, storage_id, new_start_path, MS_INVALID, is_recursive, uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("set_folder_validity failed [%d] ", scan_type);
		}
	}

	if (__msc_check_folder_path(new_start_path)) {
		ms_insert_folder(handle, storage_id, new_start_path, uid);
	}

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = __msc_check_stop_status(storage_type);
		if (ret != MS_MEDIA_ERR_NONE) {
			goto STOP_SCAN;
		}
		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index(dir_array, 0);
//		MS_DBG_SLOG("%d", dir_array->len);

		if (__msc_check_scan_ignore(current_path)) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		ms_insert_folder_start(handle);

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = __msc_check_stop_status(storage_type);
				if (ret != MS_MEDIA_ERR_NONE) {
					goto STOP_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.')
					continue;

				 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strappend failed");
					continue;
				}

				if (entry.d_type & DT_REG) {
					/* insert into media DB */
					if (scan_function(handle, storage_id, path, uid) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("failed to update db : %d", scan_type);
						continue;
					}
				} else if (entry.d_type & DT_DIR) {
					if (scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
						/* this request is recursive scanning */
						/* add new directory to dir_array */
						new_path = strdup(path);
						if (new_path != NULL) {
							g_array_append_val(dir_array, new_path);

							if (ms_insert_folder(handle, storage_id, new_path, uid) != MS_MEDIA_ERR_NONE) {
								MS_DBG_ERR("insert folder failed");
							}
						} else {
							MS_DBG_ERR("strdup failed");
							continue;
						}
					} else {
						/* this request is recursive scanning */
						/* don't add new directory to dir_array */
						if (ms_insert_folder(handle, storage_id, path, uid) != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("insert folder failed");
							continue;
						}
					}
				}
			}
			/* update modified time of directory */
			if (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE && storage_type == MS_STORAGE_INTERNAL)
				ms_update_folder_time(handle, INTERNAL_STORAGE_ID, current_path, uid);
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
		}
		if (dp) closedir(dp);
		dp = NULL;

		ms_insert_folder_end(handle, uid);

		MS_SAFE_FREE(current_path);
	}		/*db update while */

		/*remove invalid folder in folder table.*/
	if (__msc_check_mount_storage(new_start_path)) {
		if (ms_delete_invalid_folder(handle, storage_id, uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("delete invalid folder failed");
			ret = MS_MEDIA_ERR_DB_DELETE_FAIL;
		}
	} else {
		MS_DBG_ERR("start path is unmounted");
	}

STOP_SCAN:
	if (dp) closedir(dp);

	MS_SAFE_FREE(new_start_path);

	__msc_clear_file_list(dir_array);

	if (ret != MS_MEDIA_ERR_NONE) MS_DBG_INFO("ret : %d", ret);

	return ret;
}

static int __msc_db_update(void **handle, const char *storage_id, const ms_comm_msg_s * scan_data)
{
	int scan_type;
	int err = MS_MEDIA_ERR_NONE;
	char *start_path = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(scan_data->msg, scan_data->uid);
	scan_type = scan_data->msg_type;
	start_path = strndup(scan_data->msg, scan_data->msg_size);

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_type != MS_MSG_STORAGE_INVALID) {
		MS_DBG_INFO("INSERT");

		err = __msc_stg_scan(handle, storage_id, start_path, storage_type, scan_type, scan_data->uid);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("error : %d", err);
		}
	} else if (scan_type == MS_MSG_STORAGE_INVALID) {
		MS_DBG_INFO("INVALID");
		/*In this case, update just validation record*/
		/*update just valid type*/
		err = ms_validaty_change_all_items(handle, storage_id, storage_type, false, scan_data->uid);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("error : %d", err);
		}

		ms_set_folder_validity(handle, storage_id, start_path, 0, TRUE, scan_data->uid);

		MS_SAFE_FREE(start_path);
	}

	sync();

	return err;
}

gboolean msc_directory_scan_thread(void *data)
{
	ms_comm_msg_s *scan_data = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	void **handle = NULL;
	int scan_type;
	char *storage_id = NULL;
	bool is_recursive = true;
	char *start_path = NULL;
	ms_storage_type_t storage_type;
	ms_noti_type_e noti_type = MS_ITEM_INSERT;
	int before_count = 0;
	int after_count = 0;
	int delete_count = 0;
	int delete_folder_count = 0;
	char *folder_uuid = NULL;

	while (1) {
		scan_data = g_async_queue_pop(scan_queue);
		if (scan_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		MS_DBG_ERR("DIRECTORY SCAN START [%s %d]", scan_data->msg, scan_data->msg_type);

		/*connect to media db, if conneting is failed, db updating is stopped*/
		ret = ms_connect_db(&handle, scan_data->uid);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_connect_db failed");
			continue;
		}

		scan_type = scan_data->msg_type;

		if (strlen(scan_data->storage_id) > 0)
			storage_id = strdup(scan_data->storage_id);
		else
			storage_id = strdup("media");

		if (storage_id == NULL) {
			MS_DBG_ERR("storage_id NULL");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		MS_DBG("path : [%s], storage_id : [%s]", scan_data->msg, storage_id);

		if (strlen(storage_id) == 0) {
			MS_DBG_ERR("storage_id length is 0. There is no information of your request [%s]", scan_data->msg);
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		if (scan_type != MS_MSG_DIRECTORY_SCANNING
			&& scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			MS_DBG_ERR("Invalid request");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		__msc_trim_dir_path(scan_data->msg);

		if (__msc_check_folder_path(scan_data->msg)) {
			is_recursive = (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) ? false : true;

			if (ms_check_folder_exist(handle, storage_id, scan_data->msg) == MS_MEDIA_ERR_NONE) {
				/*already exist in media DB*/
				noti_type = MS_ITEM_UPDATE;
				MS_DBG_ERR("[%s] already exist", scan_data->msg);
			} else {
				noti_type = MS_ITEM_INSERT;
				MS_DBG_ERR("[%s] new insert path", scan_data->msg);
			}
		} else {
			/*directory is deleted*/
			is_recursive = true;
			noti_type = MS_ITEM_DELETE;

			/*get the UUID of deleted folder*/
			ms_get_folder_id(handle, storage_id, scan_data->msg, &folder_uuid);

			MS_DBG_ERR("[%s][%s] delete path", scan_data->msg, folder_uuid);
		}

		ms_check_subfolder_count(handle, storage_id, scan_data->msg, &before_count);
		MS_DBG_ERR("BEFORE COUNT[%d]", before_count);

		/* folder validity set 0 under the start_path in folder table*/
		if (ms_set_folder_validity(handle, storage_id, scan_data->msg, MS_INVALID, is_recursive, scan_data->uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("set_folder_validity failed [%d] ", scan_type);
		}

		/*change validity before scanning*/
		if (ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_INVALID, is_recursive, scan_data->uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("set_folder_validity failed [%d] ", scan_type);
		}

		/*insert data into media db */
		storage_type = ms_get_storage_type_by_full(scan_data->msg, scan_data->uid);
		start_path = strndup(scan_data->msg, scan_data->msg_size);

		ret = __msc_dir_scan(handle, storage_id, start_path, storage_type, scan_data->msg_type, is_recursive, scan_data->uid);

		ms_check_subfolder_count(handle, storage_id, scan_data->msg, &after_count);
		MS_DBG_ERR("BEFORE COUNT[%d]", before_count);

		if (ms_count_delete_items_in_folder(handle, storage_id, scan_data->msg, &delete_count) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("counting invalid items failed");
		}

		if (ms_delete_invalid_items_in_folder(handle, storage_id, scan_data->msg, is_recursive, scan_data->uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("deleting invalid items in folder failed");
		}

		/*remove invalid folder in folder table.*/
		if (ms_delete_invalid_folder_by_path(handle, storage_id, scan_data->msg, scan_data->uid, &delete_folder_count) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("deleting invalid folder failed");
		}

		MS_DBG_SLOG("delete folder count %d", delete_folder_count);

		if (ret != MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			MS_DBG_INFO("working normally");

			if (noti_type != MS_ITEM_UPDATE) {
				ms_send_dir_update_noti(handle, storage_id, scan_data->msg, folder_uuid, noti_type, scan_data->pid);
			} else {
				int insert_count = ms_get_insert_count();
				int delete_count = ms_get_delete_count();

				MS_DBG_SLOG("delete count %d", delete_count);
				MS_DBG_SLOG("insert count %d", insert_count);

				if (!(delete_count == 0 && insert_count == 0)
					|| !(before_count == after_count)
					|| delete_folder_count != 0) {
					ms_send_dir_update_noti(handle, storage_id, scan_data->msg, folder_uuid, noti_type, scan_data->pid);
				}
			}
		}

		ms_reset_insert_count();
		ms_reset_delete_count();

		if (power_off) {
			MS_DBG_WARN("power off");
			goto _POWEROFF;
		}

		/*disconnect from media db*/
		if (handle) ms_disconnect_db(&handle);
NEXT:
		/*Active flush */
		malloc_trim(0);

		msc_send_result(ret, scan_data);

		MS_SAFE_FREE(scan_data);
		MS_SAFE_FREE(storage_id);
		MS_SAFE_FREE(folder_uuid);

		MS_DBG_INFO("DIRECTORY SCAN END [%d]", ret);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (handle) ms_disconnect_db(&handle);

	return false;
}

/* this thread process only the request of media-server */
static int _check_folder_from_list(char *folder_path, int item_num, GArray *dir_array)
{
	int i = 0;
	int array_len = dir_array->len;
	ms_dir_info_s* dir_info = NULL;
	struct stat buf;
	time_t mtime;
	bool find_flag = false;

	if (stat(folder_path, &buf) == 0) {
		mtime = buf.st_mtime;
	} else {
		return MS_MEDIA_ERR_INTERNAL;
	}

	for (i = 0; i < array_len; i++) {
		dir_info = g_array_index(dir_array, ms_dir_info_s*, i);
		if (strcmp(folder_path, dir_info->dir_path) == 0) {
			/* if modified time is same, the folder does not need updating */
			if ((mtime == dir_info->modified_time) && (item_num == dir_info->item_num)) {
				if (mtime == 0)
					continue;

				g_array_remove_index(dir_array, i);
				MS_SAFE_FREE(dir_info->dir_path);
				MS_SAFE_FREE(dir_info);
			}
			find_flag = true;
			break;
		}
	}

	/* this folder does not exist in media DB, so this folder has to be inserted to DB */
	if (find_flag == false) {
		dir_info = NULL;
		dir_info = malloc(sizeof(ms_dir_info_s));
		if (dir_info == NULL) {
			MS_DBG_ERR("MALLOC failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
		memset(dir_info, 0, sizeof(ms_dir_info_s));
		dir_info->dir_path = strdup(folder_path);
		dir_info->modified_time = -1;
		g_array_append_val(dir_array, dir_info);
	}

	return MS_MEDIA_ERR_NONE;
}

static int __msc_compare_with_db(void **handle, const char *storage_id, const char*update_path, int scan_type, GArray **dir_array)
{
	DIR *dp = NULL;
	GArray *read_dir_array = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char * start_path = strdup(update_path);
	int item_num = 0;

	MS_DBG_FENTER();

	/*get directories list from media db*/
	ret = ms_get_folder_list(handle, storage_id, start_path, dir_array);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_SAFE_FREE(start_path);
		MS_DBG_ERR("ms_get_folder_list is failed", ret);
		return ret;
	}

	/* make new array for storing directory */
	read_dir_array = g_array_new(FALSE, FALSE, sizeof(char*));
	if (read_dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	/* add first direcotiry to directory array */
	g_array_append_val(read_dir_array, start_path);

	/*start db update. the number of element in the array , db update is complete.*/
	while (read_dir_array->len != 0) {
		/* get the current path from directory array */
		current_path = g_array_index(read_dir_array , char*, 0);
		g_array_remove_index(read_dir_array, 0);
//		MS_DBG_ERR("%s", current_path);

		if (__msc_check_scan_ignore(current_path)) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				if (result == NULL)
					break;

				if (entry.d_name[0] == '.') {
					continue;
				}

				 if (entry.d_type & DT_DIR) {
					 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("ms_strappend failed");
						continue;
					}
					/* add new directory to dir_array */
					new_path = strdup(path);
					g_array_append_val(read_dir_array, new_path);
				} else if (entry.d_type & DT_REG) {
					item_num++;
				}
			}

			/* find and compare modified time */
			_check_folder_from_list(current_path, item_num, *dir_array);
			item_num = 0;
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
		}
		if (dp) closedir(dp);
		dp = NULL;
		MS_SAFE_FREE(current_path);
	}		/*db update while */

	__msc_clear_file_list(read_dir_array);

	MS_DBG_INFO("ret : %d", ret);
	MS_DBG_INFO("update count : %d", (*dir_array)->len);

	return ret;
}

static int _msc_db_update_partial(void **handle, const char *storage_id, ms_storage_type_t storage_type, GArray *dir_array, uid_t uid)
{
	unsigned int i;
	int err = MS_MEDIA_ERR_NONE;
	ms_dir_info_s* dir_info = NULL;
	char *update_path = NULL;

	for (i = 0; i < dir_array->len; i++) {
		dir_info = g_array_index(dir_array, ms_dir_info_s*, i);
		update_path = strdup(dir_info->dir_path);

//		MS_DBG_SLOG("update_path : %s, %d", update_path, dir_info->modified_time);
		if (dir_info->modified_time != -1) {
			err = ms_set_folder_item_validity(handle, storage_id, update_path, MS_INVALID, MS_NON_RECURSIVE, uid);
			if (err != MS_MEDIA_ERR_NONE) {
				MS_DBG_SLOG("update_path : %s, %d", update_path, dir_info->modified_time);
				MS_DBG_ERR("error : %d", err);
			}
		}

		__msc_stg_scan(handle, storage_id, update_path, storage_type, MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE, uid);

//		if (dir_info->modified_time != -1) {
//			ms_update_folder_time(handle, tmp_path);
//		}
	}

	/*delete all node*/
	while (dir_array->len != 0) {
		ms_dir_info_s *data = NULL;
		data = g_array_index(dir_array , ms_dir_info_s*, 0);
		g_array_remove_index(dir_array, 0);
		MS_SAFE_FREE(data->dir_path);
		MS_SAFE_FREE(data);
	}
	g_array_free(dir_array, FALSE);
	dir_array = NULL;

	return MS_MEDIA_ERR_NONE;
}

gboolean msc_storage_scan_thread(void *data)
{
	ms_comm_msg_s *scan_data = NULL;
	int ret;
	int err;
	void **handle = NULL;
	ms_storage_type_t storage_type = MS_STORAGE_INTERNAL;
	int scan_type;
	bool valid_status = TRUE;
	char *update_path = NULL;
	GArray *dir_array = NULL;
	char *storage_id = NULL;

	while (1) {
		__msc_set_storage_scan_status(MS_STORAGE_SCAN_PREPARE);
		scan_data = g_async_queue_pop(storage_queue);
		if (scan_data->pid == POWEROFF) {
			MS_DBG_WARN("power off");
			goto _POWEROFF;
		}

		__msc_set_storage_scan_status(MS_STORAGE_SCAN_PROCESSING);
		MS_DBG_ERR("[No-Error] STORAGE SCAN START [%s][%s]", scan_data->msg, scan_data->storage_id);

		scan_type = scan_data->msg_type;
		if (scan_type != MS_MSG_STORAGE_ALL
			&& scan_type != MS_MSG_STORAGE_PARTIAL
			&& scan_type != MS_MSG_STORAGE_INVALID) {
			MS_DBG_ERR("Invalid request[%d]", scan_type);
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = ms_connect_db(&handle, scan_data->uid);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		storage_type = ms_get_storage_type_by_full(scan_data->msg, scan_data->uid);
		update_path = strndup(scan_data->msg, scan_data->msg_size);

		if (strlen(scan_data->storage_id) > 0)
			storage_id = strdup(scan_data->storage_id);
		else
			storage_id = strdup("media");

		ms_set_storage_scan_status(handle, storage_id, MEDIA_SCAN_PROCESSING, scan_data->uid);

		/*start db updating */
		__msc_set_db_status(MS_DB_UPDATING, storage_type);

		valid_status = (scan_type == MS_MSG_STORAGE_PARTIAL) ? TRUE : FALSE;
		__msc_bacth_commit_enable(handle, TRUE, valid_status, MS_NOTI_SWITCH_OFF, 0);

#ifdef FMS_PERF
		ms_check_start_time(&g_mmc_start_time);
#endif

		if (scan_type == MS_MSG_STORAGE_PARTIAL && storage_type == MS_STORAGE_INTERNAL) {
			ms_validaty_change_all_items(handle, storage_id, storage_type, true, scan_data->uid);

			/* find and compare modified time */
			ret = __msc_compare_with_db(handle, storage_id, update_path, scan_data->msg_type, &dir_array);
			if (ret != MS_MEDIA_ERR_NONE) {
				MS_DBG_ERR("__msc_compare_with_db is falied");
				goto NEXT;
			}

			if (dir_array->len != 0) {
				MS_DBG_INFO("DB UPDATING IS NEEDED");

				ret = _msc_db_update_partial(handle, storage_id, storage_type, dir_array, scan_data->uid);
			} else {
				MS_DBG_INFO("THERE IS NO UPDATE");
			}
		} else {
			if (scan_type == MS_MSG_STORAGE_ALL) {
				/* Delete all data before full scanning */
				if (!ms_delete_all_items(handle, storage_id, storage_type, scan_data->uid)) {
					MS_DBG_ERR("msc_delete_all_record fails");
				}

				if (storage_type == MS_STORAGE_EXTERNAL) {
					/*storage info updated in media-server*/
					/*ms_update_mmc_info();*/
				}
			} else if (scan_type == MS_MSG_STORAGE_PARTIAL) {
				ms_validaty_change_all_items(handle, storage_id, storage_type, false, scan_data->uid);
			}

			ret = __msc_db_update(handle, storage_id, scan_data);
		}

		/*call for bundle commit*/
		__msc_bacth_commit_disable(handle, TRUE, valid_status, scan_data->msg, scan_data->uid);

		if (scan_type == MS_MSG_STORAGE_PARTIAL && ret == MS_MEDIA_ERR_NONE) {
			int del_count = 0;

			/*check delete count*/
			MS_DBG_INFO("update path : %s", update_path);
			ms_count_delete_items_in_folder(handle, storage_id, update_path, &del_count);

			/*if there is no delete content, do not call delete API*/
			if (del_count != 0) {
				MS_DBG_ERR("storage thread delete count [%d]", del_count);
				ms_delete_invalid_items(handle, storage_id, storage_type, scan_data->uid);
			}
		}

		/* send notification */
		ms_send_dir_update_noti(handle, storage_id, update_path, NULL, MS_ITEM_UPDATE, scan_data->pid);

#ifdef FMS_PERF
		ms_check_end_time(&g_mmc_end_time);
		ms_check_time_diff(&g_mmc_start_time, &g_mmc_end_time);
#endif

		if (ret == MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			ms_set_storage_scan_status(handle, storage_id, MEDIA_SCAN_STOP, scan_data->uid);
			__msc_set_storage_scan_status(MS_STORAGE_SCAN_STOP);
			/*set vconf key mmc loading for indicator */
			__msc_set_db_status(MS_DB_STOPPED, storage_type);
		} else {
			ms_set_storage_scan_status(handle, storage_id, MEDIA_SCAN_COMPLETE, scan_data->uid);
			__msc_set_storage_scan_status(MS_STORAGE_SCAN_COMPLETE);
			/*set vconf key mmc loading for indicator */
			__msc_set_db_status(MS_DB_UPDATED, storage_type);
		}

NEXT:

		MS_SAFE_FREE(update_path);
		MS_SAFE_FREE(storage_id);

		if (power_off) {
			MS_DBG_ERR("[No-Error] power off");
			goto _POWEROFF;
		}

		/*disconnect from media db*/
		if (handle) ms_disconnect_db(&handle);

		/*Active flush */
		malloc_trim(0);

		msc_send_result(ret, scan_data);

		MS_SAFE_FREE(scan_data);

		MS_DBG_ERR("STORAGE SCAN END[%d]", ret);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (handle) ms_disconnect_db(&handle);

	return false;
}

static void __msc_insert_register_request(GArray *register_array, ms_comm_msg_s *insert_data)
{
	MS_DBG_SLOG("path : %s", insert_data->msg);

	if (insert_data->pid == POWEROFF) {
		g_array_prepend_val(register_array, insert_data);
	} else if (insert_data->msg_type == MS_MSG_BURSTSHOT_INSERT) {
		g_array_prepend_val(register_array, insert_data);
	} else {
		g_array_append_val(register_array, insert_data);
	}
}

static bool __msc_is_valid_path(const char *path, uid_t uid)
{
	bool ret = false;
	char *usr_path = NULL;

	if (path == NULL)
		return false;

	usr_path = __msc_get_path(uid);
	if (usr_path == NULL)
		return false;

	if (strncmp(path, usr_path, strlen(usr_path)) == 0) {
		ret = true;
	} else if (MS_STRING_VALID(MEDIA_ROOT_PATH_SDCARD) && (strncmp(path, MEDIA_ROOT_PATH_SDCARD, strlen(MEDIA_ROOT_PATH_SDCARD)) == 0)) {
		ret = true;
	} else if (MS_STRING_VALID(MEDIA_ROOT_PATH_USB) && (strncmp(path, MEDIA_ROOT_PATH_USB, strlen(MEDIA_ROOT_PATH_USB)) == 0)) {
		ret = true;
	} else {
		ret = false;
	}

	MS_SAFE_FREE(usr_path);

	return ret;
}

static int __msc_check_file_path(const char *file_path, uid_t uid)
{
	int exist;
	struct stat file_st;

	/* check location of file */
	/* file must exists under "/opt/usr/media" or "/opt/storage/sdcard" */
	if (!__msc_is_valid_path(file_path, uid)) {
		MS_DBG_ERR("Invalid path : %s", file_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	/* check the file exits actually */
	exist = open(file_path, O_RDONLY);
	if (exist < 0) {
		MS_DBG_STRERROR("Open failed");
		MS_DBG_ERR("error path [%s]", file_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}
	close(exist);

	/* check type of the path */
	/* It must be a regular file */
	memset(&file_st, 0, sizeof(struct stat));
	if (stat(file_path, &file_st) == 0) {
		if (!S_ISREG(file_st.st_mode)) {
			/* In this case, it is not a regula file */
			MS_DBG_ERR("this path is not a file");
			return MS_MEDIA_ERR_INVALID_PATH;
		}
	} else {
		MS_DBG_STRERROR("stat failed");
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	return MS_MEDIA_ERR_NONE;
}

static bool __msc_check_folder_path(const char *folder_path)
{
	DIR *dp = NULL;

	dp = opendir(folder_path);
	if (dp == NULL) {
		MS_DBG_ERR("Deleted folder path");
		return false;
	}
	closedir(dp);

	return true;
}

static int __msc_clear_file_list(GArray *path_array)
{
	if (path_array) {
		while (path_array->len != 0) {
			char *data = NULL;
			data = g_array_index(path_array , char*, 0);
			g_array_remove_index(path_array, 0);
			MS_SAFE_FREE(data);
		}
		g_array_free(path_array, FALSE);
		path_array = NULL;
	}

	return MS_MEDIA_ERR_NONE;
}

static int __msc_check_ignore_dir(const char *full_path, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *dir_path = NULL;
	char *leaf_path = NULL;
	char *usr_path = NULL;

	ret = __msc_check_file_path(full_path, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("invalid path : %s", full_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	dir_path = g_path_get_dirname(full_path);
	if (strcmp(dir_path, ".") == 0) {
		MS_DBG_ERR("getting directory path is failed : %s", full_path);
		MS_SAFE_FREE(dir_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	usr_path = __msc_get_path(uid);
	if (usr_path == NULL) {
		MS_DBG_ERR("__msc_get_path() fail");
		MS_SAFE_FREE(dir_path);
		return MS_MEDIA_ERR_INTERNAL;
	}

	while (1) {
		if (__msc_check_scan_ignore(dir_path)) {
			ret = MS_MEDIA_ERR_INVALID_PATH;
			break;
		}

		/*If root path, Stop Scanning*/
		if (strcmp(dir_path, usr_path) == 0)
			break;
		else if (MS_STRING_VALID(MEDIA_ROOT_PATH_SDCARD) && (strcmp(dir_path, MEDIA_ROOT_PATH_SDCARD) == 0))
			break;
		else if (MS_STRING_VALID(MEDIA_ROOT_PATH_USB) && (strcmp(dir_path, MEDIA_ROOT_PATH_USB) == 0))
			break;

		leaf_path = strrchr(dir_path, '/');
		if (leaf_path != NULL) {
				int seek_len = leaf_path -dir_path;
				dir_path[seek_len] = '\0';
		} else {
			MS_DBG_ERR("Fail to find leaf path");
			ret = MS_MEDIA_ERR_INVALID_PATH;
			break;
		}
	}

	MS_SAFE_FREE(dir_path);
	MS_SAFE_FREE(usr_path);

	return ret;
}

static int __msc_make_file_list(char *file_path, GArray **path_array, uid_t uid)
{
	FILE *fp = NULL;
	char buf[MS_FILE_PATH_LEN_MAX] = {0,};
	char *path = NULL;
	int length;
	int res = MS_MEDIA_ERR_NONE;
	int ret = MS_MEDIA_ERR_NONE;

	/* load the file list from file */
	fp = fopen(file_path, "rt");
	if (fp == NULL) {
		MS_DBG_STRERROR("fopen failed");
		res = MS_MEDIA_ERR_FILE_OPEN_FAIL;
		goto FREE_RESOURCE;
	}

	memset(buf, 0x0, MS_FILE_PATH_LEN_MAX);
	/* This is an array for storing the path of insert datas*/
	*path_array = g_array_new(FALSE, FALSE, sizeof(char *));
	if (*path_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		res = MS_MEDIA_ERR_OUT_OF_MEMORY;
		goto FREE_RESOURCE;
	}

	/* read registering file path from stored file */
	while (fgets(buf, MS_FILE_PATH_LEN_MAX, fp) != NULL) {
		length = strlen(buf); /*the return value of function, strlen(), includes "\n" */
		path = strndup(buf, length - 1); /*copying except "\n" and strndup fuction adds "\0" at the end of the copying string */

		/* check valid path */
		ret = __msc_check_ignore_dir(path, uid);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("invalide path : %s", path);
			MS_SAFE_FREE(path);
			continue;
		}
		/* insert getted path to the list */
		if (g_array_append_val(*path_array, path) == NULL) {
			MS_DBG_ERR("g_array_append_val failed");
			res = MS_MEDIA_ERR_OUT_OF_MEMORY;
			goto FREE_RESOURCE;
		}
	}

	if (fp) fclose(fp);
	fp = NULL;

	return MS_MEDIA_ERR_NONE;

FREE_RESOURCE:

	__msc_clear_file_list(*path_array);

	if (fp) fclose(fp);
	fp = NULL;

	return res;
}

static int __msc_batch_insert(ms_msg_type_e current_msg, int pid, GArray *path_array, uid_t uid)
{
	int err;
	unsigned int i;
	void **handle = NULL;
	char *insert_path = NULL;
	int (*insert_function)(void **, const char*, const char*, uid_t) = NULL;
	char storage_id[MS_UUID_SIZE] = {0,};

	insert_function = (current_msg == MS_MSG_BULK_INSERT) ? ms_insert_item_batch : ms_insert_burst_item;

	/* connect to media db, if conneting is failed, db updating is stopped */
	err = ms_connect_db(&handle, uid);
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;

	/*start db updating */
	/*call for bundle commit*/
	__msc_bacth_commit_enable(handle, TRUE, FALSE, MS_NOTI_SWITCH_ON, pid);

	MS_DBG_ERR("BULK REGISTER START[%d]", pid);

	/* get the inserting file path from array and insert to db */
	for (i = 0; i < path_array->len; i++) {

		insert_path = g_array_index(path_array, char*, i);

		/* get storage list */
		memset(storage_id, 0x0, MS_UUID_SIZE);
		if (ms_get_storage_id(handle, insert_path, storage_id) < 0) {
			MS_DBG_ERR("There is no storage id in media db");
			continue;
		}

		/* insert to db */
		err = insert_function(handle, storage_id, insert_path, uid);

		if (power_off) {
			MS_DBG_ERR("power off");
			/*call for bundle commit*/
			ms_register_end(handle, NULL, uid);
			break;
		}
	}

	/*call for bundle commit*/
	__msc_bacth_commit_disable(handle, TRUE, FALSE, NULL, uid);

	/*disconnect form media db*/
	if (handle) ms_disconnect_db(&handle);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_pop_register_request(GArray *register_array, ms_comm_msg_s **register_data)
{
	int remain_request;
	ms_comm_msg_s *insert_data = NULL;

	while (1) {
		remain_request = g_async_queue_length(reg_queue);

		/*updating requests remain*/
		if (register_array->len != 0 && remain_request == 0) {
			*register_data = g_array_index(register_array, ms_comm_msg_s*, 0);
			g_array_remove_index(register_array, 0);
			break;
		} else if (remain_request != 0) {
			insert_data = g_async_queue_pop(reg_queue);
			__msc_insert_register_request(register_array, insert_data);
			continue;
		} else if (register_array->len == 0 && remain_request == 0) {
		/*Threre is no request, Wait until pushung new request*/
			insert_data = g_async_queue_pop(reg_queue);
			__msc_insert_register_request(register_array, insert_data);
			continue;
		}
	}

	if (((*register_data)->msg_size <= 0) || ((*register_data)->msg_size > MS_FILE_PATH_LEN_MAX)) {
		MS_DBG_ERR("message size[%d] is wrong", (*register_data)->msg_size);
		return MS_MEDIA_ERR_INVALID_IPC_MESSAGE;
	}

	return MS_MEDIA_ERR_NONE;

}

gboolean msc_register_thread(void *data)
{
	ms_comm_msg_s *register_data = NULL;
	GArray *register_array = NULL;
	GArray *path_array = NULL;
	char *file_path = NULL;
	int ret;
	int pid = 0;
	ms_msg_type_e current_msg = MS_MSG_MAX;

	/*create array for processing overlay data*/
	register_array = g_array_new(FALSE, FALSE, sizeof(ms_comm_msg_s *));
	if (register_array == NULL) {
		MS_DBG_ERR("g_array_new error");
		return false;
	}

	while (1) {
		ret = __msc_pop_register_request(register_array, &register_data);
		if (register_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("__msc_pop_register_request failed [%d]", ret);
			goto FREE_RESOURCE;
		}

		/* check current request */
		current_msg = register_data->msg_type;
		pid = register_data->pid;

		if ((current_msg != MS_MSG_BULK_INSERT) &&
			(current_msg != MS_MSG_BURSTSHOT_INSERT)) {
			MS_DBG_ERR("wrong message type");
			goto FREE_RESOURCE;
		}

		file_path = strndup(register_data->msg, register_data->msg_size);
		if (file_path == NULL) {
			MS_DBG_ERR("file_path is NULL");
			goto FREE_RESOURCE;
		}

		ret = __msc_make_file_list(file_path, &path_array, register_data->uid);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("__msc_make_file_list failed [%d]", ret);
			goto FREE_RESOURCE;
		}

		ret = __msc_batch_insert(current_msg, pid, path_array, register_data->uid);

FREE_RESOURCE:
		/*Active flush */
		malloc_trim(0);

		/* If register_files operation is stopped, there is no necessrty for sending result. */
		msc_send_result(ret, register_data);

		MS_DBG_ERR("BULK REGISTER END [%d |%s]", ret, register_data->msg);

		__msc_clear_file_list(path_array);

		MS_SAFE_FREE(file_path);
		MS_SAFE_FREE(register_data);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(file_path);
	MS_SAFE_FREE(register_data);
	if (register_array) {
		while (register_array->len != 0) {
			ms_comm_msg_s *data = NULL;
			data = g_array_index(register_array , ms_comm_msg_s*, 0);
			g_array_remove_index(register_array, 0);
			MS_SAFE_FREE(data);
		}
		g_array_free(register_array, FALSE);
		register_array = NULL;
	}

	__msc_clear_file_list(path_array);

	return false;
}

static void __msc_bacth_commit_enable(void* handle, bool ins_status, bool valid_status, bool noti_enable, int pid)
{
	/*call for bundle commit*/
	if (ins_status) ms_register_start(handle, noti_enable, pid);
	if (valid_status) ms_validate_start(handle);

	return;
}

static void __msc_bacth_commit_disable(void* handle, bool ins_status, bool valid_status, const char *path, uid_t uid)
{
	/*call for bundle commit*/
	if (ins_status) ms_register_end(handle, path, uid);
	if (valid_status) ms_validate_end(handle, uid);

	return;
}

int msc_set_cancel_path(const char *cancel_path)
{
	if (g_cancel_path != NULL) {
		MS_DBG_ERR("g_cancel_path is not NULL");
		free(g_cancel_path);
		g_cancel_path = NULL;
	}

	g_cancel_path = strdup(cancel_path);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_dir_scan_meta_update(void **handle, const char*start_path, ms_storage_type_t storage_type, uid_t uid)
{
	DIR *dp = NULL;
	GArray *dir_array = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int (*scan_function)(void **, const char*, uid_t) = ms_update_meta_batch;

	/* make new array for storing directory */
	dir_array = g_array_new(FALSE, FALSE, sizeof(char*));
	if (dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	/* add first direcotiry to directory array */
	g_array_append_val(dir_array, start_path);

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = __msc_check_stop_status(storage_type);
		if (ret != MS_MEDIA_ERR_NONE) {
			goto STOP_SCAN;
		}

		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index(dir_array, 0);
//		MS_DBG_SLOG("%d", dir_array->len);

		if (__msc_check_scan_ignore(current_path)) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = __msc_check_stop_status(storage_type);
				if (ret != MS_MEDIA_ERR_NONE) {
					goto STOP_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.')
					continue;

				if (entry.d_type & DT_REG) {
					 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("ms_strappend failed");
						continue;
					}

					/* insert into media DB */
					MS_DBG_ERR("%s", path);
					if (scan_function(handle, path, uid) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("failed to update db");
						continue;
					}
				} else if (entry.d_type & DT_DIR) {
					/* this request is recursive scanning */
					 if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("ms_strappend failed");
						continue;
					}
					/* add new directory to dir_array */
					new_path = strdup(path);
					g_array_append_val(dir_array, new_path);
				}
			}
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
		}

		if (dp) closedir(dp);
		dp = NULL;
		MS_SAFE_FREE(current_path);
	}		/*db update while */
STOP_SCAN:
	if (dp) closedir(dp);

	__msc_clear_file_list(dir_array);

	if (ret != MS_MEDIA_ERR_NONE) MS_DBG_INFO("ret : %d", ret);

	return ret;
}


gboolean msc_metadata_update(void *data)
{
	ms_comm_msg_s *scan_data = data;
	int ret = MS_MEDIA_ERR_NONE;
	void **handle = NULL;
	char *start_path = NULL;
	ms_storage_type_t storage_type = MS_STORAGE_INTERNAL;

	MS_DBG_INFO("META UPDATE START");

	/*connect to media db, if conneting is failed, db updating is stopped*/
	ret = ms_connect_db(&handle, scan_data->uid);
	if (ret != MS_MEDIA_ERR_NONE)
		return false;

	/*call for bundle commit*/
	ms_update_start(handle);

	/*insert data into media db */
	char *usr_path = __msc_get_path(scan_data->uid);
	if (usr_path == NULL) {
		MS_DBG_ERR("__msc_get_path() fail");
		if (handle) ms_disconnect_db(&handle);
		return MS_MEDIA_ERR_INTERNAL;
	}

	start_path = strdup(usr_path);
	ret = __msc_dir_scan_meta_update(handle, start_path, storage_type, scan_data->uid);
	MS_SAFE_FREE(usr_path);

	/* send notification */
	ms_send_dir_update_noti(handle, INTERNAL_STORAGE_ID, start_path, NULL, MS_ITEM_UPDATE, scan_data->pid);

	if (mmc_state == MS_STG_INSERTED) {
		storage_type = MS_STORAGE_EXTERNAL;
		if (MS_STRING_VALID(MEDIA_ROOT_PATH_SDCARD)) {
			start_path = strdup(MEDIA_ROOT_PATH_SDCARD);
			if (MS_STRING_VALID(start_path)) {
				ret = __msc_dir_scan_meta_update(handle, start_path, storage_type, scan_data->uid);
				/* send notification */
				ms_send_dir_update_noti(handle, MMC_STORAGE_ID, MEDIA_ROOT_PATH_SDCARD, NULL, MS_ITEM_UPDATE, scan_data->pid);
			}
		}
	}

	/*FIX ME*/
	/*__msc_dir_scan_meta_update For Each USB Storage*/

	/*call for bundle commit*/
	ms_update_end(handle, scan_data->uid);

	if (power_off) {
		MS_DBG_WARN("power off");
		goto _POWEROFF;
	}

	/*disconnect form media db*/
	if (handle) ms_disconnect_db(&handle);

	/*Active flush */
	malloc_trim(0);

	msc_send_result(ret, scan_data);

	MS_SAFE_FREE(scan_data);

	MS_DBG_INFO("META UPDATE END [%d]", ret);


_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (handle) ms_disconnect_db(&handle);

	return false;
}


void msc_metadata_update_thread(ms_comm_msg_s *recv_msg)
{
	 g_thread_new("update_thread", (GThreadFunc)msc_metadata_update, recv_msg);
}

static int __msc_set_storage_scan_status(ms_storage_scan_status_e status)
{
	int res = MS_MEDIA_ERR_NONE;
#if 0
	if (!ms_config_set_int(MS_SCANNER_STATUS, status)) {
		res = MS_MEDIA_ERR_VCONF_SET_FAIL;
		MS_DBG_ERR("ms_config_set_int failed");
	}
#else
	stg_scan_status = status;
#endif
	return res;
}

int msc_init_scanner(void)
{
	if (!scan_queue) scan_queue = g_async_queue_new();
	if (!reg_queue) reg_queue = g_async_queue_new();
	if (!storage_queue) storage_queue = g_async_queue_new();

	/*Init mutex variable*/
	g_mutex_init(&scan_req_mutex);

	return MS_MEDIA_ERR_NONE;
}

int msc_deinit_scanner(void)
{
	if (scan_queue) g_async_queue_unref(scan_queue);
	if (reg_queue) g_async_queue_unref(reg_queue);
	if (storage_queue) g_async_queue_unref(storage_queue);

	/*Clear db mutex variable*/
	g_mutex_clear(&scan_req_mutex);

	return MS_MEDIA_ERR_NONE;
}

int msc_set_mmc_status(ms_stg_status_e status)
{
	mmc_state = status;

	return MS_MEDIA_ERR_NONE;
}

int msc_push_scan_request(ms_scan_type_e scan_type, ms_comm_msg_s *recv_msg)
{
	int ret = MS_MEDIA_ERR_NONE;

	switch (scan_type) {
		case MS_SCAN_STORAGE:
			g_async_queue_push(storage_queue, GINT_TO_POINTER(recv_msg));
			break;
		case MS_SCAN_DIRECTORY:
			g_async_queue_push(scan_queue, GINT_TO_POINTER(recv_msg));
			break;
		case MS_SCAN_REGISTER:
			g_async_queue_push(reg_queue, GINT_TO_POINTER(recv_msg));
			break;
		default:
			MS_DBG_ERR("invalid parameter");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			break;
	}

	return ret;
}

int msc_send_power_off_request(void)
{
	ms_comm_msg_s *data;

	power_off = true;

	if (scan_queue) {
		/*notify to scannig thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		data->pid = POWEROFF;
		msc_push_scan_request(MS_SCAN_DIRECTORY, data);
	}

	if (reg_queue) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		data->pid = POWEROFF;
		msc_push_scan_request(MS_SCAN_REGISTER, data);
	}

	if (storage_queue) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		data->pid = POWEROFF;
		msc_push_scan_request(MS_SCAN_STORAGE, data);
	}

	return MS_MEDIA_ERR_NONE;
}

int msc_remove_dir_scan_request(ms_comm_msg_s *recv_msg)
{
	char *cancel_path = recv_msg->msg;
	int pid = recv_msg->pid;
	int i = 0;
	int len = g_async_queue_length(scan_queue);
	ms_comm_msg_s *msg = NULL;
	GAsyncQueue *temp_scan_queue = NULL;

	MS_DBG_ERR("scan_req_mutex is LOCKED");
	g_mutex_lock(&scan_req_mutex);

	if (len == 0) {
		MS_DBG_ERR("Request is not stacked");
		goto END_REMOVE_REQUEST;
	}

	msc_set_cancel_path(recv_msg->msg);

	temp_scan_queue = g_async_queue_new();

	for (i = 0; i < len; i++) {
		/*create new queue to compare request*/
		msg = g_async_queue_pop(scan_queue);
		if ((strcmp(msg->msg, cancel_path) == 0) && (pid == msg->pid)) {
			MS_SAFE_FREE(msg);
		} else {
			g_async_queue_push(temp_scan_queue, GINT_TO_POINTER(msg));
		}
	}
	g_async_queue_unref(scan_queue);
	scan_queue = temp_scan_queue;

END_REMOVE_REQUEST:
	g_mutex_unlock(&scan_req_mutex);
	MS_DBG_ERR("scan_req_mutex is UNLOCKED");

	return MS_MEDIA_ERR_NONE;
}


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

#define _GNU_SOURCE
#include <dirent.h>	/* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <grp.h>
#include <pwd.h>

#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <dd-display.h>
#include <vconf.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-common-external-storage.h"
#include "media-common-db-svc.h"
#include "media-scanner-dbg-v2.h"
#include "media-scanner-common-v2.h"
#include "media-scanner-socket-v2.h"
#include "media-scanner-scan-v2.h"
#include "media-scanner-extract-v2.h"
#define MAX_SCAN_COUNT 300

#define DIR_SCAN_NON_SCAN		0
#define DIR_SCAN_RECURSIVE		1
#define DIR_SCAN_NON_RECURSIVE 2

GAsyncQueue *storage_queue2;
GAsyncQueue *scan_queue2;
GAsyncQueue *reg_queue2;
GMutex scan_req_mutex2;
GMutex blocked_mutex2;
GMutex storage_scan_mutex2;
GMutex dir_scan_mutex2;

char *g_cancel_path2;
char *g_blocked_path2;

int insert_count_for_partial = 0;
int set_count_for_partial = 0;
int g_directory_scan_processing2 = DIR_SCAN_NON_SCAN;
char *g_storage_scan_path;
char* g_dir_scan_path;

typedef int (*DIR_SCAN_CB)();
DIR_SCAN_CB g_dir_scan_cb;

#ifdef FMS_PERF
extern struct timeval g_mmc_start_time;
extern struct timeval g_mmc_end_time;
#endif

static int __msc_set_power_mode(ms_db_status_type_t status);
static int __msc_set_db_status(ms_db_status_type_t status, ms_storage_type_t storage_type);
static int __msc_check_stop_status(int scan_type, ms_storage_type_t storage_type, const char *start_path);
static int __msc_db_update(void **handle, const char *storage_id, const ms_comm_msg_s * scan_data);
static int __msc_check_file_path(const char *file_path, uid_t uid);
static int __msc_make_file_list(char *file_path, GArray **path_array, uid_t uid);
static int __msc_batch_insert(ms_msg_type_e current_msg, int pid, GArray *path_array, uid_t uid);
static int __msc_pop_register_request(GArray *register_array, ms_comm_msg_s **register_data);
static int __msc_clear_file_list(GArray *path_array);
static int __msc_check_scan_ignore(char * path);
static bool __msc_is_valid_path(const char *path, uid_t uid);
static void __msc_trim_dir_path(char *dir_path);
static void __msc_insert_register_request(GArray *register_array, ms_comm_msg_s *insert_data);
static void __msc_bacth_commit_enable(void* handle, bool ins_status, bool valid_status, bool noti_enable, int pid);
static void __msc_bacth_commit_disable(void* handle, bool ins_status, bool valid_status, const char *path, int result, uid_t uid);
static int __msc_dir_scan_for_folder(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, int pid, uid_t uid);
static int __msc_dir_scan_for_storage(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, int pid, uid_t uid);
static bool __msc_storage_mount_status(const char* start_path);
static char* __msc_get_path(uid_t uid);

static int __msc_get_null_scan_folder_list(void **handle, const char *stroage_id, char *path, GArray *dir_array);
static int __msc_dir_scan_cb();
static int __msc_check_scan_same_path(char *scan_path);
static int __msc_set_storage_scan_cur_path(char *scan_path);
static int __msc_set_dir_scan_cur_path(char *scan_path);
static int __msc_dir_and_storage_scan_same_path(char *start_path);

static int __msc_set_dir_scan_cb(DIR_SCAN_CB cb)
{
	g_dir_scan_cb = cb;

	return MS_MEDIA_ERR_NONE;
}

static int __msc_call_dir_scan_cb()
{
	if (g_dir_scan_cb != NULL) {
		g_dir_scan_cb();

		return MS_MEDIA_ERR_NONE;
	}

	return MS_MEDIA_ERR_INTERNAL;
}

static int __msc_remove_dir_scan_cb()
{
	g_dir_scan_cb = NULL;

	return MS_MEDIA_ERR_NONE;
}

static char* __msc_get_path(uid_t uid)
{
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
		err = display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
		if (err != 0)
			res = MS_MEDIA_ERR_INTERNAL;
		break;
	case MS_DB_UPDATED:
		err = display_unlock_state(LCD_OFF, PM_RESET_TIMER);
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
	} else {
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
		MS_DBG_ERR("__msc_tv_set_power_mode fail");
		res = err;
	}

	return res;
}

static int __msc_check_scan_ignore(char * path)
{
	int fd = -1;
	int exist = -1;
	const char *ignore_path = "/.scan_ignore";
	char *check_ignore_file = NULL;
	int ret = MS_MEDIA_ERR_NONE;

	if (strstr(path, "/.")) {
		MS_DBG_ERR("hidden path");
		ret = MS_MEDIA_ERR_INVALID_PATH;
		goto ERROR;
	}

	fd = open(path, O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		MS_DBG_ERR("%s folder opendir fails", path);
		ret = MS_MEDIA_ERR_INVALID_PATH;

		if (!MS_STRING_VALID(MEDIA_ROOT_PATH_USB)) {
			MS_DBG_ERR("Fail to get USB path");
			goto ERROR;
		}

		if (strstr(path, MEDIA_ROOT_PATH_USB) != NULL) {
			if (errno == ENOENT) {
				/*if the directory does not exist, check the device is unmounted*/
				if (!__msc_storage_mount_status(path)) {
					MS_DBG_ERR("Device is unmounted[%s]", path);
					ret = MS_MEDIA_ERR_USB_UNMOUNTED;
					goto ERROR;
				}
			}
		}

		struct stat folder_st;
		if (stat(path, &folder_st) == 0) {
			MS_DBG_ERR("DEV[%ld] INODE[%lld] UID[%ld] GID[%ld] MODE[%lo] PATH[%s]", (long)folder_st.st_dev, (long long)folder_st.st_ino,
				(long)folder_st.st_uid, (long)folder_st.st_gid, (unsigned long) folder_st.st_mode, path);
		} else {
			MS_DBG_ERR("%s folder stat fails", path);
		}

		goto ERROR;
	} else {
		/* check the file exits actually */
		int path_len = 0;

		path_len = strlen(path) + strlen(ignore_path) + 1;
		check_ignore_file = malloc(path_len);
		if (check_ignore_file != NULL) {
			memset(check_ignore_file, 0x0, path_len);
			snprintf(check_ignore_file, path_len, "%s%s", path, ignore_path);

			exist = open(check_ignore_file, O_RDONLY);
			if (exist >= 0) {
				MS_DBG_ERR("scan_ignore exists [%s]", check_ignore_file);
				ret = MS_MEDIA_ERR_INVALID_PATH;
			}

			MS_SAFE_FREE(check_ignore_file);
		} else {
			MS_DBG_ERR("malloc failed");
			ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
	}

ERROR:

	if (fd != -1) {
		close(fd);
		fd = -1;
	}

	if (exist >= 0) close(exist);

	return ret;
}

GCond data_cond2;	/* Must be initialized somewhere */
GMutex data_mutex2;	/* Must be initialized somewhere */
gpointer current_data2 = NULL;

int msc_init_scan_thread()
{
	g_mutex_init(&data_mutex2);
	g_mutex_init(&blocked_mutex2);
	g_mutex_init(&scan_req_mutex2);
	g_mutex_init(&storage_scan_mutex2);

	g_mutex_init(&dir_scan_mutex2);

	g_cond_init(&data_cond2);

	return MS_MEDIA_ERR_NONE;
}

int msc_deinit_scan_thread()
{
	g_mutex_clear(&data_mutex2);
	g_mutex_clear(&blocked_mutex2);
	g_mutex_clear(&scan_req_mutex2);
	g_mutex_clear(&storage_scan_mutex2);

	g_mutex_clear(&dir_scan_mutex2);

	g_cond_clear(&data_cond2);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_resume_scan()
{
	g_mutex_lock(&data_mutex2);

	g_cond_signal(&data_cond2);

	g_mutex_unlock(&data_mutex2);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_pause_scan()
{
	g_mutex_lock(&data_mutex2);

	g_cond_wait(&data_cond2, &data_mutex2);

	g_mutex_unlock(&data_mutex2);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_check_stop_status(int scan_type, ms_storage_type_t storage_type, const char *start_path)
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
		g_mutex_lock(&scan_req_mutex2);
		/* check cancel path */
		if (g_cancel_path2 != NULL) {
			MS_DBG_ERR("check blocked storage [%s][%s]", g_cancel_path2, start_path);
			if (strncmp(g_cancel_path2, start_path, strlen(g_cancel_path2)) == 0) {
				MS_DBG_ERR("Receive cancel request [%s][%s]. STOP scan!!",
				g_cancel_path2, start_path);
				unsigned int path_len = strlen(g_cancel_path2);

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

			MS_SAFE_FREE(g_cancel_path2);
		}

		g_mutex_unlock(&scan_req_mutex2);
	}

	if (scan_type == MS_MSG_STORAGE_ALL || scan_type == MS_MSG_STORAGE_PARTIAL) {
		g_mutex_lock(&blocked_mutex2);
		/* check cancel path */
		if (g_blocked_path2 != NULL) {
			MS_DBG_ERR("check blocked storage [%s][%s]", g_blocked_path2, start_path);
			if (strncmp(start_path, g_blocked_path2, strlen(start_path)) == 0) {
				MS_DBG_ERR("Receive blocked message[%s][%s]. STOP scan!!",
				g_blocked_path2, start_path);
				ret = MS_MEDIA_ERR_SCANNER_FORCE_STOP;
			}

			MS_SAFE_FREE(g_blocked_path2);
		}

		g_mutex_unlock(&blocked_mutex2);
	}

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

#define BUF_SIZE 1024

static int __msc_dir_scan_for_folder(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, int pid, uid_t uid)
{
	GArray *dir_array = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = {0, };
	bool is_recursive = true;
	char *new_start_path = NULL;

	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int scan_count = 0;
	const char *trash = "$RECYCLE.BIN";

	insert_count_for_partial = 0;
	set_count_for_partial = 0;

	MS_DBG_ERR("storage id [%s] start path [%s]", storage_id, start_path);

	/* make new array for storing directory */
	dir_array = g_array_new(FALSE, FALSE, sizeof(char*));

	if (dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	/* add first direcotiry to directory array */
	new_start_path = strdup(start_path);
	if (new_start_path == NULL) {
		MS_DBG_ERR("strdup failed");
		g_array_free(dir_array, FALSE);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	//MS_DBG_ERR("new start path [%s]", new_start_path);
	g_array_append_val(dir_array, start_path);
	ms_insert_folder_end(handle, uid);
	if (ms_insert_folder(handle, storage_id, new_start_path, uid) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("insert folder failed");
	}

	is_recursive = (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) ? false : true;

	/* folder validity set 0 under the start_path in folder table*/
	if (ms_set_folder_validity(handle, storage_id, new_start_path, MS_INVALID, is_recursive, uid) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("set_folder_validity failed [%d] ", scan_type);
	}

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = __msc_check_stop_status(scan_type, storage_type, new_start_path);
		if (ret != MS_MEDIA_ERR_NONE) {
			if (is_recursive == true) {
				ms_change_validity_item_batch(handle, storage_id, new_start_path, 1, 2, uid);
			}
			goto STOP_SCAN;
		}
		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index(dir_array, 0);

		__msc_set_dir_scan_cur_path(current_path);

		ret = __msc_check_scan_ignore(current_path);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			if (ret == MS_MEDIA_ERR_USB_UNMOUNTED) {
				goto STOP_SCAN;
			} else {
				ret = MS_MEDIA_ERR_NONE;
			}

			continue;
		}

		ms_insert_folder_start(handle);
		ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_PROCESSING, uid);

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = __msc_check_stop_status(scan_type, storage_type, new_start_path);
				if (ret != MS_MEDIA_ERR_NONE) {
					if (is_recursive == true) {
						ms_change_validity_item_batch(handle, storage_id, new_start_path, 1, 2, uid);
					}
					goto STOP_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.' || strcmp(entry.d_name, trash) == 0)
					continue;

				if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strappend failed");
					continue;
				}

				if (entry.d_type & DT_REG) {
					/* insert into media DB */
					if (ms_scan_validate_item(handle, storage_id, path, uid, &insert_count_for_partial, &set_count_for_partial) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("failed to update db : %d\n", scan_type);
						continue;
					} else {
						++scan_count;
						//MS_DBG_ERR("insert count %d", nScanCount);
						if (scan_count >= MAX_SCAN_COUNT) {
							scan_count = 0;
							//MS_DBG_ERR("storage_id = [%s]", storage_id);
							msc_insert_exactor_request(scan_type, FALSE, storage_id, current_path, 0, uid);
						}

						if (insert_count_for_partial >= MAX_SCAN_COUNT) {
							if (pid != 0) {
								msc_send_result_partial(ret, MS_MSG_SCANNER_PARTIAL, pid, (char*)start_path);
							}
							insert_count_for_partial = 0;
						}

						if (set_count_for_partial >= MAX_SCAN_COUNT) {
							if (pid != 0) {
								msc_send_result_partial(ret, MS_MSG_SCANNER_PARTIAL, pid, (char*)start_path);
							}
							set_count_for_partial = 0;
						}
					}
				} else if (entry.d_type & DT_DIR) {
					if  (scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
						/* this request is recursive scanning */
						/* add new directory to dir_array */
						new_path = strdup(path);
						g_array_append_val(dir_array, new_path);

						if (ms_insert_folder(handle, storage_id, new_path, uid) != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("insert folder failed");
						}
					} else {
						/* this request is non-recursive scanning */
						/* don't add new directory to dir_array */
						if (ms_insert_folder(handle, storage_id, path, uid) != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("insert folder failed");
							continue;
						}
					}
				}
			}
			/*update modifiec type for the internal storage*/
			if (scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE && storage_type == MS_STORAGE_INTERNAL)
				ms_update_folder_time(handle, INTERNAL_STORAGE_ID, current_path, uid);
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
			ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_STOP, uid);
		}

		ms_insert_folder_end(handle, uid);
		ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_DONE, uid);

		if (dp) closedir(dp);
		dp = NULL;

		MS_SAFE_FREE(current_path);
		__msc_set_dir_scan_cur_path(NULL);
	}

STOP_SCAN:
	if (dp) closedir(dp);
	MS_SAFE_FREE(new_start_path);
	__msc_set_dir_scan_cur_path(NULL);

	__msc_clear_file_list(dir_array);

	if (ret != MS_MEDIA_ERR_NONE) MS_DBG_INFO("ret : %d", ret);

	return ret;
}

static int __msc_dir_scan_for_storage(void **handle, const char *storage_id, const char*start_path, ms_storage_type_t storage_type, int scan_type, int pid, uid_t uid)
{
	GArray *dir_array = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int (*scan_function)(void **, const char*, const char*, uid_t, int *, int *) = NULL;
	char *new_start_path = NULL;

	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result = NULL;
	int scan_count = 0;
	const char *trash = "$RECYCLE.BIN";

	bool is_missing = false;
	ms_dir_scan_status_e scan_status = MS_DIR_SCAN_NONE;

	MS_DBG_ERR("storage id [%s] start path [%s]", storage_id, start_path);

	/* make new array for storing directory */
	dir_array = g_array_new(FALSE, FALSE, sizeof(char*));

	if (dir_array == NULL) {
		MS_DBG_ERR("g_array_new failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	/* add first direcotiry to directory array */
	new_start_path = strdup(start_path);
	if (new_start_path == NULL) {
		MS_DBG_ERR("strdup failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_array_append_val(dir_array, start_path);
	ms_insert_folder_end(handle, uid);
	if (ms_insert_folder(handle, storage_id, new_start_path, uid) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("insert folder failed");
	}

	scan_function = (scan_type == MS_MSG_STORAGE_ALL) ? ms_scan_item_batch : ms_scan_validate_item;

	/*start db update. the number of element in the array , db update is complete.*/
	while (dir_array->len != 0) {
		/*check poweroff status*/
		ret = __msc_check_stop_status(scan_type, storage_type, new_start_path);
		if (ret != MS_MEDIA_ERR_NONE) {
			goto STOP_SCAN;
		}
		/* get the current path from directory array */
		current_path = g_array_index(dir_array , char*, 0);
		g_array_remove_index(dir_array, 0);

		__msc_set_storage_scan_cur_path(current_path);

		ret = __msc_check_scan_ignore(current_path);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			if (ret == MS_MEDIA_ERR_USB_UNMOUNTED) {
				goto STOP_SCAN;
			} else {
				ret = MS_MEDIA_ERR_NONE;
			}

			continue;
		}

		ms_get_folder_scan_status(handle, storage_id, current_path, (int*)&scan_status);

		if (scan_status == MS_DIR_SCAN_PROCESSING && g_directory_scan_processing2 != DIR_SCAN_NON_SCAN) {

			MS_DBG_ERR("%s is scanning, ignore current path, scan next path !!", current_path);

			is_missing = true;
			goto NEXT_SCAN;

		} else if (scan_status == MS_DIR_SCAN_DONE) {
			if (g_directory_scan_processing2 == DIR_SCAN_NON_RECURSIVE) {
				MS_DBG_ERR("%s is scan done, ignore current path, get subfolder list!!", current_path);

				__msc_get_null_scan_folder_list(handle, storage_id, current_path, dir_array);

				continue;

			} else if (g_directory_scan_processing2 == DIR_SCAN_RECURSIVE) {
				MS_DBG_ERR("%s is scan done, ignore current path, scan next path!!", current_path);

				is_missing = true;
				goto NEXT_SCAN;
			}
		}

		ms_insert_folder_start(handle);
		ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_PROCESSING, uid);

		dp = opendir(current_path);
		if (dp != NULL) {
			while (!readdir_r(dp, &entry, &result)) {
				/*check poweroff status*/
				ret = __msc_check_stop_status(scan_type, storage_type, new_start_path);
				if (ret != MS_MEDIA_ERR_NONE) {
					goto STOP_SCAN;
				}

				if (result == NULL)
					break;

				if (entry.d_name[0] == '.' || strcmp(entry.d_name, trash) == 0)
					continue;

				if (ms_strappend(path, sizeof(path), "%s/%s", current_path, entry.d_name) != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strappend failed");
					continue;
				}

				if (entry.d_type & DT_REG) {
					/* insert into media DB */
					if (scan_function(handle, storage_id, path, uid, &insert_count_for_partial, &set_count_for_partial) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("failed to update db : %d\n", scan_type);
						continue;
					} else {
						++scan_count;
						//MS_DBG_ERR("insert count %d", nScanCount);
						if (scan_count >= MAX_SCAN_COUNT) {
							scan_count = 0;
							//MS_DBG_ERR("storage_id = [%s]", storage_id);
							msc_insert_exactor_request(scan_type, FALSE, storage_id, current_path, 0, uid);
						}
					}
				} else if (entry.d_type & DT_DIR) {
					/* this request is recursive scanning */
					/* add new directory to dir_array */
					new_path = strdup(path);
					g_array_append_val(dir_array, new_path);

					if (ms_insert_folder(handle, storage_id, new_path, uid) != MS_MEDIA_ERR_NONE) {
						MS_DBG_ERR("insert folder failed");
					}
				}
			}
		} else {
			MS_DBG_ERR("%s folder opendir fails", current_path);
			ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_STOP, uid);
		}

		ms_insert_folder_end(handle, uid);
		ms_set_folder_scan_status(handle, storage_id, current_path, MS_DIR_SCAN_DONE, uid);

		if (dp) closedir(dp);
		dp = NULL;

NEXT_SCAN:
		MS_SAFE_FREE(current_path);
		__msc_set_storage_scan_cur_path(NULL);

		if (dir_array->len == 0 && is_missing) {
			is_missing = false;

			if (g_directory_scan_processing2 == DIR_SCAN_NON_SCAN) {
				ret = __msc_get_null_scan_folder_list(handle, storage_id, NULL, dir_array);

				MS_DBG_ERR("no wait folder scan callback");

				if (ret != MS_MEDIA_ERR_NONE) {
					break;
				}
			} else {
				__msc_set_dir_scan_cb(__msc_dir_scan_cb);

				__msc_pause_scan();

				MS_DBG_ERR("wait folder scan callback");

				__msc_remove_dir_scan_cb();

				ret = __msc_get_null_scan_folder_list(handle, storage_id, NULL, dir_array);

				if (ret != MS_MEDIA_ERR_NONE) {
					break;
				}
			}
		}
	}

	/*remove invalid folder in folder table.*/
	if (__msc_storage_mount_status(new_start_path)) {
		if ((g_directory_scan_processing2 != DIR_SCAN_NON_SCAN)
			&& (__msc_dir_and_storage_scan_same_path(new_start_path) == MS_MEDIA_ERR_NONE)) {
			__msc_set_dir_scan_cb(__msc_dir_scan_cb);
			MS_DBG_ERR("storage scan pause...\n");
			__msc_pause_scan();

			MS_DBG_ERR("wait folder scan callback");

			__msc_remove_dir_scan_cb();
		}
		if (ms_delete_invalid_folder(handle, storage_id, uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("delete invalid folder failed");
			ret = MS_MEDIA_ERR_DB_DELETE_FAIL;
		}
	} else {
		MS_DBG_ERR("start path is unmounted");
	}

STOP_SCAN:
	if (dp) closedir(dp);
	dp = NULL;

	MS_SAFE_FREE(new_start_path);
	__msc_set_storage_scan_cur_path(NULL);
	__msc_clear_file_list(dir_array);

	if (ret != MS_MEDIA_ERR_NONE) MS_DBG_INFO("ret : %d", ret);

	return ret;
}

int __msc_get_null_scan_folder_list(void **handle, const char *stroage_id, char *path, GArray *dir_array)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *new_path = NULL;
	char *current_path = NULL;
	GArray *cur_dir_array = NULL;

	ret = ms_get_null_scan_folder_list(handle, stroage_id, path, &cur_dir_array);

	if (ret == MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("cur_dir_array->len = [%d]", cur_dir_array->len);
		while (cur_dir_array->len != 0) {
			current_path = g_array_index(cur_dir_array , char*, 0);
			g_array_remove_index(cur_dir_array, 0);
			MS_DBG("current_path = [%s]", current_path);

			new_path = strdup(current_path);
			g_array_append_val(dir_array, new_path);
			MS_SAFE_FREE(current_path);
		}
	}

	__msc_clear_file_list(cur_dir_array);

	return ret;
}

int __msc_check_scan_same_path(char *scan_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	unsigned int storage_len, folder_len, min_len;

	g_mutex_lock(&storage_scan_mutex2);

	if (NULL == scan_path || NULL == g_storage_scan_path) {
		g_mutex_unlock(&storage_scan_mutex2);

		return MS_MEDIA_ERR_INTERNAL;
	}

	storage_len = strlen(g_storage_scan_path);
	folder_len = strlen(scan_path);
	min_len = storage_len > folder_len ? folder_len : storage_len;

	ret = strncmp(g_storage_scan_path, scan_path, min_len);

	g_mutex_unlock(&storage_scan_mutex2);

	return abs(ret);
}

int __msc_set_storage_scan_cur_path(char *scan_path)
{
	int ret = MS_MEDIA_ERR_NONE;

	g_mutex_lock(&storage_scan_mutex2);

	if (NULL == scan_path) {
		MS_SAFE_FREE(g_storage_scan_path);
	} else {
		MS_SAFE_FREE(g_storage_scan_path);
		g_storage_scan_path = strdup(scan_path);
	}

	g_mutex_unlock(&storage_scan_mutex2);

	return ret;
}

static int __msc_dir_and_storage_scan_same_path(char *start_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	unsigned int storage_len, folder_len;

	char *storage_path = NULL;
	char *remain_path = NULL;
	int remain_len = 0;

	remain_path = strstr(start_path+strlen(MEDIA_ROOT_PATH_USB) +1, "/");
	if (remain_path != NULL)
		remain_len = strlen(remain_path);

	storage_path = strndup(start_path, strlen(start_path) - remain_len);

	g_mutex_lock(&dir_scan_mutex2);

	if (NULL == g_dir_scan_path || NULL == storage_path) {
		MS_SAFE_FREE(storage_path);
		g_mutex_unlock(&dir_scan_mutex2);

		return MS_MEDIA_ERR_INTERNAL;
	}

	storage_len = strlen(storage_path);
	folder_len = strlen(g_dir_scan_path);

	if (folder_len < storage_len) {
		ret = -1;
	} else {
		ret = strncmp(storage_path, g_dir_scan_path, storage_len);
	}

	MS_SAFE_FREE(storage_path);
	g_mutex_unlock(&dir_scan_mutex2);

	return abs(ret);
}


int __msc_set_dir_scan_cur_path(char *scan_path)
{
	int ret = MS_MEDIA_ERR_NONE;

	g_mutex_lock(&dir_scan_mutex2);

	if (NULL == scan_path) {
		MS_SAFE_FREE(g_dir_scan_path);
	} else {
		MS_SAFE_FREE(g_dir_scan_path);
		g_dir_scan_path = strdup(scan_path);
	}

	g_mutex_unlock(&dir_scan_mutex2);

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

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_type != MS_MSG_STORAGE_INVALID) {
		MS_DBG_ERR("INSERT");
		start_path = strndup(scan_data->msg, scan_data->msg_size);
		if (scan_type == MS_MSG_DIRECTORY_SCANNING || scan_type == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			err = __msc_dir_scan_for_folder(handle, storage_id, start_path, storage_type, scan_type, scan_data->pid, scan_data->uid);
		} else if (scan_type == MS_MSG_STORAGE_ALL || scan_type == MS_MSG_STORAGE_PARTIAL) {
			err = __msc_dir_scan_for_storage(handle, storage_id, start_path, storage_type, scan_type, scan_data->pid, scan_data->uid);
		}

		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("error : %d", err);
		}
	} else if (scan_type == MS_MSG_STORAGE_INVALID) {
		MS_DBG_ERR("INVALID");
#if 1
		/*In this case, update just validation record*/
		/*update just valid type*/
		err = ms_validaty_change_all_items(handle, storage_id, storage_type, false, scan_data->uid);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("error : %d", err);
		}

		/* folder validity set 0 under the start_path in folder table*/
		if (ms_set_folder_validity(handle, storage_id, scan_data->msg, MS_INVALID, TRUE, scan_data->uid) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("set_folder_validity failed");
		}
#endif
		msc_remove_extract_request(scan_data);
	}

	sync();

	return err;
}
static int __msc_pop_new_request(ms_comm_msg_s **scan_data)
{
	*scan_data = g_async_queue_pop(scan_queue2);

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

gboolean msc_directory_scan_thread(void *data)
{
	ms_comm_msg_s *scan_data = NULL;
	int err;
	int ret = MS_MEDIA_ERR_NONE;
	void **handle = NULL;
	int scan_type;
	char *storage_id = NULL;
	bool modified = FALSE;
	int index = 0;
	ms_storage_type_t storage_type = MS_STORAGE_INTERNAL;
	ms_dir_scan_status_e scan_status = MS_DIR_SCAN_NONE;
	ms_noti_type_e noti_type = MS_ITEM_INSERT;
	int before_count = 0;
	int after_count = 0;
	int delete_count = 0;
	int delete_folder_count = 0;
	char *folder_uuid = NULL;
	bool is_recursive = true;
	bool power_off_status = FALSE;

	while (1) {
		__msc_pop_new_request(&scan_data);

		if (scan_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		MS_DBG_ERR("DIRECTORY SCAN START [%s, %d]", scan_data->msg, scan_data->msg_type);

		g_directory_scan_processing2 = DIR_SCAN_NON_RECURSIVE;

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = ms_connect_db(&handle, scan_data->uid);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		scan_type = scan_data->msg_type;
		storage_type = ms_get_storage_type_by_full(scan_data->msg, scan_data->uid);
		ms_reset_insert_count();

		storage_id = strdup(scan_data->storage_id);
		if (storage_id == NULL) {
			MS_DBG_ERR("storage_id NULL");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		MS_DBG("path [%s], storage_id [%s], scan_type [%d]", scan_data->msg, storage_id, scan_type);

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
			scan_type = MS_MSG_DIRECTORY_SCANNING;
			scan_data->msg_type = MS_MSG_DIRECTORY_SCANNING;

			/*get the UUID of deleted folder*/
			ms_get_folder_id(handle, storage_id, scan_data->msg, &folder_uuid);

			MS_DBG_ERR("[%s][%s] delete path", scan_data->msg, folder_uuid);
		}

		ms_check_subfolder_count(handle, storage_id, scan_data->msg, &before_count);
		MS_DBG_ERR("BEFORE COUNT[%d]", before_count);

		if (scan_type == MS_MSG_DIRECTORY_SCANNING) {

			MS_DBG_ERR("start recursive dir scan!!");

			g_directory_scan_processing2 = DIR_SCAN_RECURSIVE;
			err = __msc_check_scan_same_path(scan_data->msg);

			if (MS_MEDIA_ERR_NONE == err) {
				MS_DBG_ERR("[%s] is scanning recursive, waiting...", scan_data->msg);

				while (1) {
					sleep(1);

					ret = __msc_check_stop_status(scan_type, storage_type, scan_data->msg);
					if (ret != MS_MEDIA_ERR_NONE) {
						goto NEXT;
					}

					if ((++index) % 3 == 0) {
						msc_send_result_partial(ret, MS_MSG_SCANNER_PARTIAL, scan_data->pid, scan_data->msg);
					}

					err = __msc_check_scan_same_path(scan_data->msg);
					if (err != MS_MEDIA_ERR_NONE) {
						break;
					}
				}

				index = 0;
				MS_DBG_ERR("[%s] scan done, wait finished", scan_data->msg);
				goto SCAN_DONE;

			} else {
				ms_get_folder_scan_status(handle, storage_id, scan_data->msg, (int*)&scan_status);

				if (scan_status == MS_DIR_SCAN_DONE) {
					ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_VALIND_V2, MS_RECURSIVE, scan_data->uid);
				} else {
					ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_INVALID, MS_RECURSIVE, scan_data->uid);
				}
			}
		} else {
			MS_DBG_ERR("start non recursive dir scan!!");

			g_directory_scan_processing2 = DIR_SCAN_NON_RECURSIVE;
			ms_check_folder_modified(handle, scan_data->msg, storage_id, &modified);

			if (modified == FALSE) {
				MS_DBG_ERR("check [%s] has not been modified !!", scan_data->msg);
				ms_get_folder_scan_status(handle, storage_id, scan_data->msg, (int*)&scan_status);
				MS_DBG_ERR("folder scan status = [%d]", scan_status);

				if (scan_status == MS_DIR_SCAN_DONE) {
					/* do nothing */
					MS_DBG_ERR("[%s] scan done", scan_data->msg);
					err = ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_VALIND, MS_NON_RECURSIVE, scan_data->uid);
					goto SCAN_DONE;
				} else if (scan_status == MS_DIR_SCAN_PROCESSING) {
					MS_DBG_ERR("[%s] scanning, waiting...", scan_data->msg);

					while (1) {
						sleep(1);

						ret = __msc_check_stop_status(scan_type, storage_type, scan_data->msg);
						if (ret != MS_MEDIA_ERR_NONE) {
							goto NEXT;
						}

						if ((++index) % 3 == 0) {
							msc_send_result_partial(ret, MS_MSG_SCANNER_PARTIAL, scan_data->pid, scan_data->msg);
						}

						ms_get_folder_scan_status(handle, storage_id, scan_data->msg, (int*)&scan_status);
						if (scan_status == MS_DIR_SCAN_DONE) {
							break;
						}
					}

					index = 0;
					MS_DBG_ERR("[%s] scan done, wait finished", scan_data->msg);
					goto SCAN_DONE;
				} else {
					err = ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_INVALID, MS_NON_RECURSIVE, scan_data->uid);
				}
			} else {
				MS_DBG_ERR("check [%s] has been modified !!", scan_data->msg);
				err = ms_set_folder_item_validity(handle, storage_id, scan_data->msg, MS_INVALID, MS_NON_RECURSIVE, scan_data->uid);
#if 0
				ms_get_folder_scan_status(handle, storage_id, scan_data->msg, &scan_status);
				MS_DBG_ERR("folder scan status = [%d]", scan_status);

				if (scan_status == MS_DIR_SCAN_PROCESSING) {
					MS_DBG_ERR("[%s] scanning, notify storage scan to stop scan %s !!", scan_data->msg, scan_data->msg);

					//add notify storage scan to stop scan
				}
#endif
			}
		}

		if (err != MS_MEDIA_ERR_NONE)
			MS_DBG_ERR("error : %d", err);

		/*call for bundle commit*/
		__msc_bacth_commit_enable(handle, TRUE, TRUE, MS_NOTI_SWITCH_OFF, 0);

		/*insert data into media db */
		ret = __msc_db_update(handle, storage_id, scan_data);
		msc_del_cancel_path();

		/*call for bundle commit*/
		__msc_bacth_commit_disable(handle, TRUE, TRUE, scan_data->msg, ret, scan_data->uid);

		MS_DBG_ERR("folder scan done, sent cb event path = [%s]", scan_data->msg);
		__msc_call_dir_scan_cb();

		ms_check_subfolder_count(handle, storage_id, scan_data->msg, &after_count);
		MS_DBG_ERR("BEFORE COUNT[%d]", before_count);

		if (ms_count_delete_items_in_folder(handle, storage_id, scan_data->msg, &delete_count) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("counting invalid items failed");
		}

		if (ms_delete_invalid_items_in_folder(handle, storage_id, scan_data->msg, is_recursive, scan_data->uid)  != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("deleting invalid items in folder failed");
		}

		/*remove invalid folder in folder table.*/
		//if(ms_delete_invalid_folder_by_path(handle, storage_id, scan_data->msg, &delete_folder_count) != MS_MEDIA_ERR_NONE) {
		//	MS_DBG_ERR("deleting invalid folder failed");
		//}

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

SCAN_DONE:
		MS_DBG_ERR("storage_id = [%s], dir Path = [%s]", storage_id, scan_data->msg);
		msc_insert_exactor_request(scan_data->msg_type, TRUE, scan_data->storage_id, scan_data->msg, scan_data->pid, scan_data->uid);

		msc_get_power_status(&power_off_status);
		if (power_off_status) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}


NEXT:
		/*Active flush */
		malloc_trim(0);

		scan_data->msg_type = MS_MSG_SCANNER_COMPLETE;
		msc_send_result(ret, scan_data);

		MS_SAFE_FREE(scan_data);
		MS_SAFE_FREE(storage_id);
		MS_SAFE_FREE(folder_uuid);

		g_directory_scan_processing2 = DIR_SCAN_NON_SCAN;

		MS_DBG_ERR("DIRECTORY SCAN END [%d]", ret);

		/*disconnect form media db*/
		if (handle) ms_disconnect_db(&handle);
	}			/*thread while*/

_POWEROFF:
	MS_SAFE_FREE(scan_data);
	if (handle) ms_disconnect_db(&handle);

	return false;
}

/* this thread process only the request of media-server */
static int _check_folder_from_list(char *folder_path, GArray *dir_array)
{
	int i;
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
			if (mtime == dir_info->modified_time) {
				g_array_remove_index(dir_array, i);
				MS_SAFE_FREE(dir_info->dir_path);
				MS_SAFE_FREE(dir_info);
			}
			find_flag = true;
			break;
		}
	}

	/* this folder does not exist in media DB, so this folder has to insert to DB */
	if (find_flag == false) {
		dir_info = NULL;
		dir_info = malloc(sizeof(ms_dir_info_s));
		if (dir_info == NULL) {
			MS_DBG_ERR("MALLOC failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
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

	MS_DBG_FENTER();

	/*get directories list from media db*/
	ret = ms_get_folder_list(handle, storage_id, start_path, dir_array);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("msc_tv_get_folder_list is failed", ret);
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

		if (__msc_check_scan_ignore(current_path) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("%s is ignore", current_path);
			MS_SAFE_FREE(current_path);
			continue;
		}

		dp = opendir(current_path);
		if (dp != NULL) {
			/* find and compare modified time */
			_check_folder_from_list(current_path, *dir_array);

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
				}
			}
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

static int _msc_db_update_partial(void **handle, const char *storage_id, ms_storage_type_t storage_type, GArray *dir_array, int pid, uid_t uid)
{
	unsigned int i;
	int err = MS_MEDIA_ERR_NONE;
	ms_dir_info_s* dir_info = NULL;
	char *update_path = NULL;

	for (i = 0; i < dir_array->len; i++) {
		dir_info = g_array_index(dir_array, ms_dir_info_s*, i);
		update_path = strdup(dir_info->dir_path);
		if (update_path == NULL) {
			MS_DBG_ERR("malloc failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}

		if (dir_info->modified_time != -1) {
			err = ms_set_folder_item_validity(handle, storage_id, update_path, MS_INVALID, MS_NON_RECURSIVE, uid);
			if (err != MS_MEDIA_ERR_NONE) {
				MS_DBG_SLOG("update_path : %s, %d", update_path, dir_info->modified_time);
				MS_DBG_ERR("error : %d", err);
			}
		}

		__msc_dir_scan_for_folder(handle, storage_id, update_path, storage_type, MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE, pid, uid);
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

int __msc_dir_scan_cb()
{
	return __msc_resume_scan();
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
	bool power_off_status = FALSE;

	while (1) {
		scan_data = g_async_queue_pop(storage_queue2);
		if (scan_data->pid == POWEROFF) {
			MS_DBG_ERR("power off");
			goto _POWEROFF;
		}

		MS_DBG_ERR("STORAGE SCAN START [%s]", scan_data->msg);

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

		ms_set_storage_scan_status(handle, scan_data->storage_id, MEDIA_SCAN_PROCESSING, scan_data->uid);

		/*start db updating */
		__msc_set_db_status(MS_DB_UPDATING, storage_type);

		valid_status = (scan_type == MS_MSG_STORAGE_PARTIAL || MS_MSG_STORAGE_ALL == scan_type) ? TRUE : FALSE;

		if (scan_type != MS_MSG_STORAGE_INVALID)
			__msc_bacth_commit_enable(handle, TRUE, valid_status, MS_NOTI_SWITCH_OFF, 0);

		if (scan_type == MS_MSG_STORAGE_PARTIAL && storage_type == MS_STORAGE_INTERNAL) {
			ms_validaty_change_all_items(handle, scan_data->storage_id, storage_type, true, scan_data->uid);

			/* find and compare modified time */
			ret = __msc_compare_with_db(handle, scan_data->storage_id, update_path, scan_data->msg_type, &dir_array);
			if (ret != MS_MEDIA_ERR_NONE) {
				MS_DBG_WARN("__msc_compare_with_db is falied");
				goto NEXT;
			}

			if (dir_array->len != 0) {
				MS_DBG_INFO("DB UPDATING IS NEEDED");
				ret = _msc_db_update_partial(handle, scan_data->storage_id, storage_type, dir_array, scan_data->pid, scan_data->uid);
			} else {
				MS_DBG_INFO("THERE IS NO UPDATE");
			}
		} else {
			if (scan_type == MS_MSG_STORAGE_ALL) {
				/* Delete all data before full scanning */
				if (!ms_delete_all_items(handle, scan_data->storage_id, storage_type, scan_data->uid)) {
					MS_DBG_ERR("msc_delete_all_record fails");
				}
			} else if (scan_type == MS_MSG_STORAGE_PARTIAL) {
					ms_validaty_change_all_items(handle, scan_data->storage_id, storage_type, false, scan_data->uid);
			}

			ret = __msc_db_update(handle, scan_data->storage_id, scan_data);
		}

		msc_del_blocked_path();

		/*call for bundle commit*/
		if (scan_type != MS_MSG_STORAGE_INVALID) {
			__msc_bacth_commit_disable(handle, TRUE, valid_status, scan_data->msg, ret, scan_data->uid);
		}

		if (scan_type == MS_MSG_STORAGE_PARTIAL && ret == MS_MEDIA_ERR_NONE) {
			ms_delete_invalid_items(handle, scan_data->storage_id, storage_type, scan_data->uid);
		}

		/* send notification */
		ms_send_dir_update_noti(handle, scan_data->storage_id, update_path, NULL, MS_ITEM_UPDATE, scan_data->pid);

		if (ret == MS_MEDIA_ERR_SCANNER_FORCE_STOP) {
			ms_set_storage_scan_status(handle, scan_data->storage_id, MEDIA_SCAN_STOP, scan_data->uid);
			/*set vconf key mmc loading for indicator */
			__msc_set_db_status(MS_DB_STOPPED, storage_type);
		} else {
			ms_set_storage_scan_status(handle, scan_data->storage_id, MEDIA_SCAN_COMPLETE, scan_data->uid);
			/*set vconf key mmc loading for indicator */
			__msc_set_db_status(MS_DB_UPDATED, storage_type);
		}

		MS_DBG_ERR("storage_id = [%s]", scan_data->storage_id);
		msc_insert_exactor_request(scan_type, TRUE, scan_data->storage_id, scan_data->msg, scan_data->pid, scan_data->uid);

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

		scan_data->msg_type = MS_MSG_SCANNER_COMPLETE;
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
		MS_DBG_ERR("[%s]open files");
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
	char *user_path = NULL;

	ret = __msc_check_file_path(full_path, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("invalid path : %s", full_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	dir_path = g_path_get_dirname(full_path);
	if (dir_path == NULL || strcmp(dir_path, ".") == 0) {
		MS_DBG_ERR("getting directory path is failed : %s", full_path);
		MS_SAFE_FREE(dir_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	while (1) {
		if (__msc_check_scan_ignore(dir_path) != MS_MEDIA_ERR_NONE) {
			ret = MS_MEDIA_ERR_INVALID_PATH;
			break;
		}

		/*If root path, Stop Scanning*/
		user_path = __msc_get_path(uid);
		if (user_path == NULL) {
			ret = MS_MEDIA_ERR_INTERNAL;
			break;
		}

		if (strcmp(dir_path, user_path) == 0)
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
	MS_SAFE_FREE(user_path);

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
	bool power_off_status = FALSE;

	insert_function = (current_msg == MS_MSG_BULK_INSERT) ? ms_insert_item_batch : ms_insert_burst_item;

	/* connect to media db, if conneting is failed, db updating is stopped */
	err = ms_connect_db(&handle, uid);
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;

	/*start db updating */
	/*call for bundle commit*/
	__msc_bacth_commit_enable(handle, TRUE, FALSE, MS_NOTI_SWITCH_ON, pid);

	MS_DBG_ERR("BULK REGISTER START");

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

		msc_get_power_status(&power_off_status);
		if (power_off_status) {
			MS_DBG_ERR("power off");
			/*call for bundle commit*/
			break;
		}
	}

	/*call for bundle commit*/
	__msc_bacth_commit_disable(handle, TRUE, FALSE, NULL, MS_MEDIA_ERR_NONE, uid);

	/*disconnect form media db*/
	if (handle) ms_disconnect_db(&handle);

	return MS_MEDIA_ERR_NONE;
}

static int __msc_pop_register_request(GArray *register_array, ms_comm_msg_s **register_data)
{
	int remain_request;
	ms_comm_msg_s *insert_data = NULL;

	while (1) {
		remain_request = g_async_queue_length(reg_queue2);

		/*updating requests remain*/
		if (register_array->len != 0 && remain_request == 0) {
			*register_data = g_array_index(register_array, ms_comm_msg_s*, 0);
			g_array_remove_index(register_array, 0);
			break;
		} else if (remain_request != 0) {
			insert_data = g_async_queue_pop(reg_queue2);
			__msc_insert_register_request(register_array, insert_data);
			continue;
		} else if (register_array->len == 0 && remain_request == 0) {
		/*Threre is no request, Wait until pushung new request*/
			insert_data = g_async_queue_pop(reg_queue2);
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
			MS_DBG_ERR("__msc_tv_pop_register_request failed [%d]", ret);
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
			MS_DBG_ERR("file path is NULL");
			goto FREE_RESOURCE;
		}

		ret = __msc_make_file_list(file_path, &path_array, register_data->uid);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("__msc_tv_make_file_list failed [%d]", ret);
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

static void __msc_bacth_commit_disable(void* handle, bool ins_status, bool valid_status, const char *path, int result, uid_t uid)
{
	/*call for bundle commit*/
	if (valid_status) ms_validate_end(handle, uid);
	if (ins_status) ms_register_end(handle, path, uid);

	return;
}

int msc_set_cancel_path(const char *cancel_path)
{
	if (g_cancel_path2 != NULL) {
		MS_DBG_WARN("g_cancel_path2 is not NULL");
		free(g_cancel_path2);
		g_cancel_path2 = NULL;
	}

	g_cancel_path2 = strdup(cancel_path);

	return MS_MEDIA_ERR_NONE;
}

int msc_del_cancel_path()
{
	if (g_cancel_path2 != NULL) {
		MS_DBG_WARN("g_tv_cancel_path is not NULL");
		free(g_cancel_path2);
		g_cancel_path2 = NULL;
	}

	return MS_MEDIA_ERR_NONE;
}

static bool __msc_storage_mount_status(const char* start_path)
{
	bool ret = false;
	char *storage_path = NULL;
	char *remain_path = NULL;
	int remain_len = 0;
	GArray *dev_list = NULL;

	if (!MS_STRING_VALID(MEDIA_ROOT_PATH_EXTERNAL)) {
		MS_DBG_ERR("Fail to get external path");
		return FALSE;
	}

	remain_path = strstr(start_path + strlen(MEDIA_ROOT_PATH_EXTERNAL) + 1, "/");
	if (remain_path != NULL)
		remain_len = strlen(remain_path);

	storage_path = strndup(start_path, strlen(start_path) - remain_len);

	MS_DBG_ERR("storage_path [%s]", storage_path);

	ret = ms_sys_get_device_list(MS_STG_TYPE_ALL, &dev_list);
	if (ret == MS_MEDIA_ERR_NONE) {
		if (dev_list != NULL) {
			MS_DBG_ERR("DEV FOUND[%d]", dev_list->len);
			int i = 0 ;
			int dev_num = dev_list->len;
			ms_block_info_s *block_info = NULL;

			for (i = 0; i < dev_num; i++) {
				block_info = (ms_block_info_s *)g_array_index(dev_list , ms_stg_type_e*, i);
				if (strcmp(block_info->mount_path, storage_path) == 0) {
					ret = TRUE;
					MS_DBG_ERR("STORAGE FOUND [%s]", block_info->mount_path);
					break;
				}
			}
			ms_sys_release_device_list(&dev_list);
		} else {
			MS_DBG_ERR("DEV NOT FOUND");
		}
	} else {
		MS_DBG_ERR("ms_sys_get_device_list failed");
	}

	MS_SAFE_FREE(storage_path);

	return ret;
}

int msc_set_blocked_path(const char *blocked_path)
{
	MS_DBG_FENTER();

	g_mutex_lock(&blocked_mutex2);

	if (g_blocked_path2 != NULL) {
		MS_DBG_ERR("g_blocked_path is not NULL [%s]", g_blocked_path2);
		free(g_blocked_path2);
		g_blocked_path2 = NULL;
	}

	g_blocked_path2 = strdup(blocked_path);

	g_mutex_unlock(&blocked_mutex2);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;
}

int msc_del_blocked_path(void)
{
	MS_DBG_FENTER();

	g_mutex_lock(&blocked_mutex2);

	if (g_blocked_path2 != NULL) {
		MS_DBG_ERR("g_blocked_path is not NULL [%s]", g_blocked_path2);
		free(g_blocked_path2);
		g_blocked_path2 = NULL;
	}

	g_mutex_unlock(&blocked_mutex2);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;
}

int msc_init_scanner(void)
{
	if (!scan_queue2) scan_queue2 = g_async_queue_new();
	if (!reg_queue2) reg_queue2 = g_async_queue_new();
	if (!storage_queue2) storage_queue2 = g_async_queue_new();

	/*Init mutex variable*/
	g_mutex_init(&scan_req_mutex2);

	return MS_MEDIA_ERR_NONE;
}
int msc_deinit_scanner(void)
{
	if (scan_queue2) g_async_queue_unref(scan_queue2);
	if (reg_queue2) g_async_queue_unref(reg_queue2);
	if (storage_queue2) g_async_queue_unref(storage_queue2);

	/*Clear db mutex variable*/
	g_mutex_clear(&scan_req_mutex2);

	return MS_MEDIA_ERR_NONE;
}

int msc_push_scan_request(ms_scan_type_e scan_type, ms_comm_msg_s *recv_msg)
{
	int ret = MS_MEDIA_ERR_NONE;

	switch (scan_type) {
		case MS_SCAN_STORAGE:
			g_async_queue_push(storage_queue2, GINT_TO_POINTER(recv_msg));
			break;
		case MS_SCAN_DIRECTORY:
			g_async_queue_push(scan_queue2, GINT_TO_POINTER(recv_msg));
			break;
		case MS_SCAN_REGISTER:
			g_async_queue_push(reg_queue2, GINT_TO_POINTER(recv_msg));
			break;
		default:
			MS_DBG_ERR("invalid parameter");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			break;
	}

	return ret;
}

int msc_stop_scan_thread(void)
{
	ms_comm_msg_s *data = NULL;

	msc_set_power_status(TRUE);

	if (scan_queue2) {
		/*notify to scannig thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		if (data != NULL) {
			data->pid = POWEROFF;
			msc_push_scan_request(MS_SCAN_DIRECTORY, data);
		} else {
			MS_DBG_ERR("memory allocation failed");
		}
	}

	if (reg_queue2) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		if (data != NULL) {
			data->pid = POWEROFF;
			msc_push_scan_request(MS_SCAN_REGISTER, data);
		} else {
			MS_DBG_ERR("memory allocation failed");
		}
	}

	if (storage_queue2) {
		/*notify to register thread*/
		MS_MALLOC(data, sizeof(ms_comm_msg_s));
		if (data != NULL) {
			data->pid = POWEROFF;
			msc_push_scan_request(MS_SCAN_STORAGE, data);
		} else {
			MS_DBG_ERR("memory allocation failed");
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int msc_get_remain_scan_request(ms_scan_type_e scan_type, int *remain_request)
{
	int ret = MS_MEDIA_ERR_NONE;

	switch (scan_type) {
		case MS_SCAN_STORAGE:
			*remain_request = g_async_queue_length(storage_queue2);
			break;
		case MS_SCAN_DIRECTORY:
			*remain_request = g_async_queue_length(scan_queue2);
			break;
		case MS_SCAN_REGISTER:
			*remain_request = g_async_queue_length(reg_queue2);
			break;
		default:
			MS_DBG_ERR("invalid parameter");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			break;
	}

	return ret;
}

int msc_get_dir_scan_status(bool *scan_status)
{
	*scan_status = g_directory_scan_processing2;

	return MS_MEDIA_ERR_NONE;
}


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
#include "media-scanner-dbg.h"
#include "media-scanner-utils.h"
#include "media-scanner-db-svc.h"
#include "media-scanner-socket.h"
#include "media-scanner-scan.h"

typedef struct msc_scan_data {
	char *name;
	struct msc_scan_data *next;
} msc_scan_data;

int mmc_state;
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
_msc_set_db_status(ms_scanning_location_t location, ms_db_status_type_t status)
{
	int res = MS_MEDIA_ERR_NONE;
	int err = 0;

	if (status == MS_DB_UPDATING) {
		if (location != MS_SCANNING_DIRECTORY) {
			if (!msc_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATING)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}
		/* this is temporay code for tizen 2.0*/
		if(location == MS_SCANNING_EXTERNAL) {
			if (!msc_config_set_int(VCONFKEY_FILEMANAGER_MMC_STATUS, VCONFKEY_FILEMANAGER_MMC_LOADING)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}

		/* Update private vconf key for media server*/
		if (location == MS_SCANNING_INTERNAL) {
			if (!msc_config_set_int(MS_SCAN_STATUS_INTERNAL, P_VCONF_SCAN_DOING)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		} else if (location == MS_SCANNING_DIRECTORY) {
			if (!msc_config_set_int(MS_SCAN_STATUS_DIRECTORY, P_VCONF_SCAN_DOING)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}
	} else if (status == MS_DB_UPDATED) {
		if (location != MS_SCANNING_DIRECTORY) {
			if(!msc_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}

		/* this is temporay code for tizen 2.0*/
		if(location == MS_SCANNING_EXTERNAL) {
			if (!msc_config_set_int(VCONFKEY_FILEMANAGER_MMC_STATUS, VCONFKEY_FILEMANAGER_MMC_LOADED)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}

		/* Update private vconf key for media server*/
		if (location == MS_SCANNING_INTERNAL) {
			if (!msc_config_set_int(MS_SCAN_STATUS_INTERNAL, P_VCONF_SCAN_DONE)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		} else if (location == MS_SCANNING_DIRECTORY) {
			if (!msc_config_set_int(MS_SCAN_STATUS_DIRECTORY, P_VCONF_SCAN_DONE)) {
				res = MS_MEDIA_ERR_VCONF_SET_FAIL;
				MSC_DBG_ERR("msc_config_set_int failed");
			}
		}
	}

	err = _msc_set_power_mode(status);
	if (err != MS_MEDIA_ERR_NONE) {
		MSC_DBG_ERR("_msc_set_power_mode fail");
		res = err;
	}

	return res;
}

static int _msc_scan_get_next_path_from_current_node(int find_folder,
				   ms_dir_scan_info **current_root,
				   ms_dir_scan_info **real_root, char **path, int *depth)
{
	int err = MS_MEDIA_ERR_NONE;
	char get_path[FAT_FILEPATH_LEN_MAX] = { 0 };

	if (find_folder == 0) {
		if ((*current_root)->Rbrother != NULL) {
			*current_root = (*current_root)->Rbrother;
		} else {
			while (1) {
				if ((*current_root)->parent == *real_root
				    || (*current_root)->parent == NULL) {
					*current_root = NULL;
					*depth = 0;
					return MS_MEDIA_ERR_NONE;
				} else if ((*current_root)->parent->Rbrother == NULL) {
					*current_root = (*current_root)->parent;
					(*depth) --;
				} else {
					*current_root = (*current_root)->parent->Rbrother;
					(*depth) --;
					break;
				}
			}
		}
		(*depth) --;
	}

	err = msc_get_full_path_from_node(*current_root, get_path, *depth);
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_INVALID_PATH;

	*path = strdup(get_path);

	return err;
}

static int _msc_scan_add_node(msc_scan_data **first_node, ms_dir_scan_info *const node, int depth)
{
	int err;
	char full_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	msc_scan_data *current_dir = NULL;
	msc_scan_data *prv_node = NULL;
	msc_scan_data *last_node = NULL;

	err = msc_get_full_path_from_node(node, full_path, depth);
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_INVALID_PATH;

	last_node = *first_node;
	while (last_node != NULL) {
		last_node = last_node->next;
	}

	/*find same folder */
	if (*first_node != NULL) {
		last_node = *first_node;
		while (last_node != NULL) {
			if (strcmp(full_path, last_node->name) == 0) {
				return MS_MEDIA_ERR_NONE;
			}
			prv_node = last_node;
			last_node = last_node->next;
		}
	}

	MS_MALLOC(current_dir, sizeof(msc_scan_data));
	current_dir->name = strdup(full_path);
	current_dir->next = NULL;

	if (*first_node == NULL) {
		*first_node = current_dir;
	} else {
		/*if next node of current node is NULL, it is the lastest node. */
		prv_node->next = current_dir;
	}

//	MSC_DBG_INFO("scan path : %s %x %p", full_path, first_node, first_node);

	return MS_MEDIA_ERR_NONE;
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

static void _msc_dir_check(msc_scan_data **first_node, const ms_comm_msg_s *scan_data)
{
	int err = 0;
	int depth = 0;
	int find_folder = 0;
	char get_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char *path = NULL;
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result;
	ms_storage_type_t storage_type;

	ms_dir_scan_info *root = NULL;
	ms_dir_scan_info *tmp_root = NULL;
	ms_dir_scan_info *cur_node = NULL; /*current node*/
	ms_dir_scan_info *prv_node = NULL; /*previous node*/
	ms_dir_scan_info *next_node = NULL;

	MS_MALLOC(root, sizeof(ms_dir_scan_info));
	if (root == NULL) {
		MSC_DBG_ERR("malloc fail");
		return;
	}

	storage_type = msc_get_storage_type_by_full(scan_data->msg);

	root->name = strndup(scan_data->msg, scan_data->msg_size);
	if (root->name == NULL) {
		MSC_DBG_ERR("strdup fail");
		MS_SAFE_FREE(root);
		return;
	}

	root->parent = NULL;
	root->Rbrother = NULL;
	root->next = NULL;
	tmp_root = root;
	prv_node = root;

	MS_MALLOC(path, sizeof(char) * MS_FILE_PATH_LEN_MAX);

	err = msc_get_full_path_from_node(tmp_root, path, depth);
	if (err != MS_MEDIA_ERR_NONE) {
		MS_SAFE_FREE(path);
		MS_SAFE_FREE(root);
		return;
	}

	_msc_scan_add_node(first_node, root, depth);

	while (1) {
		/*check poweroff status*/
		err = _ms_check_stop_status(storage_type);
		if (err != MS_MEDIA_ERR_NONE) {
			goto FREE_RESOURCES;
		}

		depth ++;
		dp = opendir(path);
		if (dp == NULL) {
			MSC_DBG_ERR("%s folder opendir fails", path);
			goto NEXT_DIR;
		}

		while (!readdir_r(dp, &entry, &result)) {
			/*check poweroff status*/
			err = _ms_check_stop_status(storage_type);
			if (err != MS_MEDIA_ERR_NONE) {
				goto FREE_RESOURCES;
			}

			if (result == NULL)
				break;

			if (entry.d_name[0] == '.')
				continue;

			if (entry.d_type & DT_DIR) {
				err = msc_strappend(get_path, sizeof(get_path), "%s/%s",path, entry.d_name);
				if (err != MS_MEDIA_ERR_NONE) {
					MSC_DBG_ERR("msc_strappend error");
					continue;
				}

				if (_msc_check_scan_ignore(get_path)) {
					MSC_DBG_ERR("%s is ignore", get_path);
					continue;
				}

				MS_MALLOC(cur_node, sizeof(ms_dir_scan_info));
				if (cur_node == NULL) {
					MSC_DBG_ERR("malloc fail");

					goto FREE_RESOURCES;
				}

				cur_node->name = strdup(entry.d_name);
				cur_node->Rbrother = NULL;
				cur_node->next = NULL;

				/*1. 1st folder */
				if (find_folder == 0) {
					cur_node->parent = tmp_root;
					tmp_root = cur_node;
				} else {
					cur_node->parent = tmp_root->parent;
					prv_node->Rbrother = cur_node;
				}
				prv_node->next = cur_node;

				/*add watch */
				_msc_scan_add_node(first_node, cur_node, depth);

				/*change previous */
				prv_node = cur_node;
				find_folder++;
			}
		}
NEXT_DIR:
		MS_SAFE_FREE(path);
		if (dp) closedir(dp);
		dp = NULL;

		err = _msc_scan_get_next_path_from_current_node(find_folder, &tmp_root, &root, &path, &depth);
		if (err != MS_MEDIA_ERR_NONE)
			break;

		if (tmp_root == NULL)
			break;

		find_folder = 0;
	}

FREE_RESOURCES:
	/*free allocated memory */
	MS_SAFE_FREE(path);
	if (dp) closedir(dp);
	dp = NULL;

	cur_node = root;
	while (cur_node != NULL) {
		next_node = cur_node->next;
		MS_SAFE_FREE(cur_node->name);
		MS_SAFE_FREE(cur_node);
		cur_node = next_node;
	}
}

static int _msc_dir_scan(void **handle, msc_scan_data **first_node, const ms_comm_msg_s * scan_data)
{
	DIR *dp = NULL;
	int scan_type;
	int ret = MS_MEDIA_ERR_NONE;
	int err = 0;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	msc_scan_data *node;
	ms_storage_type_t storage_type;

	err = msc_strcopy(path, sizeof(path), "%s", scan_data->msg);
	if (err != MS_MEDIA_ERR_NONE) {
		MSC_DBG_ERR("error : %d", err );
		return err;
	}

	storage_type = msc_get_storage_type_by_full(scan_data->msg);
	scan_type = scan_data->msg_type;

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_type != MS_MSG_STORAGE_INVALID) {
		struct dirent entry;
		struct dirent *result = NULL;

		if (scan_data->msg_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			_msc_dir_check(first_node, scan_data);
		} else {
			msc_scan_data *scan_node;

			MS_MALLOC(scan_node, sizeof(msc_scan_data));

			scan_node->name = strdup(scan_data->msg);
			scan_node->next = NULL;

			*first_node = scan_node;
		}

		node = *first_node;

		/*start db update. If node is null, db update is done.*/
		while (node != NULL) {
			/*check poweroff status*/
			ret = _ms_check_stop_status(storage_type);
			if (ret != MS_MEDIA_ERR_NONE) {
				goto STOP_SCAN;
			}

			dp = opendir(node->name);
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
						err = msc_strappend(path, sizeof(path), "%s/%s", node->name, entry.d_name);
						if (err < 0) {
							MSC_DBG_ERR("error : %d", err);
							continue;
						}

						if (scan_type == MS_MSG_STORAGE_ALL)
							err = msc_insert_item_batch(handle, path);
						else
							err = msc_validate_item(handle,path);
						if (err < 0) {
							MSC_DBG_ERR("failed to update db : %d , %d\n", err, scan_type);
							continue;
						}
					}
				}
			} else {
				MSC_DBG_ERR("%s folder opendir fails", node->name);
			}
			if (dp) closedir(dp);
			dp = NULL;

			node = node->next;
		}		/*db update while */
	} else if ( scan_type == MS_MSG_STORAGE_INVALID) {
		/*In this case, update just validation record*/
		/*update just valid type*/
		err = msc_invalidate_all_items(handle, storage_type);
		if (err != MS_MEDIA_ERR_NONE)
			MSC_DBG_ERR("error : %d", err);
	}
STOP_SCAN:
	if (dp) closedir(dp);

	/*delete all node*/
	node = *first_node;

	while (node != NULL) {
		MS_SAFE_FREE(node->name);
		MS_SAFE_FREE(node);
	}

	*first_node = NULL;

	sync();

	MSC_DBG_INFO("ret : %d", ret);

	return ret;
}


static void _msc_insert_array(GArray *garray, ms_comm_msg_s *insert_data)
{
	ms_scan_data_t *data;
	bool insert_ok = false;
	int len = garray->len;
	int i;

	MSC_DBG_INFO("the length of array : %d", len);
	MSC_DBG_INFO("path : %s", insert_data->msg);
	MSC_DBG_INFO("scan_type : %d", insert_data->msg_type);

	if (insert_data->pid == POWEROFF) {
		g_array_prepend_val(garray, insert_data);
	} else {
		for (i=0; i < len; i++) {
			data = g_array_index(garray, ms_scan_data_t*, i);
/*
			if (data->pid != POWEROFF) {
				if (data->storage_type == insert_data->storage_type) {
					if(data->scan_type > insert_data->scan_type) {
						g_array_remove_index (garray, i);
						g_array_insert_val(garray, i, insert_data);
						insert_ok =  true;
					}
				}
			}
*/
		}

		if (insert_ok == false)
			g_array_append_val(garray, insert_data);
	}
}

void _msc_check_dir_path(char *dir_path)
{
	int len = strlen(dir_path);

	if (dir_path[len -1] == '/')
		dir_path[len -1] = '\0';
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
	msc_scan_data *first_node = NULL;

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

		storage_type = msc_get_storage_type_by_full(scan_data->msg);
		scan_type = scan_data->msg_type;

		if (scan_type != MS_MSG_DIRECTORY_SCANNING
			&& scan_type != MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE) {
			MSC_DBG_ERR("Invalid request");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto NEXT;
		}

		/*start db updating */
//		_msc_set_db_status(MS_SCANNING_DIRECTORY, MS_DB_UPDATING);

		_msc_check_dir_path(scan_data->msg);

		/*change validity before scanning*/
		if (scan_type == MS_MSG_DIRECTORY_SCANNING)
			err = msc_set_folder_validity(handle, scan_data->msg, MS_INVALID, MS_RECURSIVE);
		else
			err = msc_set_folder_validity(handle, scan_data->msg, MS_INVALID, MS_NON_RECURSIVE);
		if (err != MS_MEDIA_ERR_NONE)
			MSC_DBG_ERR("error : %d", err);

		/*call for bundle commit*/
		msc_register_start(handle);
		msc_validate_start(handle);

		/*insert data into media db */
		ret = _msc_dir_scan(handle, &first_node, scan_data);

		/*call for bundle commit*/
		msc_register_end(handle);
		msc_validate_end(handle);

		if (ret == MS_MEDIA_ERR_NONE) {
			MSC_DBG_INFO("working normally");
			msc_delete_invalid_items_in_folder(handle, scan_data->msg);
		}

		/*set vconf key mmc loading for indicator */
//		_msc_set_db_status(MS_SCANNING_DIRECTORY, MS_DB_UPDATED);

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

//		MS_SAFE_FREE(scan_data->msg);
		MS_SAFE_FREE(scan_data);

		MSC_DBG_INFO("DIRECTORY SCAN END");
	}			/*thread while*/

_POWEROFF:
//	MS_SAFE_FREE(scan_data->msg);
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
	msc_scan_data *first_node = NULL;

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

		storage_type = msc_get_storage_type_by_full(scan_data->msg);
		MSC_DBG_INFO("%d", storage_type);

		/*connect to media db, if conneting is failed, db updating is stopped*/
		err = msc_connect_db(&handle);
		if (err != MS_MEDIA_ERR_NONE)
			continue;

		/*start db updating */
		if (storage_type == MS_STORAGE_INTERNAL)
			_msc_set_db_status(MS_SCANNING_INTERNAL, MS_DB_UPDATING);
		else if (storage_type == MS_STORAGE_EXTERNAL)
			_msc_set_db_status(MS_SCANNING_EXTERNAL, MS_DB_UPDATING);

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
			msc_update_mmc_info();
		}

#ifdef FMS_PERF
		if (storage_type == MS_STORAGE_EXTERNAL) {
			msc_check_start_time(&g_mmc_start_time);
		}
#endif
		/*call for bundle commit*/
		msc_register_start(handle);
		if (scan_type == MS_MSG_STORAGE_PARTIAL) {
			/*enable bundle commit*/
			msc_validate_start(handle);
		}

		/*add inotify watch and insert data into media db */
		ret = _msc_dir_scan(handle, &first_node, scan_data);

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
			msc_check_end_time(&g_mmc_end_time);
			msc_check_time_diff(&g_mmc_start_time, &g_mmc_end_time);
		}
#endif

		/*set vconf key mmc loading for indicator */
		if (storage_type == MS_STORAGE_INTERNAL)
			_msc_set_db_status(MS_SCANNING_INTERNAL, MS_DB_UPDATED);
		else if (storage_type == MS_STORAGE_EXTERNAL)
			_msc_set_db_status(MS_SCANNING_EXTERNAL, MS_DB_UPDATED);

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

//		MS_SAFE_FREE(scan_data->msg);
		MS_SAFE_FREE(scan_data);

		MSC_DBG_INFO("STORAGE SCAN END");
	}			/*thread while*/

_POWEROFF:
//	MS_SAFE_FREE(scan_data->msg);
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
		MSC_DBG_ERR("Not exist path : %s", file_path);
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

		/* copy from received data */
		MS_MALLOC(file_path, path_size);
		if (file_path == NULL) {
			MSC_DBG_ERR("MS_MALLOC failed");
			goto FREE_RESOURCE;
		}

		ret = msc_strcopy(file_path, path_size, "%s", register_data->msg);
		if (ret != MS_MEDIA_ERR_NONE){
			MSC_DBG_ERR("msc_strcopy failed : %d", ret);
			goto FREE_RESOURCE;
		}

		/* load the file list from file */
		fp = fopen(register_data->msg, "rt");
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
			length = strlen(buf);
			buf[length - 1] = '\0';
			path = strdup(buf);
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
		msc_register_start(handle);

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
			err = msc_insert_item_batch(handle, insert_path);

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

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
 * @file		media-server-db-svc.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */

#include <dlfcn.h>

#include "media-util.h"

#include "media-common-utils.h"
#include "media-scanner-dbg.h"
#include "media-scanner-db-svc.h"

#include <tzplatform_config.h>

#define CONFIG_PATH tzplatform_mkpath(TZ_SYS_DATA,"file-manager-service/plugin-config")
#define EXT ".so"
#define EXT_LEN 3
#define MSC_REGISTER_COUNT 100 /*For bundle commit*/
#define MSC_VALID_COUNT 100 /*For bundle commit*/

GMutex db_mutex;
GArray *so_array;
void ***func_array;
int lib_num;
void **scan_func_handle = NULL; /*dlopen handel*/
extern int insert_count;

enum func_list {
	eCONNECT,
	eDISCONNECT,
	eEXIST,
	eINSERT_BEGIN,
	eINSERT_END,
	eINSERT_BATCH,
	eINSERT_ITEM_IMMEDIATELY,
	eSET_ALL_VALIDITY,
	eSET_VALIDITY_BEGIN,
	eSET_VALIDITY_END,
	eSET_VALIDITY,
	eDELETE_ALL,
	eDELETE_INVALID_ITEMS,
	eUPDATE_BEGIN,
	eUPDATE_END,
	eSET_FOLDER_VALIDITY,
	eDELETE_FOLDER,
	eINSERT_BURST,
	eSEND_DIR_UPDATE_NOTI,
	eCOUNT_DELETE_ITEMS_IN_FOLDER,
	eDELETE_ITEM,
	eGET_FOLDER_LIST,
	eUPDATE_FOLDER_TIME,
	eUPDATE_ITEM_META,
	eUPDATE_ITEM_BEGIN,
	eUPDATE_ITEM_END,
	eFUNC_MAX
};

static int
_msc_token_data(char *buf, char **name)
{
	int len;
	char* pos = NULL;

	pos = strstr(buf, EXT);
	if (pos == NULL) {
		MSC_DBG_ERR("This is not shared object library.");
		name = NULL;
		return -1;
	} else {
		len = pos - buf + EXT_LEN;
		*name = strndup(buf, len);
	}

	return 0;
}

static bool
_msc_load_config()
{
	int ret;
	FILE *fp;
	char *so_name = NULL;
	char buf[256] = {0};

	fp = fopen(CONFIG_PATH, "rt");
	if (fp == NULL) {
		MSC_DBG_ERR("fp is NULL");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}
	while(1) {
		if (fgets(buf, 256, fp) == NULL)
			break;

		ret = _msc_token_data(buf, &so_name);
		if (ret == 0) {
			/*add to array*/
			g_array_append_val(so_array, so_name);
			so_name = NULL;
		}
	}

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}

int
msc_load_functions(void)
{
	int lib_index = 0, func_index;
	char func_list[eFUNC_MAX][40] = {
		"connect_db",
		"disconnect_db",
		"check_item_exist",
		"insert_item_begin",
		"insert_item_end",
		"insert_item",
		"insert_item_immediately",
		"set_all_storage_items_validity",
		"set_item_validity_begin",
		"set_item_validity_end",
		"set_item_validity",
		"delete_all_items_in_storage",
		"delete_all_invalid_items_in_storage",
		"update_begin",
		"update_end",
		"set_folder_item_validity",
		"delete_all_invalid_items_in_folder",
		"insert_burst_item",
		"send_dir_update_noti",
		"count_delete_items_in_folder",
		"delete_item",
		"get_folder_list",
		"update_folder_time",
		"update_item_meta",
		"update_item_begin",
		"update_item_end",
		};
	/*init array for adding name of so*/
	so_array = g_array_new(FALSE, FALSE, sizeof(char*));

	/*load information of so*/
	_msc_load_config();

	if(so_array->len == 0) {
		MSC_DBG_ERR("There is no information for functions");
		return MS_MEDIA_ERR_DYNAMIC_LINK;
	}

	/*the number of functions*/
	lib_num = so_array->len;

	MSC_DBG_SLOG("The number of information of so : %d", lib_num);
	MS_MALLOC(scan_func_handle, sizeof(void*) * lib_num);
	if (scan_func_handle == NULL) {
		MSC_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	while(lib_index < lib_num) {
		/*get handle*/
		MSC_DBG_SLOG("[name of so : %s]", g_array_index(so_array, char*, lib_index));
		scan_func_handle[lib_index] = dlopen(g_array_index(so_array, char*, lib_index), RTLD_LAZY);
		if (!scan_func_handle[lib_index]) {
			MSC_DBG_ERR("%s", dlerror());
			MS_SAFE_FREE(scan_func_handle);
			return MS_MEDIA_ERR_DYNAMIC_LINK;
		}
		lib_index++;
	}

	dlerror();    /* Clear any existing error */

	/*allocate for array of functions*/
	MS_MALLOC(func_array, sizeof(void**) * lib_num);
	if (func_array == NULL) {
		MSC_DBG_ERR("malloc failed");
		MS_SAFE_FREE(scan_func_handle);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	for(lib_index = 0 ; lib_index < lib_num; lib_index ++) {
		MS_MALLOC(func_array[lib_index], sizeof(void*) * eFUNC_MAX);
		if (func_array[lib_index] == NULL) {
			int index;

			for (index = 0; index < lib_index; index ++) {
				MS_SAFE_FREE(func_array[index]);
			}
			MS_SAFE_FREE(func_array);
			MS_SAFE_FREE(scan_func_handle);

			MSC_DBG_ERR("malloc failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
	}

	/*add functions to array */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		for (func_index = 0; func_index < eFUNC_MAX ; func_index++) {
			func_array[lib_index][func_index] = dlsym(scan_func_handle[lib_index], func_list[func_index]);
			if (func_array[lib_index][func_index] == NULL) {
				int index;

				for (index = 0; index < lib_index; index ++) {
					MS_SAFE_FREE(func_array[index]);
				}
				MS_SAFE_FREE(func_array);
				MS_SAFE_FREE(scan_func_handle);

				MSC_DBG_ERR("dlsym failed [%s]", func_list[func_index]);
				return MS_MEDIA_ERR_DYNAMIC_LINK;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

void
msc_unload_functions(void)
{
	int lib_index;

	for (lib_index = 0; lib_index < lib_num; lib_index ++)
		dlclose(scan_func_handle[lib_index]);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
			if (func_array[lib_index]) {
				MS_SAFE_FREE(func_array[lib_index]);
			}
	}

	MS_SAFE_FREE (func_array);
	MS_SAFE_FREE (scan_func_handle);

	if (so_array) {
		/*delete all node*/
		while(so_array->len != 0) {
			char *so_name = NULL;
			so_name = g_array_index(so_array , char*, 0);
			g_array_remove_index (so_array, 0);
			MS_SAFE_FREE(so_name);
		}
		g_array_free(so_array, FALSE);
		so_array = NULL;
	}
}

int
msc_connect_db(void ***handle, uid_t uid)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	/*Lock mutex for openning db*/
	g_mutex_lock(&db_mutex);

	MS_MALLOC(*handle, sizeof (void*) * lib_num);
	if (*handle == NULL) {
		MSC_DBG_ERR("malloc failed");
		g_mutex_unlock(&db_mutex);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CONNECT)func_array[lib_index][eCONNECT])(&((*handle)[lib_index]), uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			g_mutex_unlock(&db_mutex);

			return MS_MEDIA_ERR_DB_CONNECT_FAIL;
		}
	}

	MSC_DBG_INFO("connect Media DB");

	g_mutex_unlock(&db_mutex);

	return MS_MEDIA_ERR_NONE;
}

int
msc_disconnect_db(void ***handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DISCONNECT)func_array[lib_index][eDISCONNECT])((*handle)[lib_index], &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
		}
	}

	MS_SAFE_FREE(*handle);

	MSC_DBG_INFO("Disconnect Media DB");

	return MS_MEDIA_ERR_NONE;
}

int
msc_validate_item(void **handle, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	bool modified = FALSE;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		/*check exist in Media DB, If file is not exist, insert data in DB. */
		ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle[lib_index], path, &modified, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_SAFE_FREE(err_msg);
			ret = msc_insert_item_batch(handle, path, uid);
			if (ret != 0) {
				res = MS_MEDIA_ERR_DB_INSERT_FAIL;
			} else {
				insert_count++;
			}
		} else {
			if (modified == FALSE) {
				/*if meta data of file exist, change valid field to "1" */
				ret = ((SET_ITEM_VALIDITY)func_array[lib_index][eSET_VALIDITY])(handle[lib_index], path, true, true, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
				}
			} else {
				/* the file has same name but it is changed, so we have to update DB */
				MSC_DBG_WAN("DELETE ITEM [%s]", path);
				ret = ((DELETE_ITEM)func_array[lib_index][eDELETE_ITEM])(handle[lib_index], path, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_DELETE_FAIL;
				} else {
					ret = msc_insert_item_batch(handle, path, uid);
					if (ret != 0) {
						res = MS_MEDIA_ERR_DB_INSERT_FAIL;
					} else {
						insert_count++;
					}
				}
			}
		}
	}

	return res;
}

int
msc_validaty_change_all_items(void **handle, ms_storage_type_t store_type, bool validity , uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ALL_STORAGE_ITEMS_VALIDITY)func_array[lib_index][eSET_ALL_VALIDITY])(handle[lib_index], store_type, validity, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return res;
}

int
msc_insert_item_batch(void **handle, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM)func_array[lib_index][eINSERT_BATCH])(handle[lib_index], path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int
msc_insert_item_immediately(void **handle, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_IMMEDIATELY)func_array[lib_index][eINSERT_ITEM_IMMEDIATELY])(handle[lib_index], path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int
msc_insert_burst_item(void **handle, const char *path , uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path,uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_BURST_ITEM)func_array[lib_index][eINSERT_BURST])(handle[lib_index], path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

bool
msc_delete_all_items(void **handle, ms_storage_type_t store_type, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	/* To reset media db when differnet mmc is inserted. */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_ITEMS_IN_STORAGE)func_array[lib_index][eDELETE_ALL])(handle[lib_index], store_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return false;
		}
	}

	return true;
}

bool
msc_delete_invalid_items(void **handle, ms_storage_type_t store_type, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_INVALID_ITMES_IN_STORAGE)func_array[lib_index][eDELETE_INVALID_ITEMS])(handle[lib_index], store_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return false;
		}
	}

	return true;
}

int
msc_set_folder_validity(void **handle, const char *path, int validity, int recursive, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_FOLDER_ITEM_VALIDITY)func_array[lib_index][eSET_FOLDER_VALIDITY])(handle[lib_index], path, validity, recursive, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_delete_invalid_items_in_folder(void **handle, const char*path, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_INVALID_ITEMS_IN_FOLDER)func_array[lib_index][eDELETE_FOLDER])(handle[lib_index], path, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_send_dir_update_noti(void **handle, const char*path)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SEND_DIR_UPDATE_NOTI)func_array[lib_index][eSEND_DIR_UPDATE_NOTI])(handle[lib_index], path, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_SEND_NOTI_FAIL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_count_delete_items_in_folder(void **handle, const char*path, int *count)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((COUNT_DELETE_ITEMS_IN_FOLDER)func_array[lib_index][eCOUNT_DELETE_ITEMS_IN_FOLDER])(handle[lib_index], path, count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_get_folder_list(void **handle, char* start_path, GArray **dir_array)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	char **folder_list = NULL;
	int *modified_time_list = NULL;
	int *item_num_list = NULL;
	int count = 0;
	int i = 0;

	msc_dir_info_s* dir_info = NULL;
	MSC_DBG_ERR("start path: %s", start_path);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_FOLDER_LIST)func_array[lib_index][eGET_FOLDER_LIST])(handle[lib_index], start_path, &folder_list, &modified_time_list, &item_num_list, &count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	MSC_DBG_ERR("OK");

	*dir_array = g_array_new(FALSE, FALSE, sizeof(msc_dir_info_s*));
	if (count != 0) {
		for(i = 0; i < count; i ++) {
			dir_info = malloc(sizeof(msc_dir_info_s));
			if(dir_info) {
				memset(dir_info, 0, sizeof(msc_dir_info_s));
				dir_info->dir_path = strdup(folder_list[i]);
				dir_info->modified_time = modified_time_list[i];
				dir_info->item_num = item_num_list[i];
				g_array_append_val(*dir_array, dir_info);
				MS_SAFE_FREE(folder_list[i]);
//				MSC_DBG("%d get path : %s, %d", i, dir_info->dir_path, dir_info->modified_time);
			}
		}
	}

	MS_SAFE_FREE(folder_list);
	MS_SAFE_FREE(modified_time_list);

	return MS_MEDIA_ERR_NONE;
}

int
msc_update_folder_time(void **handle, char *folder_path, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_FOLDER_TIME)func_array[lib_index][eUPDATE_FOLDER_TIME])(handle[lib_index], folder_path, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int
msc_update_meta_batch(void **handle, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	MSC_DBG_ERR("");

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_META)func_array[lib_index][eUPDATE_ITEM_META])(handle[lib_index], path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/

void
msc_register_start(void **handle, ms_noti_status_e noti_status, int pid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_BEGIN)func_array[lib_index][eINSERT_BEGIN])(handle[lib_index], MSC_REGISTER_COUNT, noti_status, pid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void
msc_register_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_END)func_array[lib_index][eINSERT_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_END)func_array[lib_index][eUPDATE_END])(uid);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void
msc_validate_start(void **handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_BEGIN)func_array[lib_index][eSET_VALIDITY_BEGIN])(handle[lib_index], MSC_VALID_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void
msc_validate_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_END)func_array[lib_index][eSET_VALIDITY_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void
msc_update_start(void **handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_BEGIN)func_array[lib_index][eUPDATE_ITEM_BEGIN])(handle[lib_index], MSC_VALID_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void
msc_update_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_END)func_array[lib_index][eUPDATE_ITEM_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MSC_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}


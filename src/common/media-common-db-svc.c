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
#include <dlfcn.h>

#include "media-util.h"

#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-common-dbg.h"
#include <tzplatform_config.h>

#define CONFIG_PATH SYSCONFDIR"/multimedia/media-server-plugin"
#define EXT ".so"
#define EXT_LEN 3
#define MSC_REGISTER_COUNT 300 /*For bundle commit*/
#define MSC_VALID_COUNT 300 /*For bundle commit*/

static GMutex db_mutex;
static GArray *so_array;
static void ***func_array;
static int lib_num;
static void **func_handle = NULL; /*dlopen handel*/
static int insert_count;
static int delete_count;

enum func_list {
	eCONNECT,
	eDISCONNECT,
	eEXIST,
	eINSERT_BEGIN,
	eINSERT_END,
	eINSERT_BATCH,
	eINSERT_SCAN,
	eUPDATE_EXTRACT,
	eINSERT_ITEM_IMMEDIATELY,
	eSET_ALL_VALIDITY,
	eSET_VALIDITY_BEGIN,
	eSET_VALIDITY_END,
	eSET_VALIDITY,
	eDELETE_ALL,
	eDELETE_INVALID_ITEMS,
	eUPDATE_BEGIN,
	eUPDATE_END,
	eSET_FOLDER_ITEM_VALIDITY,
	eDELETE_FOLDER,
	eINSERT_BURST,
	eSEND_DIR_UPDATE_NOTI,
	eCOUNT_DELETE_ITEMS_IN_FOLDER,
	eDELETE_ITEM,
	eGET_FOLDER_LIST,
	eUPDATE_FOLDER_TIME,
	eGET_STORAGE_ID,
	eGET_STORAGE_SCAN_STATUS,
	eSET_STORAGE_SCAN_STATUS,
	eGET_STORAGE_LIST,
	eINSERT_FOLDER,
	eDELETE_INVALID_FOLDER,
	eSET_FOLDER_VALIDITY,
	eINSERT_FOLDER_BEGIN,
	eINSERT_FOLDER_END,
	eGET_FOLDER_SCAN_STATUS,
	eSET_FOLDER_SCAN_STATUS,
	eCHECK_FOLDER_MODIFIED,
	eGET_NULL_SCAN_FOLDER_LIST,
	eCHANGE_VALIDITY_ITEM_BATCH,
	eCHECK_DB,
	eGET_UUID,
	eGET_MMC_INFO,
	eCHECK_STORAGE,
	eINSERT_STORAGE,
	eUPDATE_STORAGE,
	eDELETE_STORAGE,
	eSET_STORAGE_VALIDITY,
	eSET_ALL_STORAGE_VALIDITY,
	eUPDATE_ITEM_META,
	eUPDATE_ITEM_BEGIN,
	eUPDATE_ITEM_END,
	eDELETE_INVALID_FOLDER_BY_PATH,
	eCHECK_FOLDER_EXIST,
	eCOUNT_SUBFOLDER,
	eGET_FOLDER_ID,
	eFUNC_MAX
};

static int _ms_token_data(char *buf, char **name)
{
	int len;
	char* pos = NULL;

	pos = strstr(buf, EXT);
	if (pos == NULL) {
		MS_DBG_ERR("This is not shared object library.");
		name = NULL;
		return -1;
	} else {
		len = pos - buf + EXT_LEN;
		*name = strndup(buf, len);
	}

	return 0;
}

static bool _ms_load_config()
{
	int ret = MS_MEDIA_ERR_NONE;
	FILE *fp;
	char *so_name = NULL;
	char buf[256] = {0, };

	if (!MS_STRING_VALID(CONFIG_PATH)) {
		MS_DBG_ERR("CONFIG_PATH is NULL");
		return MS_MEDIA_ERR_INTERNAL;
	}

	fp = fopen(CONFIG_PATH, "rt");
	if (fp == NULL) {
		MS_DBG_ERR("fp is NULL");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}
	while (1) {
		if (fgets(buf, 256, fp) == NULL)
			break;

		ret = _ms_token_data(buf, &so_name);
		if (ret == 0) {
			/*add to array*/
			g_array_append_val(so_array, so_name);
			so_name = NULL;
		}
	}

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}

int ms_load_functions(void)
{
	int lib_index = 0, func_index;
	char func_list[eFUNC_MAX][40] = {
		"connect_db",
		"disconnect_db",
		"check_item_exist",
		"insert_item_begin",
		"insert_item_end",
		"insert_item",
		"insert_item_scan",
		"update_item_extract",
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
		"get_storage_id",
		"get_storage_scan_status",
		"set_storage_scan_status",
		"get_storage_list",
		"insert_folder",
		"delete_invalid_folder",
		"set_folder_validity",
		"insert_folder_begin",
		"insert_folder_end",
		"get_folder_scan_status",
		"set_folder_scan_status",
		"check_folder_modified",
		"get_null_scan_folder_list",
		"change_validity_item_batch",
		"check_db",
		"get_uuid",
		"get_mmc_info",
		"check_storage",
		"insert_storage",
		"update_storage",
		"delete_storage",
		"set_storage_validity",
		"set_all_storage_validity",
		"update_item_meta",
		"update_item_begin",
		"update_item_end",
		"delete_invalid_folder_by_path",
		"check_folder_exist",
		"count_subfolder",
		"get_folder_id",
		};
	/*init array for adding name of so*/
	so_array = g_array_new(FALSE, FALSE, sizeof(char*));

	/*load information of so*/
	_ms_load_config();

	if (so_array->len == 0) {
		MS_DBG_ERR("There is no information for functions");
		return MS_MEDIA_ERR_DYNAMIC_LINK;
	}

	/*the number of functions*/
	lib_num = so_array->len;

	MS_DBG_SLOG("The number of information of so : %d", lib_num);
	MS_MALLOC(func_handle, sizeof(void*) * lib_num);
	if (func_handle == NULL) {
		MS_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	while (lib_index < lib_num) {
		/*get handle*/
		MS_DBG_SLOG("[name of so : %s]", g_array_index(so_array, char*, lib_index));
		func_handle[lib_index] = dlopen(g_array_index(so_array, char*, lib_index), RTLD_LAZY);
		if (!func_handle[lib_index]) {
			MS_DBG_ERR("%s", dlerror());
			MS_SAFE_FREE(func_handle);
			return MS_MEDIA_ERR_DYNAMIC_LINK;
		}
		lib_index++;
	}

	dlerror();	/* Clear any existing error */

	/*allocate for array of functions*/
	MS_MALLOC(func_array, sizeof(void**) * lib_num);
	if (func_array == NULL) {
		MS_DBG_ERR("malloc failed");
		MS_SAFE_FREE(func_handle);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	for (lib_index = 0 ; lib_index < lib_num; lib_index++) {
		MS_MALLOC(func_array[lib_index], sizeof(void*) * eFUNC_MAX);
		if (func_array[lib_index] == NULL) {
			int index;

			for (index = 0; index < lib_index; index++) {
				MS_SAFE_FREE(func_array[index]);
			}
			MS_SAFE_FREE(func_array);
			MS_SAFE_FREE(func_handle);

			MS_DBG_ERR("malloc failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
	}

	/*add functions to array */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		for (func_index = 0; func_index < eFUNC_MAX ; func_index++) {
			func_array[lib_index][func_index] = dlsym(func_handle[lib_index], func_list[func_index]);
			if (func_array[lib_index][func_index] == NULL) {
				int index;

				for (index = 0; index < lib_index; index++) {
					MS_SAFE_FREE(func_array[index]);
				}
				MS_SAFE_FREE(func_array);
				MS_SAFE_FREE(func_handle);

				MS_DBG_ERR("dlsym failed[%s]", func_list[func_index]);
				return MS_MEDIA_ERR_DYNAMIC_LINK;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

void ms_unload_functions(void)
{
	int lib_index;

	for (lib_index = 0; lib_index < lib_num; lib_index++)
		dlclose(func_handle[lib_index]);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
			if (func_array[lib_index]) {
				MS_SAFE_FREE(func_array[lib_index]);
			}
	}

	MS_SAFE_FREE(func_array);
	MS_SAFE_FREE(func_handle);

	if (so_array) {
		/*delete all node*/
		while (so_array->len != 0) {
			char *so_name = NULL;
			so_name = g_array_index(so_array , char*, 0);
			g_array_remove_index(so_array, 0);
			MS_SAFE_FREE(so_name);
		}
		g_array_free(so_array, FALSE);
		so_array = NULL;
	}
}

int ms_get_insert_count()
{
	return insert_count;
}

int ms_get_delete_count()
{
	return delete_count;
}

void ms_reset_insert_count()
{
	insert_count = 0;
}

void ms_reset_delete_count()
{
	delete_count = 0;
}

int ms_connect_db(void ***handle, uid_t uid)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	/*Lock mutex for openning db*/
	g_mutex_lock(&db_mutex);

	MS_MALLOC(*handle, sizeof(void*) * lib_num);
	if (*handle == NULL) {
		MS_DBG_ERR("malloc failed");
		g_mutex_unlock(&db_mutex);
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CONNECT)func_array[lib_index][eCONNECT])(&((*handle)[lib_index]), uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			g_mutex_unlock(&db_mutex);

			return MS_MEDIA_ERR_DB_CONNECT_FAIL;
		}
	}

	MS_DBG_INFO("connect Media DB");

	g_mutex_unlock(&db_mutex);

	return MS_MEDIA_ERR_NONE;
}

int ms_disconnect_db(void ***handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DISCONNECT)func_array[lib_index][eDISCONNECT])((*handle)[lib_index], &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
		}
	}

	MS_SAFE_FREE(*handle);

	MS_DBG_INFO("Disconnect Media DB");

	return MS_MEDIA_ERR_NONE;
}

int ms_validate_item(void **handle, const char *storage_id, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	bool modified = FALSE;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		/*check exist in Media DB, If file is not exist, insert data in DB. */
		ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle[lib_index], storage_id, path, &modified, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_SAFE_FREE(err_msg);
			ret = ms_insert_item_batch(handle, storage_id, path, uid);
			if (ret != 0) {
				res = MS_MEDIA_ERR_DB_INSERT_FAIL;
			} else {
				insert_count++;
			}
		} else {
			if (modified == FALSE) {
				/*if meta data of file exist, change valid field to "1" */
				ret = ((SET_ITEM_VALIDITY)func_array[lib_index][eSET_VALIDITY])(handle[lib_index], storage_id, path, true, true, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
				}
			} else {
				/* the file has same name but it is changed, so we have to update DB */
				ret = ((DELETE_ITEM)func_array[lib_index][eDELETE_ITEM])(handle[lib_index], storage_id, path, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_DELETE_FAIL;
				} else {
					ret = ms_insert_item_batch(handle, storage_id, path, uid);
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

int ms_scan_validate_item(void **handle, const char *storage_id, const char *path, uid_t uid, int *insert_count_for_partial, int *set_count_for_partial)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	bool modified = FALSE;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		/*check exist in Media DB, If file is not exist, insert data in DB. */
		ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle[lib_index], storage_id, path, &modified, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_SAFE_FREE(err_msg);
			ret = ms_scan_item_batch(handle, storage_id, path, uid, insert_count_for_partial, set_count_for_partial);
			if (ret != 0) {
				res = MS_MEDIA_ERR_DB_INSERT_FAIL;
			} else {
				insert_count++;
				(*insert_count_for_partial)++;
			}
		} else {
			if (modified == FALSE) {
				/*if meta data of file exist, change valid field to "1" */
				ret = ((SET_ITEM_VALIDITY)func_array[lib_index][eSET_VALIDITY])(handle[lib_index], storage_id, path, true, true, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
				} else {
					(*set_count_for_partial)++;
				}
			} else {
				/* the file has same name but it is changed, so we have to update DB */
				ret = ((DELETE_ITEM)func_array[lib_index][eDELETE_ITEM])(handle[lib_index], storage_id, path, uid, &err_msg); /*dlopen*/
				if (ret != 0) {
					MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
					MS_SAFE_FREE(err_msg);
					res = MS_MEDIA_ERR_DB_DELETE_FAIL;
				} else {
					ret = ms_scan_item_batch(handle, storage_id, path, uid, insert_count_for_partial, set_count_for_partial);
					if (ret != 0) {
						res = MS_MEDIA_ERR_DB_INSERT_FAIL;
					} else {
						insert_count++;
						(*insert_count_for_partial)++;
					}
				}
			}
		}
	}

	return res;
}

int ms_validaty_change_all_items(void **handle, const char *storage_id, ms_storage_type_t store_type, bool validity , uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ALL_STORAGE_ITEMS_VALIDITY)func_array[lib_index][eSET_ALL_VALIDITY])(handle[lib_index], storage_id, store_type, validity, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return res;
}

int ms_insert_item_batch(void **handle, const char* storage_id, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	MS_DBG_FENTER();

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM)func_array[lib_index][eINSERT_BATCH])(handle[lib_index], storage_id, path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_scan_item_batch(void **handle, const char* storage_id, const char *path, uid_t uid, int *insert_count_for_partial, int *set_count_for_partial)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	MS_DBG_FENTER();

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_SCAN)func_array[lib_index][eINSERT_SCAN])(handle[lib_index], storage_id, path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_insert_item_pass2(void **handle, const char* storage_id, int storage_type, const char *path, int scan_type, int burst, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	//ms_storage_type_t storage_type;

	//storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_EXTRACT)func_array[lib_index][eUPDATE_EXTRACT])(handle[lib_index], storage_id, storage_type, scan_type, uid, path, burst, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_insert_item_immediately(void **handle, const char *storage_id, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_IMMEDIATELY)func_array[lib_index][eINSERT_ITEM_IMMEDIATELY])(handle[lib_index], storage_id, path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_insert_burst_item(void **handle, const char *storage_id, const char *path , uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_BURST_ITEM)func_array[lib_index][eINSERT_BURST])(handle[lib_index], storage_id, path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

bool ms_delete_all_items(void **handle, const char *storage_id, ms_storage_type_t store_type, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	/* To reset media db when differnet mmc is inserted. */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_ITEMS_IN_STORAGE)func_array[lib_index][eDELETE_ALL])(handle[lib_index], storage_id, store_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return false;
		}
	}

	return true;
}

bool ms_delete_invalid_items(void **handle, const char *storage_id, ms_storage_type_t store_type, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_INVALID_ITMES_IN_STORAGE)func_array[lib_index][eDELETE_INVALID_ITEMS])(handle[lib_index], storage_id, store_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return false;
		}
	}

	return true;
}

int ms_set_folder_item_validity(void **handle, const char *storage_id, const char *path, int validity, int recursive, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_FOLDER_ITEM_VALIDITY)func_array[lib_index][eSET_FOLDER_ITEM_VALIDITY])(handle[lib_index], storage_id, path, validity, recursive, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_delete_invalid_items_in_folder(void **handle, const char* storage_id, const char*path, bool is_recursive, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_INVALID_ITEMS_IN_FOLDER)func_array[lib_index][eDELETE_FOLDER])(handle[lib_index], storage_id, path, is_recursive, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_send_dir_update_noti(void **handle, const char* storage_id, const char*path, const char*folder_id, ms_noti_type_e noti_type, int pid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;
	char *get_path = NULL;

	if (MS_STRING_VALID(path)) {
		get_path = strndup(path, strlen(path));
	} else {
		MS_DBG_ERR("path is invalid string");
		return MS_MEDIA_ERR_SEND_NOTI_FAIL;
	}

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SEND_DIR_UPDATE_NOTI)func_array[lib_index][eSEND_DIR_UPDATE_NOTI])(handle[lib_index], storage_id, get_path, folder_id, (int)noti_type, pid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			MS_SAFE_FREE(get_path);
			return MS_MEDIA_ERR_SEND_NOTI_FAIL;
		}
	}
	MS_SAFE_FREE(get_path);

	return MS_MEDIA_ERR_NONE;
}

int ms_count_delete_items_in_folder(void **handle, const char *storage_id, const char*path, int *count)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((COUNT_DELETE_ITEMS_IN_FOLDER)func_array[lib_index][eCOUNT_DELETE_ITEMS_IN_FOLDER])(handle[lib_index], storage_id, path, count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	delete_count = *count;

	return MS_MEDIA_ERR_NONE;
}

int ms_get_folder_list(void **handle, const char* storage_id, char* start_path, GArray **dir_array)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	char **folder_list = NULL;
	int *modified_time_list = NULL;
	int *item_num_list = NULL;
	int count = 0;
	int i = 0;

	ms_dir_info_s* dir_info = NULL;
	MS_DBG("start path: %s", start_path);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_FOLDER_LIST)func_array[lib_index][eGET_FOLDER_LIST])(handle[lib_index], storage_id, start_path, &folder_list, &modified_time_list, &item_num_list, &count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	*dir_array = g_array_new(FALSE, FALSE, sizeof(ms_dir_info_s*));
	if (count != 0) {
		for (i = 0; i < count; i++) {
			dir_info = malloc(sizeof(ms_dir_info_s));
			if (dir_info != NULL) {
				dir_info->dir_path = strdup(folder_list[i]);
				dir_info->modified_time = modified_time_list[i];
				dir_info->item_num = item_num_list[i];
				g_array_append_val(*dir_array, dir_info);
			} else {
				MS_DBG_ERR("malloc failed");
				ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
				goto ERROR;
			}
		}
	}

	for (i = 0; i < count; i++) {
		MS_SAFE_FREE(folder_list[i]);
	}

	MS_SAFE_FREE(folder_list);
	MS_SAFE_FREE(modified_time_list);

	return MS_MEDIA_ERR_NONE;
ERROR:

	if (*dir_array) {
		while ((*dir_array)->len != 0) {
			ms_dir_info_s *data = NULL;
			data = g_array_index(*dir_array , ms_dir_info_s*, 0);
			g_array_remove_index(*dir_array, 0);
			MS_SAFE_FREE(data->dir_path);
		}
		g_array_free(*dir_array, FALSE);
		*dir_array = NULL;
	}

	for (i = 0; i < count; i++) {
		MS_SAFE_FREE(folder_list[i]);
	}

	MS_SAFE_FREE(folder_list);
	MS_SAFE_FREE(modified_time_list);

	return ret;
}

int ms_update_folder_time(void **handle, const char *storage_id, char *folder_path, uid_t uid)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_FOLDER_TIME)func_array[lib_index][eUPDATE_FOLDER_TIME])(handle[lib_index], storage_id, folder_path, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_get_storage_id(void **handle, const char *path, char *storage_id)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_STORAGE_ID)func_array[lib_index][eGET_STORAGE_ID])(handle[lib_index], path, storage_id, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("storage_id [%s]", storage_id);

	return res;
}

int ms_get_storage_scan_status(void **handle, char *storage_id, media_scan_status_e *scan_status)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	int status = 0;

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_STORAGE_SCAN_STATUS)func_array[lib_index][eGET_STORAGE_SCAN_STATUS])(handle[lib_index], storage_id, &status, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INTERNAL;
		} else {
			*scan_status = status;
		}
	}

	MS_DBG("storage_id [%s], scan_status [%d]", storage_id, *scan_status);

	return res;
}

int ms_set_storage_scan_status(void **handle, char *storage_id, media_scan_status_e scan_status, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	int status = scan_status;

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_STORAGE_SCAN_STATUS)func_array[lib_index][eSET_STORAGE_SCAN_STATUS])(handle[lib_index], storage_id, status, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("storage_id [%s], scan_status [%d]", storage_id, scan_status);

	return res;
}

int ms_get_storage_list(void **handle, GArray **storage_array)
{
	int lib_index;
	int ret;
	char *err_msg = NULL;

	char **storage_list = NULL;
	char **storage_id_list = NULL;
	int *scan_status = NULL;
	int count = 0;
	int i = 0;

	ms_stg_info_s* stg_info = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_STORAGE_LIST)func_array[lib_index][eGET_STORAGE_LIST])(handle[lib_index], &storage_list, &storage_id_list, &scan_status, &count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	MS_DBG_ERR("OK");

	*storage_array = g_array_new(FALSE, FALSE, sizeof(ms_stg_info_s*));
	if (count != 0) {
		for (i = 0; i < count; i++) {
			stg_info = malloc(sizeof(ms_stg_info_s));
			stg_info->stg_path = strdup(storage_list[i]);
			stg_info->storage_id = strdup(storage_id_list[i]);
			stg_info->scan_status = scan_status[i];
			g_array_append_val(*storage_array, stg_info);
			MS_SAFE_FREE(storage_list[i]);
			MS_SAFE_FREE(storage_id_list[i]);
			MS_DBG("%d get path : %s, %s", i, stg_info->stg_path, stg_info->storage_id);
		}
	}

	MS_SAFE_FREE(storage_list);
	MS_SAFE_FREE(storage_id_list);
	MS_SAFE_FREE(scan_status);

	return MS_MEDIA_ERR_NONE;
}

int ms_insert_folder(void **handle, const char *storage_id, const char *path, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;
	storage_type = ms_get_storage_type_by_full(path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_FOLDER)func_array[lib_index][eINSERT_FOLDER])(handle[lib_index], storage_id, path, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_delete_invalid_folder(void **handle, const char *storage_id, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_INVALID_FOLDER)func_array[lib_index][eDELETE_INVALID_FOLDER])(handle[lib_index], storage_id, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_set_folder_validity(void **handle, const char *storage_id, const char *path, int validity, bool is_recursive, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_FOLDER_VALIDITY)func_array[lib_index][eSET_FOLDER_VALIDITY])(handle[lib_index], storage_id, path, validity, is_recursive, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_check_db_upgrade(void **handle, uid_t uid)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHECK_DB)func_array[lib_index][eCHECK_DB])(handle[lib_index], uid, &err_msg);
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}
	MS_DBG_FLEAVE();

	return res;
}

int ms_genarate_uuid(void **handle, char **uuid)
{
	int lib_index = 0;
	int ret;
	char * err_msg = NULL;

	ret = ((GET_UUID)func_array[lib_index][eGET_UUID])(handle[lib_index], uuid, &err_msg); /*dlopen*/
	if (ret != 0) {
		MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
		MS_SAFE_FREE(err_msg);
		return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_get_mmc_info(void **handle, char **storage_name, char **storage_path, int *validity, bool *info_exist)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret = 0;
	char *err_msg = NULL;

	MS_DBG("ms_get_mmc_info Start");

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_MMC_INFO)func_array[lib_index][eGET_MMC_INFO])(handle[lib_index], storage_name, storage_path, validity, info_exist, &err_msg);
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("ms_get_mmc_info End");

	return res;
}

int ms_check_storage(void **handle, const char *storage_id, const char *storage_name, char **storage_path, int *validity)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret = 0;
	char *err_msg = NULL;

	MS_DBG("ms_check_storage Start");

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHECK_STORAGE)func_array[lib_index][eCHECK_STORAGE])(handle[lib_index], storage_id, storage_name, storage_path, validity, &err_msg);
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_INTERNAL;
		}
	}

	MS_DBG("ms_check_storage End");

	return res;
}

int ms_insert_storage(void **handle, const char *storage_id, const char *storage_name, const char *storage_path, uid_t uid)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret = 0;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	MS_DBG("ms_insert_storage Start");

	storage_type = ms_get_storage_type_by_full(storage_path, uid);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_STORAGE)func_array[lib_index][eINSERT_STORAGE])(handle[lib_index], storage_id, storage_type, storage_name, storage_path, uid, &err_msg);
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("ms_insert_storage End");

	return res;
}

int ms_update_storage(void **handle, const char *storage_id, const char *storage_path, uid_t uid)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret = 0;
	char *err_msg = NULL;

	MS_DBG("ms_update_storage Start");

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_STORAGE)func_array[lib_index][eUPDATE_STORAGE])(handle[lib_index], storage_id, storage_path, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("ms_update_storage End");

	return res;
}

int ms_delete_storage(void **handle, const char *storage_id, const char *storage_name, uid_t uid)
{
	int lib_index = 0;
	int res = MS_MEDIA_ERR_NONE;
	int ret = 0;
	char *err_msg = NULL;

	MS_DBG("ms_delete_storage Start");

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_STORAGE)func_array[lib_index][eDELETE_STORAGE])(handle[lib_index], storage_id, storage_name, uid, &err_msg);
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("ms_delete_storage End");

	return res;
}

int ms_set_storage_validity(void **handle, const char *storage_id, int validity, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_STORAGE_VALIDITY)func_array[lib_index][eSET_STORAGE_VALIDITY])(handle[lib_index], storage_id, validity, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return res;
}

int ms_set_all_storage_validity(void **handle, int validity, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ALL_STORAGE_VALIDITY)func_array[lib_index][eSET_ALL_STORAGE_VALIDITY])(handle[lib_index], validity, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	return res;
}

int ms_update_meta_batch(void **handle, const char *path, const char *storage_id, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_storage_type_t storage_type;

	storage_type = ms_get_storage_type_by_full(path, uid);

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_META)func_array[lib_index][eUPDATE_ITEM_META])(handle[lib_index], path, storage_id, storage_type, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, path);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_delete_invalid_folder_by_path(void **handle, const char *storage_id, const char *folder_path, uid_t uid, int *delete_count)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_INVALID_FOLDER_BY_PATH)func_array[lib_index][eDELETE_INVALID_FOLDER_BY_PATH])(handle[lib_index], storage_id, folder_path, uid, delete_count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_check_folder_exist(void **handle, const char *storage_id, const char *folder_path)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHECK_FOLDER_EXIST)func_array[lib_index][eCHECK_FOLDER_EXIST])(handle[lib_index], storage_id, folder_path, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

int ms_check_subfolder_count(void **handle, const char *storage_id, const char *folder_path, int *count)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	int cnt = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((COUNT_SUBFOLDER)func_array[lib_index][eCOUNT_SUBFOLDER])(handle[lib_index], storage_id, folder_path, &cnt, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	*count = cnt;

	return res;
}

int ms_get_folder_id(void **handle, const char *storage_id, const char *path, char **folder_id)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	char folder_uuid[MS_UUID_SIZE] = {0,};

	MS_DBG_FENTER();

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_FOLDER_ID)func_array[lib_index][eGET_FOLDER_ID])(handle[lib_index], storage_id, path, folder_uuid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	if (strlen(folder_uuid) == (MS_UUID_SIZE-1)) {
		*folder_id = strdup(folder_uuid);
	} else {
		*folder_id = NULL;
	}

	MS_DBG("folder_id [%s]", folder_id);

	return res;
}

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/

void ms_register_start(void **handle, ms_noti_switch_e noti_status, int pid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_BEGIN)func_array[lib_index][eINSERT_BEGIN])(handle[lib_index], MSC_REGISTER_COUNT, noti_status, pid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_register_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_END)func_array[lib_index][eINSERT_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_END)func_array[lib_index][eUPDATE_END])(uid);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_validate_start(void **handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_BEGIN)func_array[lib_index][eSET_VALIDITY_BEGIN])(handle[lib_index], MSC_VALID_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_validate_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_END)func_array[lib_index][eSET_VALIDITY_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_insert_folder_start(void **handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_FOLDER_BEGIN)func_array[lib_index][eINSERT_FOLDER_BEGIN])(handle[lib_index], MSC_REGISTER_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_insert_folder_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_FOLDER_END)func_array[lib_index][eINSERT_FOLDER_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_update_start(void **handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_BEGIN)func_array[lib_index][eUPDATE_ITEM_BEGIN])(handle[lib_index], MSC_VALID_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

void ms_update_end(void **handle, uid_t uid)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((UPDATE_ITEM_END)func_array[lib_index][eUPDATE_ITEM_END])(handle[lib_index], uid, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
		}
	}
}

int ms_get_folder_scan_status(void **handle, const char *storage_id, const char *path, int *scan_status)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	int status = 0;

	//MS_DBG("");
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_FOLDER_SCAN_STATUS)func_array[lib_index][eGET_FOLDER_SCAN_STATUS])(handle[lib_index], storage_id, path, &status, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		} else {
			*scan_status = status;
		}
	}

	MS_DBG("OK path = [%s],  scan_status = [%d]", path, *scan_status);

	return res;
}

int ms_set_folder_scan_status(void **handle, const char *storage_id, const char *path, int scan_status, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	int status = scan_status;

	//MS_DBG("");
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_FOLDER_SCAN_STATUS)func_array[lib_index][eSET_FOLDER_SCAN_STATUS])(handle[lib_index], storage_id, path, status, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	MS_DBG("OK path = [%s],  scan_status = [%d]", path, scan_status);

	return res;
}

int ms_check_folder_modified(void **handle, const char *path, const char *storage_id, bool *modified)
{
	MS_DBG("path = [%s], storage_id = [%s]", path, storage_id);

	int lib_index;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHECK_FOLDER_MODIFIED)func_array[lib_index][eCHECK_FOLDER_MODIFIED])(handle[lib_index], path, storage_id, modified, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_get_null_scan_folder_list(void **handle, const char *stroage_id, const char *path, GArray **dir_array)
{
	//MS_DBG("folder stroage_id: %s", stroage_id);

	int lib_index;
	int ret;
	char *err_msg = NULL;
	char **folder_list = NULL;
	char *sub_path = NULL;
	int count = 0;
	int i = 0;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_NULL_SCAN_FOLDER_LIST)func_array[lib_index][eGET_NULL_SCAN_FOLDER_LIST])(handle[lib_index], stroage_id, path, &folder_list, &count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	//MS_DBG("GET_NULL_SCAN_FOLDER_LIST OK");

	*dir_array = g_array_new(FALSE, FALSE, sizeof(char*));
	if (count != 0) {
		for (i = 0; i < count; i++) {
			sub_path = strdup(folder_list[i]);
			g_array_append_val(*dir_array, sub_path);
			MS_SAFE_FREE(folder_list[i]);
		}
	}

	MS_SAFE_FREE(folder_list);

	return MS_MEDIA_ERR_NONE;
}

int ms_change_validity_item_batch(void **handle, const char *storage_id, const char *path, int des_validity, int src_validity, uid_t uid)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHANGE_VALIDITY_ITEM_BATCH)func_array[lib_index][eCHANGE_VALIDITY_ITEM_BATCH])(handle[lib_index], storage_id, path, des_validity, src_validity, uid, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}

#if 0
int ms_get_scan_done_items(void **handle, const char *storage_id, const char *path, int validity, int *count)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((GET_SCAN_DONE_ITEMS)func_array[lib_index][eGET_SCAN_DONE_ITEMS])(handle[lib_index], storage_id, path, validity, count, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s] %s", g_array_index(so_array, char*, lib_index), err_msg, storage_id);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_INSERT_FAIL;
		}
	}

	return res;
}
#endif


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

#include <aul/aul.h>
#include <drm-service.h>

#include "media-server-utils.h"
#include "media-server-inotify.h"
#include "media-server-db-svc.h"

GMutex * db_mutex;
extern GMutex *queue_mutex;

GAsyncQueue* soc_queue;
GArray *reg_list;
GMutex *list_mutex;
GMutex *queue_mutex;

#define MS_REGISTER_COUNT 100 /*For bundle commit*/
#define MS_VALID_COUNT 100 /*For bundle commit*/
#define MS_MOVE_COUNT 100 /*For bundle commit*/

void **func_handle = NULL; /*dlopen handel*/

CHECK_ITEM f_check;
CONNECT f_connect;
DISCONNECT f_disconnect;
CHECK_ITEM_EXIST f_exist;
INSERT_ITEM_BEGIN f_insert_begin;
INSERT_ITEM_END f_insert_end;
INSERT_ITEM f_insert_batch;
INSERT_ITEM_IMMEDIATELY f_insert;
MOVE_ITEM_BEGIN f_move_begin;
MOVE_ITEM_END f_move_end;
MOVE_ITEM f_move;
SET_ALL_STORAGE_ITEMS_VALIDITY f_set_all_validity;
SET_ITEM_VALIDITY_BEGIN f_set_validity_begin;
SET_ITEM_VALIDITY_END f_set_validity_end;
SET_ITEM_VALIDITY f_set_validity;
DELETE_ITEM f_delete;
DELETE_ALL_ITEMS_IN_STORAGE f_delete_all;
DELETE_ALL_INVALID_ITMES_IN_STORAGE f_delete_invalid_items;
UPDATE_BEGIN f_update_begin;
UPDATE_END f_update_end;

enum func_list {
	eCHECK,
	eCONNECT,
	eDISCONNECT,
	eEXIST,
	eINSERT_BEGIN,
	eINSERT_END,
	eINSERT_BATCH,
	eINSERT,
	eMOVE_BEGIN,
	eMOVE_END,
	eMOVE,
	eSET_ALL_VALIDITY,
	eSET_VALIDITY_BEGIN,
	eSET_VALIDITY_END,
	eSET_VALIDITY,
	eDELETE,
	eDELETE_ALL,
	eDELETE_INVALID_ITEMS,
	eUPDATE_BEGIN,
	eUPDATE_END,
	eFUNC_MAX
};

static void
_ms_insert_reg_list(const char *path)
{
	char *reg_path = strdup(path);

	g_mutex_lock(list_mutex);

	g_array_append_val(reg_list, reg_path);

	g_mutex_unlock(list_mutex);
}

static bool
_ms_find_reg_list(const char *path)
{
	int list_index;
	int len = reg_list->len;
	char *data;
	bool res = false;

	g_mutex_lock(list_mutex);
	MS_DBG("array length : %d", len);

	for(list_index = 0; list_index < len; list_index++) {
		data = g_array_index (reg_list, char*, list_index);
		if(!strcmp(data, path))
			res = true;
	}

	g_mutex_unlock(list_mutex);

	return res;
}

static void
_ms_delete_reg_list(const char *path)
{
	int list_index;
	int len = reg_list->len;
	char *data;

	MS_DBG("Delete : %s", path);
	g_mutex_lock(list_mutex);

	for(list_index = 0; list_index < len; list_index++) {
		data = g_array_index (reg_list, char*, list_index);
		MS_DBG("%s", data);
		if(!strcmp(data, path))	{
			MS_DBG("Delete complete : %s", data);
			free(data);
			g_array_remove_index(reg_list, list_index);
			break;
		}
	}

	g_mutex_unlock(list_mutex);
}

static int
_ms_get_mime_in_drm_info(const char *path, char *mime)
{
	int ret;
	drm_content_info_t contentInfo = { 0 };

	if (path == NULL || mime == NULL)
		return MS_ERR_ARG_INVALID;

	ret = drm_svc_get_content_info(path, &contentInfo);
	if (ret != DRM_RESULT_SUCCESS) {
		MS_DBG("drm_svc_get_content_info() fails. ");
		return MS_ERR_DRM_GET_INFO_FAIL;
	}

	strncpy(mime, contentInfo.contentType, 100);
	MS_DBG("DRM contentType : %s", contentInfo.contentType);
	MS_DBG("DRM mime : %s", mime);

	return MS_ERR_NONE;
}

static int
_ms_get_mime(const char *path, char *mimetype)
{
	MS_DBG_START();

	int ret = 0;

	if (path == NULL)
		return MS_ERR_ARG_INVALID;

	/*get content type and mime type from file. */
	/*in case of drm file. */
	if (drm_svc_is_drm_file(path) == DRM_TRUE) {
		DRM_FILE_TYPE drm_type = DRM_FILE_TYPE_NONE;
		drm_type = drm_svc_get_drm_type(path);
		if (drm_type == DRM_FILE_TYPE_NONE) {
			MS_DBG("There is no TYPE");
			return MS_ERR_DRM_GET_TYPE_FAIL;
		} else {
			ret =  _ms_get_mime_in_drm_info(path, mimetype);
			if (ret != 0) {
				MS_DBG("Fail to get mime");
				return ret;
			}
		}
	} else {
		/*in case of normal files */
		if (aul_get_mime_from_file(path, mimetype, 255) < 0) {
			MS_DBG("aul_get_mime_from_file fail");
			return MS_ERR_ARG_INVALID;
		}
	}

	MS_DBG("mime type : %s", mimetype);

	MS_DBG_END();

	return MS_ERR_NONE;
}

#define CONFIG_PATH "/opt/data/file-manager-service/plugin-config"
#define EXT ".so"
#define EXT_LEN 3

GArray *so_array;
void ***func_array;
int lib_num;

static int
_ms_check_category(const char *path, const char *mimetype, int index)
{
	int ret;
	char *err_msg = NULL;

	ret = ((CHECK_ITEM)func_array[index][eCHECK])(path, mimetype, &err_msg);
	if (ret != 0)
		free(err_msg);

	return ret;
}

static int
_ms_drm_register(const char* path)
{
	MS_DBG("THIS IS DRM FILE");
	int res = MS_ERR_NONE;
	DRM_RESULT dres;

	ms_inoti_add_ignore_file(path);
	dres = drm_svc_register_file(path);
	if (dres != DRM_RESULT_SUCCESS) {
		MS_DBG("drm_svc_register_file error : %d", res);
		res = MS_ERR_DRM_REGISTER_FAIL;
	}

	return res;
}

static void
_ms_drm_unregister(const char* path)
{
	ms_ignore_file_info *ignore_file;

	drm_svc_unregister_file(path, false);

	ignore_file = ms_inoti_find_ignore_file(path);
	if (ignore_file != NULL)
		ms_inoti_delete_ignore_file(ignore_file);
}

static int
_ms_token_data(char *buf, char **name)
{
	int len;
	char* pos = NULL;

	pos = strstr(buf, EXT);
	if (pos == NULL) {
		MS_DBG("This is not shared object library.");
		name = NULL;
		return -1;
	} else {
		len = pos - buf + EXT_LEN;
		*name = strndup(buf, len);
		MS_DBG("%s", *name);
	}

	return 0;
}


static bool
_ms_load_config()
{
	char *cret;
	int ret;
	FILE *fp;
	char *so_name = NULL;
	char buf[256] = {0};

	fp = fopen(CONFIG_PATH, "rt");
	if (fp == NULL) {
		MS_DBG("fp is NULL");
		return MS_ERR_FILE_OPEN_FAIL;
	}
	while(1) {
		cret = fgets(buf, 256, fp);
		if (cret == NULL)
			break;

		ret = _ms_token_data(buf, &so_name);
		if (ret == 0) {
			/*add to array*/
			g_array_append_val(so_array, so_name);
			so_name = NULL;
		}
	}

	fclose(fp);

	return MS_ERR_NONE;
}

int
ms_load_functions(void)
{
	int lib_index = 0, func_index;
	char func_list[eFUNC_MAX][40] = {
		"check_item",
		"connect",
		"disconnect",
		"check_item_exist",
		"insert_item_begin",
		"insert_item_end",
		"insert_item",
		"insert_item_immediately",
		"move_item_begin",
		"move_item_end",
		"move_item",
		"set_all_storage_items_validity",
		"set_item_validity_begin",
		"set_item_validity_end",
		"set_item_validity",
		"delete_item",
		"delete_all_items_in_storage",
		"delete_all_invalid_items_in_storage",
		"update_begin",
		"update_end"
		};
	/*init array for adding name of so*/
	so_array = g_array_new(FALSE, FALSE, sizeof(char*));

	/*load information of so*/
	_ms_load_config();

	if(so_array->len == 0) {
		MS_DBG("There is no information for functions");
		return -1;
	}

	/*the number of functions*/
	lib_num = so_array->len;

	MS_DBG("The number of information of so : %d", lib_num);
	func_handle = malloc(sizeof(void*) * lib_num);

	while(lib_index < lib_num) {
		/*get handle*/
		MS_DBG("[name of so : %s]", g_array_index(so_array, char*, lib_index));
		func_handle[lib_index] = dlopen(g_array_index(so_array, char*, lib_index), RTLD_LAZY);
		if (!func_handle[lib_index]) {
			MS_DBG("%s", dlerror());
			return -1;
		}
		lib_index++;
	}

	dlerror();    /* Clear any existing error */

	/*allocate for array of functions*/
	func_array = malloc(sizeof(void*) * lib_num);
	for(lib_index = 0 ; lib_index < lib_num; lib_index ++) {
		func_array[lib_index] = malloc(sizeof(void*) * eFUNC_MAX);
	}

	/*add functions to array */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		for (func_index = 0; func_index < eFUNC_MAX ; func_index++) {
			func_array[lib_index][func_index] = dlsym(func_handle[lib_index], func_list[func_index]);
		}
	}

	MS_DBG("FUNCTIONS LOAD COMPLETE");

	return MS_ERR_NONE;
}

void
ms_unload_functions(void)
{
	int lib_index, func_index;

	for (lib_index = 0; lib_index < lib_num; lib_index ++)
		dlclose(func_handle[lib_index]);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		for (func_index = 0; func_index < eFUNC_MAX ; func_index++) {
			free(func_array[lib_index][func_index]);
		}
	}

	free(func_array);
	free(func_handle);
	g_array_free(so_array, TRUE);
}

int
ms_connect_db(void **handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	/*Lock mutex for openning db*/
	g_mutex_lock(db_mutex);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CONNECT)func_array[lib_index][eCONNECT])(handle, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			g_mutex_unlock(db_mutex);

			return MS_ERR_DB_CONNECT_FAIL;
		}
	}

	MS_DBG("connect Media DB");

	g_mutex_unlock(db_mutex);

	return MS_ERR_NONE;
}

int
ms_disconnect_db(void *handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DISCONNECT)func_array[lib_index][eDISCONNECT])(handle, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			return MS_ERR_DB_DISCONNECT_FAIL;
		}
	}

	MS_DBG("Disconnect Media DB");

	return MS_ERR_NONE;
}

int
ms_validate_item(void *handle, char *path)
{
	MS_DBG_START();

	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	char mimetype[255] = {0};
	ms_store_type_t storage_type;

	MS_DBG("%s", path);

	_ms_get_mime(path, mimetype);
	storage_type = ms_get_store_type_by_full(path);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		if (!_ms_check_category(path, mimetype, lib_index)) {
			/*check exist in Media DB, If file is not exist, insert data in DB. */
			ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle, path, storage_type, &err_msg); /*dlopen*/
			if (ret != 0) {
				MS_DBG("not exist in Music DB. insert data");
				MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
				free(err_msg);

				ret = ms_insert_item_batch(handle, path);
				if (ret != MS_ERR_NONE) {
					MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
					res = ret;
				}
			} else {
				/*if meta data of file exist, change valid field to "1" */
				MS_DBG("Item exist");

				ret = ((SET_ITEM_VALIDITY)func_array[lib_index][eSET_VALIDITY])(handle, path, true, mimetype, true, &err_msg); /*dlopen*/
				if (ret != 0) {
					MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
					free(err_msg);
					res = MS_ERR_DB_UPDATE_RECORD_FAIL;
				}
			}
		}
	}

	if (drm_svc_is_drm_file(path) == DRM_TRUE) {
		ret = _ms_drm_register(path);
	}

	MS_DBG_END();

	return res;
}

int
ms_invalidate_all_items(void *handle, ms_store_type_t store_type)
{
	MS_DBG_START();
	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ALL_STORAGE_ITEMS_VALIDITY)func_array[lib_index][eSET_ALL_VALIDITY])(handle, store_type, false, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			res = MS_ERR_DB_UPDATE_RECORD_FAIL;
		}
	}

	MS_DBG_END();

	return res;
}

int
ms_register_file(void *handle, const char *path, GAsyncQueue* queue)
{
	MS_DBG_START();
	MS_DBG("[%d]register file : %s", syscall(__NR_gettid), path);

	int res = MS_ERR_NONE;
	int ret;

	if (path == NULL) {
		MS_DBG("path == NULL");
		return MS_ERR_ARG_INVALID;
	}

	g_mutex_lock(queue_mutex);
	/*first request for this file*/
	if(!_ms_find_reg_list(path)) {
		/*insert registering file list*/
		_ms_insert_reg_list(path);
	} else {
		MS_DBG("______________________ALREADY INSERTING");
		if(queue != NULL) {
			MS_DBG("Need reply");
			soc_queue = queue;
		}
		g_mutex_unlock(queue_mutex);
		return MS_ERR_NOW_REGISTER_FILE;
	}
	g_mutex_unlock(queue_mutex);

	ret = ms_insert_item(handle, path);
	if (ret != MS_ERR_NONE) {
		MS_DBG("ms_media_db_insert error : %d", ret);
		int lib_index;
		char mimetype[255];
		ms_store_type_t storage_type;

		_ms_get_mime(path, mimetype);
		storage_type = ms_get_store_type_by_full(path);

		for (lib_index = 0; lib_index < lib_num; lib_index++) {
			/*check item is already inserted*/
			if (!_ms_check_category(path, mimetype, lib_index)) {
				char *err_msg = NULL;

				ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle, path, storage_type, &err_msg); /*dlopen*/
				if (ret == 0) {
					MS_DBG("Media Item exist");
					res = MS_ERR_NONE;
				} else {
					MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
					free(err_msg);
					res = MS_ERR_DB_INSERT_RECORD_FAIL;
				}
			}
		}
	}

	if (drm_svc_is_drm_file(path) == DRM_TRUE) {
		ret = _ms_drm_register(path);
	}

FREE_RESOURCE:
	g_mutex_lock(queue_mutex);

	_ms_delete_reg_list(path);

	if (soc_queue != NULL) {
		MS_DBG("%d", res);
		g_async_queue_push(soc_queue, GINT_TO_POINTER(res+MS_ERR_MAX));
		MS_DBG("Return OK");
	}
	soc_queue = NULL;
	g_mutex_unlock(queue_mutex);
	MS_DBG_END();
	return res;
}

int
ms_insert_item_batch(void *handle, const char *path)
{
	MS_DBG_START();

	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char mimetype[255] = {0};
	char *err_msg = NULL;
	ms_store_type_t storage_type;

	MS_DBG("%s", path);

	_ms_get_mime(path, mimetype);
	storage_type = ms_get_store_type_by_full(path);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		if (!_ms_check_category(path, mimetype, lib_index)) {
			ret = ((INSERT_ITEM)func_array[lib_index][eINSERT_BATCH])(handle, path, storage_type, mimetype, &err_msg); /*dlopen*/
			if (ret != 0) {
				MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
				free(err_msg);
				res = MS_ERR_DB_INSERT_RECORD_FAIL;
			}
		}
	}

	if (drm_svc_is_drm_file(path) == DRM_TRUE) {
		ret = _ms_drm_register(path);
		res = ret;
	}

	MS_DBG_END();
	return res;
}

int
ms_insert_item(void *handle, const char *path)
{
	MS_DBG_START();
	MS_DBG("%s", path);

	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char mimetype[255] = {0};
	char *err_msg = NULL;
	ms_store_type_t storage_type;

	_ms_get_mime(path, mimetype);
	storage_type = ms_get_store_type_by_full(path);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		if (!_ms_check_category(path, mimetype, lib_index)) {
			ret = ((INSERT_ITEM_IMMEDIATELY)func_array[lib_index][eINSERT])(handle, path, storage_type, mimetype, &err_msg); /*dlopen*/
			if (ret != 0) {
				MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
				free(err_msg);
				res = MS_ERR_DB_INSERT_RECORD_FAIL;
			}
		}
	}

	MS_DBG_END();
	return res;
}

int
ms_delete_item(void *handle, const char *path)
{
	MS_DBG_START();

	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	ms_store_type_t storage_type;

	storage_type = ms_get_store_type_by_full(path);
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CHECK_ITEM_EXIST)func_array[lib_index][eEXIST])(handle, path, storage_type, &err_msg); /*dlopen*/
		if (ret == 0) {
			ret = ((DELETE_ITEM)func_array[lib_index][eDELETE])(handle, path, storage_type, &err_msg); /*dlopen*/
			if (ret !=0 ) {
				MS_DBG("delete fail");
				MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
				free(err_msg);
				res = MS_ERR_DB_DELETE_RECORD_FAIL;
			}
		} else {
			MS_DBG("Item does not exist");
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			res = MS_ERR_DB_DELETE_RECORD_FAIL;
		}
	}

	_ms_drm_unregister(path);

	MS_DBG_END();

	return res;
}

int
ms_move_item(void *handle,
		ms_store_type_t src_store, ms_store_type_t dst_store,
		const char *src_path, const char *dst_path)
{
	MS_DBG_START();
	int lib_index;
	int res = MS_ERR_NONE;
	int ret;
	char mimetype[255];
	char *err_msg = NULL;

	_ms_get_mime(dst_path, mimetype);
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		if (!_ms_check_category(dst_path, mimetype, lib_index)) {
			ret = ((MOVE_ITEM)func_array[lib_index][eMOVE])(handle, src_path, src_store,
							dst_path, dst_store, mimetype, &err_msg); /*dlopen*/
			if (ret != 0) {
				MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
				free(err_msg);
				res = MS_ERR_DB_UPDATE_RECORD_FAIL;
			}
		}
	}

	MS_DBG_END();

	return res;
}

bool
ms_delete_all_items(void *handle, ms_store_type_t store_type)
{
	MS_DBG_START();
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	/* To reset media db when differnet mmc is inserted. */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_ITEMS_IN_STORAGE)func_array[lib_index][eDELETE_ALL])(handle, store_type, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			return false;
		}
	}
	MS_DBG_END();
	return true;
}

bool
ms_delete_invalid_items(void *handle, ms_store_type_t store_type)
{
	MS_DBG_START();
	int lib_index;
	int ret;
	char *err_msg = NULL;
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DELETE_ALL_INVALID_ITMES_IN_STORAGE)func_array[lib_index][eDELETE_INVALID_ITEMS])(handle, store_type, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
			return false;
		}
	}

	MS_DBG_END();

	return true;
}

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/

void
ms_register_start(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_BEGIN)func_array[lib_index][eINSERT_BEGIN])(handle, MS_REGISTER_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

void
ms_register_end(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((INSERT_ITEM_END)func_array[lib_index][eINSERT_END])(handle, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

void
ms_validate_start(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_BEGIN)func_array[lib_index][eSET_VALIDITY_BEGIN])(handle, MS_VALID_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

void
ms_validate_end(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ITEM_VALIDITY_END)func_array[lib_index][eSET_VALIDITY_END])(handle, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

void
ms_move_start(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((MOVE_ITEM_BEGIN)func_array[lib_index][eMOVE_BEGIN])(handle, MS_MOVE_COUNT, &err_msg);/*dlopen*/
		if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

void
ms_move_end(void *handle)
{
	int lib_index;
	int ret = 0;
	char *err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
	       ret = ((MOVE_ITEM_END)func_array[lib_index][eMOVE_END])(handle, &err_msg);/*dlopen*/
	   	if (ret != 0) {
			MS_DBG("error : %s", g_array_index(so_array, char*, lib_index));
			free(err_msg);
		}
	}
}

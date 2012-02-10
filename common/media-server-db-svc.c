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
#include <db-util.h>
#include <drm-service.h>
#include <media-thumbnail.h>
#include <visual-svc-error.h>
#include <visual-svc-types.h>
#include <visual-svc.h>
#include <audio-svc-error.h>
#include <audio-svc-types.h>
#include <audio-svc.h>

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

void _ms_insert_reg_list(const char *path)
{
	char *reg_path = strdup(path);

	g_mutex_lock(list_mutex);

	g_array_append_val(reg_list, reg_path);

	g_mutex_unlock(list_mutex);
}

bool _ms_find_reg_list(const char *path)
{
	int i;
	int len = reg_list->len;
	char *data;
	bool ret = false;

	g_mutex_lock(list_mutex);
	MS_DBG("array length : %d", len);

	for(i = 0; i < len; i++) {
		data = g_array_index (reg_list, char*, i);
		if(!strcmp(data, path))
			ret = true;
	}

	g_mutex_unlock(list_mutex);

	return ret;
}

void _ms_delete_reg_list(const char *path)
{
	int i;
	int len = reg_list->len;
	char *data;

	MS_DBG("Delete : %s", path);
	g_mutex_lock(list_mutex);

	for(i = 0; i < len; i++) {
		data = g_array_index (reg_list, char*, i);
		MS_DBG("%s", data);
		if(!strcmp(data, path))	{
			MS_DBG("Delete complete : %s", data);
			free(data);
			g_array_remove_index(reg_list, i);
			break;
		}
	}

	g_mutex_unlock(list_mutex);
}



int
ms_media_db_open(MediaSvcHandle **handle)
{
	int err;

	/*Lock mutex for openning db*/
	g_mutex_lock(db_mutex);

	err = media_svc_connect(handle);
	if (err != MEDIA_INFO_ERROR_NONE) {
		MS_DBG("media_svc_connect() error : %d", err);

		g_mutex_unlock(db_mutex);

		return MS_ERR_DB_CONNECT_FAIL;
	}

	MS_DBG("connect Media DB");

	g_mutex_unlock(db_mutex);

	return MS_ERR_NONE;
}

int
ms_media_db_close(MediaSvcHandle *handle)
{
	int err;

	err = media_svc_disconnect(handle);
	if (err != MEDIA_INFO_ERROR_NONE) {
		MS_DBG("media_svc_disconnect() error : %d", err);
		return MS_ERR_DB_DISCONNECT_FAIL;
	}

	MS_DBG("Disconnect Media DB");

	return MS_ERR_NONE;
}

int
ms_update_valid_type(MediaSvcHandle *handle, char *path)
{
	MS_DBG_START();

	int res = MS_ERR_NONE;
	int err;
	int category = MS_CATEGORY_UNKNOWN;

	MS_DBG("%s", path);

	err = ms_get_category_from_mime(path, &category);
	if (err < 0) {
		MS_DBG("ms_get_category_from_mime fails");
		return err;
	}

	/*if music, call mp_svc_set_item_valid() */
	if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		/*check exist in Music DB, If file is not exist, insert data in DB. */
		err = audio_svc_check_item_exist(handle, path);
		if (err == AUDIO_SVC_ERROR_DB_NO_RECORD) {
			MS_DBG("not exist in Music DB. insert data");
#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(handle, path, category);
#else
			err = ms_media_db_insert(handle, path, category, true);
#endif
			if (err != MS_ERR_NONE) {
				MS_DBG ("ms_media_db_insert() error %d", err);
				res = err;
			}
		}
		else if (err == AUDIO_SVC_ERROR_NONE) {
			/*if meta data of file exist, change valid field to "1" */
			MS_DBG("Item exist");

			err = audio_svc_set_item_valid(handle, path, true);
			if (err != AUDIO_SVC_ERROR_NONE)
				MS_DBG("audio_svc_set_item_valid() error : %d", err);
		}
		else
		{
			MS_DBG("audio_svc_check_item_exist() error : %d", err);
			res = MS_ERR_DB_OPERATION_FAIL;
		}
	}
	/*if file is image file, call mb_svc_set_db_item_valid() */
	else if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
		ms_store_type_t store_type;
		minfo_store_type mb_stroage;

		store_type = ms_get_store_type_by_full(path);
		mb_stroage = (store_type == MS_MMC) ? MINFO_MMC : MINFO_PHONE;

		err = minfo_set_item_valid(handle, mb_stroage, path, true);
		if (err != MB_SVC_ERROR_NONE) {
			MS_DBG("not exist in Media DB. insert data");

			ms_update_valid_type_end(handle);

#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(handle, path, category);
#else
			err = ms_media_db_insert(handle, path, category, true);
#endif
			if (err != MS_ERR_NONE)
				MS_DBG("ms_media_db_insert() error : %d", err);

			ms_update_valid_type_start(handle);
		}
		else {
			MS_DBG("Item exist");
		}
	}

	if (category & MS_CATEGORY_DRM) {
		DRM_RESULT drm_res;

		ms_inoti_add_ignore_file(path);
		drm_res = drm_svc_register_file(path);
		if (drm_res != DRM_RESULT_SUCCESS) {
			MS_DBG("drm_svc_register_file error : %d", drm_res);
			res = MS_ERR_DRM_REGISTER_FAIL;
		}
	}

	MS_DBG_END();

	return res;
}

int
ms_change_valid_type(MediaSvcHandle *handle, ms_store_type_t store_type, bool validation)
{
	MS_DBG_START();
	int res = MS_ERR_NONE;
	int err;

	audio_svc_storage_type_e audio_storage;
	minfo_store_type visual_storage;

	audio_storage = (store_type == MS_PHONE)
		? AUDIO_SVC_STORAGE_PHONE : AUDIO_SVC_STORAGE_MMC;
	visual_storage = (store_type == MS_PHONE)
		? MINFO_PHONE : MINFO_MMC;

	err = audio_svc_set_db_valid(handle, audio_storage, validation);
	if (err < AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_set_db_valid error :%d", err);
		res = MS_ERR_DB_OPERATION_FAIL;
	}

	err = minfo_set_db_valid(handle, visual_storage, validation);
	if (err < MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_set_db_valid : error %d", err);
		res = MS_ERR_DB_OPERATION_FAIL;
	}

	MS_DBG_END();

	return res;
}


int
ms_register_file(MediaSvcHandle *handle, const char *path, GAsyncQueue* queue)
{
	MS_DBG_START();
	MS_DBG("[%d]register file : %s", syscall(__NR_gettid), path);

	int err;
	int category = MS_CATEGORY_UNKNOWN;

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

	err = ms_get_category_from_mime(path, &category);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error : %d", err);
		goto FREE_RESOURCE;
	}

	if (category <= MS_CATEGORY_ETC) {
		MS_DBG("This is not media contents");
		err = MS_ERR_NOT_MEDIA_FILE;
		goto FREE_RESOURCE;
	} else {
#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(handle, path, category);
#else
			err = ms_media_db_insert(handle, path, category, false);
#endif
		if (err != MS_ERR_NONE) {
			MS_DBG("ms_media_db_insert error : %d", err);

			/*if music, call mp_svc_set_item_valid() */
			if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
				/*check exist in Music DB, If file is not exist, insert data in DB. */
				err = audio_svc_check_item_exist(handle, path);
				if (err == AUDIO_SVC_ERROR_NONE) {
					MS_DBG("Audio Item exist");
					err = MS_ERR_NONE;
				}
			} else if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
				Mitem *mi = NULL;

				/*get an item based on its url. */
				err = minfo_get_item(handle, path, &mi);
				if (err == MB_SVC_ERROR_NONE) {
					MS_DBG("Visual Item exist");
					err = MS_ERR_NONE;
				}

				minfo_destroy_mtype_item(mi);
			}
		}

		if (category & MS_CATEGORY_DRM) {
			MS_DBG("THIS IS DRM FILE");
			DRM_RESULT res;

			ms_inoti_add_ignore_file(path);
			res = drm_svc_register_file(path);
			if (res != DRM_RESULT_SUCCESS) {
				MS_DBG("drm_svc_register_file error : %d", res);
				err = MS_ERR_DRM_REGISTER_FAIL;
			}
		}
	}

FREE_RESOURCE:
	g_mutex_lock(queue_mutex);

	_ms_delete_reg_list(path);

	if (soc_queue != NULL) {
		MS_DBG("%d", err);
		g_async_queue_push(soc_queue, GINT_TO_POINTER(err+MS_ERR_MAX));
		MS_DBG("Return OK");
	}
	soc_queue = NULL;
	g_mutex_unlock(queue_mutex);
	MS_DBG_END();
	return err;
}

int
ms_register_scanfile(MediaSvcHandle *handle, const char *path)
{
	MS_DBG_START();
	MS_DBG("register scanfile : %s", path);

	int err = MS_ERR_NONE;
	int category = MS_CATEGORY_UNKNOWN;

	if (path == NULL) {
		MS_DBG("path == NULL");
		return MS_ERR_ARG_INVALID;
	}

	err = ms_get_category_from_mime(path, &category);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error : %d", err);
		return err;
	}

	if (category <= MS_CATEGORY_ETC) {
		MS_DBG("This is not media contents");
		return MS_ERR_NOT_MEDIA_FILE;
	}

	err = ms_media_db_insert(handle, path, category, true);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_media_db_insert error : %d", err);
	}

	if (category & MS_CATEGORY_DRM) {
		MS_DBG("THIS IS DRM FILE");
		DRM_RESULT res;

		ms_inoti_add_ignore_file(path);

		res = drm_svc_register_file(path);
		if (res != DRM_RESULT_SUCCESS) {
			MS_DBG("drm_svc_register_file error : %d", err);
			err = MS_ERR_DRM_REGISTER_FAIL;
		}
	}

	MS_DBG_END();
	return err;
}

#ifdef THUMB_THREAD
int
ms_media_db_insert_with_thumb(MediaSvcHandle *handle, const char *path, int category)
{
	MS_DBG_START();
	MS_DBG("%s", path);

	int ret = MS_ERR_NONE;
	int err;
	ms_store_type_t store_type;
	audio_svc_category_type_e audio_category;
	minfo_file_type visual_category;
	audio_svc_storage_type_e storage;

	if (category & MS_CATEGORY_VIDEO ||category & MS_CATEGORY_IMAGE) {
		visual_category = (category & MS_CATEGORY_IMAGE)
			? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		err = minfo_add_media(handle, path, visual_category);
		if (err < 0) {
			MS_DBG(" minfo_add_media error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		} else {
			char thumb_path[1024];

			err = thumbnail_request_from_db(path, thumb_path, sizeof(thumb_path));
			if (err < 0) {
				MS_DBG("thumbnail_request_from_db falied: %d", err);
			} else {
				MS_DBG("thumbnail_request_from_db success: %s", thumb_path);
			}
		}

		MS_DBG("IMAGE");
	}
	else if (category & MS_CATEGORY_SOUND || category & MS_CATEGORY_MUSIC) {
		store_type = ms_get_store_type_by_full(path);

		storage = (store_type == MS_MMC)
			? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		audio_category = (category & MS_CATEGORY_SOUND)
			? AUDIO_SVC_CATEGORY_SOUND : AUDIO_SVC_CATEGORY_MUSIC;

		err = audio_svc_insert_item(handle, storage, path, audio_category);
		if (err < 0) {
			MS_DBG("mp_svc_insert_item fails error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		}
		MS_DBG("SOUND");
	}

	MS_DBG_END();
	return ret;
}
#endif

int
ms_media_db_insert(MediaSvcHandle *handle, const char *path, int category, bool bulk)
{
	MS_DBG_START();
	MS_DBG("%s", path);

	int ret = MS_ERR_NONE;
	int err;
	ms_store_type_t store_type;
	audio_svc_category_type_e audio_category;
	minfo_file_type visual_category;
	audio_svc_storage_type_e storage;

	if (category & MS_CATEGORY_VIDEO ||category & MS_CATEGORY_IMAGE) {
		visual_category = (category & MS_CATEGORY_IMAGE)
			? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		if(bulk)
			err = minfo_add_media_batch(handle, path, visual_category);
		else
			err = minfo_add_media(handle, path, visual_category);
		if (err < 0) {
			MS_DBG(" minfo_add_media error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		}
#ifndef THUMB_THREAD
		else {
			char thumb_path[1024];

			err = thumbnail_request_from_db(path, thumb_path, sizeof(thumb_path));
			if (err < 0) {
				MS_DBG("thumbnail_request_from_db falied: %d", err);
			} else {
				MS_DBG("thumbnail_request_from_db success: %s", thumb_path);
			}
#endif
		}

		MS_DBG("IMAGE");
	}
	else if (category & MS_CATEGORY_SOUND || category & MS_CATEGORY_MUSIC) {
		store_type = ms_get_store_type_by_full(path);

		storage = (store_type == MS_MMC)
			? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		audio_category = (category & MS_CATEGORY_SOUND)
			? AUDIO_SVC_CATEGORY_SOUND : AUDIO_SVC_CATEGORY_MUSIC;

		err = audio_svc_insert_item(handle, storage, path, audio_category);
		if (err < 0) {
			MS_DBG("mp_svc_insert_item fails error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		}
		MS_DBG("SOUND");
	}

	MS_DBG_END();
	return ret;
}



int
ms_check_file_exist_in_db(MediaSvcHandle *handle, const char *path)
{
	int err;
	int category;

	/*get an item based on its url. */
	err = minfo_check_item_exist(handle, path);
	if (err != MS_ERR_NONE) {
		err = audio_svc_check_item_exist(handle, path);
		if (err != MS_ERR_NONE)
			category = MS_CATEGORY_UNKNOWN;
		else
			category = MS_CATEGORY_MUSIC;
	} else {
		category = MS_CATEGORY_IMAGE;
	}

	MS_DBG("Category : %d", category);

	return category;
}

int
ms_media_db_delete(MediaSvcHandle *handle, const char *path)
{
	MS_DBG_START();
	int ret = MS_ERR_NONE;
	int category;
	ms_ignore_file_info *ignore_file;

	category = ms_check_file_exist_in_db(handle, path);

	if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
		ret = minfo_delete_media(handle, path);
		if (ret != MS_ERR_NONE) {
			MS_DBG("minfo_delete_media error : %d", ret);
			return ret;
		}
		MS_DBG("VIDEO or IMAGE");
	} else if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		ret = audio_svc_delete_item_by_path(handle, path);
		if (ret != MS_ERR_NONE) {
			MS_DBG("audio_svc_delete_item_by_path error : %d", ret);
			return ret;
		}
		MS_DBG("MUSIC or SOUND");
	}

	drm_svc_unregister_file(path, false);

	ignore_file = ms_inoti_find_ignore_file(path);
	if (ignore_file != NULL)
		ms_inoti_delete_ignore_file(ignore_file);

	MS_DBG_END();

	return ret;
}

int
ms_media_db_move(MediaSvcHandle *handle,
		ms_store_type_t src_store, ms_store_type_t dst_store,
		const char *src_path, const char *dst_path)
{
	MS_DBG_START();

	int category = MS_CATEGORY_UNKNOWN;
	minfo_file_type visual_category;
	audio_svc_storage_type_e dst_storage;
	audio_svc_storage_type_e src_storage;
	int ret = 0;

	ret = ms_get_category_from_mime(dst_path, &category);
	if (ret != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error %d", ret);
		return ret;
	}

	MS_DBG("category = %d", category);

	if (category & MS_CATEGORY_IMAGE || category & MS_CATEGORY_VIDEO) {
		visual_category = (category & MS_CATEGORY_IMAGE) ? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		ret = minfo_move_media(handle, src_path, dst_path, visual_category);
		if (ret != MB_SVC_ERROR_NONE) {
			MS_DBG("minfo_move_media error : %d", ret);
			return ret;
		}
		MS_DBG("VISUAL");
	} else if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		src_storage = (src_store == MS_MMC) ? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		dst_storage = (dst_store == MS_MMC) ? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;

		ret = audio_svc_move_item(handle, src_storage, src_path, dst_storage, dst_path);
		if (ret < 0) {
			MS_DBG("mp_svc_move_item error : %d", ret);
			if (ret == AUDIO_SVC_ERROR_DB_NO_RECORD) {
				MS_DBG(" This is a new file, Add DB");
				/*if source file does not exist in DB, it is new file. */

				ret = ms_register_file(handle, dst_path, NULL);

				if (ret != MS_ERR_NONE) {
					MS_DBG("ms_register_file error : %d", ret);
					return MS_ERR_DB_INSERT_RECORD_FAIL;
				}
			}

			return ret;
		}
		MS_DBG("AUDIO");
	}

	MS_DBG_END();

	return ret;
}

bool
ms_delete_all_record(MediaSvcHandle *handle, ms_store_type_t store_type)
{
	MS_DBG_START();
	int err = 0;
	minfo_store_type visual_storage;
	audio_svc_storage_type_e audio_storage;

	visual_storage = (store_type == MS_PHONE) ? MINFO_PHONE : MINFO_MMC;
	audio_storage = (store_type == MS_PHONE) ? AUDIO_SVC_STORAGE_PHONE : AUDIO_SVC_STORAGE_MMC;

	/* To reset media db when differnet mmc is inserted. */
	err = audio_svc_delete_all(handle, audio_storage);
	if (err != AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_delete_all error : %d\n", err);
#if 0 /*except temporary*/
		return false;
#endif
	}

	err = minfo_delete_all_media_records(handle, visual_storage);
	if (err != MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_delete_all_media_records error : %d\n", err);
		return false;
	}

	MS_DBG_END();

	return true;
}


bool
ms_delete_invalid_records(MediaSvcHandle *handle, ms_store_type_t store_type)
{
	MS_DBG_START();
	int err;
	
	minfo_store_type visual_storage;
	audio_svc_storage_type_e audio_storage;

	visual_storage = (store_type == MS_PHONE) ? MINFO_PHONE : MINFO_MMC;
	audio_storage = (store_type == MS_PHONE) ? AUDIO_SVC_STORAGE_PHONE : AUDIO_SVC_STORAGE_MMC;

	err = audio_svc_delete_invalid_items(handle, audio_storage);
	if (err != AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_delete_invalid_items error : %d\n", err);
#if 0 /*except temporary*/
		return false;
#endif
	}

	err = minfo_delete_invalid_media_records(handle, visual_storage);
	if (err != MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_delete_invalid_media_records error : %d\n", err);
		return false;
	}

	MS_DBG_END();

	return true;
}

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/

void
ms_register_start(MediaSvcHandle *handle)
{
	minfo_add_media_start(handle, MS_REGISTER_COUNT);
	audio_svc_insert_item_start(handle, MS_REGISTER_COUNT);
}

void
ms_register_end(MediaSvcHandle *handle)
{
	minfo_add_media_end(handle);
	audio_svc_insert_item_end(handle);
}

void
ms_update_valid_type_start(MediaSvcHandle *handle)
{
	audio_svc_set_item_valid_start(handle, MS_VALID_COUNT);
	minfo_set_item_valid_start(handle, MS_VALID_COUNT);
}

void
ms_update_valid_type_end(MediaSvcHandle *handle)
{
	audio_svc_set_item_valid_end(handle);
	minfo_set_item_valid_end(handle);
}

void
ms_media_db_move_start(MediaSvcHandle *handle)
{
	audio_svc_move_item_start(handle, MS_MOVE_COUNT);
	minfo_move_media_start(handle, MS_MOVE_COUNT);
}

void
ms_media_db_move_end(MediaSvcHandle *handle)
{
       audio_svc_move_item_end(handle);
	minfo_move_media_end(handle);
}

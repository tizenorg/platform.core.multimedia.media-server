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
 * @file		media-server-db-svc.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_SERVER_DB_SVC_H_
#define _MEDIA_SERVER_DB_SVC_H_

#include "media-server-global.h"

typedef int (*CHECK_ITEM)(const char*, const char*, char **);
typedef int (*CONNECT)(void**, char **);
typedef int (*DISCONNECT)(void*, char **);
typedef int (*CHECK_ITEM_EXIST)(void*, const char*, int, char **);
typedef int (*INSERT_ITEM_BEGIN)(void*, int, char **);
typedef int (*INSERT_ITEM_END)(void*, char **);
typedef int (*INSERT_ITEM)(void*, const char*, int, const char*, char **);
typedef int (*INSERT_ITEM_IMMEDIATELY)(void*, const char*, int, const char*, char **);
typedef int (*MOVE_ITEM_BEGIN)(void*, int, char **);
typedef int (*MOVE_ITEM_END)(void*, char **);
typedef int (*MOVE_ITEM)(void*, const char*, int, const char*, int, const char*, char **);
typedef int (*SET_ALL_STORAGE_ITEMS_VALIDITY)(void*, int, int, char **);
typedef int (*SET_ITEM_VALIDITY_BEGIN)(void*, int, char **);
typedef int (*SET_ITEM_VALIDITY_END)(void*, char **);
typedef int (*SET_ITEM_VALIDITY)(void*, const char*, int, const char*, int, char **);
typedef int (*DELETE_ITEM)(void*, const char*, int, char **);
typedef int (*DELETE_ALL_ITEMS_IN_STORAGE)(void*, int, char **);
typedef int (*DELETE_ALL_INVALID_ITMES_IN_STORAGE)(void*, int, char **);
typedef int (*UPDATE_BEGIN)(char **);
typedef int (*UPDATE_END)(char **);
typedef int (*REFRESH_ITEM)(void*, const char *, int, const char*, char**);

int
ms_load_functions(void);

void
ms_unload_functions(void);

int
ms_connect_db(void ***handle);

int
ms_disconnect_db(void ***handle);

int
ms_validate_item(void **handle, char *path);

int
ms_register_file(void **handle, const char *path, GAsyncQueue* queue);

int
ms_insert_item_batch(void **handle, const char *path);

int
ms_insert_item(void **handle, const char *path);

int
ms_delete_item(void **handle, const char *full_file_path);

int
ms_move_item(void **handle,
			ms_storage_type_t src_store_type,
		     	ms_storage_type_t dest_store_type,
		     	const char *src_file_full_path,
		     	const char *dest_file_full_path);

bool
ms_delete_all_items(void **handle, ms_storage_type_t store_type);

int
ms_invalidate_all_items(void **handle, ms_storage_type_t table_id);

bool
ms_delete_invalid_items(void **handle, ms_storage_type_t store_type);

int
ms_refresh_item(void **handle, const char *path);

int
ms_check_exist(void **handle, const char *path);

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/

void
ms_register_start(void **handle);

void
ms_register_end(void **handle);

void
ms_move_start(void **handle);

void
ms_move_end(void **handle);

void
ms_validate_start(void **handle);

void
ms_validate_end(void **handle);

#endif /*_MEDIA_SERVER_DB_SVC_H_*/
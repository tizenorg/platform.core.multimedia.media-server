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
#include <media-svc.h>
#include "media-server-global.h"

int
ms_update_valid_type(MediaSvcHandle *handle, char *path);

int
ms_media_db_open(MediaSvcHandle **handle);

int
ms_media_db_close(MediaSvcHandle *handle);

void
ms_register_start(MediaSvcHandle *handle);

void
ms_register_end(MediaSvcHandle *handle);

int
ms_register_file(MediaSvcHandle *handle, const char *path, GAsyncQueue* queue);

int
ms_register_scanfile(MediaSvcHandle *handle, const char *path);

void
ms_update_valid_type_start(MediaSvcHandle *handle);

void
ms_update_valid_type_end(MediaSvcHandle *handle);

int
ms_change_valid_type(MediaSvcHandle *handle, ms_store_type_t table_id, bool validation);

#ifdef THUMB_THREAD
int
ms_media_db_insert_with_thumb(MediaSvcHandle *handle, const char *path, int category);
#endif

int
ms_media_db_insert(MediaSvcHandle *handle, const char *path, int category, bool bulk);

int
ms_check_file_exist_in_db(MediaSvcHandle *handle, const char *file_full_path);

int
ms_media_db_delete(MediaSvcHandle *handle, const char *full_file_path);

void
ms_media_db_move_start(MediaSvcHandle *handle);

void
ms_media_db_move_end(MediaSvcHandle *handle);

int
ms_media_db_move(MediaSvcHandle *handle,
			ms_store_type_t src_store_type,
		     	ms_store_type_t dest_store_type,
		     	const char *src_file_full_path,
		     	const char *dest_file_full_path);

bool
ms_delete_all_record(MediaSvcHandle *handle, ms_store_type_t store_type);

bool
ms_delete_invalid_records(MediaSvcHandle *handle, ms_store_type_t store_type);
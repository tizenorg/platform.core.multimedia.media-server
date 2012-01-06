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
 * @file		media-server-common.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef __FEXPLORER_ENGINE_DB_PUBLIC_H__
#define __FEXPLORER_ENGINE_DB_PUBLIC_H__

#include <sqlite3.h>
#include "media-server-global.h"

#define MS_SQL_MAX_LEN 4096

int ms_update_valid_type(char *path);

int ms_set_db_status(ms_db_status_type_t status);

int ms_db_init(bool need_db_create);

int ms_media_db_open(void);

int ms_media_db_close(void);

int ms_update_mmc_info(void);

void ms_mmc_removed_handler(void);

void ms_mmc_vconf_cb(void *data);

bool ms_is_mmc_supported(void);

ms_dir_scan_type_t ms_get_mmc_state(void);

void ms_usb_vconf_cb(void *data);

void ms_register_start(void);

void ms_register_end(void);

int ms_register_file(const char *path, GAsyncQueue* queue);

int ms_register_scanfile(const char *path);

int ms_start(bool need_db_create);

void ms_end(void);

int ms_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path);

ms_store_type_t ms_get_store_type_by_full(const char *path);

int ms_strappend(char *res, const int size, const char *pattern,
		 const char *str1, const char *str2);

int ms_strcopy(char *res, const int size, const char *pattern,
	       const char *str1);

#ifdef _WITH_SETTING_AND_NOTI_
bool ms_config_get_int(const char *key, int *value);

bool ms_config_set_int(const char *key, int value);

bool ms_config_get_str(const char *key, char *value);

bool ms_config_set_str(const char *key, const char *value);
#endif

void ms_check_db_updating(void);

int ms_get_category_from_mime(const char *path, int *category);

bool fex_make_default_path(void);

bool fex_make_default_path_mmc(void);

/****************************************************************************************************
LIBMEDIA_INFO
*****************************************************************************************************/
#ifdef _WITH_MP_PB_

void ms_update_valid_type_start(void);

void ms_update_valid_type_end(void);

int ms_change_valid_type(ms_store_type_t table_id, bool validation);
#ifdef THUMB_THREAD
int ms_media_db_insert_with_thumb(const char *path, int category);
#endif
int ms_media_db_insert(const char *path, int category, bool bulk);

int ms_check_file_exist_in_db(const char *file_full_path);

int ms_media_db_delete(const char *full_file_path);

void ms_media_db_move_start(void);

void ms_media_db_move_end(void);

int ms_media_db_move(ms_store_type_t src_store_type,
		     ms_store_type_t dest_store_type,
		     const char *src_file_full_path,
		     const char *dest_file_full_path);

bool ms_delete_all_record(ms_store_type_t store_type);

#ifdef FMS_PERF
void ms_check_start_time(struct timeval *start_time);

void ms_check_end_time(struct timeval *end_time);

void ms_check_time_diff(struct timeval *start_time, struct timeval *end_time);
#endif				/*FMS_PERF */
#endif/*_WITH_MP_PB_*/

#ifdef PROGRESS
struct quickpanel;

void ms_create_quickpanel(struct quickpanel *ms_quickpanel);

void ms_update_progress(struct quickpanel *ms_quickpanel, double progress);

void ms_delete_quickpanel(struct quickpanel *ms_quickpanel);
#endif /*PROGRSS*/
/**
 * @}
 */
#endif				/*__FEXPLORER_ENGINE_DB_PUBLIC_H__*/


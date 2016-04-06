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
#ifndef _MEDIA_COMMON_DB_SVC_H_
#define _MEDIA_COMMON_DB_SVC_H_

#include <glib.h>

#include "media-common-types.h"

#define INTERNAL_STORAGE_ID	"media"
#define MMC_STORAGE_ID		"media"
#define USB_STORAGE_REMOVED "usb_removed"

typedef struct ms_dir_info_s {
	char *dir_path;
	int modified_time;
	int item_num;
} ms_dir_info_s;

typedef struct ms_stg_info_s {
	char *stg_path;
	char *storage_id;
	int scan_status;
} ms_stg_info_s;

typedef enum {
	MS_NOTI_SWITCH_ON = 0,
	MS_NOTI_SWITCH_OFF = 1,
} ms_noti_switch_e;

typedef enum {
	MS_ITEM_INSERT		= 0,
	MS_ITEM_DELETE 		= 1,
	MS_ITEM_UPDATE		= 2,
}ms_noti_type_e;

typedef int (*CONNECT)(void**, uid_t, char **);
typedef int (*DISCONNECT)(void*, char **);
typedef int (*INSERT_ITEM_BEGIN)(void*, int, int, int, char **);
typedef int (*INSERT_ITEM_END)(void*, uid_t, char **);
typedef int (*SET_ITEM_VALIDITY_BEGIN)(void*, int, char **);
typedef int (*SET_ITEM_VALIDITY_END)(void*, uid_t, char **);
typedef int (*UPDATE_BEGIN)(void);
typedef int (*UPDATE_END)(uid_t);

typedef int (*SEND_DIR_UPDATE_NOTI)(void *, const char *, const char *, const char *, int, int, char **);
typedef int (*CHECK_ITEM_EXIST)(void*, const char *, const char *, bool *, char **);
typedef int (*INSERT_ITEM)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*INSERT_ITEM_IMMEDIATELY)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*INSERT_BURST_ITEM)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*SET_ALL_STORAGE_ITEMS_VALIDITY)(void *, const char *, int, int, uid_t, char **);
typedef int (*SET_FOLDER_ITEM_VALIDITY)(void *, const char *, const char *, int, int, uid_t, char**);
typedef int (*SET_ITEM_VALIDITY)(void *, const char *, const char *, int, int, uid_t, char **);
typedef int (*DELETE_ITEM)(void *, const char *, const char *, uid_t, char **);
typedef int (*DELETE_ALL_ITEMS_IN_STORAGE)(void *, const char *, int, uid_t, char **);
typedef int (*DELETE_ALL_INVALID_ITMES_IN_STORAGE)(void *, const char *, int, uid_t, char **);
typedef int (*DELETE_ALL_INVALID_ITEMS_IN_FOLDER)(void *, const char *, const char *, bool, uid_t, char**);
typedef int (*COUNT_DELETE_ITEMS_IN_FOLDER)(void *, const char *, const char *, int *, char **);
typedef int (*GET_FOLDER_LIST)(void *, const char *, char *, char ***, int **, int **, int *, char **);
typedef int (*UPDATE_FOLDER_TIME)(void *, const char *, const char *, uid_t, char **);
typedef int (*GET_STORAGE_ID)(void *, const char *, char *, char **);
typedef int (*GET_STORAGE_SCAN_STATUS)(void *, const char *, int *, char **);
typedef int (*SET_STORAGE_SCAN_STATUS)(void *, const char *, int, uid_t, char **);
typedef int (*GET_STORAGE_LIST)(void *, char ***, char ***, int **, int *, char **);
typedef int (*INSERT_FOLDER)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*DELETE_INVALID_FOLDER)(void *, const char *, uid_t, char **);
typedef int (*SET_FOLDER_VALIDITY)(void *, const char *, const char *, int, bool, uid_t, char **);
typedef int (*INSERT_FOLDER_BEGIN)(void *, int, char **);
typedef int (*INSERT_FOLDER_END)(void *, uid_t, char **);
typedef int (*INSERT_ITEM_SCAN)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*UPDATE_ITEM_EXTRACT)(void *, const char *, int, int, uid_t, const char *, int, char **);
typedef int (*GET_FOLDER_SCAN_STATUS)(void *, const char *, const char *, int *, char **);
typedef int (*SET_FOLDER_SCAN_STATUS)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*CHECK_FOLDER_MODIFIED)(void *, const char *, const char *, bool *, char **);
typedef int (*GET_NULL_SCAN_FOLDER_LIST)(void *, const char *, const char *, char ***, int *, char **);
typedef int (*CHANGE_VALIDITY_ITEM_BATCH)(void *, const char *, const char *, int, int, uid_t, char **);
//typedef int (*GET_SCAN_DONE_ITEMS)(void *, const char *, const char *, int, int *, char **);

typedef int (*CHECK_DB)(void*, uid_t, char **);
typedef int (*GET_UUID)(void *, char **, char **);
typedef int (*GET_MMC_INFO)(void *, char **, char **, int *, bool *, char **);
typedef int (*CHECK_STORAGE)(void *, const char *, const char *, char **, int *, char **);
typedef int (*INSERT_STORAGE)(void *, const char *, int, const char *, const char *, uid_t, char **);
typedef int (*UPDATE_STORAGE)(void *, const char *, const char *, uid_t, char **);
typedef int (*DELETE_STORAGE)(void *, const char *, const char *, uid_t, char **);
typedef int (*SET_STORAGE_VALIDITY)(void *, const char *, int, uid_t uid, char **);
typedef int (*SET_ALL_STORAGE_VALIDITY)(void *, int, uid_t, char **);

typedef int (*UPDATE_ITEM_META)(void *, const char *, const char *, int, uid_t, char **);
typedef int (*UPDATE_ITEM_BEGIN)(void *, int, char **);
typedef int (*UPDATE_ITEM_END)(void *, uid_t, char **);

typedef int (*DELETE_INVALID_FOLDER_BY_PATH)(void *, const char *, const char *, uid_t, int *, char **);
typedef int (*CHECK_FOLDER_EXIST)(void*, const char*, const char*, char **);
typedef int (*COUNT_SUBFOLDER)(void*, const char*, const char*, int *, char **);
typedef int (*GET_FOLDER_ID)(void *, const char *, const char *, char *, char **);

int ms_load_functions(void);
void ms_unload_functions(void);
int ms_get_insert_count();
void ms_reset_insert_count();
int ms_connect_db(void ***handle, uid_t uid);
int ms_disconnect_db(void ***handle);
int ms_validate_item(void **handle, const char *storage_id, const char *path, uid_t uid);
int ms_insert_item_batch(void **handle, const char *storage_id, const char *path, uid_t uid);
int ms_insert_item_pass2(void **handle, const char *storage_id, int storage_type, const char *path, int scan_type, int burst, uid_t uid);
int ms_insert_item_immediately(void **handle, const char *storage_id, const char *path, uid_t uid);
int ms_insert_burst_item(void **handle, const char *storage_id, const char *path, uid_t uid);
bool ms_delete_all_items(void **handle, const char *storage_id, ms_storage_type_t store_type, uid_t uid);
int ms_validaty_change_all_items(void **handle, const char *storage_id, ms_storage_type_t store_type, bool validity, uid_t uid);
bool ms_delete_invalid_items(void **handle, const char *storage_id, ms_storage_type_t store_type, uid_t uid);
int ms_set_folder_item_validity(void **handle, const char *storage_id, const char *path, int validity, int recursive, uid_t uid);
int ms_delete_invalid_items_in_folder(void **handle, const char *storage_id, const char *path, bool is_recursive, uid_t uid);
int ms_send_dir_update_noti(void **handle, const char *storage_id, const char *path, const char *folder_id, ms_noti_type_e noti_type, int pid);
int ms_count_delete_items_in_folder(void **handle, const char *storage_id, const char *path, int *count);
int ms_get_folder_list(void **handle, const char *storage_id, char *start_path, GArray **dir_array);
int ms_update_folder_time(void **handle, const char *storage_id, char *folder_path, uid_t uid);
int ms_get_storage_id(void **handle, const char *path, char *storage_id);
int ms_get_storage_scan_status(void **handle, char *storage_id, media_scan_status_e *scan_status);
int ms_set_storage_scan_status(void **handle, char *storage_id, media_scan_status_e scan_status, uid_t uid);
int ms_get_storage_list(void **handle, GArray **storage_array);
int ms_insert_folder(void **handle, const char *storage_id, const char *path, uid_t uid);
int ms_delete_invalid_folder(void **handle, const char *storage_id, uid_t uid);
int ms_set_folder_validity(void **handle, const char *storage_id, const char *start_path, int validity, bool is_recursive, uid_t uid);
int ms_scan_item_batch(void **handle, const char *storage_id, const char *path, uid_t uid, int *insert_count_for_partial, int *set_count_for_partial);
int ms_scan_validate_item(void **handle, const char *storage_id, const char *path, uid_t uid, int *insert_count_for_partial, int *set_count_for_partial);
int ms_get_folder_scan_status(void **handle, const char *storage_id, const char *path, int *scan_status);
int ms_set_folder_scan_status(void **handle, const char *storage_id, const char *path, int scan_status, uid_t uid);
int ms_check_folder_modified(void **handle, const char *path, const char *storage_id, bool *modified);
int ms_get_null_scan_folder_list(void **handle, const char *stroage_id, const char *path, GArray **dir_array);
int ms_change_validity_item_batch(void **handle, const char *storage_id, const char *path, int des_validity, int src_validity, uid_t uid);
//int ms_get_scan_done_items(void **handle, const char *storage_id, const char *path, int validity, int *count);

int ms_check_db_upgrade(void **handle, uid_t uid);
int ms_genarate_uuid(void **handle, char **uuid);
int ms_get_mmc_info(void **handle, char **storage_name, char **storage_path, int *validity, bool *info_exist);
int ms_check_storage(void **handle, const char *storage_id, const char *storage_name, char **storage_path, int *validity);
int ms_insert_storage(void **handle, const char *storage_id, const char *storage_name, const char *storage_path, uid_t uid);
int ms_update_storage(void **handle, const char *storage_id, const char *storage_path, uid_t uid);
int ms_delete_storage(void **handle, const char *storage_id, const char *storage_name, uid_t uid);
int ms_set_storage_validity(void **handle, const char *storage_id, int validity, uid_t uid);
int ms_set_all_storage_validity(void **handle, int validity, uid_t uid);
int ms_update_meta_batch(void **handle, const char *path, const char *storage_id, uid_t uid);
int ms_delete_invalid_folder_by_path(void **handle, const char *storage_id, const char *folder_path, uid_t uid, int *delete_count);
int ms_check_folder_exist(void **handle, const char *storage_id, const char *folder_path);
int ms_check_subfolder_count(void **handle, const char *storage_id, const char *folder_path, int *count);
int ms_get_folder_id(void **handle, const char *storage_id, const char *path, char **folder_id);
int ms_get_delete_count();
void ms_reset_delete_count();

/****************************************************************************************************
FOR BULK COMMIT
*****************************************************************************************************/
void ms_register_start(void **handle, ms_noti_switch_e noti_status, int pid);
void ms_register_end(void **handle, uid_t uid);
void ms_validate_start(void **handle);
void ms_validate_end(void **handle, uid_t uid);
void ms_insert_folder_start(void **handle);
void ms_insert_folder_end(void **handle, uid_t uid);
void ms_update_start(void **handle);
void ms_update_end(void **handle, uid_t uid);

#endif /*_MEDIA_COMMON_DB_SVC_H_*/

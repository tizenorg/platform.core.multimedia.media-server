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
 * @file		media-server-types.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_SERVER_TYPES_H_
#define _MEDIA_SERVER_TYPES_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define FMS_PERF
#define MS_INOTI_ENABLE 0

#define MS_VALIND 1
#define MS_INVALID 0
#define MS_RECURSIVE 1
#define MS_NON_RECURSIVE 0

/*This macro is used to save and check information of inserted memory card*/
#define MS_MMC_INFO_KEY "db/private/mediaserver/mmc_info"

/* store scanning status of each storage */
#define MS_SCAN_STATUS_INTERNAL "file/private/mediaserver/scan_internal"
#define MS_SCAN_STATUS_DIRECTORY "file/private/mediaserver/scan_directory"
enum{
	P_VCONF_SCAN_DOING = 0,
	P_VCONF_SCAN_DONE,
};

/*Use for Poweroff sequence*/
#define POWEROFF_NOTI_NAME "power_off_start" /*poeroff noti from system-server*/
#define POWEROFF_DIR_PATH "/opt/usr/media/_POWER_OFF" /*This path uses for stopping Inotify thread and Socket thread*/
#define POWEROFF_DIR_NAME "_POWER_OFF" /*This path uses for stopping Inotify thread and Socket thread*/
#define POWEROFF -1 /*This number uses for stopping Scannig thread*/

#define MS_SAFE_FREE(src)      { if(src) {free(src); src = NULL;} }
#define MS_MALLOC(src, size)	{ if (size > SIZE_MAX || size <= 0) {src = NULL;} \
							else { src = malloc(size); memset(src, 0x0, size);} }

/*System default folder definition*/
#define FAT_FILENAME_LEN_MAX          255	/* not inc null */
#define FAT_FILEPATH_LEN_MAX          4096	/* inc null */

/* The following MACROs(TAF_XXX) are defined in "fs-limit.h"*/
#define MS_FILE_NAME_LEN_MAX     FAT_FILENAME_LEN_MAX		 /**< File name max length on file system */
#define MS_FILE_PATH_LEN_MAX     FAT_FILEPATH_LEN_MAX		 /**< File path max length (include file name) on file system */

#define MS_SOCK_NOT_ALLOCATE -1

typedef enum {
	MS_STORAGE_INTERNAL,    /**< Stored only in phone */
	MS_STORAGE_EXTERNAL,	     /**< Stored only in MMC */
} ms_storage_type_t;

typedef enum {
	MS_SCANNING_INTERNAL,
	MS_SCANNING_EXTERNAL,
	MS_SCANNING_DIRECTORY,
} ms_scanning_location_t;

typedef enum {
	MS_SCAN_INVALID,
	MS_SCAN_PART,
	MS_SCAN_ALL,
} ms_dir_scan_type_t;

typedef enum {
	MS_DB_UPDATING = 0,
	MS_DB_UPDATED = 1
} ms_db_status_type_t;

typedef struct ms_dir_scan_info {
	char *name;
	struct ms_dir_scan_info *parent;
	struct ms_dir_scan_info *Rbrother;
	struct ms_dir_scan_info *next;
} ms_dir_scan_info;

typedef struct {
	char *path;
	ms_storage_type_t storage_type;
	ms_dir_scan_type_t scan_type;
	int pid;
	bool recursive_on;
} ms_scan_data_t;

typedef struct {
	char *path;
	int pid;
} ms_register_data_t;

/**
 * @}
 */

#endif /*_MEDIA_SERVER_TYPES_H_*/

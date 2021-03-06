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

#include <stdbool.h>
#include <glib.h>

#if !defined(__TYPEDEF_INT64__)
#define __TYPEDEF_INT64__
typedef long long int64;
#endif

#define MS_SAFE_FREE(src)      { if(src) {free(src); src = NULL;} }

/*System default folder definition*/
#define FAT_FILENAME_LEN_MAX          255	/* not inc null */
#define FAT_FILEPATH_LEN_MAX          4096	/* inc null */
#define FAT_DIRECTORY_LEN_MAX         244

/* The following MACROs(TAF_XXX) are defined in "fs-limit.h"*/
#define MS_FILE_NAME_LEN_MAX     FAT_FILENAME_LEN_MAX		 /**< File name max length on file system */
#define MS_FILE_PATH_LEN_MAX     FAT_FILEPATH_LEN_MAX		 /**< File path max length (include file name) on file system */

typedef enum {
	MS_STORAGE_INTERNAL,    /**< Stored only in phone */
	MS_STORATE_EXTERNAL,	     /**< Stored only in MMC */
} ms_storage_type_t;

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
} ms_scan_data_t;

/**
 * @}
 */

#endif /*_MEDIA_SERVER_TYPES_H_*/

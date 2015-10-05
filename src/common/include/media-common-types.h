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

#ifndef _MEDIA_SERVER_TYPES_H_
#define _MEDIA_SERVER_TYPES_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <tzplatform_config.h>

#define FMS_PERF

#define MS_VALIND 1
#define MS_INVALID 0
#define MS_RECURSIVE 1
#define MS_NON_RECURSIVE 0

/*This macro is used to save and check information of inserted memory card*/
//#define MS_MMC_INFO_KEY "db/private/mediaserver/mmc_info"

/*Use for Poweroff sequence*/
#define POWEROFF -1 /*This number uses for stopping Scannig thread*/

#define MS_SAFE_FREE(src)      { if(src) {free(src); src = NULL;} }
#define MS_MALLOC(src, size)	{ if (size > SIZE_MAX || size <= 0) {src = NULL;} \
							else { src = malloc(size); if(src) memset(src, 0x0, size);} }
#define MS_STRING_VALID(str)	\
								((str != NULL && strlen(str) > 0) ? TRUE : FALSE)

/*System default folder definition*/
#define FAT_FILENAME_LEN_MAX          255	/* not inc null */
#define FAT_FILEPATH_LEN_MAX          4096	/* inc null */

/* The following MACROs(TAF_XXX) are defined in "fs-limit.h"*/
#define MS_FILE_NAME_LEN_MAX     FAT_FILENAME_LEN_MAX		 /**< File name max length on file system */
#define MS_FILE_PATH_LEN_MAX     FAT_FILEPATH_LEN_MAX		 /**< File path max length (include file name) on file system */

#define MS_SOCK_NOT_ALLOCATE -1

#define MS_INI_DEFAULT_PATH "/usr/etc/media_content_config.ini"

typedef enum {
	MS_STORAGE_INTERNAL = 0,	/**< The device's internal storage */
	MS_STORAGE_EXTERNAL = 1,	/**< The device's external storage */
	MS_STORAGE_EXTERNAL_USB = 2,	/**< The external USB storage (Since 2.4) */
} ms_storage_type_t;

typedef enum {
	MS_SDCARD_INSERTED,    /**< Stored only in phone */
	MS_SDCARD_REMOVED,	     /**< Stored only in MMC */
} ms_sdcard_status_type_t;

typedef enum {
	MS_SCAN_INVALID,
	MS_SCAN_PART,
	MS_SCAN_META,
	MS_SCAN_ALL,
} ms_dir_scan_type_t;

typedef enum {
	MS_DB_UPDATING = 0,
	MS_DB_UPDATED = 1,
	MS_DB_STOPPED = 2
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


/*use for MS_SCANNER_STATUS */
typedef enum {
	MS_STORAGE_SCAN_NONE, 			/**< Media Scanner not detect storage yet*/
	MS_STORAGE_SCAN_PREPARE,			/**< Media Scanner detect storage but not scanning yet*/
	MS_STORAGE_SCAN_PROCESSING,		/**< Media Scanner Start Scanning storage*/
	MS_STORAGE_SCAN_STOP, 			/**< Stop scanning storage*/
	MS_STORAGE_SCAN_DONE, 			/**< Scanning Done but need to extract metadata*/
	MS_STORAGE_SCAN_META_PROCESSING,	/**< Scanning Done and start to extract metadata*/
	MS_STORAGE_SCAN_META_STOP,		/**< Stop extract metadata*/
	MS_STORAGE_SCAN_COMPLETE, 		/**< Complete scanning*/
}ms_storage_scan_status_e;

typedef enum{
	MEDIA_SCAN_PREPARE		= 0,	/**< Prepare scanning*/
	MEDIA_SCAN_PROCESSING	= 1,	/**< Process scanning*/
	MEDIA_SCAN_STOP			= 2,	/**< Stop scanning*/
	MEDIA_SCAN_COMPLETE		= 3,	/**< Complete scanning*/
	MEDIA_SCAN_MAX			= 4,	/**< Invalid status*/
	MEDIA_EXTRACT_PREPARE		= 5,	/**< Prepare extract*/
	MEDIA_EXTRACT_PROCESSING	= 6,	/**< Process extract*/
	MEDIA_EXTRACT_STOP			= 7,	/**< Stop extract*/
	MEDIA_EXTRACT_COMPLETE		= 8		/**< Complete extract*/
}media_scan_status_e;

#endif /*_MEDIA_SERVER_TYPES_H_*/

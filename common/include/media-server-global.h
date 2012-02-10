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
 * @file		media-server-global.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief		
 */

#ifndef _MEDIA_SERVER_GLOBAL_H_
#define _MEDIA_SERVER_GLOBAL_H_
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <glib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <errno.h>

#include <dlog.h>

#include "media-server-error.h"
#include "media-server-types.h"

#define FMS_PERF

/* To enable progress bar in quickpanel */
/*#define PROGRESS*/

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MEDIA-SERVER"
#define MS_DBG_START()              LOGD("[%d, %s-%d] ========[ START ]========\n" , syscall(__NR_gettid), __func__ , __LINE__);
#define MS_DBG_END()                  LOGD("[%d, %s-%d] ========[  END  ]========\n" ,syscall(__NR_gettid), __func__ , __LINE__);
#define MS_DBG_FUNC_LINE()      LOGD("[%s-%d] debug\n" , __func__ , __LINE__);
#define MS_DBG(fmt, args...)        LOGD("[%d, %s-%d] " fmt "\n" , syscall(__NR_gettid), __func__ , __LINE__ , ##args);

#define MALLOC(a) malloc(a)
#define FREE(a) free(a)

#define MS_PHONE_ROOT_PATH	"opt/media"
#define MS_MMC_ROOT_PATH		"opt/storage/sdcard"
#define MS_DB_UPDATE_NOTI_PATH "/opt/data/file-manager-service"

/*This macro is used to check the lastest usb mode*/
#define MS_USB_MODE_KEY "db/Apps/mediaserver/usbmode"
enum {
	MS_VCONFKEY_NORMAL_MODE = 0x00,
	MS_VCONFKEY_MASS_STORAGE_MODE = 0x01
};

/*This macro is used to save and check information of inserted memory card*/
#define MS_MMC_INFO_KEY "db/Apps/mediaserver/mmc_info"

/**
 * @}
 */

#endif /*_MEDIA_SERVER_GLOBAL_H_*/

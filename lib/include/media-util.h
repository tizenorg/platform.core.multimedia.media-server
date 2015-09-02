/*
 *  Media Utility
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

#ifndef _MEDIA_UTIL_H_
#define _MEDIA_UTIL_H_

#include <media-util-err.h>
#include <media-util-register.h>
#include <media-util-db.h>
#include <media-util-noti.h>
#include <media-util-ipc.h>
#include <media-util-cynara.h>


#include <tzplatform_config.h>

#define MEDIA_ROOT_PATH_INTERNAL	tzplatform_getenv(TZ_USER_HOME)
#define MEDIA_ROOT_PATH_SDCARD	tzplatform_getenv(TZ_SYS_STORAGE)
#define MEDIA_ROOT_PATH_USB		tzplatform_getenv(TZ_SYS_STORAGE)
#define MEDIA_ROOT_PATH_CLOUD	tzplatform_mkpath(TZ_SYS_STORAGE, "tnfs/cloud")
#define MEDIA_DATA_PATH		tzplatform_mkpath(TZ_SYS_DATA, "file-manager-service")
#define MEDIA_DB_NAME		tzplatform_mkpath(TZ_USER_DB, ".media.db")		/**<  media db name*/
#define MEDIA_CONTENT_PATH		"content"		/**<  user content folder name*/


#endif /*_MEDIA_UTIL_H_*/

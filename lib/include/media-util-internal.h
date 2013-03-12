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

/**
 * This file defines api utilities of contents manager engines.
 *
 * @file		media-util-internal.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#ifndef _MEDIA_UTIL_INTERNAL_H_
#define _MEDIA_UTIL_INTERNAL_H_

#include "media-util-db.h"

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#define MS_SAFE_FREE(src)      { if(src) {free(src); src = NULL;} }
#define MS_STRING_VALID(str)	\
	((str != NULL && strlen(str) > 0) ? TRUE : FALSE)

#define MS_MEDIA_DBUS_PATH "/com/mediaserver/dbus/notify"
#define MS_MEDIA_DBUS_INTERFACE "com.mediaserver.dbus.Signal"
#define MS_MEDIA_DBUS_NAME "ms_db_updated"
#define MS_MEDIA_DBUS_MATCH_RULE "type='signal',interface='com.mediaserver.dbus.Signal'"


#define MS_SCAN_STATUS_DIRECTORY "file/private/mediaserver/scan_directory"
enum{
	VCONF_SCAN_DOING = 0,
	VCONF_SCAN_DONE,
};

int media_db_update_db(MediaDBHandle *handle, const char *query_str);

int media_db_update_db_batch_start(const char *query_str);
int media_db_update_db_batch(const char *query_str);
int media_db_update_db_batch_end(MediaDBHandle *handle, const char *query_str);

#endif /*_MEDIA_UTIL_INTERNAL_H_*/

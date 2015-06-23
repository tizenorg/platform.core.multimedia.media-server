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
 * @file		media-util-not-commoni.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
 #ifndef _MEDIA_UTIL_NOTI_COMMON_H_
#define _MEDIA_UTIL_NOTI_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MS_MEDIA_ITEM_FILE			= 0,
	MS_MEDIA_ITEM_DIRECTORY	= 1,
}media_item_type_e;

typedef enum {
	MS_MEDIA_ITEM_INSERT		= 0,
	MS_MEDIA_ITEM_DELETE 		= 1,
	MS_MEDIA_ITEM_UPDATE		= 2,
}media_item_update_type_e;

typedef enum {
	MS_MEDIA_UNKNOWN	= -1,	 /**< Unknown Conntent*/
	MS_MEDIA_IMAGE	= 0,		/**< Image Content*/
	MS_MEDIA_VIDEO	= 1,		/**< Video Content*/
	MS_MEDIA_SOUND	= 2,		/**< Sound Content like Ringtone*/
	MS_MEDIA_MUSIC	= 3,		/**< Music Content like mp3*/
	MS_MEDIA_OTHER	= 4,		/**< Invalid Content*/
}media_type_e;

typedef void (*db_update_cb)(int pid, /* mandatory */
							media_item_type_e item, /* mandatory */
							media_item_update_type_e update_type, /* mandatory */
							char* path, /* mandatory */
							char* uuid, /* optional */
							media_type_e media_type, /* optional */
							char *mime_type, /* optional */
							void *user_data);

typedef void (*clear_user_data_cb)(void * user_data);

typedef void *MediaNotiHandle;		/**< Handle */

#define MS_MEDIA_DBUS_PATH "/com/mediaserver/dbus/notify"
#define MS_MEDIA_DBUS_INTERFACE "com.mediaserver.dbus.Signal"
#define MS_MEDIA_DBUS_MATCH_RULE "type='signal',interface='com.mediaserver.dbus.Signal'"

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_NOTI_COMMON_H_*/

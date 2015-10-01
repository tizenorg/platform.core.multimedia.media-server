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
 * @file		media-util-noti.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
 #ifndef _MEDIA_UTIL_NOTI_INTERNAL_H_
#define _MEDIA_UTIL_NOTI_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "media-util-noti-common.h"

/**
* @fn 		int media_db_update_subscribe(void);
* @brief 		This function announce media database is updated to other applications.<br>
* @return	This function returns 0 on success, and -1 on failure.
* @param[in]	none
* @remark  	This function is recommandation for other application being aware of database updating.<br>
* @par example
* @code

#include <stdio.h>
#include <glib.h>
#include <media-util-noti.h>

void callback()
{
        printf("listen dbus from media-server\n");
}

int
main (int argc, char **argv)
{
	GMainLoop *loop;

	loop = g_main_loop_new (NULL, FALSE);

	media_db_update_subscribe(callback);

	g_main_loop_run (loop);

	return 0;
}

*/

int media_db_update_send_internal(int pid, /* mandatory */
							media_item_type_e item, /* mandatory */
							media_item_update_type_e update_type, /* mandatory */
							char* path, /* mandatory */
							char* uuid, /* optional */
							media_type_e media_type, /* optional */
							char *mime_type /* optional */
							);

int media_db_update_subscribe_internal(MediaNotiHandle *handle, db_update_cb user_cb, void *user_data);

int media_db_update_unsubscribe_internal(MediaNotiHandle handle, clear_user_data_cb clear_cb);


/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_NOTI_INTERNAL_H_*/

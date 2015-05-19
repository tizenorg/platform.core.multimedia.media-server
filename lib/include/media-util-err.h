/*
 *  Media Utility
 *
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
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
 * @file		media-util-err.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_UTIL_ERR_H_
#define _MEDIA_UTIL_ERR_H_

#define MS_MEDIA_ERR_NONE 0

/*internal operation error*/
#define MS_MEDIA_ERR_INTERNAL 				-1
#define MS_MEDIA_ERR_INVALID_CONTENT 		-2   /**< Invalid content */
#define MS_MEDIA_ERR_INVALID_PARAMETER 		-3   /**< invalid parameter(s) */
#define MS_MEDIA_ERR_INVALID_PATH 			-4   /**< Invalid path */
#define MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL 	-5   /**< exception of memory allocation */
#define MS_MEDIA_ERR_DIR_OPEN_FAIL 			-6   /**< exception of dir open*/
#define MS_MEDIA_ERR_FILE_OPEN_FAIL 			-7   /**< exception of file doesn't exist*/

/*DB operation error*/
#define MS_MEDIA_ERR_DB_CONNECT_FAIL 		-11  /**< connecting database fails */
#define MS_MEDIA_ERR_DB_DISCONNECT_FAIL 		-12  /**< disconnecting database fails */
#define MS_MEDIA_ERR_DB_INSERT_FAIL 			-13  /**< inserting record fails */
#define MS_MEDIA_ERR_DB_DELETE_FAIL 			-14  /**< deleting record fails */
#define MS_MEDIA_ERR_DB_UPDATE_FAIL 			-15  /**< updating record fails */
#define MS_MEDIA_ERR_DB_EXIST_ITEM_FAIL 		-16  /**< item does not exist */
#define MS_MEDIA_ERR_DB_BUSY_FAIL 			-17  /**< DB Busy */

/*DRM operation error*/
#define MS_MEDIA_ERR_DRM_REGISTER_FAIL 		-21  /**< interting into drm db fails */
#define MS_MEDIA_ERR_DRM_GET_INFO_FAIL 		-22  /**< getting inforamtion fails from DRM content */

/*IPC operation error*/
#define MS_MEDIA_ERR_SOCKET_INTERNAL			-31  /**< receive error from socket API */
#define MS_MEDIA_ERR_SOCKET_CONN				-32  /**< socket connect error */
#define MS_MEDIA_ERR_SOCKET_BIND				-33  /**< socket binding fails */
#define MS_MEDIA_ERR_SOCKET_SEND				-34  /**< socket sending fails */
#define MS_MEDIA_ERR_SOCKET_RECEIVE			-35  /**< socket receiving fails */
#define MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT	-36  /**< socket receive timeout error */
#define MS_MEDIA_ERR_DBUS_ADD_FILTER			-37  /**< DBUS add filter fails*/
#define MS_MEDIA_ERR_DBUS_GET					-38  /**< DBUS get fails */
#define MS_MEDIA_ERR_DATA_TAINTED				-39  /**< received data is tainted */
#define MS_MEDIA_ERR_SEND_NOTI_FAIL			-40  /**< sending notification is failed */

/* SERVER error*/
#define MS_MEDIA_ERR_NOW_REGISTER_FILE		-41  /**< already inserting into DB */
#define MS_MEDIA_ERR_SCANNING_BUSY			-42  /**< already directory scanning is running */
#define MS_MEDIA_ERR_DB_SERVER_BUSY_FAIL	-43  /**< DB server busy */
#define MS_MEDIA_ERR_SCANNER_FORCE_STOP		-44  /**< scanning is stopped forcely */

/*ETC*/
#define MS_MEDIA_ERR_VCONF_SET_FAIL			-51  /**< vconf set fail*/
#define MS_MEDIA_ERR_VCONF_GET_FAIL			-52  /**< vconf get fail*/
#define MS_MEDIA_ERR_MIME_GET_FAIL			-53  /**< not media file*/
#define MS_MEDIA_ERR_SCANNER_NOT_READY		-54  /**< not media file*/
#define MS_MEDIA_ERR_DYNAMIC_LINK			-55

#define MS_MEDIA_ERR_ACCESS_DENIED			-56

#define MS_MEDIA_ERR_MAX						-999 /**< not media file*/

#endif /*_MEDIA_UTIL_ERR_H_*/

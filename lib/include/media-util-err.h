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

#ifndef _MEDIA_UTIL_ERR_H_
#define _MEDIA_UTIL_ERR_H_

#define MS_MEDIA_ERR_NONE 						0

/* Internal operation error*/
#define MS_MEDIA_ERR_INTERNAL 		 			-1
#define MS_MEDIA_ERR_INVALID_PARAMETER 			-2  /* Invalid parameter(s) */
#define MS_MEDIA_ERR_INVALID_PATH 				-3  /* Invalid path */
#define MS_MEDIA_ERR_OUT_OF_MEMORY				-4  /* Out of memory */

/* DB operation error*/
#define MS_MEDIA_ERR_DB_CONNECT_FAIL	 			-101  /* connecting database fails */
#define MS_MEDIA_ERR_DB_DISCONNECT_FAIL 			-102  /* disconnecting database fails */
#define MS_MEDIA_ERR_DB_INSERT_FAIL 				-103  /* inserting record fails */
#define MS_MEDIA_ERR_DB_DELETE_FAIL 				-104  /* deleting record fails */
#define MS_MEDIA_ERR_DB_UPDATE_FAIL 				-105  /* updating record fails */
#define MS_MEDIA_ERR_DB_BUSY_FAIL 					-106  /* DB Busy */
#define MS_MEDIA_ERR_DB_SERVER_BUSY_FAIL			-107  /* DB Server Busy */
#define MS_MEDIA_ERR_DB_CONSTRAINT_FAIL 			-108  /* DB CONSTRAINT fails - In case of insert, the record already exists */
#define MS_MEDIA_ERR_DB_BATCH_UPDATE_BUSY		-109  /* Batch update thread is full */
#define MS_MEDIA_ERR_DB_NO_RECORD				-110  /* Item not found in DB */
#define MS_MEDIA_ERR_DB_CORRUPT					-112  /* DB corrut error */
#define MS_MEDIA_ERR_DB_PERMISSION  				-113  /* DB permission error */
#define MS_MEDIA_ERR_DB_FULL_FAIL					-114  /* DB storage full error */
#define MS_MEDIA_ERR_DB_INTERNAL					-150  /* DB internal error */

/* IPC operation error*/
#define MS_MEDIA_ERR_SOCKET_CONN					-201  /* socket connect error */
#define MS_MEDIA_ERR_SOCKET_BIND					-202  /* socket binding fails */
#define MS_MEDIA_ERR_SOCKET_SEND					-203  /* socket sending fails */
#define MS_MEDIA_ERR_SOCKET_RECEIVE				-204  /* socket receiving fails */
#define MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT		-205  /* socket receive timeout error */
#define MS_MEDIA_ERR_SOCKET_ACCEPT				-206  /* socket accept fails */
#define MS_MEDIA_ERR_SOCKET_INTERNAL				-250  /* receive error from socket API */

/* DIRECTORY error*/
#define MS_MEDIA_ERR_DIR_OPEN_FAIL 				-501  /* direcotry opennig fails */
#define MS_MEDIA_ERR_DIR_CLOSE_FAIL				-502  /* directory closing fails */
#define MS_MEDIA_ERR_DIR_READ_FAIL 				-503  /* directory reading fails */

/* FILE error*/
#define MS_MEDIA_ERR_FILE_OPEN_FAIL 				-601  /* file opennig fails */
#define MS_MEDIA_ERR_FILE_CLOSE_FAIL 				-602  /* file closing fails */
#define MS_MEDIA_ERR_FILE_READ_FAIL 				-603  /* file reading fails */
#define MS_MEDIA_ERR_FILE_WRITE_FAIL 				-604  /* file writing fails */
#define MS_MEDIA_ERR_FILE_NOT_EXIST 				-605  /* file does not exist */

/* MEDIA SERVER error*/
#define MS_MEDIA_ERR_SCANNER_FORCE_STOP			-701  /* scanning is stopped forcely */
#define MS_MEDIA_ERR_PERMISSION_DENIED			-702  /* Do have permission of request */

/* Thumbnail error*/
#define MS_MEDIA_ERR_THUMB_TOO_BIG				-801  /* Original is too big to make thumb */
#define MS_MEDIA_ERR_THUMB_DUPLICATED_REQUEST	-802  /* Duplicated request of same path */

/*ETC*/
#define MS_MEDIA_ERR_VCONF_SET_FAIL				-901  /* vconf setting fails*/
#define MS_MEDIA_ERR_VCONF_GET_FAIL				-902  /* vconf getting fails*/
#define MS_MEDIA_ERR_SCANNER_NOT_READY			-903  /* scanner is not ready */
#define MS_MEDIA_ERR_DYNAMIC_LINK				-904  /* fail to dynamic link */
#define MS_MEDIA_ERR_INVALID_IPC_MESSAGE			-905  /* received message is not valid */
#define MS_MEDIA_ERR_DATA_TAINTED				-906  /* received data is tainted */
#define MS_MEDIA_ERR_SEND_NOTI_FAIL				-907  /* sending notification is failed */
#define MS_MEDIA_ERR_USB_UNMOUNTED				-908 /* USB unmounted */

#define MS_MEDIA_ERR_MAX							-999

#endif /*_MEDIA_UTIL_ERR_H_*/

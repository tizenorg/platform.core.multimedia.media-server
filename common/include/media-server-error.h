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
 * @file		media-server-error.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief		
 */

#ifndef _MEDIA_SERVER_ERROR_H_
#define _MEDIA_SERVER_ERROR_H_

#define ERROR_MASKL16       0xFFFF
#define ERROR(X)        (X & ERROR_MASKL16)

#define MID_CONTENTS_MGR_ERROR  0	/*MID_CONTENTS_MGR_SERVICE */

#define MS_ERR_NONE 0

/*internal operation error*/
#define MS_ERR_ARG_INVALID					(MID_CONTENTS_MGR_ERROR - ERROR(0x01))	   /**< invalid argument */
#define MS_ERR_ALLOCATE_MEMORY_FAIL		(MID_CONTENTS_MGR_ERROR - ERROR(0x02))	   /**< exception of memory allocation */
#define MS_ERR_OUT_OF_RANGE		              (MID_CONTENTS_MGR_ERROR - ERROR(0x03))
#define MS_ERR_INVALID_DIR_PATH     		(MID_CONTENTS_MGR_ERROR - ERROR(0x04))	   /**< exception of invalid dir path */
#define MS_ERR_INVALID_FILE_PATH			(MID_CONTENTS_MGR_ERROR - ERROR(0x05))	   /**< exception of invalid file path */
#define MS_ERR_NOW_REGISTER_FILE			(MID_CONTENTS_MGR_ERROR - ERROR(0x06))	   /**< exception of invalid file path */

/*directory operation error*/
#define MS_ERR_DIR_OPEN_FAIL				(MID_CONTENTS_MGR_ERROR - ERROR(0x10))	   /**< exception of dir open*/
#define MS_ERR_DIR_NOT_FOUND				(MID_CONTENTS_MGR_ERROR - ERROR(0x11))	   /**< exception of dir doesn't exist*/
#define MS_ERR_DIR_READ_FAIL					(MID_CONTENTS_MGR_ERROR - ERROR(0x13))	  /**< exception of dir read*/

/*file operation error*/
#define MS_ERR_FILE_NOT_FOUND				(MID_CONTENTS_MGR_ERROR - ERROR(0x20))	   /**< exception of file doesn't exist*/
#define MS_ERR_FILE_OPEN_FAIL				(MID_CONTENTS_MGR_ERROR - ERROR(0x21))	   /**< exception of file doesn't exist*/

/*db operation error*/
#define MS_ERR_DB_TRUNCATE_TABLE_FAIL	(MID_CONTENTS_MGR_ERROR - ERROR(0x30))	   /**< truncating table fails */
#define MS_ERR_DB_INSERT_RECORD_FAIL		(MID_CONTENTS_MGR_ERROR - ERROR(0x31))	   /**< inserting record fails */
#define MS_ERR_DB_DELETE_RECORD_FAIL		(MID_CONTENTS_MGR_ERROR - ERROR(0x32))	   /**< deleting record fails */
#define MS_ERR_DB_UPDATE_RECORD_FAIL		(MID_CONTENTS_MGR_ERROR - ERROR(0x33))	   /**< updating record fails */
#define MS_ERR_DB_CONNECT_FAIL			(MID_CONTENTS_MGR_ERROR - ERROR(0x34))	   /**< connecting database fails */
#define MS_ERR_DB_DISCONNECT_FAIL			(MID_CONTENTS_MGR_ERROR - ERROR(0x35))	   /**< connecting database fails */
#define MS_ERR_DB_OPERATION_FAIL			(MID_CONTENTS_MGR_ERROR - ERROR(0x36))	   /**< connecting database fails */

/*drm operation error*/
#define MS_ERR_DRM_GET_TYPE				(MID_CONTENTS_MGR_ERROR - ERROR(0x40))	   
#define MS_ERR_DRM_MOVE_FAIL				(MID_CONTENTS_MGR_ERROR - ERROR(0x41))	 /**< can't copy/move drm file because of permission */
#define MS_ERR_DRM_REGISTER_FAIL			(MID_CONTENTS_MGR_ERROR - ERROR(0x42))	
#define MS_ERR_DRM_EXTRACT_FAIL			(MID_CONTENTS_MGR_ERROR - ERROR(0x43))	

/*IPC operation error*/
#define MS_ERR_SOCKET_CONN				(MID_CONTENTS_MGR_ERROR - ERROR(0x50))	 /**< Socket connect error */
#define MS_ERR_SOCKET_BIND					(MID_CONTENTS_MGR_ERROR - ERROR(0x51))	 /**< Socket connect error */
#define MS_ERR_SOCKET_MSG					(MID_CONTENTS_MGR_ERROR - ERROR(0x52))	 /**< Socket message error */
#define MS_ERR_SOCKET_SEND				(MID_CONTENTS_MGR_ERROR - ERROR(0x53))	 /**< Socket send error */
#define MS_ERR_SOCKET_RECEIVE				(MID_CONTENTS_MGR_ERROR - ERROR(0x54))	 /**< Socket receive error */

/*ETC*/
#define MS_ERR_LOW_MEMORY					(MID_CONTENTS_MGR_ERROR - ERROR(0x60))	 /**< low memory*/
#define MS_ERR_UNKNOWN_ERROR				(MID_CONTENTS_MGR_ERROR - ERROR(0x61))	 /**< unknow error*/
#define MS_ERR_VCONF_SET_FAIL				(MID_CONTENTS_MGR_ERROR - ERROR(0x62))	 /**< vconf set fail*/
#define MS_ERR_VCONF_GET_FAIL				(MID_CONTENTS_MGR_ERROR - ERROR(0x63))	 /**< vconf get fail*/
#define MS_ERR_NOT_MEDIA_FILE				(MID_CONTENTS_MGR_ERROR - ERROR(0x64))	 /**< not media file*/

#define MS_ERR_MAX							(MID_CONTENTS_MGR_ERROR - ERROR(0xff))	 /**< not media file*/
#endif/* _MEDIA_SERVER_ERROR_H_ */
/**
 * @}
 */

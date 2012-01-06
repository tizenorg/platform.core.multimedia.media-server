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
 * @file		media-util-err.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_UTIL_ERR_H_
#define _MEDIA_UTIL_ERR_H_

#define MS_ERROR_MASKL16       0xFFFF
#define MS_ERROR(X)        (X & MS_ERROR_MASKL16)

#define MS_MEDIA_ERR_NONE 0
#define MS_MEDIA_ERR_OCCURRED	(MS_MEDIA_ERR_NONE - MS_ERROR(0x01))
#define MS_MEDIA_ERR_INVALID_PARAMETER (MS_MEDIA_ERR_NONE - MS_ERROR(0x01))	/**< invalid parameter(s) */
#define MS_MEDIA_ERR_INVALID_PATH (MS_MEDIA_ERR_NONE - MS_ERROR(0x02))	 /**< Invalid file path */

#define MS_MEDIA_ERR_SOCKET_CONN (MS_MEDIA_ERR_NONE - MS_ERROR(0x03))/**< Socket connect error */
#define MS_MEDIA_ERR_SOCKET_MSG (MS_MEDIA_ERR_NONE - MS_ERROR(0x04))/**< Socket message error */
#define MS_MEDIA_ERR_SOCKET_SEND (MS_MEDIA_ERR_NONE - MS_ERROR(0x05))/**< Socket send error */
#define MS_MEDIA_ERR_SOCKET_RECEIVE (MS_MEDIA_ERR_NONE - MS_ERROR(0x06))/**< Socket receive error */
#define MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT (MS_MEDIA_ERR_NONE - MS_ERROR(0x07))/**< Socket time out */

#define MS_MEDIA_ERR_INVALID_MEDIA (MS_MEDIA_ERR_NONE - MS_ERROR(0x08))/**< Invalid media content */
#define MS_MEDIA_ERR_INSERT_FAIL (MS_MEDIA_ERR_NONE - MS_ERROR(0x09))/**< DB insert fail */
#define MS_MEDIA_ERR_DRM_INSERT_FAIL (MS_MEDIA_ERR_NONE - MS_ERROR(0x0a))/**< DRM file insert fail */

#define MS_MEDIA_ERR_UNKNOWN (MS_MEDIA_ERR_NONE - MS_ERROR(0x10)) /**<Unknown error*/

#endif /*_MEDIA_UTIL_ERR_H_*/

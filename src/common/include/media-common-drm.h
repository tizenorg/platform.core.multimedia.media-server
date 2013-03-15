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
 * @file		media-server-drm.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */
#ifndef _MEDIA_SERVER_DRM_H_
#define _MEDIA_SERVER_DRM_H_

bool
ms_is_drm_file(const char *path);

int
ms_get_mime_in_drm_info(const char *path, char *mime);

int
ms_drm_register(const char* path);

void
ms_drm_unregister(const char* path);

void
ms_drm_unregister_all(void);

bool
ms_drm_insert_ext_memory(void);

bool
ms_drm_extract_ext_memory(void);

#endif /*_MEDIA_SERVER_DRM_H_*/
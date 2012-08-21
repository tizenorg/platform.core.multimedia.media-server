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
 * @file		media-server-drm.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */
#include <drm_client_types.h>
#include <drm_client.h>

#include "media-server-global.h"
#include "media-server-inotify.h"
#include "media-server-drm.h"

bool
ms_is_drm_file(const char *path)
{
	int ret;
	drm_bool_type_e is_drm_file = DRM_UNKNOWN;

	ret = drm_is_drm_file(path,&is_drm_file);
	if(DRM_RETURN_SUCCESS == ret && DRM_TRUE == is_drm_file)
		return true;

	return false;
}

int
ms_get_mime_in_drm_info(const char *path, char *mime)
{
	int ret;
	drm_content_info_s contentInfo;

	if (path == NULL || mime == NULL)
		return MS_ERR_ARG_INVALID;

	memset(&contentInfo,0x0,sizeof(drm_content_info_s));
	ret = drm_get_content_info(path, &contentInfo);
	if (ret != DRM_RETURN_SUCCESS) {
		MS_DBG_ERR("drm_svc_get_content_info() fails. ");
		return MS_ERR_DRM_GET_INFO_FAIL;
	}

	strncpy(mime, contentInfo.mime_type, 100);
	MS_DBG("DRM contentType : %s", contentInfo.mime_type);
	MS_DBG("DRM mime : %s", mime);

	return MS_ERR_NONE;
}

int
ms_drm_register(const char* path)
{
	MS_DBG("THIS IS DRM FILE");
	int res = MS_ERR_NONE;
	int ret;

//	ms_inoti_add_ignore_file(path);
	ret = drm_process_request(DRM_REQUEST_TYPE_REGISTER_FILE, (void *)path, NULL);
	if (ret != DRM_RETURN_SUCCESS) {
		MS_DBG_ERR("drm_svc_register_file error : %d", ret);
		res = MS_ERR_DRM_REGISTER_FAIL;
	}

	return res;
}

void
ms_drm_unregister(const char* path)
{
	int ret;
	ms_ignore_file_info *ignore_file;

	ret = drm_process_request(DRM_REQUEST_TYPE_UNREGISTER_FILE, (void *)path, NULL);
	if (ret != DRM_RETURN_SUCCESS)
		MS_DBG_ERR("drm_process_request error : %d", ret);

	ignore_file = ms_inoti_find_ignore_file(path);
	if (ignore_file != NULL)
		ms_inoti_delete_ignore_file(ignore_file);
}

void
ms_drm_unregister_all(void)
{
	if (drm_process_request(DRM_REQUEST_TYPE_UNREGISTER_ALL_FILES , NULL, NULL) == DRM_RETURN_SUCCESS)
		MS_DBG("drm_svc_unregister_all_contents OK");
}

bool
ms_drm_insert_ext_memory(void)
{
	MS_DBG("");
	if (drm_process_request(DRM_REQUEST_TYPE_INSERT_EXT_MEMORY, NULL, NULL) != DRM_RETURN_SUCCESS)
		return false;

	return true;
}

bool
ms_drm_extract_ext_memory(void)
{
	MS_DBG("");
	if (drm_process_request(DRM_REQUEST_TYPE_EXTRACT_EXT_MEMORY , NULL, NULL)  != DRM_RETURN_SUCCESS)
		return false;

	return true;
}


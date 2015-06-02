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

#include "media-util.h"

#include "media-common-dbg.h"
#include "media-common-types.h"
#include "media-common-drm.h"

bool
ms_is_drm_file(const char *path)
{
#ifdef __SUPPORT_DRM
	int ret;
	drm_bool_type_e is_drm_file = DRM_UNKNOWN;

	ret = drm_is_drm_file(path,&is_drm_file);
	if(DRM_RETURN_SUCCESS == ret && DRM_TRUE == is_drm_file)
		return true;
#endif
	return false;
}

int
ms_drm_register(const char* path)
{

	int res = MS_MEDIA_ERR_NONE;
#ifdef __SUPPORT_DRM
	int ret;
	ret = drm_process_request(DRM_REQUEST_TYPE_REGISTER_FILE, (void *)path, NULL);
	if (ret != DRM_RETURN_SUCCESS) {
		MS_DBG_ERR("drm_svc_register_file error : %d", ret);
		res = MS_MEDIA_ERR_DRM_REGISTER_FAIL;
	}
#endif
	return res;
}

void
ms_drm_unregister(const char* path)
{
#ifdef __SUPPORT_DRM
	int ret;

	ret = drm_process_request(DRM_REQUEST_TYPE_UNREGISTER_FILE, (void *)path, NULL);
	if (ret != DRM_RETURN_SUCCESS)
		MS_DBG_ERR("drm_process_request error : %d", ret);
#endif
}

void
ms_drm_unregister_all(void)
{
#ifdef __SUPPORT_DRM
	if (drm_process_request(DRM_REQUEST_TYPE_UNREGISTER_ALL_FILES , NULL, NULL) == DRM_RETURN_SUCCESS)
		MS_DBG("drm_svc_unregister_all_contents OK");
#endif
}

bool
ms_drm_insert_ext_memory(void)
{
#ifdef __SUPPORT_DRM
	MS_DBG("");
	if (drm_process_request(DRM_REQUEST_TYPE_INSERT_EXT_MEMORY, NULL, NULL) != DRM_RETURN_SUCCESS)
		return false;
#endif
	return true;
}

bool
ms_drm_extract_ext_memory(void)
{
#ifdef __SUPPORT_DRM
	MS_DBG("");
	if (drm_process_request(DRM_REQUEST_TYPE_EXTRACT_EXT_MEMORY , NULL, NULL)  != DRM_RETURN_SUCCESS)
		return false;
#endif
	return true;
}


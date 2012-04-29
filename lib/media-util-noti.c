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
 * @file		media-util-noti.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "media-util-dbg.h"
#include "media-util-err.h"
#include "media-util-internal.h"
#include "media-util-noti.h"

 int ms_noti_update_complete(void)
{
	int ret;
	int err = MS_MEDIA_ERR_NONE;

	ret = mkdir(MS_MEDIA_UPDATE_NOTI_PATH, 0777);
	if (ret != 0) {
		MSAPI_DBG("%s", strerror(errno));
		err = MS_MEDIA_ERR_OCCURRED;
	}

	return err;
}


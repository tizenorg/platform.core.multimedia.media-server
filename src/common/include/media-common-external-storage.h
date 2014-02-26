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
#ifndef _MEDIA_SERVER_EXTERNAL_STORAGE_H_
#define _MEDIA_SERVER_EXTERNAL_STORAGE_H_

#include "media-common-types.h"

void
ms_init_default_path(void);

void
ms_make_default_path_mmc(void);

int
ms_update_mmc_info(void);

void
ms_mmc_removed_handler(void);

int
ms_present_mmc_status(ms_sdcard_status_type_t status);

void
ms_mmc_vconf_cb(void *data);

ms_dir_scan_type_t
ms_get_mmc_state(void);

#endif /*_MEDIA_SERVER_EXTERNAL_STORAGE_H_*/

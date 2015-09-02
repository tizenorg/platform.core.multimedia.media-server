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

#ifndef DISABLE_NOTIFICATION
int ms_present_mmc_status(ms_sdcard_status_type_t status);
#endif

int ms_get_mmc_id(char **cid);
int ms_get_stg_changed_event(void);

int ms_read_device_info(const char *root_path, char **device_uuid);
int ms_write_device_info(const char *root_path, char *device_uuid);

#endif /*_MEDIA_SERVER_EXTERNAL_STORAGE_H_*/

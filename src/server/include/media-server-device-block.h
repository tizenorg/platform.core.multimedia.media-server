/*
 * Media Server
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
#ifndef _MEDIA_SERVER_DEVICE_BLOCK_H_
#define _MEDIA_SERVER_DEVICE_BLOCK_H_

#include "media-common-system.h"

int ms_mmc_insert_handler(const char *mount_path);
int ms_mmc_remove_handler(const char *mount_path);
int ms_usb_insert_handler(const char *mount_path, const char *mount_uuid);
int ms_usb_remove_handler(const char *mount_path, const char *mount_uuid);
void ms_device_block_changed_cb(ms_block_info_s *block_info, void *user_data);

#endif
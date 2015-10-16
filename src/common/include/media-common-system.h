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
#ifndef _MEDIA_COMMON_SYSTEM_H_
#define _MEDIA_COMMON_SYSTEM_H_

#include <glib.h>

typedef enum {
	MS_STG_REMOVED = 0,
	MS_STG_INSERTED = 1,
	MS_STG_MAX,
} ms_stg_status_e;

typedef enum {
	MS_STG_TYPE_USB = 0,
	MS_STG_TYPE_MMC = 1,
	MS_STG_TYPE_ALL = 2,
	MS_STG_TYPE_MAX,
} ms_stg_type_e;

typedef struct ms_block_info_s{
	char *mount_path;
	int state;
	int block_type;
} ms_block_info_s;

typedef void (*block_changed_cb)(ms_block_info_s *block_info, void *user_data);
int ms_sys_set_device_block_event_cb(block_changed_cb usr_callback, void *usr_data);
int ms_sys_unset_device_block_event_cb(void);
int ms_sys_get_device_list(ms_stg_type_e stg_type, GArray **dev_list);
int ms_sys_release_device_list(GArray **dev_list);

typedef struct ms_power_info_s{
	int option;
} ms_power_info_s;

typedef void (*power_off_cb)(ms_power_info_s *power_info, void *user_data);
int ms_sys_set_poweroff_cb(power_off_cb user_callback, void *user_data);
int ms_sys_unset_poweroff_cb(void);

int ms_sys_get_uid(uid_t *uid);


#endif

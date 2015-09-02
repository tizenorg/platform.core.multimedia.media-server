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

#ifndef _MEDIA_SCANNER_SCAN_H_
#define _MEDIA_SCANNER_SCAN_H_

#include "media-common-system.h"

typedef enum {
	MS_SCAN_STORAGE = 0,
	MS_SCAN_DIRECTORY = 1,
	MS_SCAN_REGISTER = 2,
	MS_SCAN_MAX,
} ms_scan_type_e;

gboolean msc_directory_scan_thread(void *data);
gboolean msc_register_thread(void *data);
gboolean msc_storage_scan_thread(void *data);
int msc_check_remain_task(void);
ms_db_status_type_t msc_check_scanning_status(void);
int msc_set_cancel_path(const char *cancel_path);
int msc_del_cancel_path(void);
int msc_set_blocked_path(const char *blocked_path);
int msc_del_blocked_path(void);
void msc_metadata_update_thread(ms_comm_msg_s *recv_msg);
int msc_init_scanner(void);
int msc_deinit_scanner(void);
int msc_set_mmc_status(ms_stg_status_e status);
int msc_push_scan_request(ms_scan_type_e scan_type, ms_comm_msg_s *recv_msg);
int msc_send_power_off_request(void);
int msc_remove_dir_scan_request(ms_comm_msg_s *recv_msg);

#endif /*_MEDIA_SCANNER_SCAN_H_*/
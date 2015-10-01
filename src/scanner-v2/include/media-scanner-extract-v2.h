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

#ifndef _MEDIA_SCANNER_EXTRACT_V2_H_
#define _MEDIA_SCANNER_EXTRACT_V2_H_

typedef enum {
	MS_EXTRACT_STORAGE = 0,
	MS_EXTRACT_DIRECTORY = 1,
	MS_EXTRACT_MAX,
} ms_extract_type_e;

int msc_init_extract_thread();
int msc_deinit_extract_thread();
gboolean msc_folder_extract_thread(void *data);
gboolean msc_storage_extract_thread(void *data);
void msc_insert_exactor_request(int message_type, bool ins_status, const char *storage_id, const char *path, int pid, uid_t uid);
int msc_remove_extract_request(const ms_comm_msg_s *recv_msg);
int msc_set_extract_cancel_path(const char *cancel_path);
int msc_del_extract_cancel_path(void);
int msc_set_extract_blocked_path(const char *blocked_path);
int msc_del_extract_blocked_path(void);
int msc_push_extract_request(ms_extract_type_e scan_type, ms_comm_msg_s *recv_msg);
int msc_stop_extract_thread(void);
int msc_get_remain_extract_request(ms_extract_type_e scan_type, int *remain_request);
int msc_get_dir_extract_status(bool *extract_status);

#endif /*_MEDIA_SCANNER_EXTRACT_V2_H_*/

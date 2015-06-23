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

#ifndef _MEDIA_SCANNER_SOCKET_H_
#define _MEDIA_SCANNER_SOCKET_H_

#include "media-common-types.h"
#include "media-server-ipc.h"

gboolean msc_receive_request(GIOChannel *src, GIOCondition condition, gpointer data);

int msc_send_ready(void);

int msc_send_result(int result, ms_comm_msg_s *scan_data);

#endif /*_MEDIA_SCANNER_SOCKET_H_*/

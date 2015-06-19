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
 * @file		media-server-thumb.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_SERVER_SOCKET_H_
#define _MEDIA_SERVER_SOCKET_H_

#include "media-common-types.h"
#include "media-server-ipc.h"

gboolean ms_read_socket(GIOChannel *src, GIOCondition condition, gpointer data);

gboolean ms_receive_message_from_scanner(GIOChannel *src, GIOCondition condition, gpointer data);

int ms_send_scan_request(ms_comm_msg_s *send_msg);

int ms_send_storage_scan_request(ms_storage_type_t storage_type, ms_dir_scan_type_t scan_type);

gboolean ms_read_db_socket(GIOChannel *src, GIOCondition condition, gpointer data);
gboolean ms_read_db_tcp_socket(GIOChannel *src, GIOCondition condition, gpointer data);

#endif /*_MEDIA_SERVER_SOCKET_H_*/
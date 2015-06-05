/*
 * media-thumbnail-server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunjun Ko <zzoon.ko@samsung.com>
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

#include <glib.h>
#ifdef _USE_UDS_SOCKET_
#include <sys/un.h>
#else
#include <sys/socket.h>
#endif
#include <arpa/inet.h>
#include "media-common-types.h"
#include "media-server-ipc.h"

#ifndef _MEDIA_SERVER_THUMB_H_
#define _MEDIA_SERVER_THUMB_H_

#define MAX_THUMB_REQUEST 100

GMainLoop *
ms_get_thumb_thread_mainloop(void);

int
ms_thumb_get_server_pid();

void
ms_thumb_reset_server_status();

gpointer
ms_thumb_agent_start_thread(gpointer data);

int
_ms_thumb_create_socket(int sock_type, int *sock);

int
_ms_thumb_create_udp_socket(int *sock);

gboolean
_ms_thumb_agent_prepare_udp_socket();

int
_ms_thumb_recv_msg(int sock, int header_size, thumbMsg *msg);

#ifdef _USE_UDS_SOCKET_
int
_ms_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size);
#else
int
_ms_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_in *from_addr, unsigned int *from_size);
#endif

int
_ms_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size);

#endif /*_MEDIA_SERVER_THUMB_H_*/


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
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <media-util-err.h>

#include "media-server-global.h"
#include "media-server-db-svc.h"
#include "media-server-socket.h"

#define MS_REGISTER_PORT 1001

GAsyncQueue* ret_queue;

gboolean ms_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
	struct sockaddr_in client_addr;
	unsigned int client_addr_len;

	char recv_buff[MS_FILE_PATH_LEN_MAX] = {0};
	int send_msg = MS_MEDIA_ERR_NONE;
	int recv_msg_size;
	int sock = -1;
	int ret;
	void **handle = data;

	memset(recv_buff, 0, sizeof(recv_buff));

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	/* Socket is readable */
	client_addr_len = sizeof(client_addr);
	if ((recv_msg_size = recvfrom(sock, &recv_buff, sizeof(recv_buff), 0, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
		MS_DBG_ERR("recvfrom failed");
		return TRUE;
	}
	ret = ms_register_file(handle, recv_buff, ret_queue);
	if (ret == MS_ERR_NOW_REGISTER_FILE) {
		ret= GPOINTER_TO_INT(g_async_queue_pop(ret_queue)) - MS_ERR_MAX;
	}

	if (ret != MS_ERR_NONE) {
		MS_DBG_ERR("ms_register_file error : %d", ret);

		if(ret == MS_ERR_ARG_INVALID) {
			send_msg = MS_MEDIA_ERR_INVALID_PARAMETER;
		} else if (ret == MS_ERR_MIME_GET_FAIL) {
			send_msg = MS_MEDIA_ERR_INVALID_MEDIA;
		} else if (ret == MS_ERR_DB_INSERT_RECORD_FAIL) {
			send_msg = MS_MEDIA_ERR_INSERT_FAIL;
		} else if (ret == MS_ERR_DRM_REGISTER_FAIL) {
			send_msg = MS_MEDIA_ERR_DRM_INSERT_FAIL;
		}
	} else {
		MS_DBG("SOCKET INSERT SECCESS : %s", recv_buff);
		send_msg = MS_MEDIA_ERR_NONE;
	}

	if (sendto(sock, &send_msg, sizeof(send_msg), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) != sizeof(send_msg)) {
		MS_DBG_ERR("sendto failed");
	} else {
		MS_DBG("Sent %d", send_msg);
	}

	/*Active flush */
	malloc_trim(0);

	return TRUE;
}

gboolean ms_prepare_socket(int *sock_fd)
{
	int sock;
	struct sockaddr_in serv_addr;
	unsigned short serv_port;

	serv_port = MS_REGISTER_PORT;

	/* Creaete a datagram/UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		MS_DBG_ERR("socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);

	/* Bind to the local address */
	if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		MS_DBG_ERR("bind failed");
		return FALSE;
	}

	MS_DBG("bind success");

	*sock_fd = sock;

	return TRUE;
}


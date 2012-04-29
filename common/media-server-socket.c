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

gboolean ms_socket_thread(void *data)
{
	int ret;
	int err;
	int state;
	int sockfd;
	int send_msg = MS_MEDIA_ERR_NONE;
	int client_addr_size;
	void *handle = NULL;

	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;

	char recv_buff[MS_FILE_PATH_LEN_MAX];

	sockfd = socket(PF_INET, SOCK_DGRAM, 0);

	if(sockfd < 0)
	{
		MS_DBG("socket create error");
		perror("socket error : ");
		return MS_ERR_SOCKET_CONN;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(MS_REGISTER_PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	state = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(state < 0)
	{
		MS_DBG("bind error");
		perror("bind error : ");
		return MS_ERR_SOCKET_BIND;
	}

	err = ms_connect_db(&handle);
	if (err != MS_ERR_NONE) {
		MS_DBG("SOCKET : sqlite3_open: ret = %d", err);
		return false;
	}

	while(1)
	{
		client_addr_size  = sizeof(client_addr);
		err = recvfrom(sockfd, recv_buff, sizeof(recv_buff), 0 ,
			(struct sockaddr*)&client_addr, (socklen_t *)&client_addr_size);
		if(err < 0){
			MS_DBG("recvfrom error :%d", errno);
			perror("recvfrom error : ");
			goto NEXT;
		} else {
			MS_DBG("receive: %s\n", recv_buff);
		}

		ret = ms_register_file(handle, recv_buff, ret_queue);
		if (ret == MS_ERR_NOW_REGISTER_FILE) {
			MS_DBG("WAIT");
			ret= GPOINTER_TO_INT(g_async_queue_pop(ret_queue)) - MS_ERR_MAX;
			MS_DBG("RECEIVE REPLAY");
		}

		if (ret != MS_ERR_NONE) {
			MS_DBG("ms_register_file error : %d", ret);

			if(ret == MS_ERR_ARG_INVALID) {
				send_msg = MS_MEDIA_ERR_INVALID_PARAMETER;
			} else if (ret == MS_ERR_NOT_MEDIA_FILE) {
				send_msg = MS_MEDIA_ERR_INVALID_MEDIA;
			} else if (ret == MS_ERR_DB_INSERT_RECORD_FAIL) {
				send_msg = MS_MEDIA_ERR_INSERT_FAIL;
			} else if (ret == MS_ERR_DRM_REGISTER_FAIL) {
				send_msg = MS_MEDIA_ERR_DRM_INSERT_FAIL;
			}
		} else {
			MS_DBG("SOCKET INSERT SECCESS");
			send_msg = MS_MEDIA_ERR_NONE;
		}

		err = sendto(sockfd, &send_msg, sizeof(send_msg), 0,
			(struct sockaddr*)&client_addr, sizeof(client_addr));
		if(err < 0){
			MS_DBG("SOCKET SEND FAIL :%d", errno);
			perror("send error : ");
		} else {
			MS_DBG("SOCKET SEND SUCCESS");
		}
NEXT:
		memset(recv_buff, 0, MS_FILE_PATH_LEN_MAX);
	}

	close(sockfd);
	MS_DBG("END SOCKET THREAD");

	err = ms_disconnect_db(handle);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_media_db_close error : %d", err);
		return false;
	}
	MS_DBG("Disconnect MEDIA DB");

	return 0;
}


/*
 *  Media Utility
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
 * @file		media-util-register.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>

#include <media-svc.h>
#include <audio-svc.h>
#include <audio-svc-error.h>

#include "media-util-dbg.h"
#include "media-util-err.h"
#include "media-util-internal.h"
#include "media-util-register.h"

static bool _is_valid_path(const char *path)
{
       if (path == NULL)
               return false;

	if (strncmp(path, MS_MEDIA_PHONE_ROOT_PATH, strlen(MS_MEDIA_PHONE_ROOT_PATH)) == 0) {
		return true;
	} else if (strncmp(path, MS_MEDIA_MMC_ROOT_PATH, strlen(MS_MEDIA_MMC_ROOT_PATH)) == 0) {
		return true;
	} else
		return false;

       return true;
}

int media_file_register(const char *file_full_path)
{
	int exist;
	int err;
	int sockfd;
	int recv_msg = MS_MEDIA_ERR_NONE;
	int server_addr_size;
	struct sockaddr_in server_addr;
	struct timeval tv_timeout = { MS_MEDIA_TIMEOUT_SEC, 0 };

	if(!_is_valid_path(file_full_path)) {
		MSAPI_DBG("Invalid path : %s", file_full_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	exist = open(file_full_path, O_RDONLY);
	if(exist < 0) {
		MSAPI_DBG("Not exist path : %s", file_full_path);
		return MS_MEDIA_ERR_INVALID_PATH;
	}
	close(exist);

	sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0)
	{
		MSAPI_DBG("socket create fail");
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/*add timeout : timeout is 10 sec.*/
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
		MSAPI_DBG("setsockopt failed");
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(MS_MEDIA_REGISTER_PORT);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	err = sendto(sockfd, file_full_path, strlen(file_full_path), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (err < 0) {
		MSAPI_DBG("sendto error");
		perror("sendto error : ");
		return MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("SEND OK");
	}

	server_addr_size = sizeof(server_addr);
	err = recvfrom(sockfd, &recv_msg, sizeof(recv_msg), 0 , (struct sockaddr*)&server_addr, (socklen_t *)&server_addr_size);
	if (err < 0) {
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG("recvfrom timeout");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG("recvfrom error");
			perror("recvfrom error : ");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	} else {
		MSAPI_DBG("RECEIVE OK");
		MSAPI_DBG("client receive: %d", recv_msg);
	}

	close(sockfd);

	return recv_msg;
}

int media_list_new(media_list *list)
{
	*list = g_array_new(TRUE, TRUE, sizeof(char*));

	return MS_MEDIA_ERR_NONE;
}

int media_list_add(media_list list, const char* file_full_path)
{
	MSAPI_DBG("");

	if (!list) {
		MSAPI_DBG("list == NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (!file_full_path) {
		MSAPI_DBG("file_full_path == NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	media_list ret_list = NULL;
	char *path = strdup(file_full_path);

	int len = list->len + 1;
	int i;
	char *data = NULL;

	ret_list = g_array_append_val(list, path);
	if(ret_list == NULL) {
		MSAPI_DBG("g_array_append_val fails");
		return MS_MEDIA_ERR_UNKNOWN;
	}

	list = ret_list;

	for(i = 0; i < len; i++) {
		data = g_array_index(list, char*, i);
		MSAPI_DBG("%d, %s", i, data);
	}

	return MS_MEDIA_ERR_NONE;
}

int media_list_free(media_list list)
{
	if (!list)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	int len = list->len + 1;
	int i;
	char *data = NULL;

	for(i = 0; i < len; i++) {
		data = g_array_index(list, char*, i);
		free(data);
	}

	g_array_free(list, TRUE);

	return MS_MEDIA_ERR_NONE;
}

int media_files_register(const media_list list)
{
	if (!list)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	int len = list->len + 1;
	int i;
	char *data;

	for(i = 0; i < len; i++) {
		data = g_array_index(list, char*, i);
		media_file_register(data);
	}

	return MS_MEDIA_ERR_NONE;
}


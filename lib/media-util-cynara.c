/*
 *  Media Server
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
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
 * This file contains Cynara integration code
 *
 * @file    media-util-cynara.c
 * @author  Jacek Bukarewicz (j.bukarewicz@samsung.com)
 * @version 1.0
 * @brief
 */

#define _GNU_SOURCE

#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include <media-util-cynara.h>
#include <media-util-dbg.h>
#include <media-util-err.h>
#include <media-util-ipc.h>

#include <cynara-client.h>
#include <cynara-session.h>
#include <cynara-error.h>
#include <cynara-creds-socket.h>


/* this definition is missing in glibc headers (version 2.21). It was introduced in kernel version 2.6.17 */
#ifndef SCM_SECURITY
#define SCM_SECURITY 0x03
#endif

static cynara *_cynara = NULL;
G_LOCK_DEFINE_STATIC(cynara_mutex);

static void ms_cynara_dbg_err(const char *prefix, int error_code)
{
	char error_buffer[256];
	int err;
	error_buffer[0] = '\0';

	err = cynara_strerror(error_code, error_buffer, sizeof(error_buffer));
	if (err == CYNARA_API_SUCCESS) {
		MSAPI_DBG_ERR("%s: %s", prefix, error_buffer);
	} else {
		MSAPI_DBG_ERR("%s: error code %i", prefix, error_code);
	}
}

int ms_cynara_initialize(void)
{
	int ret = cynara_initialize(&_cynara, NULL);
	if (ret != CYNARA_API_SUCCESS) {
		ms_cynara_dbg_err("cynara_initialize", ret);
		return MS_MEDIA_ERR_INTERNAL;
	}

	return MS_MEDIA_ERR_NONE;
}

void ms_cynara_finish(void)
{
	cynara_finish(_cynara);
	_cynara = NULL;
}

int ms_cynara_receive_untrusted_message(int sockfd, ms_comm_msg_s *recv_msg, ms_peer_credentials *credentials)
{
	int ret = 0;
	int recv_msg_size = 0;

	if (!recv_msg ||!credentials)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	if ((recv_msg_size = read(sockfd, recv_msg, sizeof(ms_comm_msg_s))) < 0) {
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_ERR("recv failed");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG_SLOG("receive msg from [%d] %d, %s", recv_msg->pid, recv_msg->msg_type, recv_msg->msg);

	if (!(recv_msg->msg_size > 0 && recv_msg->msg_size < MAX_FILEPATH_LEN)) {
		MSAPI_DBG_ERR("IPC message is wrong. message size is %d", recv_msg->msg_size);
		return  MS_MEDIA_ERR_INVALID_IPC_MESSAGE;
	}

	ret = cynara_creds_socket_get_pid(sockfd, &(credentials->pid));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_pid failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	ret = cynara_creds_socket_get_user(sockfd, USER_METHOD_UID, &(credentials->uid));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_user failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	ret = cynara_creds_socket_get_client(sockfd, CLIENT_METHOD_SMACK, &(credentials->smack));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_client failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

//	MSAPI_DBG_ERR("cynara_creds_info : P[%d]U[%s]S[%s]", credentials->pid, credentials->uid, credentials->smack);

	return MS_MEDIA_ERR_NONE;
}

int ms_cynara_receive_untrusted_message_udp(int sockfd, thumbMsg *recv_msg, unsigned int msg_size,
        struct sockaddr_un *recv_addr, unsigned int *addr_size, ms_peer_credentials *credentials)
{
	int ret = 0;
	int recv_msg_len = 0;
	unsigned int from_addr_size = sizeof(struct sockaddr_un);
	unsigned char *buf = NULL;

	if (!recv_msg || !recv_addr || !credentials)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	buf = (unsigned char*)malloc(sizeof(thumbMsg));

	if ((recv_msg_len = recvfrom(sockfd, buf, sizeof(thumbMsg), 0, (struct sockaddr *)recv_addr, &from_addr_size)) < 0) {
		SAFE_FREE(buf);
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_ERR("recvfrom failed : %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	memcpy(recv_msg, buf, msg_size);

	if (recv_msg->origin_path_size <= 0) {
		SAFE_FREE(buf);
		MSAPI_DBG_ERR("msg->origin_path_size is invalid %d", recv_msg->origin_path_size );
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	strncpy(recv_msg->org_path, (char*)buf + msg_size, recv_msg->origin_path_size);

	if (recv_msg->dest_path_size <= 0) {
		SAFE_FREE(buf);
		MSAPI_DBG_ERR("msg->origin_path_size is invalid %d", recv_msg->dest_path_size );
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	strncpy(recv_msg->dst_path, (char*)buf + msg_size + recv_msg->origin_path_size, recv_msg->dest_path_size);

	SAFE_FREE(buf);
	*addr_size = from_addr_size;

	ret = cynara_creds_socket_get_pid(sockfd, &(credentials->pid));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_pid failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	ret = cynara_creds_socket_get_user(sockfd, USER_METHOD_UID, &(credentials->uid));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_user failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	ret = cynara_creds_socket_get_client(sockfd, CLIENT_METHOD_SMACK, &(credentials->smack));
	if(ret < 0) {
		MSAPI_DBG_ERR("cynara_creds_socket_get_client failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

//	MSAPI_DBG_ERR("cynara_creds_info : P[%d]U[%s]S[%s]", credentials->pid, credentials->uid, credentials->smack);

	return MS_MEDIA_ERR_NONE;
}

int ms_cynara_get_credentials_from_connected_socket(int sockfd, ms_peer_credentials *credentials)
{
	int ret;
	struct ucred ucreds;
	socklen_t optlen;

	if (!credentials)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	optlen = sizeof(ucreds);
	ret = getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED, &ucreds, &optlen);
	if (ret != 0) {
		MSAPI_DBG_ERR("Failed to get peer credentials from connected socket");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	optlen = sizeof(credentials->smack);
	ret = getsockopt(sockfd, SOL_SOCKET, SO_PEERSEC, credentials->smack, &optlen);
	if (ret != 0) {
		MSAPI_DBG_ERR("Failed to get peer security label from connected socket");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	cynara_creds_socket_get_pid(sockfd, &(credentials->pid));
	cynara_creds_socket_get_user(sockfd, USER_METHOD_UID, &(credentials->uid));

	return MS_MEDIA_ERR_NONE;
}

int ms_cynara_check(const ms_peer_credentials *creds, const char *privilege)
{
	int result;
	char *session;

	if (!creds || !privilege)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	session = cynara_session_from_pid(creds->pid);
	if (session == NULL) {
		MSAPI_DBG_ERR("cynara_session_from_pid failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	G_LOCK(cynara_mutex);
	result = cynara_check(_cynara, creds->smack, session, creds->uid, privilege);
	G_UNLOCK(cynara_mutex);

	if (result < 0)
		ms_cynara_dbg_err("cynara_check", result);

	free(session);
	return result == CYNARA_API_ACCESS_ALLOWED ? MS_MEDIA_ERR_NONE : MS_MEDIA_ERR_PERMISSION_DENIED;
}

int ms_cynara_enable_credentials_passing(int fd)
{
	const int optval = 1;
	int r;

	r = setsockopt(fd, SOL_SOCKET, SO_PASSSEC, &optval, sizeof(optval));
	if (r != 0) {
		MSAPI_DBG_ERR("Failed to set SO_PASSSEC socket option");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	r = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval));
	if (r != 0) {
		MSAPI_DBG_ERR("Failed to set SO_PASSCRED socket option");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	return MS_MEDIA_ERR_NONE;
}



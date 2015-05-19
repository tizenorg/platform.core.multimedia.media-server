/*
 *  Media Server
 *
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
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
 * @author	Jacek Bukarewicz (j.bukarewicz@samsung.com)
 * @version	1.0
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


/* this definition is missing in glibc headers (version 2.21). It was introduced in kernel version 2.6.17 */
#ifndef SCM_SECURITY
#define SCM_SECURITY 0x03
#endif

static cynara *_cynara = NULL;
G_LOCK_DEFINE_STATIC(cynara_mutex);

static void ms_cynara_dbg_err(const char *prefix, int error_code)
{
	char error_buffer[256];
	error_buffer[0] = '\0';

	cynara_strerror(error_code, error_buffer, sizeof(error_buffer));
	MSAPI_DBG_ERR("%s: %s", prefix, error_buffer);
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

int ms_cynara_receive_untrusted_message(int sockfd, void *recv_msg, unsigned int msg_size,
        struct sockaddr_un *recv_addr, unsigned int *addr_size, ms_peer_credentials *credentials)
{
	struct msghdr hdr;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	struct sockaddr_un addr;
	char msg_control[1024];
	ssize_t received;
	bool seclabel_received = false;
	bool ucreds_received = false;

	if (!recv_msg || !recv_addr || !credentials)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	iov[0].iov_base=recv_msg;
	iov[0].iov_len=msg_size;

	hdr.msg_name = &addr;
	hdr.msg_namelen = sizeof(addr);
	hdr.msg_iov = iov;
	hdr.msg_iovlen = sizeof(iov)/sizeof(iov[0]);
	hdr.msg_control = msg_control;
	hdr.msg_controllen = sizeof(msg_control);

	received = TEMP_FAILURE_RETRY(recvmsg(sockfd, &hdr, 0));
	if (received < 0) {
		MSAPI_DBG_ERR("recvmsg failed [%s]", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	for (cmsg = CMSG_FIRSTHDR(&hdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&hdr, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;

		switch (cmsg->cmsg_type) {
			case SCM_SECURITY:
			{
				int datalen = cmsg->cmsg_len - sizeof(*cmsg);
				if (datalen < sizeof(credentials->peersec)) {
					memcpy(credentials->peersec, CMSG_DATA(cmsg), datalen);
					credentials->peersec[datalen] = '\0';
					seclabel_received = true;
				} else {
					MSAPI_DBG_ERR("ms_peer_credentials::seclabel buffer is too small");
					return MS_MEDIA_ERR_INVALID_PARAMETER;
				}
				break;
			}
			case SCM_CREDENTIALS:
			{
				struct ucred *ucredp = (struct ucred *) CMSG_DATA(cmsg);
				credentials->pid = ucredp->pid;
				credentials->uid = ucredp->uid;
				ucreds_received = true;
				break;
			}

		}
	}

	if (!seclabel_received || !ucreds_received) {
		MSAPI_DBG_ERR("Failed to obtain credentials");
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	if (recv_addr != NULL)
		*recv_addr = addr;
	if (addr_size != NULL)
		*addr_size = sizeof(addr);

	return MS_MEDIA_ERR_NONE;
}

int ms_cynara_get_credentials_from_connected_socket(int sockfd, ms_peer_credentials *credentials)
{
	int ret;
	struct ucred ucreds;
	socklen_t optlen;

	optlen = sizeof(credentials->peersec);
	ret = getsockopt(sockfd, SOL_SOCKET, SO_PEERSEC, credentials->peersec, &optlen);
	if (ret != 0) {
		MSAPI_DBG_ERR("Failed to get peer security label from connected socket");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	optlen = sizeof(ucreds);
	ret = getsockopt(sockfd, SOL_SOCKET, SO_PEERCRED, &ucreds, &optlen);
	if (ret != 0) {
		MSAPI_DBG_ERR("Failed to get peer credentials from connected socket");
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	credentials->uid = ucreds.uid;
	credentials->pid = ucreds.pid;
	return MS_MEDIA_ERR_NONE;
}

int ms_cynara_check(const ms_peer_credentials *creds, const char *privilege)
{
	char user[32];
	int result;
	char *session;

	if (snprintf(user, sizeof(user), "%u", creds->uid) < 0) {
		MSAPI_DBG_ERR("Failed to convert uid to string");
		return MS_MEDIA_ERR_INTERNAL;
	}

	session = cynara_session_from_pid(creds->pid);
	if (session == NULL) {
		MSAPI_DBG_ERR("cynara_session_from_pid failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	G_LOCK(cynara_mutex);
	result = cynara_check(_cynara, creds->peersec, session, user, privilege);
	G_UNLOCK(cynara_mutex);

	if (result < 0)
		ms_cynara_dbg_err("cynara_check", result);

	free(session);
	return result == CYNARA_API_ACCESS_ALLOWED ? MS_MEDIA_ERR_NONE : MS_MEDIA_ERR_ACCESS_DENIED;
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


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
 * This file defines api utilities of IPC.
 *
 * @file		media-util-ipc.c
 * @author	Haejeong Kim(backto.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "media-util-dbg.h"
#include "media-util.h"

#ifdef _USE_UDS_SOCKET_
char MEDIA_IPC_PATH[][50] ={
	{"/tmp/media_ipc_dbbatchupdate.dat"},
	{"/tmp/media_ipc_scandaemon.dat"},
	{"/tmp/media_ipc_scancomm.dat"},
	{"/tmp/media_ipc_scanner.dat"},
	{"/tmp/media_ipc_dbupdate.dat"},
	{"/tmp/media_ipc_thumbcreator.dat"},
	{"/tmp/media_ipc_thumbcomm.dat"},
	{"/tmp/media_ipc_thumbdaemon.dat"},
};

char MEDIA_IPC_PATH_CLIENT[][50] ={
	{"/tmp/media_ipc_dbbatchupdate_client.dat"},
	{"/tmp/media_ipc_scandaemon_client.dat"},
	{"/tmp/media_ipc_scancomm_client.dat"},
	{"/tmp/media_ipc_scanner_client.dat"},
	{"/tmp/media_ipc_dbupdate_client.dat"},
	{"/tmp/media_ipc_thumbcreator_client.dat"},
	{"/tmp/media_ipc_thumbcomm_client.dat"},
	{"/tmp/media_ipc_thumbdaemon_client.dat"},
};
#endif

#ifdef _USE_UDS_SOCKET_
int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd, int port)
#else
int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd)
#endif
{
	int sock = -1;

	struct timeval tv_timeout = { timeout_sec, 0 };

	if(protocol == MS_PROTOCOL_UDP)
	{
#ifdef _USE_UDS_SOCKET_
		struct sockaddr_un serv_addr;
#endif

		/* Create a datagram/UDP socket */
#ifdef _USE_UDS_SOCKET_
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
#else
		if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
#endif
			MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_CONN;
		}

#ifdef _USE_UDS_SOCKET_
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sun_family = AF_UNIX;
		MSAPI_DBG("%s", MEDIA_IPC_PATH_CLIENT[port]);
		unlink(MEDIA_IPC_PATH_CLIENT[port]);
		strcpy(serv_addr.sun_path, MEDIA_IPC_PATH_CLIENT[port]);

		/* Bind to the local address */
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			MSAPI_DBG_ERR("bind failed : %s", strerror(errno));
			close(sock);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
#endif
	}
	else
	{
		/*Create TCP Socket*/
#ifdef _USE_UDS_SOCKET_
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
#else
		if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
#endif
			MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	if (timeout_sec > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			MSAPI_DBG_ERR("setsockopt failed: %s", strerror(errno));
			close(sock);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	*sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_create_server_socket(ms_protocol_e protocol, int port, int *sock_fd)
{
	int i;
	bool bind_success = false;
	int sock = -1;
	int n_reuse = 1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	unsigned short serv_port;

	serv_port = port;

	if(protocol == MS_PROTOCOL_UDP)
	{
		/* Create a datagram/UDP socket */
#ifdef _USE_UDS_SOCKET_
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
#else
		if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
#endif
			MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}
	else
	{
		/* Create a TCP socket */
#ifdef _USE_UDS_SOCKET_
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
#else
		if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
#endif
			MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}
#ifdef _USE_UDS_SOCKET_
#else
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &n_reuse, sizeof(n_reuse)) == -1) {
		MSAPI_DBG_ERR("setsockopt failed: %s", strerror(errno));
		close(sock);
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}
#endif
	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[serv_port]);
	unlink(MEDIA_IPC_PATH[serv_port]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[serv_port]);
#else
	serv_addr.sin_family = AF_INET;
//	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_addr.sin_port = htons(serv_port);
#endif
	/* Bind to the local address */
	for (i = 0; i < 20; i ++) {
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
			bind_success = true;
			break;
		}
		MSAPI_DBG("%d",i);
		usleep(250000);
	}

	if (bind_success == false) {
		MSAPI_DBG_ERR("bind failed : %s %d_", strerror(errno), errno);
		close(sock);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	MSAPI_DBG("bind success");

	/* Listening */
	if (protocol == MS_PROTOCOL_TCP) {
		if (listen(sock, SOMAXCONN) < 0) {
			MSAPI_DBG_ERR("listen failed : %s", strerror(errno));
			close(sock);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}

		MSAPI_DBG("Listening...");
	}

	*sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_send_msg_to_server(int sockfd, int port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr)
#else
int ms_ipc_send_msg_to_server(int sockfd, int port, ms_comm_msg_s *send_msg, struct sockaddr_in *serv_addr)
#endif
{
	int res = MS_MEDIA_ERR_NONE;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un addr;
#else
	struct sockaddr_in addr;
#endif

	/* Set server Address */
	memset(&addr, 0, sizeof(addr));
#ifdef _USE_UDS_SOCKET_
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, MEDIA_IPC_PATH[port]);
	MSAPI_DBG("%s", addr.sun_path);
#else
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	addr.sin_port = htons(port);
#endif

	if (sendto(sockfd, send_msg, sizeof(*(send_msg)), 0, (struct sockaddr *)&addr, sizeof(addr)) != sizeof(*(send_msg))) {
		MSAPI_DBG_ERR("sendto failed [%s]", strerror(errno));
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent %d", send_msg->result);
		MSAPI_DBG("sent %s", send_msg->msg);
		if (serv_addr != NULL)
			*serv_addr = addr;
	}

	return res;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_send_msg_to_client(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_un *client_addr)
#else
int ms_ipc_send_msg_to_client(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_in *client_addr)
#endif
{
	int res = MS_MEDIA_ERR_NONE;

#ifdef _USE_UDS_SOCKET_
	MSAPI_DBG("the path of client address : %s", client_addr->sun_path);
#endif
	if (sendto(sockfd, send_msg, sizeof(*(send_msg)), 0, (struct sockaddr *)client_addr, sizeof(*(client_addr))) != sizeof(*(send_msg))) {
		MSAPI_DBG_ERR("sendto failed [%s]", strerror(errno));
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent %d", send_msg->result);
		MSAPI_DBG("sent %s", send_msg->msg);
	}

	return MS_MEDIA_ERR_NONE;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size)
#else
int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_in *recv_addr, unsigned int *addr_size)
#endif
{
	int recv_msg_size;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un addr;
#else
	struct sockaddr_in addr;
#endif
	socklen_t addr_len;

	if (!recv_msg)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

#ifdef _USE_UDS_SOCKET_
	addr_len = sizeof(addr);
#else
	addr_len = sizeof(struct sockaddr_in);
#endif

	if ((recv_msg_size = recvfrom(sockfd, recv_msg, msg_size, 0, (struct sockaddr *)&addr, &addr_len)) < 0) {
		MSAPI_DBG_ERR("recvfrom failed [%s]", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

#ifdef _USE_UDS_SOCKET_
	MSAPI_DBG("the path of received client address : %s", addr.sun_path);
#endif

	if (recv_addr != NULL)
		*recv_addr = addr;
	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size)
#else
int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_in *recv_addr, unsigned int *addr_size)
#endif
{
	int recv_msg_size;
	socklen_t addr_len;

	if (!recv_msg ||!recv_addr)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

#ifdef _USE_UDS_SOCKET_
	addr_len = sizeof(struct sockaddr_un);
#else
	addr_len = sizeof(struct sockaddr_in);
#endif

	if ((recv_msg_size = recvfrom(sockfd, recv_msg, msg_size, 0, (struct sockaddr *)recv_addr, &addr_len)) < 0) {
		MSAPI_DBG_ERR("recvfrom failed [%s]", strerror(errno));
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("recvfrom Timeout.");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_ERR("recvfrom error [%s]", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}


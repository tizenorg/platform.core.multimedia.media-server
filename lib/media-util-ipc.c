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
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "media-util-dbg.h"
#include "media-util.h"

char MEDIA_IPC_PATH[][70] ={
	{"/var/run/media-server/media_ipc_dbbatchupdate.socket"},
	{"/var/run/media-server/media_ipc_scandaemon.socket"},
	{"/var/run/media-server/media_ipc_scancomm.socket"},
	{"/var/run/media-server/media_ipc_scanner.socket"},
	{"/var/run/media-server/media_ipc_dbupdate.socket"},
	{"/var/run/media-server/media_ipc_thumbcreator.socket"},
	{"/var/run/media-server/media_ipc_thumbcomm.socket"},
	{"/var/run/media-server/media_ipc_thumbdaemon.socket"},
};

char MEDIA_IPC_PATH_CLIENT[][80] ={
	{"/var/run/user/%i/media-server/media_ipc_dbbatchupdate_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_scandaemon_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_scancomm_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_scanner_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_dbupdate_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_thumbcreator_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_thumbcomm_client%i.socket"},
	{"/var/run/user/%i/media-server/media_ipc_thumbdaemon_client%i.socket"},
};

char MEDIA_IPC_PATH_CLIENT_ROOT[][80] ={
	{"/var/run/media-server/media_ipc_dbbatchupdate_client%i.socket"},
	{"/var/run/media-server/media_ipc_scandaemon_client%i.socket"},
	{"/var/run/media-server/media_ipc_scancomm_client%i.socket"},
	{"/var/run/media-server/media_ipc_scanner_client%i.socket"},
	{"/var/run/media-server/media_ipc_dbupdate_client%i.socket"},
	{"/var/run/media-server/media_ipc_thumbcreator_client%i.socket"},
	{"/var/run/media-server/media_ipc_thumbcomm_client%i.socket"},
	{"/var/run/media-server/media_ipc_thumbdaemon_client%i.socket"},
};

static int _mkdir(const char *dir, mode_t mode) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, mode);
                        *p = '/';
                }
        return mkdir(tmp, mode);
}

int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd, int port)
{
	int sock = -1;

	struct timeval tv_timeout = { timeout_sec, 0 };

	char *buffer;
	char *path;
	int cx,len;

	if (tzplatform_getuid(TZ_USER_NAME) == 0 ){
		cx = snprintf ( NULL, 0, MEDIA_IPC_PATH_CLIENT_ROOT[port],getpid());
		buffer = (char*)malloc((cx + 1 )*sizeof(char));
		snprintf ( buffer, cx + 1,  MEDIA_IPC_PATH_CLIENT_ROOT[port],getpid());
	} else {
		len = snprintf ( NULL, 0, "/var/run/user/%i/media-server", tzplatform_getuid(TZ_USER_NAME));
		path = (char*)malloc((len + 1 )*sizeof(char));
		snprintf ( path, len + 1, "/var/run/user/%i/media-server", tzplatform_getuid(TZ_USER_NAME));
		_mkdir(path, 0777);
		free(path);
		cx = snprintf ( NULL, 0, MEDIA_IPC_PATH_CLIENT[port], tzplatform_getuid(TZ_USER_NAME),getpid());
		buffer = (char*)malloc((cx + 1 )*sizeof(char));
		snprintf ( buffer, cx + 1,  MEDIA_IPC_PATH_CLIENT[port],tzplatform_getuid(TZ_USER_NAME),getpid());
	}
		
	if(protocol == MS_PROTOCOL_UDP)
	{
		struct sockaddr_un serv_addr;

		/* Create a datagram/UDP socket */
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sun_family = AF_UNIX;
		MSAPI_DBG("%s", buffer);
		unlink(buffer);
		strcpy(serv_addr.sun_path, buffer);
		
		/* Bind to the local address */
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			MSAPI_DBG_STRERROR("bind failed");
			close(sock);
			free(buffer);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}
	else
	{
		/*Create TCP Socket*/
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			free(buffer);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	if (timeout_sec > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			MSAPI_DBG_STRERROR("setsockopt failed");
			close(sock);
			free(buffer);

			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	*sock_fd = sock;
	free(buffer);

	return MS_MEDIA_ERR_NONE;
}

#ifdef _USE_UDS_SOCKET_TCP_
int ms_ipc_create_client_tcp_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd, int port)
{
	int sock = -1;

	struct timeval tv_timeout = { timeout_sec, 0 };

	/*Create TCP Socket*/
	if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
			MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_CONN;
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

int ms_ipc_create_server_tcp_socket(ms_protocol_e protocol, int port, int *sock_fd)
{
	int i;
	bool bind_success = false;
	int sock = -1;

	struct sockaddr_un serv_addr;
	mode_t orig_mode;

	/* Create a TCP socket */
	if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
		MSAPI_DBG_ERR("socket failed: %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[port]);
	unlink(MEDIA_IPC_PATH[port]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[port]);

	orig_mode = umask(0);

	/* Bind to the local address */
	for (i = 0; i < 20; i ++) {
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
			bind_success = true;
			break;
		}
		MSAPI_DBG("%d",i);
		usleep(250000);
	}

	umask(orig_mode);

	if (bind_success == false) {
		MSAPI_DBG_ERR("bind failed : %s %d_", strerror(errno), errno);
		close(sock);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	MSAPI_DBG("bind success");

	/* Listening */
	if (listen(sock, SOMAXCONN) < 0) {
		MSAPI_DBG_ERR("listen failed : %s", strerror(errno));
		close(sock);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	MSAPI_DBG("Listening...");

	*sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

#endif

int ms_ipc_create_server_socket(ms_protocol_e protocol, int port, int *sock_fd)
{
	int i;
	bool bind_success = false;
	int sock = -1;
	int n_reuse = 1;
	struct sockaddr_un serv_addr;
	mode_t orig_mode;
	unsigned short serv_port;

	serv_port = port;

	if(protocol == MS_PROTOCOL_UDP)
	{
		/* Create a datagram/UDP socket */
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}
	else
	{
		/* Create a TCP socket */
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
//	MSAPI_DBG_SLOG("%s", MEDIA_IPC_PATH[serv_port]);
	unlink(MEDIA_IPC_PATH[serv_port]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[serv_port]);
	orig_mode = umask(0);

	/* Bind to the local address */
	for (i = 0; i < 20; i ++) {
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
			bind_success = true;
			break;
		}
		MSAPI_DBG("%d",i);
		usleep(250000);
	}

	umask(orig_mode);

	if (bind_success == false) {
		MSAPI_DBG_STRERROR("bind failed");
		close(sock);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	MSAPI_DBG("bind success");

	/* Listening */
	if (protocol == MS_PROTOCOL_TCP) {
		if (listen(sock, SOMAXCONN) < 0) {
			MSAPI_DBG_ERR("listen failed");
			close(sock);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}

		MSAPI_DBG("Listening...");
	}

	*sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_send_msg_to_server(int sockfd, int port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr)
{
	int res = MS_MEDIA_ERR_NONE;
	struct sockaddr_un addr;

	/* Set server Address */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, MEDIA_IPC_PATH[port]);
	MSAPI_DBG("%s", addr.sun_path);

	if (sendto(sockfd, send_msg, sizeof(*(send_msg)), 0, (struct sockaddr *)&addr, sizeof(addr)) != sizeof(*(send_msg))) {
		MSAPI_DBG_STRERROR("sendto failed");
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent result [%d]", send_msg->result);
		MSAPI_DBG_SLOG("result message [%s]", send_msg->msg);
		if (serv_addr != NULL)
			*serv_addr = addr;
	}

	return res;
}

int ms_ipc_send_msg_to_client(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_un *client_addr)
{
	int res = MS_MEDIA_ERR_NONE;

	if (sendto(sockfd, send_msg, sizeof(*(send_msg)), 0, (struct sockaddr *)client_addr, sizeof(*(client_addr))) != sizeof(*(send_msg))) {
		MSAPI_DBG_STRERROR("sendto failed");
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent result [%d]", send_msg->result);
		MSAPI_DBG_SLOG("result message [%s]", send_msg->msg);
	}

	return res;
}

int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size, int *recv_socket)
{
	int recv_msg_size;
	int client_socket = -1;
	struct sockaddr_un addr;
	socklen_t addr_len;

	if (!recv_msg)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	addr_len = sizeof(addr);
	if ((recv_msg_size = recvfrom(sockfd, recv_msg, msg_size, 0, (struct sockaddr *)&addr, &addr_len)) < 0) {
		MSAPI_DBG_STRERROR("recvfrom failed");
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	MSAPI_DBG_SLOG("the path of received client address : %s", addr.sun_path);

	if (recv_addr != NULL)
		*recv_addr = addr;
	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size, int connected)
{
	int recv_msg_size;
	socklen_t addr_len;

	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	int client_socket = -1;
	
	if (!recv_msg ||!recv_addr)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	addr_len = sizeof(struct sockaddr_un);
	if ((recv_msg_size = recvfrom(sockfd, recv_msg, msg_size, 0, (struct sockaddr *)recv_addr, &addr_len)) < 0) {
		MSAPI_DBG_STRERROR("recvfrom failed");
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("recvfrom Timeout.");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_STRERROR("recvfrom error");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}


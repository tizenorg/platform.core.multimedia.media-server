/*
 * Media Utility
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

#define _GNU_SOURCE
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
#include "media-util-internal.h"

#define MS_SOCK_UDP_BLOCK_SIZE 512

char MEDIA_IPC_PATH[][70] = {
	{"media-server/media_ipc_dbbatchupdate.socket"},
	{"media-server/media_ipc_scandaemon.socket"},
	{"media-server/media_ipc_scanner.socket"},
	{"media-server/media_ipc_dbupdate.socket"},
	{"media-server/media_ipc_thumbcreator.socket"},
	{"media-server/media_ipc_thumbcomm.socket"},
	{"media-server/media_ipc_thumbdaemon.socket"},
	{"media-server/media_ipc_dcmcreator.socket"},
	{"media-server/media_ipc_dcmcomm.socket"},
	{"media-server/media_ipc_dcmdaemon.socket"},
};

char MEDIA_IPC_PATH_CLIENT[][80] = {
	{"user/%i/media-server/media_ipc_dbbatchupdate_client%i.socket"},
	{"user/%i/media-server/media_ipc_scandaemon_client%i.socket"},
	{"user/%i/media-server/media_ipc_scanner_client%i.socket"},
	{"user/%i/media-server/media_ipc_dbupdate_client%i.socket"},
	{"user/%i/media-server/media_ipc_thumbcreator_client%i.socket"},
	{"user/%i/media-server/media_ipc_thumbcomm_client%i.socket"},
	{"user/%i/media-server/media_ipc_thumbdaemon_client%i.socket"},
	{"user/%i/media-server/media_ipc_dcmcreator_client%i.socket"},
	{"user/%i/media-server/media_ipc_dcmcomm_client%i.socket"},
	{"user/%i/media-server/media_ipc_dcmdaemon_client%i.socket"},
};

char MEDIA_IPC_PATH_CLIENT_ROOT[][80] = {
	{"media-server/media_ipc_dbbatchupdate_client%i.socket"},
	{"media-server/media_ipc_scandaemon_client%i.socket"},
	{"media-server/media_ipc_scanner_client%i.socket"},
	{"media-server/media_ipc_dbupdate_client%i.socket"},
	{"media-server/media_ipc_thumbcreator_client%i.socket"},
	{"media-server/media_ipc_thumbcomm_client%i.socket"},
	{"media-server/media_ipc_thumbdaemon_client%i.socket"},
	{"media-server/media_ipc_dcmcreator_client%i.socket"},
	{"media-server/media_ipc_dcmcomm_client%i.socket"},
	{"media-server/media_ipc_dcmdaemon_client%i.socket"},
};

static int _mkdir(const char *dir, mode_t mode)
{
	char tmp[256];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", dir);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;
	for (p = tmp + 1; *p; p++)
		if (*p == '/') {
			*p = 0;
			mkdir(tmp, mode);
			*p = '/';
		}

	return mkdir(tmp, mode);
}

int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, ms_sock_info_s* sock_info)
{
	int sock = -1;
	struct sockaddr_un serv_addr;

	struct timeval tv_timeout = { timeout_sec, 0 };

	if (protocol == MS_PROTOCOL_UDP) {
		char *path = NULL;
		int cx = 0, len = 0;

		if (tzplatform_getuid(TZ_USER_NAME) == 0) {
			cx = snprintf(NULL, 0, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH_CLIENT_ROOT[sock_info->port]), getpid());
			sock_info->sock_path = (char*)malloc((cx + 1)*sizeof(char));
			snprintf(sock_info->sock_path, cx + 1, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH_CLIENT_ROOT[sock_info->port]), getpid());
		} else {
			len = snprintf(NULL, 0, tzplatform_mkpath(TZ_SYS_RUN, "user/%i/media-server"), tzplatform_getuid(TZ_USER_NAME));
			path = (char*)malloc((len + 1)*sizeof(char));
			snprintf(path, len + 1, tzplatform_mkpath(TZ_SYS_RUN, "user/%i/media-server"), tzplatform_getuid(TZ_USER_NAME));
			_mkdir(path, 0777);
			free(path);
			cx = snprintf(NULL, 0, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH_CLIENT[sock_info->port]), tzplatform_getuid(TZ_USER_NAME), getpid());
			sock_info->sock_path = (char*)malloc((cx + 1)*sizeof(char));
			if (sock_info->sock_path != NULL)
				snprintf(sock_info->sock_path, cx + 1, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH_CLIENT[sock_info->port]), tzplatform_getuid(TZ_USER_NAME), getpid());
		}

		/* Create a datagram/UDP socket */
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sun_family = AF_UNIX;
		MSAPI_DBG("%s", sock_info->sock_path);
		if (sock_info->sock_path != NULL) {
			unlink(sock_info->sock_path);
			strncpy(serv_addr.sun_path, sock_info->sock_path, strlen(sock_info->sock_path));
		}
		/* Bind to the local address */
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			MSAPI_DBG_STRERROR("bind failed");
			close(sock);
			free(sock_info->sock_path);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	} else {
		/*Create TCP Socket*/
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
		sock_info->sock_path = NULL;
	}

	if (timeout_sec > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			MSAPI_DBG_STRERROR("setsockopt failed");
			close(sock);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	sock_info->sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_delete_client_socket(ms_sock_info_s* sock_info)
{
	int err = 0;

	close(sock_info->sock_fd);
	MSAPI_DBG("sockfd %d close", sock_info->sock_fd);
	if (sock_info->sock_path != NULL) {
		err = unlink(sock_info->sock_path);
		if (err < 0) {
			MSAPI_DBG_STRERROR("unlink failed");
		}
		free(sock_info->sock_path);
	}

	return 0;
}

int ms_ipc_create_server_socket(ms_protocol_e protocol, ms_msg_port_type_e port, int *sock_fd)
{
	int i;
	bool bind_success = false;
	int sock = -1;
	struct sockaddr_un serv_addr;
	unsigned short serv_port;

	serv_port = port;

	if (protocol == MS_PROTOCOL_UDP) {
		/* Create a datagram/UDP socket */
		if ((sock = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	} else {
		/* Create a TCP socket */
		if ((sock = socket(PF_FILE, SOCK_STREAM, 0)) < 0) {
			MSAPI_DBG_STRERROR("socket failed");
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
//	MSAPI_DBG_SLOG("%s", tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[serv_port]));
	unlink(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[serv_port]));
	strncpy(serv_addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[serv_port]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[serv_port])));

	/* Bind to the local address */
	for (i = 0; i < 100; i++) {
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
			bind_success = true;
			break;
		}
		MSAPI_DBG("%d", i);
		usleep(250000);
	}

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

	/*change permission of sock file*/
	if (chmod(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[serv_port]), 0777) < 0)
		MSAPI_DBG_STRERROR("chmod failed");

	*sock_fd = sock;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_send_msg_to_server(int sockfd, ms_msg_port_type_e port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr)
{
	int res = MS_MEDIA_ERR_NONE;
	struct sockaddr_un addr;

	/* Set server Address */
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[port]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[port])));
//	MSAPI_DBG_SLOG("%s", addr.sun_path);

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

int ms_ipc_send_msg_to_server_tcp(int sockfd, ms_msg_port_type_e port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr)
{
	int res = MS_MEDIA_ERR_NONE;
	struct sockaddr_un addr;

	/* Set server Address */
	memset(&addr, 0, sizeof(addr));

	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[port]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[port])));
//	MSAPI_DBG("%s", addr.sun_path);

	/* Connecting to the media db server */
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		MSAPI_DBG_STRERROR("connect error");
		close(sockfd);

		res = MS_MEDIA_ERR_SOCKET_CONN;

		if (errno == EACCES || errno == EPERM)
			res = MS_MEDIA_ERR_PERMISSION_DENIED;

		return res;
	}

	if (write(sockfd, send_msg, sizeof(*(send_msg))) != sizeof(*(send_msg))) {
		MSAPI_DBG_STRERROR("write failed");
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

int ms_ipc_send_msg_to_client_tcp(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_un *client_addr)
{
	int res = MS_MEDIA_ERR_NONE;

	if (write(sockfd, send_msg, sizeof(*(send_msg))) != sizeof(*(send_msg))) {
		MSAPI_DBG_STRERROR("sendto failed");
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent result [%d]", send_msg->result);
		MSAPI_DBG_SLOG("result message [%s]", send_msg->msg);
	}

	return res;
}

int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size)
{
	int recv_msg_size;

	if (!recv_msg)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	if ((recv_msg_size = read(sockfd, recv_msg, sizeof(ms_comm_msg_s))) < 0) {
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_STRERROR("recv failed");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size)
{
	int recv_msg_size;
	socklen_t addr_len;

	if (!recv_msg || !recv_addr)
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
		*addr_size = addr_len;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_wait_block_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size)
{
	int recv_msg_size;
	socklen_t addr_len;
	unsigned char *block_buf;
	int block_size = 0;
	int recv_size = 0;
	unsigned int remain_size = msg_size;

	if (!recv_msg || !recv_addr)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	addr_len = sizeof(struct sockaddr_un);
	block_size = MS_SOCK_UDP_BLOCK_SIZE;
	block_buf = malloc(block_size * sizeof(unsigned char));
	if (block_buf == NULL) {
		MSAPI_DBG_ERR("malloc failed.");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	memset(block_buf, 0, block_size * sizeof(unsigned char));

	while (remain_size > 0) {
		if (remain_size < MS_SOCK_UDP_BLOCK_SIZE) {
			block_size = remain_size;
		}
		if ((recv_msg_size = recvfrom(sockfd, block_buf, block_size, 0, (struct sockaddr *)recv_addr, &addr_len)) < 0) {
			MSAPI_DBG_STRERROR("recvfrom failed");
			if (errno == EWOULDBLOCK) {
				MSAPI_DBG_ERR("recvfrom Timeout.");
				MS_SAFE_FREE(block_buf);
				return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
			} else {
				MSAPI_DBG_STRERROR("recvfrom error");
				MS_SAFE_FREE(block_buf);
				return MS_MEDIA_ERR_SOCKET_RECEIVE;
			}
		}

		memcpy(recv_msg+recv_size, block_buf, block_size);
		recv_size += block_size;
		remain_size -= block_size;
	}
	if (addr_size != NULL)
		*addr_size = addr_len;

	MS_SAFE_FREE(block_buf);

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_accept_client_tcp(int serv_sock, int* client_sock)
{
	int sockfd = -1;
	struct sockaddr_un client_addr;
	socklen_t client_addr_len;

	if (client_sock == NULL)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

	client_addr_len = sizeof(client_addr);
	if ((sockfd = accept(serv_sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		MSAPI_DBG_STRERROR("accept failed");
		*client_sock = -1;
		return MS_MEDIA_ERR_SOCKET_ACCEPT;
	}

	*client_sock = sockfd;

	return MS_MEDIA_ERR_NONE;
}

int ms_ipc_receive_message_tcp(int client_sock, ms_comm_msg_s *recv_msg)
{
	int recv_msg_size = 0;

	if ((recv_msg_size = read(client_sock, recv_msg, sizeof(ms_comm_msg_s))) < 0) {
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_STRERROR("recv failed");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG_SLOG("receive msg from [%d] %d, %s", recv_msg->pid, recv_msg->msg_type, recv_msg->msg);

	if (!(recv_msg->msg_size > 0 && recv_msg->msg_size < MAX_MSG_SIZE)) {
		MSAPI_DBG_ERR("IPC message is wrong. message size is %d", recv_msg->msg_size);
		return MS_MEDIA_ERR_INVALID_IPC_MESSAGE;
	}

	return MS_MEDIA_ERR_NONE;
}


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

#ifdef _USE_UDS_SOCKET_
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

#elif defined(_USE_UDS_SOCKET_TCP_)
char MEDIA_IPC_PATH[][50] ={
	{"/tmp/media_ipc_dbbatchupdate.dat"},
	{"/tmp/media_ipc_thumbcreator.dat"},
};
#endif

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

#ifdef _USE_UDS_SOCKET_
int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd, int port)
#else
int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, int *sock_fd)
#endif
{
	int sock = -1;

	struct timeval tv_timeout = { timeout_sec, 0 };

	char *buffer;
	char *path;
	int cx,len;

#ifdef _USE_UDS_SOCKET_
#else
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
#endif
		
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
		MSAPI_DBG("%s", buffer);
		unlink(buffer);
		strcpy(serv_addr.sun_path, buffer);
		
		/* Bind to the local address */
		if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			MSAPI_DBG_ERR("bind failed : %s", strerror(errno));
			close(sock);
#ifdef _USE_UDS_SOCKET_
			free(buffer);
#endif
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
#ifdef _USE_UDS_SOCKET_
			free(buffer);
#endif
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	if (timeout_sec > 0) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			MSAPI_DBG_ERR("setsockopt failed: %s", strerror(errno));
			close(sock);
#ifdef _USE_UDS_SOCKET_
			free(buffer);
#endif
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
	}

	*sock_fd = sock;
#ifdef _USE_UDS_SOCKET_
	free(buffer);
#endif

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
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
	mode_t orig_mode;
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
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//	serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_addr.sin_port = htons(serv_port);
#endif

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

    if (connect(sockfd, &addr, sizeof(addr)) < 0) {
        MSAPI_DBG_ERR("connect failed [%s]",strerror(errno));
        return MS_MEDIA_ERR_SOCKET_SEND;
    }

	if (send(sockfd, send_msg, sizeof(*(send_msg)), 0) != sizeof(*(send_msg))) {
		MSAPI_DBG_ERR("send failed [%s]", strerror(errno));
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
	
	if (send(sockfd, send_msg, sizeof(*(send_msg)), 0) != sizeof(*(send_msg))) {
		MSAPI_DBG_ERR("send failed [%s]", strerror(errno));
		res = MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("sent %d", send_msg->result);
		MSAPI_DBG("sent %s", send_msg->msg);
	}

	return res;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size, int *recv_socket)
#else
int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_in *recv_addr, unsigned int *addr_size)
#endif
{
	int recv_msg_size;
	int client_socket = -1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un addr, client_addr;
	unsigned int client_addr_len;
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

    if ((client_socket = accept(sockfd,(struct sockaddr *) &client_addr,(socklen_t *) &client_addr_len)) < 0) {
        MSAPI_DBG_ERR("accept failed [%s]",strerror(errno));
        return MS_MEDIA_ERR_SOCKET_RECEIVE;
    }

	if ((recv_msg_size = recv(client_socket, recv_msg, msg_size, 0)) < 0) {
		MSAPI_DBG_ERR("recv failed [%s]", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

#ifdef _USE_UDS_SOCKET_
	if (recv_socket != NULL)
		*recv_socket = client_socket;
#endif

	if (recv_addr != NULL)
		*recv_addr = addr;
	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}

#ifdef _USE_UDS_SOCKET_
int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *addr_size, int connected)
#else
int ms_ipc_wait_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_in *recv_addr, unsigned int *addr_size)
#endif
{
	int recv_msg_size;
	socklen_t addr_len;

	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	int client_socket = -1;
	
	if (!recv_msg ||!recv_addr)
		return MS_MEDIA_ERR_INVALID_PARAMETER;

#ifdef _USE_UDS_SOCKET_
	addr_len = sizeof(struct sockaddr_un);
#else
	addr_len = sizeof(struct sockaddr_in);
#endif

	if (connected != TRUE){

		if ((client_socket = accept(sockfd,(struct sockaddr *) &client_addr,(socklen_t *) &client_addr_len)) < 0) {
			MSAPI_DBG_ERR("accept failed [%s]",strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
		if ((recv_msg_size = recv(client_socket, recv_msg, msg_size, 0)) < 0) {
			MSAPI_DBG_ERR("recv failed [%s]", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	} else {
			if ((recv_msg_size = recv(sockfd, recv_msg, msg_size, 0)) < 0) {
			MSAPI_DBG_ERR("recv failed [%s]", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	if (addr_size != NULL)
		*addr_size  = addr_len;

	return MS_MEDIA_ERR_NONE;
}


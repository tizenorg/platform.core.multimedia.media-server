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
 
#define _GNU_SOURCE
 
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <malloc.h>
#include <vconf.h>

#include "media-util.h"
#include "media-util-internal.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-server-dbg.h"
#include "media-server-db-svc.h"
#include "media-server-scanner.h"
#include "media-server-socket.h"
#include "media-server-db.h"

extern GAsyncQueue *scan_queue;
GAsyncQueue* ret_queue;
GArray *owner_list;
extern GMutex *scanner_mutex;

typedef struct ms_req_owner_data
{
	int pid;
	int index;
	struct sockaddr_un *client_addr;
	int client_socket;
}ms_req_owner_data;

int _ms_add_owner(ms_req_owner_data *owner_data)
{

	owner_data->index = -1;
	g_array_append_val(owner_list, owner_data);

	return MS_MEDIA_ERR_NONE;
}

int _ms_find_owner(int pid, ms_req_owner_data **owner_data)
{
	int i;
	int len = owner_list->len;
	ms_req_owner_data *data = NULL;

	*owner_data = NULL;

	MS_DBG("length list :  %d", len);

	for (i=0; i < len; i++) {
		data = g_array_index(owner_list, ms_req_owner_data*, i);
		MS_DBG("%d %d", data->pid, pid);
		if (data->pid == pid) {
			data->index = i;
			*owner_data = data;
			MS_DBG("FIND OWNER");
			break;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int _ms_delete_owner(ms_req_owner_data *owner_data)
{
	if (owner_data->index != -1) {
		g_array_remove_index(owner_list, owner_data->index);
		close(owner_data->client_socket);
		MS_SAFE_FREE(owner_data->client_addr);
		MS_SAFE_FREE(owner_data);
		MS_DBG("DELETE OWNER");
	}

	return MS_MEDIA_ERR_NONE;
}

gboolean ms_read_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	struct sockaddr_un *client_addr = NULL;
	socklen_t client_addr_len;
	ms_comm_msg_s recv_msg;
	ms_comm_msg_s scan_msg;
	int msg_size;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int ret;
	int pid;
	int req_num;
	int path_size;
	int client_sock = -1;

	g_mutex_lock(scanner_mutex);

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	}

	/* Socket is readable */
	MS_MALLOC(client_addr, sizeof(struct sockaddr_un));
	if (client_addr == NULL) {
		MS_DBG_ERR("malloc failed");
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	}
	client_addr_len = sizeof(struct sockaddr_un);
	ret = ms_ipc_receive_message(sockfd, &recv_msg, sizeof(recv_msg), client_addr, NULL, &client_sock);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_receive_message failed");
		MS_SAFE_FREE(client_addr);
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	}

	MS_DBG("receive msg from [%d] %d, %s, uid %d", recv_msg.pid, recv_msg.msg_type, recv_msg.msg, recv_msg.uid);

	if (recv_msg.msg_size > 0 && recv_msg.msg_size < MS_FILE_PATH_LEN_MAX) {
		msg_size = recv_msg.msg_size;
		path_size = msg_size + 1;
	} else {
		/*NEED IMPLEMETATION*/
		MS_SAFE_FREE(client_addr);
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	}

	/* copy received data */
	req_num = recv_msg.msg_type;
	pid = recv_msg.pid;

	/* register file request
         * media server inserts the meta data of one file into media db */
	if (req_num == MS_MSG_DIRECTORY_SCANNING
		||req_num == MS_MSG_BULK_INSERT
		||req_num == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE
		|| req_num == MS_MSG_BURSTSHOT_INSERT) {
		/* this request process in media scanner */

		ms_req_owner_data *owner_data = NULL;

		/* If owner list is NULL, create it */
		/* pid and client address are stored in ower list */
		/* These are used for sending result of scanning */
		if (owner_list == NULL) {
			/*create array for processing overlay data*/
			owner_list = g_array_new (FALSE, FALSE, sizeof (ms_req_owner_data *));
			if (owner_list == NULL) {
				MS_DBG_ERR("g_array_new error");
				MS_SAFE_FREE(client_addr);
				g_mutex_unlock(scanner_mutex);
				return TRUE;
			}
		}

		/* store pid and client address */
		MS_MALLOC(owner_data, sizeof(ms_req_owner_data));
		owner_data->pid = recv_msg.pid;
		owner_data->client_addr = client_addr;
		owner_data->client_socket = client_sock;

		_ms_add_owner(owner_data);

		/* create send message for media scanner */
		scan_msg.msg_type = req_num;
		scan_msg.pid = pid;
		scan_msg.msg_size = msg_size;
		scan_msg.uid = recv_msg.uid;
		ms_strcopy(scan_msg.msg, path_size, "%s", recv_msg.msg);

		g_mutex_unlock(scanner_mutex);

		if (ms_get_scanner_status()) {
			MS_DBG("Scanner is ready");
			ms_send_scan_request(&scan_msg);
		} else {
			MS_DBG("Scanner starts");
			ret = ms_scanner_start();
			if(ret == MS_MEDIA_ERR_NONE) {
				ms_send_scan_request(&scan_msg);
			} else {
				MS_DBG("Scanner starting failed. %d", ret);
			}
		}
	} else {
		/* NEED IMPLEMENTATION */
		MS_SAFE_FREE(client_addr);
		g_mutex_unlock(scanner_mutex);
	}

	/*Active flush */
	malloc_trim(0);

	return TRUE;
}
gboolean ms_receive_message_from_scanner(GIOChannel *src, GIOCondition condition, gpointer data)
{
	ms_comm_msg_s recv_msg;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int msg_type;
	int ret;
	int client_sock = -1;
	struct sockaddr_un client_addr;

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	/* Socket is readable */
	ret = ms_ipc_receive_message(sockfd, &recv_msg, sizeof(recv_msg), &client_addr, NULL, NULL);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_receive_message failed [%s]", strerror(errno));
		return TRUE;
	}

	MS_DBG("receive msg from [%d] %d, %s", recv_msg.pid, recv_msg.msg_type, recv_msg.msg);

	msg_type = recv_msg.msg_type;
	if ((msg_type == MS_MSG_SCANNER_RESULT) ||
		(msg_type == MS_MSG_SCANNER_BULK_RESULT)) {
		if (owner_list != NULL) {
			/* If the owner of result message is not media-server, media-server notify to the owner */
			/* The owner of message is distingushied by pid in received message*/
			/* find owner data */
			ms_req_owner_data *owner_data = NULL;

			_ms_find_owner(recv_msg.pid, &owner_data);
			if (owner_data != NULL) {
				MS_DBG("PID : %d", owner_data->pid);

				if (msg_type == MS_MSG_SCANNER_RESULT) {
					MS_DBG("DIRECTORY SCANNING IS DONE");
				}

				/* owner data exists */
				/* send result to the owner of request */
				ms_ipc_send_msg_to_client(sockfd, &recv_msg, owner_data->client_addr);

				/* free owner data*/
				_ms_delete_owner(owner_data);
			}
		} else {
			/* owner data does not exist*/
			/*  this is result of request of media server*/
		}
	} else {
		MS_DBG_ERR("This result message is wrong : %d", recv_msg.msg_type );
	}

	return TRUE;
}

int ms_send_scan_request(ms_comm_msg_s *send_msg)
{
	int ret;
	int res = MS_MEDIA_ERR_NONE;
	int sockfd = -1;

	/*Create Socket*/
#ifdef _USE_UDS_SOCKET_
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, 0, &sockfd, MS_SCAN_DAEMON_PORT);
#else
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, 0, &sockfd);
#endif
	if (ret != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_SOCKET_CONN;

	ret = ms_ipc_send_msg_to_server(sockfd, MS_SCAN_DAEMON_PORT, send_msg, NULL);
	if (ret != MS_MEDIA_ERR_NONE)
		res = ret;

	close(sockfd);

	return res;
}

int ms_send_storage_scan_request(ms_storage_type_t storage_type, ms_dir_scan_type_t scan_type)
{
	int ret = MS_MEDIA_ERR_NONE;
	ms_comm_msg_s scan_msg = {
		.msg_type = MS_MSG_STORAGE_INVALID,
		.pid = 0, /* pid 0 means media-server */
		.result = -1,
		.msg_size = 0,
		.msg = {0},
	};

	/* msg_type */
	switch (scan_type) {
		case MS_SCAN_PART:
			scan_msg.msg_type = MS_MSG_STORAGE_PARTIAL;
			break;
		case MS_SCAN_ALL:
			scan_msg.msg_type = MS_MSG_STORAGE_ALL;
			break;
		case MS_SCAN_INVALID:
			scan_msg.msg_type = MS_MSG_STORAGE_INVALID;
			break;
		default :
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			MS_DBG_ERR("ms_send_storage_scan_request invalid parameter");
			goto ERROR;
			break;
	}

	/* msg_size & msg */
	switch (storage_type) {
		case MS_STORAGE_INTERNAL:
			scan_msg.msg_size = strlen(MEDIA_ROOT_PATH_INTERNAL);
			strncpy(scan_msg.msg, MEDIA_ROOT_PATH_INTERNAL, scan_msg.msg_size );
			break;
		case MS_STORAGE_EXTERNAL:
			scan_msg.msg_size = strlen(MEDIA_ROOT_PATH_SDCARD);
			strncpy(scan_msg.msg, MEDIA_ROOT_PATH_SDCARD, scan_msg.msg_size );
			break;
		default :
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			MS_DBG_ERR("ms_send_storage_scan_request invalid parameter");
			goto ERROR;
			break;
	}

	g_mutex_lock(scanner_mutex);

	if (ms_get_scanner_status()) {
		ms_send_scan_request(&scan_msg);
		g_mutex_unlock(scanner_mutex);
	} else {
		g_mutex_unlock(scanner_mutex);

		ret = ms_scanner_start();
		if(ret == MS_MEDIA_ERR_NONE) {
			ms_send_scan_request(&scan_msg);
		} else {
			MS_DBG("Scanner starting failed. ");
		}
	}

ERROR:

	return ret;
}

gboolean ms_read_db_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	struct sockaddr_un client_addr;
	ms_comm_msg_s recv_msg;
	int send_msg = MS_MEDIA_ERR_NONE;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int client_sock = -1;
	int ret = MS_MEDIA_ERR_NONE;
	MediaDBHandle *db_handle = NULL;
	ms_comm_msg_s msg;
	char * sql_query = NULL;

	memset(&recv_msg, 0, sizeof(recv_msg));

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	ret = ms_ipc_receive_message(sockfd, &recv_msg, sizeof(recv_msg), &client_addr, NULL, &client_sock);
	if (ret != MS_MEDIA_ERR_NONE) {
		return TRUE;
	}

//	MS_DBG("msg_type[%d], msg_size[%d] msg[%s]", recv_msg.msg_type, recv_msg.msg_size, recv_msg.msg);

	if((recv_msg.msg_size <= 0) ||(recv_msg.msg_size > MS_FILE_PATH_LEN_MAX)  || (!MS_STRING_VALID(recv_msg.msg))) {
		MS_DBG_ERR("invalid query. size[%d]", recv_msg.msg_size);
		return TRUE;
	}

	media_db_connect(&db_handle, recv_msg.uid);

	sql_query = strndup(recv_msg.msg, recv_msg.msg_size);
	if (sql_query != NULL) {
		ret = media_db_update_db(db_handle, sql_query);
		if (ret != MS_MEDIA_ERR_NONE)
			MS_DBG_ERR("media_db_update_db error : %d", ret);

		send_msg = ret;
		MS_SAFE_FREE(sql_query);
	} else {
		send_msg = MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	memset(&msg, 0x0, sizeof(ms_comm_msg_s));
	msg.result = send_msg;

	ms_ipc_send_msg_to_client(sockfd, &msg, &client_addr);

	media_db_disconnect(db_handle);

	close(client_sock);

	/*Active flush */
	malloc_trim(0);

	return TRUE;
}

gboolean ms_read_db_tcp_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	struct sockaddr_un client_addr;
	unsigned int client_addr_len;

	ms_comm_msg_s recv_msg;
	int sock = -1;
	int client_sock = -1;
	int send_msg = MS_MEDIA_ERR_NONE;
	int recv_msg_size = -1;
	int ret = MS_MEDIA_ERR_NONE;
	char * sql_query = NULL;
	MediaDBHandle *db_handle = NULL;

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}
	memset((void *)&recv_msg, 0, sizeof(ms_comm_msg_s));

	if ((client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		MS_DBG_ERR("accept failed : %s", strerror(errno));
		return TRUE;
	}

	MS_DBG("Client[%d] is accepted", client_sock);

	while(1) {
		if ((recv_msg_size = recv(client_sock, &recv_msg, sizeof(ms_comm_msg_s), 0)) < 0) {
			MS_DBG_ERR("recv failed : %s", strerror(errno));

			close(client_sock);
			if (errno == EWOULDBLOCK) {
				MS_DBG_ERR("Timeout. Can't try any more");
				return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
			} else {
				MS_DBG_ERR("recv failed : %s", strerror(errno));
				return MS_MEDIA_ERR_SOCKET_RECEIVE;
			}
		}

//		MS_DBG("Received [%d](%d) [%s]", recv_msg.msg_type, recv_msg.msg_size, recv_msg.msg);

		if((recv_msg.msg_size <= 0) ||(recv_msg.msg_size > MS_FILE_PATH_LEN_MAX)  || (!MS_STRING_VALID(recv_msg.msg))) {
			MS_DBG_ERR("invalid query. size[%d]", recv_msg.msg_size);
			MS_DBG_ERR("Received [%d](%d) [%s]", recv_msg.msg_type, recv_msg.msg_size, recv_msg.msg);
			close(client_sock);
			return TRUE;
		}

		sql_query = strndup(recv_msg.msg, recv_msg.msg_size);
		if (sql_query != NULL) {
			media_db_connect(&db_handle, recv_msg.uid);
			if (recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_START) {
				ret = media_db_update_db_batch_start(sql_query);
			} else if(recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_END) {
				ret = media_db_update_db_batch_end(db_handle, sql_query);
			} else if(recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH) {
				ret = media_db_update_db_batch(sql_query);
			} else {

			}
			media_db_disconnect(db_handle);

			MS_SAFE_FREE(sql_query);
			send_msg = ret;

			if (send(client_sock, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
				MS_DBG_ERR("send failed : %s", strerror(errno));
			} else {
				MS_DBG("Sent successfully");
			}

			if (recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_END)
				break;

			memset((void *)&recv_msg, 0, sizeof(ms_comm_msg_s));
		} else {
			MS_DBG_ERR("MS_MALLOC failed");
			close(client_sock);
			return TRUE;
		}
	}

	close(client_sock);
	return TRUE;
}

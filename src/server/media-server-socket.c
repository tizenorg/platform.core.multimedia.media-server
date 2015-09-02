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
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h>
#include <errno.h>
#include <malloc.h>
#include <vconf.h>
#include <grp.h>
#include <pwd.h>

#include "media-util.h"
#include "media-util-internal.h"
#include "media-server-ipc.h"
#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-server-dbg.h"
#include "media-server-scanner.h"
#include "media-server-socket.h"

extern GAsyncQueue *scan_queue;
GAsyncQueue* ret_queue;
GArray *owner_list;
GMutex scanner_mutex;
gint cur_running_task;

extern bool power_off;

typedef struct ms_req_owner_data
{
	int pid;
	int index;
	int client_sockfd;
	char *req_path;
}ms_req_owner_data;

static int __ms_add_owner(int pid, int client_sock, char *path)
{
	if (pid != 0) {
		ms_req_owner_data *owner_data = NULL;
		int len = strlen(path);

		if (len > 0) {
			/* If owner list is NULL, create it */
			/* pid and client address are stored in ower list */
			/* These are used for sending result of scanning */
			if (owner_list == NULL) {
				/*create array for processing overlay data*/
				owner_list = g_array_new (FALSE, FALSE, sizeof (ms_req_owner_data *));
				if (owner_list == NULL) {
					MS_DBG_ERR("g_array_new error");
					return MS_MEDIA_ERR_OUT_OF_MEMORY;
				}
			}

			/* store pid and client address */
			MS_MALLOC(owner_data, sizeof(ms_req_owner_data));

			if (owner_data == NULL) {
				MS_DBG_ERR("MS_MALLOC failed");
				g_array_free (owner_list, FALSE);
				owner_list = NULL;
				return MS_MEDIA_ERR_OUT_OF_MEMORY;
			}

			owner_data->pid = pid;
			owner_data->client_sockfd = client_sock;

			if (path[len -1] == '/')
				path[len -1] = '\0';
			owner_data->req_path = strdup(path);
		//	MS_DBG("the length of array : %d", owner_list->len);
		//	MS_DBG("pid : %d", owner_data->pid);
		//	MS_DBG("client_addr : %p", owner_data->client_addr);

			owner_data->index = -1;
			g_array_append_val(owner_list, owner_data);
		}
	}

	return MS_MEDIA_ERR_NONE;
}

static int __ms_find_owner(int pid, const char *req_path, ms_req_owner_data **owner_data)
{
	int i;
	int len = owner_list->len;
	ms_req_owner_data *data = NULL;

	*owner_data = NULL;

	MS_DBG("length list :  %d", len);

	for (i=0; i < len; i++) {
		data = g_array_index(owner_list, ms_req_owner_data*, i);
		MS_DBG("%d %d", data->pid, pid);
		MS_DBG("%s %s", data->req_path, req_path);
		if (data->pid == pid && (strcmp(data->req_path, req_path) == 0)) {
			data->index = i;
			*owner_data = data;
			MS_DBG("FIND OWNER");
			break;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

static int __ms_delete_owner(ms_req_owner_data *owner_data)
{
	if (owner_data->index != -1) {
		g_array_remove_index(owner_list, owner_data->index);
		MS_SAFE_FREE(owner_data->req_path);
		MS_SAFE_FREE(owner_data);
		MS_DBG("DELETE OWNER");
	}

	return MS_MEDIA_ERR_NONE;
}

static int __ms_send_result_to_client(int pid, ms_comm_msg_s *recv_msg)
{
	if(strlen(recv_msg->msg) == 0) {
		MS_DBG_ERR("msg is NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (owner_list != NULL) {
		/* If the owner of result message is not media-server, media-server notify to the owner */
		/* The owner of message is distingushied by pid in received message*/
		/* find owner data */
		ms_req_owner_data *owner_data = NULL;
		char *res_path = strdup(recv_msg->msg);
		if(res_path  == NULL) {
			MS_DBG_ERR("res_path is NULL");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}

		__ms_find_owner(pid, res_path, &owner_data);
		if (owner_data != NULL) {
			MS_DBG("PID : %d", owner_data->pid);
			/* owner data exists */
			/* send result to the owner of request */
			ms_ipc_send_msg_to_client_tcp(owner_data->client_sockfd, recv_msg, NULL);
			close(owner_data->client_sockfd);

			MS_SAFE_FREE(res_path);

			/* free owner data*/
			__ms_delete_owner(owner_data);
		} else {
			MS_DBG_ERR("Not found Owner");
			MS_SAFE_FREE(res_path);
			return MS_MEDIA_ERR_INTERNAL;
		}
	} else {
		/* owner data does not exist*/
		/*  this is result of request of media server*/
		MS_DBG_ERR("There is no request, Owner list is NULL");
		return MS_MEDIA_ERR_INTERNAL;
	}

	return MS_MEDIA_ERR_NONE;

}

gboolean ms_read_socket(gpointer user_data)
{
	ms_comm_msg_s recv_msg;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int ret;
	int res;
	int pid;
	int req_num = -1;
	int client_sock = -1;
	GIOChannel *src = user_data;
	ms_peer_credentials creds;

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	/* get client socket fd */
	ret = ms_ipc_accept_client_tcp(sockfd, &client_sock);
	if (ret != MS_MEDIA_ERR_NONE) {
		return TRUE;
	}

	memset(&creds, 0, sizeof(creds));

	ret = ms_cynara_receive_untrusted_message(client_sock, &recv_msg, &creds);
	if (ret != MS_MEDIA_ERR_NONE) {
		res = ret;
		goto ERROR;
	}

	if (ms_cynara_check(&creds, MEDIA_STORAGE_PRIVILEGE) != MS_MEDIA_ERR_NONE) {
		res = ret;
		goto ERROR;
	}

	MS_SAFE_FREE(creds.smack);
	MS_SAFE_FREE(creds.uid);

	/* copy received data */
	req_num = recv_msg.msg_type;
	pid = recv_msg.pid;

	/* register file request
         * media server inserts the meta data of one file into media db */
	if (req_num == MS_MSG_DIRECTORY_SCANNING
		||req_num == MS_MSG_BULK_INSERT
		||req_num == MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE
		|| req_num == MS_MSG_BURSTSHOT_INSERT
		|| req_num == MS_MSG_DIRECTORY_SCANNING_CANCEL) {
		if ((ret = ms_send_scan_request(&recv_msg, client_sock)) != MS_MEDIA_ERR_NONE) {
			res = ret;
			goto ERROR;
		}

		if (req_num == MS_MSG_DIRECTORY_SCANNING_CANCEL)
			ms_remove_request_owner(pid, recv_msg.msg);
	} else {
		/* NEED IMPLEMENTATION */
		close(client_sock);
	}

	/*Active flush */
	malloc_trim(0);

	return TRUE;
ERROR:
	{
		ms_comm_msg_s res_msg;

		memset(&res_msg, 0x0, sizeof(ms_comm_msg_s));

		switch(req_num) {
			case MS_MSG_DIRECTORY_SCANNING:
			case MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE:
				res_msg.msg_type = MS_MSG_SCANNER_RESULT;
				break;
			case MS_MSG_BULK_INSERT:
			case MS_MSG_BURSTSHOT_INSERT:
				res_msg.msg_type = MS_MSG_SCANNER_BULK_RESULT;
				break;
			default :
				break;
		}

		res_msg.result = res;

		ms_ipc_send_msg_to_client_tcp(client_sock, &res_msg, NULL);
		close(client_sock);
	}

	return TRUE;
}

static int __ms_send_request(ms_comm_msg_s *send_msg)
{
	int res = MS_MEDIA_ERR_NONE;
	int fd = -1;
	int err = -1;

	fd = open(MS_SCANNER_FIFO_PATH_REQ, O_WRONLY);
	if (fd < 0) {
		MS_DBG_STRERROR("fifo open failed");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	/* send message */
	err = write(fd, send_msg, sizeof(ms_comm_msg_s));
	if (err < 0) {
		MS_DBG_STRERROR("fifo write failed");
		close(fd);
		return MS_MEDIA_ERR_FILE_READ_FAIL;
	}

	close(fd);

	return res;
}

int ms_send_scan_request(ms_comm_msg_s *send_msg, int client_sock)
{
	int res = MS_MEDIA_ERR_NONE;
	int err =  MS_MEDIA_ERR_NONE;
	int pid = send_msg->pid;

	g_mutex_lock(&scanner_mutex);

	if (ms_get_scanner_status()) {
		MS_DBG_WARN("Scanner is ready");
		__ms_send_request(send_msg);

		g_mutex_unlock(&scanner_mutex);
	} else {
		MS_DBG_WARN("Scanner starts");
		g_mutex_unlock(&scanner_mutex);

		err = ms_scanner_start();
		if(err == MS_MEDIA_ERR_NONE) {
			err = __ms_send_request(send_msg);
			if(err != MS_MEDIA_ERR_NONE) {
				MS_DBG_ERR("__ms_send_request failed", err);
			}
		} else {
			MS_DBG_ERR("Scanner starting failed. %d", err);
			res = err;
		}
	}

	if ((res == MS_MEDIA_ERR_NONE) && (send_msg->msg_type != MS_MSG_DIRECTORY_SCANNING_CANCEL)) {
		/* this request process in media scanner */
		if ((err = __ms_add_owner(pid, client_sock, send_msg->msg)) != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("__ms_add_owner failed. %d", err);
			res = err;
		}
	}

	return res;
}

int ms_send_storage_scan_request(const char *root_path, const char *storage_id, ms_dir_scan_type_t scan_type)
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
		case MS_SCAN_META:
			scan_msg.msg_type = MS_MSG_STORAGE_META;
			break;
		default :
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			MS_DBG_ERR("ms_send_storage_scan_request invalid parameter");
			goto ERROR;
			break;
	}

	/* msg_size & msg */
	if (root_path != NULL) {
		scan_msg.msg_size = strlen(root_path);
		strncpy(scan_msg.msg, root_path, scan_msg.msg_size );
	}

	if (storage_id != NULL) {
		strncpy(scan_msg.storage_id, storage_id, MS_UUID_SIZE-1);
	}

	ret = ms_send_scan_request(&scan_msg, -1);

ERROR:

	return ret;
}

gboolean ms_read_db_tcp_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	int sock = -1;
	int client_sock = -1;
	char * sql_query = NULL;
	ms_comm_msg_s recv_msg;
	int ret = MS_MEDIA_ERR_NONE;
	MediaDBHandle *db_handle = (MediaDBHandle *)data;
	int send_msg = MS_MEDIA_ERR_NONE;
	ms_peer_credentials creds;

	if (power_off == TRUE) {
		MS_DBG_WARN("in the power off sequence");
		return TRUE;
	}

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	/* get client socket fd */
	ret = ms_ipc_accept_client_tcp(sock, &client_sock);
	if (ret != MS_MEDIA_ERR_NONE) {
		return TRUE;
	}

	memset(&creds, 0, sizeof(creds));

	ret = ms_cynara_receive_untrusted_message(client_sock, &recv_msg, &creds);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_receive_message_tcp failed [%d]", ret);
		send_msg = ret;
		goto ERROR;
	}

	if (ms_cynara_check(&creds, CONTENT_WRITE_PRIVILEGE) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("invalid query. size[%d]", recv_msg.msg_size);
		send_msg = MS_MEDIA_ERR_PERMISSION_DENIED;
		goto ERROR;
	}

	MS_SAFE_FREE(creds.smack);
	MS_SAFE_FREE(creds.uid);

	if(media_db_connect(&db_handle, recv_msg.uid, TRUE) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("Failed to connect DB");
		goto ERROR;
	}

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

	media_db_disconnect(db_handle);

ERROR:
	if (write(client_sock, &send_msg, sizeof(send_msg)) != sizeof(send_msg)) {
		MS_DBG_STRERROR("send failed");
	} else {
		MS_DBG("Sent successfully");
	}

	if (close(client_sock) <0) {
		MS_DBG_STRERROR("close failed");
	}

	return TRUE;
}


void _ms_process_tcp_message(gpointer data,  gpointer user_data)
{
	int ret = MS_MEDIA_ERR_NONE;
	char * sql_query = NULL;
	ms_comm_msg_s recv_msg;
	int client_sock = GPOINTER_TO_INT (data);
	int send_msg = MS_MEDIA_ERR_NONE;
	MediaDBHandle *db_handle = NULL;

	memset((void *)&recv_msg, 0, sizeof(ms_comm_msg_s));

//	MS_DBG_ERR("client sokcet : %d", client_sock);

	while(1) {
		if (power_off == TRUE) {
			MS_DBG_WARN("in the power off sequence");
			break;
		}

		ret = ms_ipc_receive_message_tcp(client_sock, &recv_msg);
		if (ret != MS_MEDIA_ERR_NONE) {
			media_db_request_update_db_batch_clear();
			MS_DBG_ERR("ms_ipc_receive_message_tcp failed [%d]", ret);
			send_msg = ret;
			goto ERROR;
		}

		/* Connect Media DB*/
		if(db_handle == NULL) {
			if(media_db_connect(&db_handle, recv_msg.uid, TRUE) != MS_MEDIA_ERR_NONE) {
				MS_DBG_ERR("Failed to connect DB");
				send_msg = MS_MEDIA_ERR_DB_CONNECT_FAIL;
				goto ERROR;
			}
		}

		sql_query = strndup(recv_msg.msg, recv_msg.msg_size);
		if (sql_query != NULL) {
			if (recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_START) {
				ret = media_db_update_db_batch_start(sql_query);
			} else if(recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_END) {
				ret = media_db_update_db_batch_end(db_handle, sql_query);
			} else if(recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH) {
				ret = media_db_update_db_batch(sql_query);
			} else {

			}

			MS_SAFE_FREE(sql_query);
			send_msg = ret;

			if (write(client_sock, &send_msg, sizeof(send_msg)) != sizeof(send_msg)) {
				MS_DBG_STRERROR("send failed");
			}

//			MS_DBG_ERR("client sokcet : %d", client_sock);
			if (recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_END) {
				MS_DBG_WARN("Batch job is successfull!client sockfd [%d]", client_sock);
				break;
			}

			if (ret < MS_MEDIA_ERR_NONE && recv_msg.msg_type == MS_MSG_DB_UPDATE_BATCH_START) {
				MS_DBG_ERR("Batch job start is failed!client sockfd [%d]", client_sock);
				media_db_request_update_db_batch_clear();
				break;
			}

			memset((void *)&recv_msg, 0, sizeof(ms_comm_msg_s));
		} else {
			MS_DBG_ERR("MS_MALLOC failed");
			media_db_request_update_db_batch_clear();
			/* send error to client */
			send_msg = MS_MEDIA_ERR_SOCKET_RECEIVE;
			goto ERROR;
		}
	}

	if (close(client_sock) <0) {
		MS_DBG_STRERROR("close failed");
	}

	/* Disconnect DB*/
	media_db_disconnect(db_handle);
	MS_DBG_ERR("END");
	if (g_atomic_int_dec_and_test(&cur_running_task) == FALSE)
		MS_DBG_ERR("g_atomic_int_dec_and_test failed");

	return;

ERROR:

	/* send error to client */
	if (write(client_sock, &send_msg, sizeof(send_msg)) != sizeof(send_msg)) {
		MS_DBG_STRERROR("send failed");
	} else {
		MS_DBG("Sent successfully");
	}

	if (close(client_sock) <0) {
		MS_DBG_STRERROR("close failed");
	}

	/* Disconnect DB*/
	media_db_disconnect(db_handle);
	MS_DBG_ERR("END");
	if (g_atomic_int_dec_and_test(&cur_running_task) == FALSE)
		MS_DBG_ERR("g_atomic_int_dec_and_test failed");

	return;
}

gboolean ms_read_db_tcp_batch_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
#define MAX_THREAD_NUM 3

	static GThreadPool *gtp = NULL;
	GError *error = NULL;
	int ret = MS_MEDIA_ERR_NONE;
	int res = MS_MEDIA_ERR_NONE;
	int sock = -1;
	int client_sock = -1;

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	/* get client socket fd */
	ret = ms_ipc_accept_client_tcp(sock, &client_sock);
	if (ret != MS_MEDIA_ERR_NONE) {
		media_db_request_update_db_batch_clear();
		return TRUE;
	}

	MS_DBG_SLOG("Client[%d] is accepted", client_sock);

	if (gtp == NULL) {
		MS_DBG_SLOG("Create New Thread Pool %d", client_sock);
		gtp = g_thread_pool_new((GFunc)_ms_process_tcp_message, NULL, MAX_THREAD_NUM, TRUE, &error);
		if (error != NULL) {
			res = MS_MEDIA_ERR_OUT_OF_MEMORY;
			goto ERROR;
		}
	}

	/*check number of running thread */
	if (g_atomic_int_get(&cur_running_task) < MAX_THREAD_NUM) {
		MS_DBG_SLOG("CURRENT RUNNING TASK %d", cur_running_task);
		g_atomic_int_inc(&cur_running_task);
		g_thread_pool_push(gtp, GINT_TO_POINTER(client_sock), &error);

		if (error != NULL) {
			res = MS_MEDIA_ERR_INTERNAL;
			goto ERROR;
		}
	} else {
		/* all thread is working, return busy error */
		res = MS_MEDIA_ERR_DB_BATCH_UPDATE_BUSY;
		goto ERROR;
	}

	return TRUE;

ERROR:
	{
		int send_msg = MS_MEDIA_ERR_NONE;

		if (error != NULL) {
			MS_DBG_SLOG("g_thread_pool_push failed [%d]", error->message);
			g_error_free(error);
			error = NULL;
		}

		/* send error to clinet*/
		send_msg = res;
		if (write(client_sock, &send_msg, sizeof(send_msg)) != sizeof(send_msg)) {
			MS_DBG_STRERROR("send failed");
		} else {
			MS_DBG("Sent successfully");
		}

		if (close(client_sock) <0) {
			MS_DBG_STRERROR("close failed");
		}
	}

	return TRUE;
}

gboolean ms_receive_message_from_scanner(GIOChannel *src, GIOCondition condition, gpointer data)
{
	ms_comm_msg_s recv_msg;
	int fd = MS_SOCK_NOT_ALLOCATE;
	int msg_type;
	int pid = -1;
	int err;

	fd = g_io_channel_unix_get_fd(src);
	if (fd < 0) {
		MS_DBG_ERR("fd is invalid!");
		return TRUE;
	}

	/* read() is blocked until media scanner sends message */
	err = read(fd, &recv_msg, sizeof(recv_msg));
	if (err < 0) {
		MS_DBG_STRERROR("fifo read failed");
		close(fd);
		return MS_MEDIA_ERR_FILE_READ_FAIL;
	}

	MS_DBG_SLOG("receive result from scanner  [%d] %d, %s", recv_msg.pid, recv_msg.result, recv_msg.msg);

	msg_type = recv_msg.msg_type;
	pid = recv_msg.pid;
	if ((msg_type == MS_MSG_SCANNER_RESULT) ||
		(msg_type == MS_MSG_SCANNER_BULK_RESULT)) {
		MS_DBG_WARN("DB UPDATING IS DONE[%d]", msg_type);

		if (pid != 0) {
			__ms_send_result_to_client(pid, &recv_msg);
		} else {
			MS_DBG_SLOG("This request is from media-server");
		}
	} else {
		MS_DBG_ERR("This result message is wrong : %d", recv_msg.msg_type );
	}

	return TRUE;
}

int ms_remove_request_owner(int pid, const char *req_path)
{
	ms_req_owner_data *owner_data = NULL;

	__ms_find_owner(pid, req_path, &owner_data);
	if (owner_data != NULL) {
		MS_DBG("PID : %d", owner_data->pid);

		close(owner_data->client_sockfd);
		/* free owner data*/
		__ms_delete_owner(owner_data);
	} else {
		MS_DBG_ERR("Not found Owner");
		return MS_MEDIA_ERR_INTERNAL;
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_send_storage_otg_scan_request(const char *path, const char *device_uuid, ms_dir_scan_type_t scan_type)
{
	int ret = MS_MEDIA_ERR_NONE;

	if (path == NULL) {
		MS_DBG_ERR("Invalid path");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (device_uuid == NULL) {
		MS_DBG_ERR("Invalid device_uuid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	ms_comm_msg_s scan_msg = {
		.msg_type = MS_MSG_STORAGE_INVALID,
		.pid = 0, /* pid 0 means media-server */
		.result = -1,
		.msg_size = 0,
		.storage_id = {0},
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
	scan_msg.msg_size = strlen(path);
	strncpy(scan_msg.msg, path, scan_msg.msg_size );
	strncpy(scan_msg.storage_id, device_uuid, MS_UUID_SIZE-1);

	ret = ms_send_scan_request(&scan_msg, -1);

ERROR:

	return ret;
}



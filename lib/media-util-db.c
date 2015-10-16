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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <db-util.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/smack.h>
#include "media-server-ipc.h"
#include "media-util-dbg.h"
#include "media-util-internal.h"
#include "media-util.h"

#define BUFSIZE 4096
static __thread char **sql_list = NULL;
static __thread int g_list_idx = 0;

static int __media_db_busy_handler(void *pData, int count);
static int __media_db_connect_db_with_handle(sqlite3 **db_handle, uid_t uid, bool need_write);
static int __media_db_disconnect_db_with_handle(sqlite3 *db_handle);

static void __media_db_destroy_sql_list()
{
	int i = 0;

	for (i = 0; i < g_list_idx; i++) {
		MS_SAFE_FREE(sql_list[i]);
	}

	MS_SAFE_FREE(sql_list);
	g_list_idx = 0;
}

static int __media_db_busy_handler(void *pData, int count)
{
	usleep(50000);

	MSAPI_DBG("media_db_busy_handler called : %d", count);

	return 200 - count;
}

static char* __media_get_media_DB(uid_t uid)
{
	char *result_psswd = NULL;
	struct group *grpinfo = NULL;
	if (uid == getuid()) {
		result_psswd = strdup(MEDIA_DB_NAME);
		grpinfo = getgrnam("users");
		if (grpinfo == NULL) {
			MSAPI_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
	} else {
		struct passwd *userinfo = getpwuid(uid);
		if (userinfo == NULL) {
			MSAPI_DBG_ERR("getpwuid(%d) returns NULL !", uid);
			return NULL;
		}
		grpinfo = getgrnam("users");
		if (grpinfo == NULL) {
			MSAPI_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
		// Compare git_t type and not group name
		if (grpinfo->gr_gid != userinfo->pw_gid) {
			MSAPI_DBG_ERR("UID [%d] does not belong to 'users' group!", uid);
			return NULL;
		}
		asprintf(&result_psswd, "%s/.applications/dbspace/.media.db", userinfo->pw_dir);
	}

	return result_psswd;
}

static int __media_db_connect_db_with_handle(sqlite3 **db_handle, uid_t uid, bool need_write)
{
	int ret = SQLITE_OK;

	/*Connect DB*/
	if (need_write) {
		ret = db_util_open_with_options(__media_get_media_DB(uid), db_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	} else {
		ret = db_util_open_with_options(__media_get_media_DB(uid), db_handle, SQLITE_OPEN_READONLY, NULL);
	}
	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("error when db open [%s]", __media_get_media_DB(uid));
		*db_handle = NULL;
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	if (*db_handle == NULL) {
		MSAPI_DBG_ERR("*db_handle is NULL");
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	/*Register busy handler*/
	ret = sqlite3_busy_handler(*db_handle, __media_db_busy_handler, NULL);

	if (SQLITE_OK != ret) {

		if (*db_handle) {
			MSAPI_DBG_ERR("[error when register busy handler] %s\n", sqlite3_errmsg(*db_handle));
		}

		db_util_close(*db_handle);
		*db_handle = NULL;

		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

static int __media_db_disconnect_db_with_handle(sqlite3 *db_handle)
{
	int ret = MS_MEDIA_ERR_NONE;

	ret = db_util_close(db_handle);

	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("error when db close");
		MSAPI_DBG_ERR("Error : %s", sqlite3_errmsg(db_handle));
		db_handle = NULL;
		return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

extern char MEDIA_IPC_PATH[][70];
#define MAX_RETRY_COUNT 3

static int __media_db_request_update_tcp(ms_msg_type_e msg_type, const char *request_msg, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	int retry_count = 0;
	sock_info.port = MS_DB_UPDATE_PORT;

	if (!MS_STRING_VALID(request_msg)) {
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if (request_msg_size >= MAX_MSG_SIZE) {
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

//	MSAPI_DBG("querysize[%d] query[%s]", request_msg_size, request_msg);

	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	strncpy(send_msg.msg, request_msg, request_msg_size);
	send_msg.uid = uid;

	/*Create Socket*/
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	sockfd = sock_info.sock_fd;
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	/*Set server Address*/
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
//	MSAPI_DBG_SLOG("%s", MEDIA_IPC_PATH[port]);
	strncpy(serv_addr.sun_path, MEDIA_IPC_PATH[sock_info.port], strlen(MEDIA_IPC_PATH[sock_info.port]));

	/* Connecting to the media db server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_STRERROR("connect error");
		close(sockfd);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Send request */
	if (send(sockfd, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
		MSAPI_DBG_STRERROR("send failed");
		close(sockfd);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	/*Receive Response*/
	int recv_msg_size = -1;
	int recv_msg = -1;
RETRY:
	if ((recv_msg_size = recv(sockfd, &recv_msg, sizeof(recv_msg), 0)) < 0) {
		MSAPI_DBG_ERR("recv failed : [%d]", sockfd);

		if (errno == EINTR) {
			MSAPI_DBG_STRERROR("catch interrupt");
			goto RETRY;
		}

		if (errno == EWOULDBLOCK) {
			if (retry_count < MAX_RETRY_COUNT)	{
				MSAPI_DBG_ERR("TIME OUT[%d]", retry_count);
				retry_count++;
				goto RETRY;
			}

			close(sockfd);
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_STRERROR("recv failed");

			close(sockfd);

			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG("RECEIVE OK [%d]", recv_msg);
	ret = recv_msg;

	close(sockfd);

	return ret;
}

static __thread int g_tcp_client_sock = -1;

static int __media_db_get_client_tcp_sock()
{
	return g_tcp_client_sock;
}

static int __media_db_prepare_tcp_client_socket()
{
	int ret = MS_MEDIA_ERR_NONE;
	int sockfd = -1;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	sock_info.port = MS_DB_BATCH_UPDATE_PORT;

	/*Create TCP Socket*/
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	sockfd = sock_info.sock_fd;
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	/*Set server Address*/
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
//	MSAPI_DBG_SLOG("%s", MEDIA_IPC_PATH[port]);
	strncpy(serv_addr.sun_path, MEDIA_IPC_PATH[sock_info.port], strlen(MEDIA_IPC_PATH[sock_info.port]));

	/* Connecting to the media db server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_STRERROR("connect error");
		close(sockfd);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	g_tcp_client_sock = sockfd;

	MSAPI_DBG("Connected successfully");

	return 0;
}

static int __media_db_close_tcp_client_socket()
{
	int ret = MS_MEDIA_ERR_NONE;

	if (g_tcp_client_sock != -1) {
		if (close(g_tcp_client_sock) < 0) {
			MSAPI_DBG_ERR("sock(%d) close failed", g_tcp_client_sock);
			MSAPI_DBG_STRERROR("socket close failed");
			ret = MS_MEDIA_ERR_SOCKET_INTERNAL;
		}
		g_tcp_client_sock = -1;
	}

	return ret;
}

static int __media_db_request_batch_update(ms_msg_type_e msg_type, const char *request_msg, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;

	if (!MS_STRING_VALID(request_msg)) {
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if (request_msg_size >= MAX_MSG_SIZE) {
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	MSAPI_DBG_SLOG("querysize[%d] query[%s]", request_msg_size, request_msg);
	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	send_msg.uid = uid;
	strncpy(send_msg.msg, request_msg, request_msg_size);

	sockfd = __media_db_get_client_tcp_sock();
	if (sockfd <= 0) {
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Send request */
	if (send(sockfd, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
		MSAPI_DBG_STRERROR("send failed");
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	/*Receive Response*/
	int recv_msg_size = -1;
	int recv_msg = -1;
	if ((recv_msg_size = recv(sockfd, &recv_msg, sizeof(recv_msg), 0)) < 0) {
		MSAPI_DBG_ERR("recv failed : [%d]", sockfd);
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_STRERROR("recv failed");
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG("RECEIVE OK [%d]", recv_msg);
	ret = recv_msg;

	if (ret != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("batch updated[%d] failed, error [%d]", msg_type, ret);
	}

	return ret;
}

#define RETRY_CNT 9
#define SLEEP_TIME 1000 * 1000
static int _media_db_update_directly(sqlite3 *db_handle, const char *sql_str)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *zErrMsg = NULL;
	int retry_count = 0;

//	MSAPI_DBG_SLOG("SQL = [%s]", sql_str);

EXEC_RETRY :
	ret = sqlite3_exec(db_handle, sql_str, NULL, NULL, &zErrMsg);

	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("DB Update Fail SQL : %s", sql_str);
		MSAPI_DBG_ERR("ERROR [%s]", zErrMsg);
		if (ret == SQLITE_BUSY) {
			ret = MS_MEDIA_ERR_DB_BUSY_FAIL;
		} else if (ret == SQLITE_CONSTRAINT) {
			ret = MS_MEDIA_ERR_DB_CONSTRAINT_FAIL;
		} else if (ret == SQLITE_FULL) {
			ret = MS_MEDIA_ERR_DB_FULL_FAIL;
		} else if (ret == SQLITE_LOCKED) {
			if (retry_count < RETRY_CNT) {
				MSAPI_DBG_ERR("Locked retry[%d]", retry_count);
				retry_count++;
				usleep(SLEEP_TIME);
				goto EXEC_RETRY;
			}
			ret = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		} else {
			ret = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}

	if (zErrMsg)
		sqlite3_free(zErrMsg);

	return ret;
}

int media_db_connect(MediaDBHandle **handle, uid_t uid, bool need_write)
{
	int ret = MS_MEDIA_ERR_NONE;
	sqlite3 * db_handle = NULL;

	MSAPI_DBG_FUNC();

	ret = __media_db_connect_db_with_handle(&db_handle, uid, need_write);
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	*handle = db_handle;
	return MS_MEDIA_ERR_NONE;
}

int media_db_disconnect(MediaDBHandle *handle)
{
	sqlite3 * db_handle = (sqlite3 *)handle;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(db_handle == NULL, MS_MEDIA_ERR_INVALID_PARAMETER, "Handle is NULL");

	return __media_db_disconnect_db_with_handle(db_handle);
}

int media_db_request_update_db(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_update_tcp(MS_MSG_DB_UPDATE, query_str, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("__media_db_request_update_tcp failed : %d", ret);
	}

	return ret;
}

int media_db_request_update_db_batch_start(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_prepare_tcp_client_socket();
	if (ret != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("__media_db_prepare_tcp_client_socket failed : %d", ret);
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_START, query_str, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		__media_db_close_tcp_client_socket();
	}

	return ret;
}

int media_db_request_update_db_batch(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH, query_str, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		__media_db_close_tcp_client_socket();
	}

	return ret;
}

int media_db_request_update_db_batch_end(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	if (!MS_STRING_VALID(query_str)) {
		MSAPI_DBG_ERR("Invalid Query");
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_END, query_str, uid);

	__media_db_close_tcp_client_socket();

	return ret;
}

int media_db_update_db(MediaDBHandle *handle, const char *query_str)
{
	sqlite3 * db_handle = (sqlite3 *)handle;
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(db_handle == NULL, MS_MEDIA_ERR_INVALID_PARAMETER, "Handle is NULL");
	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = _media_db_update_directly(db_handle, query_str);

	return ret;
}

int media_db_update_db_batch_start(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	if (g_list_idx != 0) {
		MSAPI_DBG_ERR("Current idx is not 0");
		ret = MS_MEDIA_ERR_DB_SERVER_BUSY_FAIL;
	} else {
		sql_list = (char**)malloc(sizeof(char*));
		MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");
		sql_list[g_list_idx++] = strdup(query_str);
		MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");
	}

	return ret;
}

int media_db_update_db_batch(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	sql_list = (char**)realloc(sql_list, (g_list_idx + 1) * sizeof(char*));
	MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");

	sql_list[g_list_idx++] = strdup(query_str);
	MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");

	return ret;
}

int media_db_update_db_batch_end(MediaDBHandle *handle, const char *query_str)
{
	sqlite3 * db_handle = (sqlite3 *)handle;
	int ret = MS_MEDIA_ERR_NONE;

	if (db_handle == NULL || (!MS_STRING_VALID(query_str))) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Handle is NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	sql_list = (char**)realloc(sql_list, (g_list_idx + 1) * sizeof(char*));
	if (sql_list == NULL) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Out of memory");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	sql_list[g_list_idx++] = strdup(query_str);
	if (sql_list[g_list_idx - 1] == NULL) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Out of memory");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	int i = 0;
	char *current_sql = NULL;
	for (i = 0; i < g_list_idx; i++) {
		current_sql = sql_list[i];
		ret = _media_db_update_directly(db_handle, current_sql);
		if (ret < 0) {
			if (i == 0) {
				/* This is fail of "BEGIN" */
				MSAPI_DBG_ERR("Query failed : %s", current_sql);
				break;
			} else if (i == g_list_idx - 1) {
				/* This is fail of "COMMIT" */
				MSAPI_DBG_ERR("Query failed : %s", current_sql);
				break;
			} else {
				MSAPI_DBG_ERR("Query failed : %s, but keep going to run remaining queries", current_sql);
			}
		}
	}

	__media_db_destroy_sql_list();

	return ret;
}

int media_db_request_update_db_batch_clear(void)
{
	int ret = MS_MEDIA_ERR_NONE;

	__media_db_destroy_sql_list();

	return ret;
}

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
 * This file defines api utilities of Media DB.
 *
 * @file		media-util-db.c
 * @author	Haejeong Kim(backto.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <db-util.h>
#include "media-server-ipc.h"
#include "media-util-dbg.h"
#include "media-util-internal.h"
#include "media-util.h"

static __thread char **sql_list = NULL;
static __thread int g_list_idx = 0;

static int __media_db_busy_handler(void *pData, int count);
static int __media_db_connect_db_with_handle(sqlite3 **db_handle);
static int __media_db_disconnect_db_with_handle(sqlite3 *db_handle);
static int __media_db_request_update(ms_msg_type_e msg_type, const char *request_msg);

void __media_db_destroy_sql_list()
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

	return 100 - count;
}
int _xsystem(const char *argv[])
{
	int status = 0;
	pid_t pid;
	pid = fork();
	switch (pid) {
	case -1:
		perror("fork failed");
		return -1;
	case 0:
		/* child */
		execvp(argv[0], (char *const *)argv);
		_exit(-1);
	default:
		/* parent */
		break;
	}
	if (waitpid(pid, &status, 0) == -1)
	{
		perror("waitpid failed");
		return -1;
	}
	if (WIFSIGNALED(status))
	{
		perror("signal");
		return -1;
	}
	if (!WIFEXITED(status))
	{
		/* shouldn't happen */
		perror("should not happen");
		return -1;
	}
	return WEXITSTATUS(status);
}
#define SCRIPT_INIT_DB "/usr/bin/media-data-sdk_create_db.sh"
static int __media_db_connect_db_with_handle(sqlite3 **db_handle)
{
	int ret = MS_MEDIA_ERR_NONE;

	/*Init DB*/
	struct stat sts;
	/* Check if the DB exists; if not, create it and initialize it */
	ret = stat(MEDIA_DB_NAME, &sts);
	if (ret == -1 && errno == ENOENT)
	{
		const char *argv_script[] = {"/bin/sh", SCRIPT_INIT_DB, NULL };
		ret = _xsystem(argv_script);
	}
	ret = db_util_open(MEDIA_DB_NAME, db_handle, DB_UTIL_REGISTER_HOOK_METHOD);
	if (SQLITE_OK != ret) {

		MSAPI_DBG_ERR("error when db open");
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

static int __media_db_request_update(ms_msg_type_e msg_type, const char *request_msg)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;
	int err = -1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	unsigned int serv_addr_len = -1;
	int port = MS_DB_UPDATE_PORT;

	if(msg_type == MS_MSG_DB_UPDATE)
		port = MS_DB_UPDATE_PORT;
	else
		port = MS_SCANNER_PORT;

	if(!MS_STRING_VALID(request_msg))
	{
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if(request_msg_size >= MAX_MSG_SIZE)
	{
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

//	MSAPI_DBG("querysize[%d] query[%s]", request_msg_size, request_msg);

	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	strncpy(send_msg.msg, request_msg, request_msg_size);

	/*Create Socket*/
#ifdef _USE_UDS_SOCKET_
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sockfd, port);
#else
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sockfd);
#endif
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	ret = ms_ipc_send_msg_to_server(sockfd, port, &send_msg, &serv_addr);
	if (ret != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("ms_ipc_send_msg_to_server failed : %d", ret);
		close(sockfd);
		return ret;
	}


	/*Receive Response*/
	ms_comm_msg_s recv_msg;
	serv_addr_len = sizeof(serv_addr);
	memset(&recv_msg, 0x0, sizeof(ms_comm_msg_s));

	err = ms_ipc_wait_message(sockfd, &recv_msg, sizeof(recv_msg), &serv_addr, NULL);
	if (err != MS_MEDIA_ERR_NONE) {
		ret = err;
	} else {
		MSAPI_DBG("RECEIVE OK [%d]", recv_msg.result);
		ret = recv_msg.result;
	}

	close(sockfd);

	return ret;
}

static int g_tcp_client_sock = -1;

static int __media_db_get_client_tcp_sock()
{
	return g_tcp_client_sock;
}

#ifdef _USE_UDS_SOCKET_
extern char MEDIA_IPC_PATH[][50];
#elif defined(_USE_UDS_SOCKET_TCP_)
extern char MEDIA_IPC_PATH[][50];
#endif

static int __media_db_prepare_tcp_client_socket()
{
	int ret = MS_MEDIA_ERR_NONE;
	int sockfd = -1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#elif defined(_USE_UDS_SOCKET_TCP_)
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	int port = MS_DB_BATCH_UPDATE_PORT;

	/*Create TCP Socket*/
#ifdef _USE_UDS_SOCKET_
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd, 0);
#elif defined(_USE_UDS_SOCKET_TCP_)
	ret = ms_ipc_create_client_tcp_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd, MS_DB_BATCH_UPDATE_TCP_PORT);
#else
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd);
#endif
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	/*Set server Address*/
	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[port]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[port]);
#elif defined(_USE_UDS_SOCKET_TCP_)
	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[MS_DB_BATCH_UPDATE_TCP_PORT]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[MS_DB_BATCH_UPDATE_TCP_PORT]);
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_addr.sin_port = htons(port);
#endif

	/* Connecting to the media db server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_ERR("connect error : %s", strerror(errno));
		close(sockfd);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	g_tcp_client_sock = sockfd;

	MSAPI_DBG("Connected successfully");

	return 0;
}

static int __media_db_close_tcp_client_socket()
{
	close(g_tcp_client_sock);
	g_tcp_client_sock = -1;

	return 0;
}

static int __media_db_request_batch_update(ms_msg_type_e msg_type, const char *request_msg)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;

	if(!MS_STRING_VALID(request_msg))
	{
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if(request_msg_size >= MAX_MSG_SIZE)
	{
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	MSAPI_DBG("querysize[%d] query[%s]", request_msg_size, request_msg);
	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	strncpy(send_msg.msg, request_msg, request_msg_size);

	sockfd = __media_db_get_client_tcp_sock();
	if (sockfd <= 0) {
		return  MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Send request */
	if (send(sockfd, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
		MSAPI_DBG_ERR("send failed : %s", strerror(errno));
		__media_db_close_tcp_client_socket(sockfd);
		return MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("Sent successfully");
	}

	/*Receive Response*/
	int recv_msg_size = -1;
	int recv_msg = -1;
	if ((recv_msg_size = recv(sockfd, &recv_msg, sizeof(recv_msg), 0)) < 0) {
		MSAPI_DBG_ERR("recv failed : %s", strerror(errno));

		__media_db_close_tcp_client_socket(sockfd);
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_ERR("recv failed : %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG("RECEIVE OK [%d]", recv_msg);
	ret = recv_msg;

	return ret;
}

static int _media_db_update_directly(sqlite3 *db_handle, const char *sql_str)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *zErrMsg = NULL;

	MSAPI_DBG_INFO("SQL = [%s]", sql_str);

	ret = sqlite3_exec(db_handle, sql_str, NULL, NULL, &zErrMsg);

	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("DB Update Fail SQL:%s [%s], err[%d]", sql_str, zErrMsg, ret);
		if (ret == SQLITE_BUSY)
			ret = MS_MEDIA_ERR_DB_BUSY_FAIL;
		else
			ret = MS_MEDIA_ERR_DB_UPDATE_FAIL;
	} else {
		MSAPI_DBG("DB Update Success");
	}

	if (zErrMsg)
		sqlite3_free (zErrMsg);

	return ret;
}

int media_db_connect(MediaDBHandle **handle)
{
	int ret = MS_MEDIA_ERR_NONE;
	sqlite3 * db_handle = NULL;

	MSAPI_DBG_FUNC();

	ret = __media_db_connect_db_with_handle(&db_handle);
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

int media_db_request_update_db(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_update(MS_MSG_DB_UPDATE, query_str);

	return ret;
}

int media_db_request_update_db_batch_start(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_prepare_tcp_client_socket();

	if (ret < MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("__media_db_prepare_tcp_client_socket failed : %d", ret);
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_START, query_str);

	return ret;
}

int media_db_request_update_db_batch(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH, query_str);

	return ret;
}

int media_db_request_update_db_batch_end(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	if (!MS_STRING_VALID(query_str)) {
		MSAPI_DBG_ERR("Invalid Query");
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_END, query_str);

	__media_db_close_tcp_client_socket();

	return ret;
}

int media_db_request_directory_scan(const char *directory_path)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(directory_path), MS_MEDIA_ERR_INVALID_PARAMETER, "Directory Path is NULL");

	ret = __media_db_request_update(MS_MSG_DIRECTORY_SCANNING, directory_path);

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
		MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL, "Out of memory");
		sql_list[g_list_idx++] = strdup(query_str);
		MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL, "Out of memory");
	}

	return ret;
}

int media_db_update_db_batch(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	sql_list = (char**)realloc(sql_list, (g_list_idx + 1) * sizeof(char*));
	MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL, "Out of memory");

	sql_list[g_list_idx++] = strdup(query_str);
	MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL, "Out of memory");

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
		return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
	}

	sql_list[g_list_idx++] = strdup(query_str);
	if (sql_list[g_list_idx - 1] == NULL) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Out of memory");
		return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
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

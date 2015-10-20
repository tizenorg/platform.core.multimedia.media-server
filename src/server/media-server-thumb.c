/*
 * media-thumbnail-server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunjun Ko <zzoon.ko@samsung.com>
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

#include <dirent.h>
#include <errno.h>

#include "media-util.h"
#include "media-common-utils.h"
#include "media-server-dbg.h"
#include "media-server-thumb.h"
#include <tzplatform_config.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_SERVER_THUMB"

#define THUMB_SERVER_NAME "media-thumbnail"
#define MS_SOCK_BLOCK_SIZE 512
#define MS_INI_GET_INT(dict, key, value, default) \
	do { \
		value = iniparser_getint(dict, key, default); \
		MS_DBG("get %s = %d", key, value); \
	} while (0)
#define THUMB_SERVER_PATH tzplatform_mkpath(TZ_SYS_BIN, "media-thumbnail-server")

gboolean _ms_thumb_agent_timer();

static GMainLoop *g_thumb_agent_loop = NULL;
static GIOChannel *g_tcp_channel = NULL;
static gboolean g_folk_thumb_server = FALSE;
static gboolean g_thumb_server_extracting = FALSE;
static gboolean g_shutdowning_thumb_server = FALSE;
static gboolean g_thumb_server_queued_all_extracting_request = FALSE;
static int g_communicate_sock = 0;
static int g_timer_id = 0;
static int g_server_pid = 0;
static int g_thumb_server_active = -1;

static GQueue *g_request_queue = NULL;
static int g_queue_work = 0;

typedef struct {
	int client_sock;
	thumbMsg *recv_msg;
} thumbRequest;

extern char MEDIA_IPC_PATH[][70];

gboolean _ms_thumb_check_queued_request(gpointer data);
gboolean _ms_thumb_agent_prepare_tcp_socket(int *sock_fd, unsigned short serv_port);


gboolean _ms_thumb_agent_start_jobs(gpointer data)
{
	MS_DBG("");

	return FALSE;
}

void _ms_thumb_agent_finish_jobs()
{
	MS_DBG("");

	return;
}

GMainLoop* ms_get_thumb_thread_mainloop(void)
{
	return g_thumb_agent_loop;
}

int ms_thumb_get_server_pid()
{
	return g_server_pid;
}

void ms_thumb_reset_server_status()
{
	g_folk_thumb_server = FALSE;
	g_shutdowning_thumb_server = FALSE;

	if (g_timer_id > 0) {
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), g_timer_id));
		g_timer_id = 0;
	}

	if (g_thumb_server_extracting) {
		/* Need to inplement when crash happens */
		MS_DBG_ERR("Thumbnail server is dead when processing all-thumbs extraction");
		g_thumb_server_extracting = FALSE;
		g_server_pid = 0;
	} else {
		g_thumb_server_extracting = FALSE;
		g_server_pid = 0;
	}

	return;
}

void _ms_thumb_create_timer(int id)
{
	if (id > 0)
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), id));

	GSource *timer_src = g_timeout_source_new_seconds(MS_TIMEOUT_SEC_20);
	g_source_set_callback(timer_src, _ms_thumb_agent_timer, NULL, NULL);
	g_timer_id = g_source_attach(timer_src, g_main_context_get_thread_default());

}

/* This checks if thumbnail server is running */
bool _ms_thumb_check_process()
{
#if 0
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
#endif
	bool ret = FALSE;
#if 0
	pdir = opendir("/proc");
	if (pdir == NULL) {
		MS_DBG_ERR("err: NO_DIR");
		return 0;
	}

	while (!readdir_r(pdir, &pinfo, &result)) {
		if (result == NULL)
			break;

		if (pinfo.d_type != 4 || pinfo.d_name[0] == '.' || pinfo.d_name[0] > 57)
			continue;

		FILE *fp;
		char buff[128];
		char path[128];

		ms_strcopy(path, sizeof(path), "/proc/%s/status", pinfo.d_name);
		fp = fopen(path, "rt");
		if (fp) {
			if (fgets(buff, 128, fp) == NULL)
				MS_DBG_ERR("fgets failed");
			fclose(fp);

			if (strstr(buff, THUMB_SERVER_NAME)) {
				ret = TRUE;
				break;
			}
		} else {
			MS_DBG_ERR("Can't read file [%s]", path);
		}
	}

	closedir(pdir);
#endif
	return ret;
}

int _media_thumb_get_error()
{
	if (errno == EWOULDBLOCK) {
		MS_DBG_ERR("Timeout. Can't try any more");
		if (!_ms_thumb_check_process()) {
			MS_DBG_ERR("Thumbnail server is not running!. Reset info for thumb server to execute");
			ms_thumb_reset_server_status();
		}

		return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
	} else {
		MS_DBG_STRERROR("recvfrom failed");
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}
}

int _ms_thumb_recv_msg(int sock, thumbMsg *msg)
{
	int recv_msg_len = 0;
	unsigned char *buf = NULL;
	unsigned int header_size = 0;

	header_size = sizeof(thumbMsg) -(MAX_FILEPATH_LEN * 2) - sizeof(unsigned char *);

	MS_MALLOC(buf, header_size);
	if (buf == NULL) {
		MS_DBG_STRERROR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recv(sock, buf, header_size, 0)) < 0) {
		MS_DBG_STRERROR("recv failed");
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}
	memcpy(msg, buf, header_size);

	MS_DBG("origin_path_size : %d, dest_path_size : %d, thumb_size : %d", msg->origin_path_size, msg->dest_path_size, msg->thumb_size);

	MS_SAFE_FREE(buf);

	if (msg->msg_type == MS_MSG_THUMB_EXTRACT_ALL_DONE || msg->msg_type == 5) {
		MS_DBG("msg_type only cases [%d]", msg->msg_type);
		return MS_MEDIA_ERR_NONE;
	}

	if (msg->origin_path_size <= 0 || msg->origin_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->origin_path_size is invalid %d", msg->origin_path_size);
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	MS_MALLOC(buf, (unsigned int)(msg->origin_path_size));
	if (buf == NULL) {
		MS_DBG_STRERROR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recv(sock, buf, msg->origin_path_size, 0)) < 0) {
		MS_DBG_STRERROR("recv failed");
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}
	strncpy(msg->org_path, (char*)buf, msg->origin_path_size);

	MS_SAFE_FREE(buf);

	if (msg->dest_path_size <= 0 || msg->dest_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->dest_path_size is invalid %d", msg->dest_path_size);
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	MS_MALLOC(buf, (unsigned int)(msg->dest_path_size));
	if (buf == NULL) {
		MS_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recv(sock, buf, msg->dest_path_size, 0)) < 0) {
		MS_DBG_ERR("recv failed : %s");
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}
	strncpy(msg->dst_path, (char*)buf, msg->dest_path_size);

	MS_SAFE_FREE(buf);

	if (msg->thumb_size < 0) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->thumb_size is invalid %d", msg->thumb_size);
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	if (msg->thumb_size > 0) {
		MS_MALLOC(buf, (unsigned int)(msg->thumb_size));
		if (buf == NULL) {
			MS_DBG_ERR("malloc failed");
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}

		if ((recv_msg_len = recv(sock, buf, msg->thumb_size, 0)) < 0) {
			MS_DBG_ERR("recv failed : %s", strerror(errno));
			MS_SAFE_FREE(buf);
			return _media_thumb_get_error();
		}

		MS_SAFE_FREE(msg->thumb_data);
		MS_MALLOC(msg->thumb_data, (unsigned int)(msg->thumb_size));
		if (msg->thumb_data == NULL) {
			MS_DBG_ERR("malloc failed");
			MS_SAFE_FREE(buf);
			return MS_MEDIA_ERR_OUT_OF_MEMORY;
		}
		memcpy(msg->thumb_data, buf, msg->thumb_size);

		MS_SAFE_FREE(buf);
	}

	return MS_MEDIA_ERR_NONE;
}

int _ms_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size)
{
	if (req_msg == NULL || buf == NULL) {
		return -1;
	}

	int org_path_len = 0;
	int dst_path_len = 0;
	int data_len = 0;
	int header_size = 0;
	unsigned int size = 0;

	header_size = sizeof(thumbMsg) -(MAX_FILEPATH_LEN * 2) - sizeof(unsigned char *);
	org_path_len = req_msg->origin_path_size;
	dst_path_len = req_msg->dest_path_size;
	data_len = req_msg->thumb_size;

	MS_DBG_SLOG("Basic Size : %d, org_path : %s[%d], dst_path : %s[%d], thumb_data : %d", header_size, req_msg->org_path, org_path_len, req_msg->dst_path, dst_path_len, req_msg->thumb_size);

	size = header_size + org_path_len + dst_path_len + data_len;
	MS_MALLOC(*buf, size);
	if (*buf == NULL) {
		MS_DBG_STRERROR("MALLOC failed");
		return -1;
	}

	memcpy(*buf, req_msg, header_size);
	memcpy((*buf)+header_size, req_msg->org_path, org_path_len);
	memcpy((*buf)+header_size + org_path_len, req_msg->dst_path, dst_path_len);
	if (data_len > 0)
		memcpy((*buf)+header_size + org_path_len + dst_path_len, req_msg->thumb_data, data_len);

	*buf_size = size;

	return 0;
}

gboolean _ms_thumb_agent_recv_msg_from_server()
{
	struct sockaddr_un serv_addr;
	unsigned int serv_addr_len;

	int sockfd = -1;

	if (g_communicate_sock <= 0) {
		_ms_thumb_agent_prepare_tcp_socket(&g_communicate_sock, MS_THUMB_COMM_PORT);
	}

	serv_addr_len = sizeof(serv_addr);

	if ((sockfd = accept(g_communicate_sock, (struct sockaddr*)&serv_addr, &serv_addr_len)) < 0) {
		MS_DBG_STRERROR("accept failed");
		return FALSE;
	}

	ms_thumb_server_msg recv_msg;
	int recv_msg_size = 0;

	recv_msg_size = ms_ipc_receive_message(sockfd, & recv_msg, sizeof(ms_thumb_server_msg));
	if (recv_msg_size != MS_MEDIA_ERR_NONE) {
		MS_DBG_STRERROR("ms_ipc_receive_message failed");
		close(sockfd);
		return FALSE;
	}

	//MS_DBG("Receive : %d(%d)", recv_msg.msg_type, recv_msg_size);
	if (recv_msg.msg_type == MS_MSG_THUMB_SERVER_READY) {
		MS_DBG("Thumbnail server is ready");
	}

	close(sockfd);
	return TRUE;
}

gboolean _ms_thumb_agent_recv_thumb_done_from_server(GIOChannel *src, GIOCondition condition, gpointer data)
{
	int ret = MS_MEDIA_ERR_NONE;
	struct sockaddr_un thumb_serv_addr;
	unsigned int thumb_serv_addr_len;

	int sockfd = -1;
	int thumb_serv_sockfd = -1;

	/* Once all-thumb extraction is done, check if there is queued all-thumb request */
	GSource *check_queued_all_thumb_request = NULL;
	check_queued_all_thumb_request = g_idle_source_new();
	g_source_set_callback(check_queued_all_thumb_request, _ms_thumb_check_queued_request, NULL, NULL);
	g_source_attach(check_queued_all_thumb_request, g_main_context_get_thread_default());

	if (g_thumb_server_extracting == FALSE) {
		MS_DBG_WARN("Recv thumb server extracting done already");
		return FALSE;
	}

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return FALSE;
	}

	thumb_serv_addr_len = sizeof(thumb_serv_addr);

	if ((thumb_serv_sockfd = accept(sockfd, (struct sockaddr*)&thumb_serv_addr, &thumb_serv_addr_len)) < 0) {
		MS_DBG_STRERROR("accept failed");
		return FALSE;
	}

	ms_thumb_server_msg recv_msg;

	MS_DBG_ERR("THUMB SERVER SOCKET %d", thumb_serv_sockfd);

	ret = ms_ipc_receive_message(thumb_serv_sockfd, &recv_msg, sizeof(ms_thumb_server_msg));
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_STRERROR("ms_ipc_receive_message failed");
		close(thumb_serv_sockfd);
		return FALSE;
	}

	close(thumb_serv_sockfd);

	MS_DBG("Receive : %d", recv_msg.msg_type);
	if (recv_msg.msg_type == MS_MSG_THUMB_EXTRACT_ALL_DONE) {
		MS_DBG("Thumbnail extracting done");
		g_thumb_server_extracting = FALSE;

		return FALSE;
	}

	return FALSE;
}

gboolean _ms_thumb_agent_execute_server()
{
	int pid;
	pid = fork();

	if (pid < 0) {
		return FALSE;
	} else if (pid == 0) {
		execl(THUMB_SERVER_PATH, "media-thumbnail-server", NULL);
	} else {
		MS_DBG("Child process is %d", pid);
		g_folk_thumb_server = TRUE;
	}

	g_server_pid = pid;

	if (!_ms_thumb_agent_recv_msg_from_server()) {
		MS_DBG_ERR("_ms_thumb_agent_recv_msg_from_server is failed");
		return FALSE;
	}

	return TRUE;
}

gboolean _ms_thumb_agent_send_msg_to_thumb_server(thumbMsg *recv_msg, thumbMsg *res_msg)
{
	int sock;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	int send_str_len = strlen(recv_msg->org_path);

	if (send_str_len > MAX_FILEPATH_LEN) {
		MS_DBG_ERR("original path's length exceeds %d(max packet size)", MAX_FILEPATH_LEN);
		return FALSE;
	}

	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		MS_DBG_ERR("ms_ipc_create_client_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, MEDIA_IPC_PATH[MS_THUMB_DAEMON_PORT], strlen(MEDIA_IPC_PATH[MS_THUMB_DAEMON_PORT]));

	/* Connecting to the thumbnail server */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MS_DBG_STRERROR("connect");
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	int buf_size = 0;
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(recv_msg, &buf, &buf_size);

	//MS_DBG("buffer size : %d", buf_size);
	if (send(sock, buf, buf_size, 0) != buf_size) {
		MS_DBG_STRERROR("send failed");
		MS_SAFE_FREE(buf);
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	MS_SAFE_FREE(buf);
	MS_DBG_SLOG("Sending msg to thumbnail server is successful");

	if (_ms_thumb_recv_msg(sock, res_msg) < 0) {
		MS_DBG_ERR("_ms_thumb_recv_msg failed");
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	MS_DBG_SLOG("recv %s from thumb daemon is successful", res_msg->dst_path);
	ms_ipc_delete_client_socket(&sock_info);

	if (res_msg->msg_type == 2 && g_communicate_sock > 0) { // THUMB_REQUEST_ALL_MEDIA
		GSource *source = NULL;
		if (g_tcp_channel == NULL)
			g_tcp_channel = g_io_channel_unix_new(g_communicate_sock);
		source = g_io_create_watch(g_tcp_channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)_ms_thumb_agent_recv_thumb_done_from_server, NULL, NULL);
		g_source_attach(source, g_main_context_get_thread_default());

		g_thumb_server_extracting = TRUE;
	}

	return TRUE;
}

gboolean _ms_thumb_check_queued_request(gpointer data)
{
	if (g_thumb_server_queued_all_extracting_request) {
		MS_DBG_WARN("There is queued request");

		/* request all-thumb extraction to thumbnail server */
		thumbMsg msg;
		thumbMsg recv_msg;
		memset((void *)&msg, 0, sizeof(msg));
		memset((void *)&recv_msg, 0, sizeof(recv_msg));

		msg.msg_type = 2; // THUMB_REQUEST_ALL_MEDIA
		msg.org_path[0] = '\0';
		msg.origin_path_size = 1;
		msg.dst_path[0] = '\0';
		msg.dest_path_size = 1;
		msg.thumb_size = 0;

		/* Command All-thumb extraction to thumbnail server */
		if (!_ms_thumb_agent_send_msg_to_thumb_server(&msg, &recv_msg)) {
			MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");
		}

		g_thumb_server_queued_all_extracting_request = FALSE;
	} else {
		MS_DBG("There is no queued request");
		return FALSE;
	}

	return FALSE;
}

gboolean _ms_thumb_agent_timer()
{
	if (g_thumb_server_extracting) {
		MS_DBG("Timer is called.. But media-thumbnail-server[%d] is busy.. so timer is recreated", g_server_pid);

		_ms_thumb_create_timer(g_timer_id);
		return FALSE;
	}

	g_timer_id = 0;
	MS_DBG("Timer is called.. Now killing media-thumbnail-server[%d]", g_server_pid);

	if (g_server_pid > 0) {
		/* Kill thumbnail server */
		thumbMsg msg;
		thumbMsg recv_msg;
		memset((void *)&msg, 0, sizeof(msg));
		memset((void *)&recv_msg, 0, sizeof(recv_msg));

		msg.msg_type = 5; // THUMB_REQUEST_KILL_SERVER
		msg.org_path[0] = '\0';
		msg.origin_path_size = 1;
		msg.dst_path[0] = '\0';
		msg.dest_path_size = 1;
		msg.thumb_size = 0;

		/* Command Kill to thumbnail server */
		g_shutdowning_thumb_server = TRUE;
		if (!_ms_thumb_agent_send_msg_to_thumb_server(&msg, &recv_msg)) {
			MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");
			g_shutdowning_thumb_server = FALSE;
		}
		usleep(200000);
		MS_SAFE_FREE(recv_msg.thumb_data);
	} else {
		MS_DBG_ERR("g_server_pid is %d. Maybe there's problem in thumbnail-server", g_server_pid);
	}

	return FALSE;
}

gboolean _ms_thumb_request_to_server(gpointer data)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	MS_DBG("Queue length : %d", req_len);

	if (req_len <= 0) {
		MS_DBG("There is no request job in the queue");
		g_queue_work = 0;
		return FALSE;
	}

	if (g_shutdowning_thumb_server) {
		MS_DBG_ERR("Thumb server is shutting down... wait for complete");
		usleep(10000);
		return TRUE;
	}

	if (g_folk_thumb_server == FALSE && g_thumb_server_extracting == FALSE) {
//		if (_ms_thumb_check_process() == FALSE) { // This logic is temporary
			MS_DBG_WARN("Thumb server is not running.. so start it");
			if (!_ms_thumb_agent_execute_server()) {
				MS_DBG_ERR("_ms_thumb_agent_execute_server is failed");
				g_queue_work = 0;
				return FALSE;
			} else {
				_ms_thumb_create_timer(g_timer_id);
			}
//		}
	} else {
		/* Timer is re-created*/
		_ms_thumb_create_timer(g_timer_id);
	}

	thumbRequest *req = NULL;
	req = (thumbRequest *)g_queue_pop_head(g_request_queue);

	if (req == NULL) {
		MS_DBG_ERR("Failed to get a request job from queue");
		return TRUE;
	}

	int client_sock = -1;
	thumbMsg *recv_msg = NULL;
	thumbMsg res_msg;
	memset((void *)&res_msg, 0, sizeof(res_msg));

	client_sock = req->client_sock;
	recv_msg = req->recv_msg;

	if (req->client_sock <= 0 || req->recv_msg == NULL) {
		MS_DBG_ERR("client sock is below 0 or recv msg is NULL");
		MS_SAFE_FREE(req->recv_msg);
		MS_SAFE_FREE(req);
		return TRUE;
	}

	if (recv_msg) {
		if (recv_msg->msg_type == 2 && g_thumb_server_extracting) { // THUMB_REQUEST_ALL_MEDIA
			MS_DBG_WARN("Thumbnail server is already extracting..This request is queued.");
			g_thumb_server_queued_all_extracting_request = TRUE;
		} else {
			if (!_ms_thumb_agent_send_msg_to_thumb_server(recv_msg, &res_msg)) {
				MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");

				thumbMsg res_msg;
				memset((void *)&res_msg, 0, sizeof(res_msg));

				res_msg.msg_type = 6; // THUMB_RESPONSE
				res_msg.status = 1; //THUMB_FAIL
				res_msg.origin_path_size = strlen(recv_msg->org_path);
				strncpy(res_msg.org_path, recv_msg->org_path, res_msg.origin_path_size);
				res_msg.dst_path[0] = '\0';
				res_msg.dest_path_size = 1;
				res_msg.thumb_data = (unsigned char *)"\0";
				res_msg.thumb_size = 1;

				int buf_size = 0;
				unsigned char *buf = NULL;
				_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

				if (send(client_sock, buf, buf_size, 0) != buf_size) {
					MS_DBG_STRERROR("sendto failed");
				} else {
					MS_DBG("Sent Refuse msg from %s", recv_msg->org_path);
				}

				close(client_sock);

				MS_SAFE_FREE(buf);
				MS_SAFE_FREE(req->recv_msg);
				MS_SAFE_FREE(req);

				return TRUE;
			}
		}
	} else {
		MS_DBG_ERR("recv_msg is NULL from queue request");
	}

	strncpy(res_msg.org_path, recv_msg->org_path, recv_msg->origin_path_size);
	res_msg.origin_path_size = recv_msg->origin_path_size;
	res_msg.dest_path_size = strlen(res_msg.dst_path) + 1;

	int buf_size = 0;
	int sending_block = 0;
	int block_size = sizeof(res_msg) - MAX_FILEPATH_LEN*2 - sizeof(unsigned char *);
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

	while (buf_size > 0) {
		if (buf_size < MS_SOCK_BLOCK_SIZE) {
			block_size = buf_size;
		}
		if (send(client_sock, buf+sending_block, block_size, 0) != block_size) {
			MS_DBG_STRERROR("sendto failed");
		}
		sending_block += block_size;
		buf_size -= block_size;
		if (block_size < MS_SOCK_BLOCK_SIZE) {
			block_size = MS_SOCK_BLOCK_SIZE;
		}
	}
	if (buf_size == 0) {
		MS_DBG_SLOG("Sent data(%d) from %s", res_msg.thumb_size, res_msg.org_path);
	}

	close(client_sock);
	MS_SAFE_FREE(buf);
	MS_SAFE_FREE(req->recv_msg);
	MS_SAFE_FREE(req);
	MS_SAFE_FREE(res_msg.thumb_data);

	return TRUE;
}

gboolean _ms_thumb_agent_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	thumbMsg *recv_msg = NULL;
	int sock = -1;
	int client_sock = -1;
	unsigned char *buf = NULL;

	struct ucred cr;
	int cl = sizeof(struct ucred);

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	client_addr_len = sizeof(client_addr);

	if ((client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		MS_DBG_STRERROR("accept failed");
		return TRUE;
	}

	MS_DBG("Client[%d] is accepted", client_sock);

	recv_msg = calloc(1, sizeof(thumbMsg));
	if (recv_msg == NULL) {
		MS_DBG_ERR("Failed to allocate memory");
		close(client_sock);
		return TRUE;
	}

	if (_ms_thumb_recv_msg(client_sock, recv_msg) < 0) {
		MS_DBG_ERR("_ms_thumb_recv_msg failed ");
		close(client_sock);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	if (getsockopt(client_sock, SOL_SOCKET, SO_PEERCRED, &cr, (socklen_t *) &cl) < 0) {
		MS_DBG_ERR("credential information error");
	}

	if (getuid() != cr.uid) {
		recv_msg->uid = cr.uid;
	}

	thumbRequest *thumb_req = NULL;

	MS_MALLOC(thumb_req, sizeof(thumbRequest));
	if (thumb_req == NULL) {
		MS_DBG_ERR("Failed to create request element");
		close(client_sock);
		MS_SAFE_FREE(recv_msg->thumb_data);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	thumb_req->client_sock = client_sock;
	thumb_req->recv_msg = recv_msg;

	if (g_request_queue == NULL) {
		MS_DBG_WARN("queue is init");
		g_request_queue = g_queue_new();
	}

	if (g_queue_get_length(g_request_queue) >= MAX_THUMB_REQUEST) {
		MS_DBG_WARN("Request Queue is full");
		thumbMsg res_msg;
		memset((void *)&res_msg, 0, sizeof(res_msg));

		res_msg.msg_type = 6; // THUMB_RESPONSE
		res_msg.status = 1; //THUMB_FAIL
		res_msg.origin_path_size = recv_msg->origin_path_size;
		strncpy(res_msg.org_path, recv_msg->org_path, res_msg.origin_path_size);
		res_msg.dst_path[0] = '\0';
		res_msg.dest_path_size = 1;
		res_msg.thumb_size = 0;

		int buf_size = 0;
		_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

		if (send(client_sock, buf, buf_size, 0) != buf_size) {
			MS_DBG_STRERROR("sendto failed");
		} else {
			MS_DBG("Sent Refuse msg from %s", recv_msg->org_path);
		}

		close(client_sock);
		MS_SAFE_FREE(buf);
		MS_SAFE_FREE(thumb_req->recv_msg);
		MS_SAFE_FREE(thumb_req);

		return TRUE;
	}

	MS_DBG_SLOG("%s is queued", recv_msg->org_path);
	g_queue_push_tail(g_request_queue, (gpointer)thumb_req);

	if (!g_queue_work) {
		GSource *src_request = NULL;
		src_request = g_idle_source_new();
		g_source_set_callback(src_request, _ms_thumb_request_to_server, NULL, NULL);
		//g_source_set_priority(src_request, G_PRIORITY_LOW);
		g_source_attach(src_request, g_main_context_get_thread_default());
		g_queue_work = 1;
	}

	return TRUE;
}

gboolean _ms_thumb_agent_prepare_tcp_socket(int *sock_fd, unsigned short serv_port)
{
	int sock;

	if (ms_ipc_create_server_socket(MS_PROTOCOL_TCP, serv_port, &sock) < 0) {
		MS_DBG_ERR("_ms_thumb_create_socket failed");
		return FALSE;
	}

	*sock_fd = sock;

	return TRUE;
}

int ms_thumb_get_config()
{
	dictionary *dict = NULL;

	dict = iniparser_load(MS_INI_DEFAULT_PATH);
	if (!dict) {
		MS_DBG_ERR("%s load failed", MS_INI_DEFAULT_PATH);
		return -1;
	}

	MS_INI_GET_INT(dict, "media-content-config:thumbnail_activation", g_thumb_server_active, 0);
	MS_DBG("Thumb-server activation level = %d", g_thumb_server_active);

	iniparser_freedict(dict);
	return 0;
}

gpointer ms_thumb_agent_start_thread(gpointer data)
{
	int sockfd = -1;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	MS_DBG_FENTER();

	/* Create and bind new TCP socket */
	if (!_ms_thumb_agent_prepare_tcp_socket(&sockfd, MS_THUMB_CREATOR_PORT)) {
		MS_DBG_ERR("Failed to create socket");
		return NULL;
	}

	context = g_main_context_new();

	if (context == NULL) {
		MS_DBG_ERR("g_main_context_new failed");
	} else {
		MS_DBG("g_main_context_new success");
	}

	g_thumb_agent_loop = g_main_loop_new(context, FALSE);
	g_main_context_push_thread_default(context);

	/* Create new channel to watch udp socket */
	channel = g_io_channel_unix_new(sockfd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Set callback to be called when socket is readable */
	g_source_set_callback(source, (GSourceFunc)_ms_thumb_agent_read_socket, NULL, NULL);
	g_source_attach(source, context);

	MS_DBG("************************************");
	MS_DBG("*** Thumbnail Agent thread is running ***");
	MS_DBG("************************************");

	g_main_loop_run(g_thumb_agent_loop);

	MS_DBG("Thumbnail Agent thread is shutting down...");
	_ms_thumb_agent_finish_jobs();

	/*close an IO channel*/
	g_io_channel_shutdown(channel, FALSE, NULL);
	g_io_channel_shutdown(g_tcp_channel, FALSE, NULL);
	g_io_channel_unref(channel);
	close(g_communicate_sock);

	g_main_loop_unref(g_thumb_agent_loop);

	close(sockfd);

	return NULL;
}


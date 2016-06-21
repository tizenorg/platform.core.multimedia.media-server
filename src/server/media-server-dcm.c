/*
 * media-server-dcm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jiyong Min <jiyong.min@samsung.com>
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
#include "media-common-system.h"
#include "media-server-dbg.h"
#include "media-server-dcm.h"
#include <tzplatform_config.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "MEDIA_SERVER_DCM"

#define DCM_SERVER_PATH tzplatform_mkpath(TZ_SYS_BIN, "dcm-svc")

static GMainLoop *g_dcm_agent_loop = NULL;
static GIOChannel *g_dcm_tcp_channel = NULL;
static gboolean g_folk_dcm_server = FALSE;
static gboolean g_dcm_server_extracting = FALSE;
static gboolean g_shutdowning_dcm_server = FALSE;
static gboolean g_dcm_server_queued_all_extracting_request = FALSE;
static int g_dcm_comm_sock = 0;
static int g_dcm_timer_id = 0;
static int g_dcm_service_pid = 0;

static GQueue *g_dcm_request_queue = NULL;
static int g_dcm_queue_work = 0;
static int g_dcm_service_active = -1;

typedef struct {
	int client_sock;
	dcmMsg *recv_msg;
} dcmRequest;

extern char MEDIA_IPC_PATH[][70];

gboolean _ms_dcm_agent_timer();
gboolean _ms_dcm_check_queued_request(gpointer data);
gboolean _ms_dcm_agent_prepare_tcp_socket(int *sock_fd, unsigned short serv_port);


int ms_dcm_get_server_pid()
{
	return g_dcm_service_pid;
}

void ms_dcm_reset_server_status()
{
	g_folk_dcm_server = FALSE;
	g_shutdowning_dcm_server = FALSE;

	if (g_dcm_timer_id > 0) {
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), g_dcm_timer_id));
		g_dcm_timer_id = 0;
	}

	if (g_dcm_server_extracting) {
		/* Need to inplement when crash happens */
		MS_DBG_ERR("DCM server is dead when processing all-dcm extraction");
		g_dcm_server_extracting = FALSE;
		MS_DBG("g_dcm_server_extracting is false");
		g_dcm_service_pid = 0;
	} else {
		g_dcm_server_extracting = FALSE;
		MS_DBG("g_dcm_server_extracting is false");
		g_dcm_service_pid = 0;
	}

	return;
}

void _ms_dcm_create_timer(int id)
{
	if (id > 0)
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), id));

	GSource *timer_src = g_timeout_source_new_seconds(MS_TIMEOUT_SEC_20);
	g_source_set_callback(timer_src, _ms_dcm_agent_timer, NULL, NULL);
	g_dcm_timer_id = g_source_attach(timer_src, g_main_context_get_thread_default());

}

int _media_dcm_get_error()
{
	if (errno == EWOULDBLOCK) {
		MS_DBG_ERR("Timeout. Can't try any more");
		return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
	} else {
		MS_DBG_STRERROR("recvfrom failed");
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}
}

int _ms_dcm_recv_msg(int sock, dcmMsg *msg)
{
	int recv_msg_len = 0;
	unsigned char *buf = NULL;

	MS_DBG("_ms_dcm_recv_msg in");

	MS_MALLOC(buf, sizeof(dcmMsg));
	if (buf == NULL) {
		MS_DBG_STRERROR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	if ((recv_msg_len = recv(sock, buf, sizeof(dcmMsg), 0)) < 0) {
		MS_DBG_STRERROR("recv failed");
		MS_SAFE_FREE(buf);
		return _media_dcm_get_error();
	}
	memcpy(msg, buf, sizeof(dcmMsg));

	MS_SAFE_FREE(buf);

	MS_DBG("_ms_dcm_recv_msg out %d", msg->msg_type);

	return MS_MEDIA_ERR_NONE;
}

gboolean _ms_dcm_agent_recv_msg_from_server()
{
	struct sockaddr_un serv_addr;
	unsigned int serv_addr_len;

	int sockfd = -1;
	int retry = 10;
	MS_DBG("_ms_dcm_agent_recv_msg_from_server in");

	if (g_dcm_comm_sock <= 0)
		_ms_dcm_agent_prepare_tcp_socket(&g_dcm_comm_sock, MS_DCM_COMM_PORT);

	serv_addr_len = sizeof(serv_addr);

	if ((sockfd = accept(g_dcm_comm_sock, (struct sockaddr*)&serv_addr, &serv_addr_len)) < 0) {
		MS_DBG_STRERROR("accept failed");
		return FALSE;
	}

	dcmMsg recv_msg;
	int recv_result = 0;

RETRY:
	recv_result = _ms_dcm_recv_msg(sockfd, & recv_msg);
	if (recv_result != MS_MEDIA_ERR_NONE) {
		MS_DBG_STRERROR("_ms_dcm_recv_msg failed");
		close(sockfd);
		return FALSE;
	}

	if (recv_msg.msg_type == MS_MSG_DCM_SERVER_READY) {
		MS_DBG("DCM service is ready");
	} else {
		MS_DBG("DCM service is not ready retry=%d", retry);
		if (retry > 0) {
			retry--;
			usleep(200000);
			goto RETRY;
		}
	}

	MS_DBG("_ms_dcm_agent_recv_msg_from_server out %d", recv_msg.msg_type);

	close(sockfd);
	return TRUE;
}

gboolean _ms_dcm_agent_recv_dcm_done_from_server(GIOChannel *src, GIOCondition condition, gpointer data)
{
	int ret = MS_MEDIA_ERR_NONE;
	struct sockaddr_un dcm_serv_addr;
	unsigned int dcm_serv_addr_len;

	int sockfd = -1;
	int dcm_serv_sockfd = -1;

	/* Once all-dcm extraction is done, check if there is queued all-dcm request */
	GSource *check_queued_all_dcm_request = NULL;
	check_queued_all_dcm_request = g_idle_source_new();
	g_source_set_callback(check_queued_all_dcm_request, _ms_dcm_check_queued_request, NULL, NULL);
	g_source_attach(check_queued_all_dcm_request, g_main_context_get_thread_default());

	if (g_dcm_server_extracting == FALSE) {
		MS_DBG_WARN("Recv DCM service extracting done already");
		return FALSE;
	}

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return FALSE;
	}

	dcm_serv_addr_len = sizeof(dcm_serv_addr);

	if ((dcm_serv_sockfd = accept(sockfd, (struct sockaddr*)&dcm_serv_addr, &dcm_serv_addr_len)) < 0) {
		MS_DBG_STRERROR("accept failed");
		return FALSE;
	}

	dcmMsg recv_msg;

	MS_DBG_ERR("DCM service SOCKET %d", dcm_serv_sockfd);

	ret = ms_ipc_receive_message(dcm_serv_sockfd, &recv_msg, sizeof(dcmMsg));
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_STRERROR("ms_ipc_receive_message failed");
		close(dcm_serv_sockfd);
		return FALSE;
	}

	close(dcm_serv_sockfd);

	MS_DBG("Receive : %d", recv_msg.msg_type);
	if (recv_msg.msg_type == MS_MSG_DCM_EXTRACT_ALL_DONE) {
		MS_DBG("DCM extracting done");
		g_dcm_server_extracting = FALSE;
		MS_DBG("g_dcm_server_extracting is false");

		return FALSE;
	}

	return FALSE;
}

gboolean _ms_dcm_agent_execute_server()
{
	int pid;
	pid = fork();

	if (pid < 0) {
		return FALSE;
	} else if (pid == 0) {
		execl(DCM_SERVER_PATH, "dcm-svc", NULL);
	} else {
		MS_DBG("Child process for dcm is %d", pid);
		g_folk_dcm_server = TRUE;
	}

	g_dcm_service_pid = pid;

	if (!_ms_dcm_agent_recv_msg_from_server()) {
		MS_DBG_ERR("_ms_dcm_agent_recv_msg_from_server is failed");
		return FALSE;
	}

	return TRUE;
}

gboolean _ms_dcm_agent_send_msg_to_dcm_server(dcmMsg *recv_msg, dcmMsg *res_msg)
{
	int sock;
	ms_sock_info_s sock_info;
	struct sockaddr_un serv_addr;
	int send_str_len = strlen(recv_msg->msg);

	if (send_str_len > MAX_FILEPATH_LEN) {
		MS_DBG_ERR("original path's length exceeds %d(max packet size)", MAX_FILEPATH_LEN);
		return FALSE;
	}

	if (ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info) < 0) {
		MS_DBG_ERR("ms_ipc_create_client_socket failed");
		return FALSE;
	}

	if (recv_msg->uid == 0)
		ms_sys_get_uid(&(recv_msg->uid));
	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[MS_DCM_DAEMON_PORT]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[MS_DCM_DAEMON_PORT])));

	/* Connecting to the DCM service */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MS_DBG_STRERROR("connect");
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	if (send(sock, recv_msg, sizeof(dcmMsg), 0) != sizeof(dcmMsg)) {
		MS_DBG_STRERROR("send failed");
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	MS_DBG_SLOG("Sending msg to DCM service is successful %d", recv_msg->msg_type);

	if (_ms_dcm_recv_msg(sock, res_msg) < 0) {
		MS_DBG_ERR("_ms_dcm_recv_msg failed");
		ms_ipc_delete_client_socket(&sock_info);
		return FALSE;
	}

	MS_DBG_SLOG("recv %s from DCM daemon is successful", res_msg->msg);
	ms_ipc_delete_client_socket(&sock_info);

	if (res_msg->msg_type == 1 && g_dcm_comm_sock > 0) { /* DCM_REQUEST_ALL_MEDIA */
		GSource *source = NULL;
		if (g_dcm_tcp_channel == NULL)
			g_dcm_tcp_channel = g_io_channel_unix_new(g_dcm_comm_sock);
		source = g_io_create_watch(g_dcm_tcp_channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)_ms_dcm_agent_recv_dcm_done_from_server, NULL, NULL);
		g_source_attach(source, g_main_context_get_thread_default());

		g_dcm_server_extracting = TRUE;
		MS_DBG("g_dcm_server_extracting is true");
	}

	return TRUE;
}

gboolean _ms_dcm_check_queued_request(gpointer data)
{
	if (g_dcm_server_queued_all_extracting_request) {
		MS_DBG_WARN("There is queued request");

		/* request all-DCM extraction to DCM service */
		dcmMsg msg;
		dcmMsg recv_msg;
		memset((void *)&msg, 0, sizeof(msg));
		memset((void *)&recv_msg, 0, sizeof(recv_msg));

		msg.msg_type = 1; /* DCM_REQUEST_ALL_MEDIA */
		ms_sys_get_uid(&(msg.uid));
		msg.msg[0] = '\0';
		msg.msg_size = 0;

		/* Command All-DCM extraction to DCM service */
		if (!_ms_dcm_agent_send_msg_to_dcm_server(&msg, &recv_msg))
			MS_DBG_ERR("_ms_dcm_agent_send_msg_to_dcm_server is failed");

		g_dcm_server_queued_all_extracting_request = FALSE;
	} else {
		MS_DBG("There is no queued request");
		return FALSE;
	}

	return FALSE;
}

gboolean _ms_dcm_agent_timer()
{
	if (g_dcm_server_extracting) {
		MS_DBG("Timer is called.. But dcm-service[%d] is busy.. so timer is recreated", g_dcm_service_pid);

		_ms_dcm_create_timer(g_dcm_timer_id);
		return FALSE;
	}

	g_dcm_timer_id = 0;
	MS_DBG("Timer is called.. Now killing dcm-service[%d]", g_dcm_service_pid);

	if (g_dcm_service_pid > 0) {
		/* Kill DCM service */
		dcmMsg msg;
		dcmMsg recv_msg;
		memset((void *)&msg, 0, sizeof(msg));
		memset((void *)&recv_msg, 0, sizeof(recv_msg));

		msg.msg_type = 4; /* DCM_REQUEST_KILL_SERVER */
		msg.msg[0] = '\0';
		msg.msg_size = 0;

		/* Command Kill to DCM service */
		g_shutdowning_dcm_server = TRUE;
		if (!_ms_dcm_agent_send_msg_to_dcm_server(&msg, &recv_msg)) {
			MS_DBG_ERR("_ms_dcm_agent_send_msg_to_dcm_server is failed");
			g_shutdowning_dcm_server = FALSE;
		}
		usleep(200000);
	} else {
		MS_DBG_ERR("g_dcm_service_pid is %d. Maybe there's problem in dcm-service", g_dcm_service_pid);
	}

	return FALSE;
}

gboolean _ms_dcm_request_to_server(gpointer data)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_dcm_request_queue);

	MS_DBG("Queue length : %d", req_len);

	if (req_len <= 0) {
		MS_DBG("There is no request job in the queue");
		g_dcm_queue_work = 0;
		return FALSE;
	}

	if (g_shutdowning_dcm_server) {
		MS_DBG_ERR("DCM service is shutting down... wait for complete");
		usleep(10000);
		return TRUE;
	}

	if (g_folk_dcm_server == FALSE && g_dcm_server_extracting == FALSE) {
		if (g_dcm_service_pid <= 0) { /* This logic is temporary */
			MS_DBG_WARN("DCM service is not running.. so start it");
			if (!_ms_dcm_agent_execute_server()) {
				MS_DBG_ERR("_ms_dcm_agent_execute_server is failed");
				g_dcm_queue_work = 0;
				return FALSE;
			} else {
				_ms_dcm_create_timer(g_dcm_timer_id);
			}
		}
	} else {
		/* Timer is re-created*/
		_ms_dcm_create_timer(g_dcm_timer_id);
	}

	dcmRequest *req = NULL;
	req = (dcmRequest *)g_queue_pop_head(g_dcm_request_queue);

	MS_DBG("Pop request %d %p", req->recv_msg->msg_type);

	if (req == NULL) {
		MS_DBG_ERR("Failed to get a request job from queue");
		return TRUE;
	}

	int client_sock = -1;
	dcmMsg *recv_msg = NULL;
	dcmMsg res_msg;
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
		if (recv_msg->msg_type == 1 && g_dcm_server_extracting) {
			MS_DBG_WARN("DCM service is already extracting..This request is queued.");
			g_dcm_server_queued_all_extracting_request = TRUE;
		} else {
			if (!_ms_dcm_agent_send_msg_to_dcm_server(recv_msg, &res_msg)) {
				MS_DBG_ERR("_ms_dcm_agent_send_msg_to_dcm_server is failed");

				dcmMsg res_msg;
				memset((void *)&res_msg, 0, sizeof(res_msg));

				res_msg.msg_type = recv_msg->msg_type;
				res_msg.msg_size = strlen(recv_msg->msg);
				if (res_msg.msg_size > 0)
					strncpy(res_msg.msg, recv_msg->msg, res_msg.msg_size);
				res_msg.result = recv_msg->result;

				if (send(client_sock, &res_msg, sizeof(res_msg), 0) != sizeof(res_msg))
					MS_DBG_STRERROR("sendto failed");
				else
					MS_DBG("Sent Refuse msg from %s", recv_msg->msg);

				close(client_sock);

				MS_SAFE_FREE(req->recv_msg);
				MS_SAFE_FREE(req);

				return TRUE;
			}
		}
	} else {
		MS_DBG_ERR("recv_msg is NULL from queue request");
	}

	strncpy(res_msg.msg, recv_msg->msg, recv_msg->msg_size);
	res_msg.msg_size = recv_msg->msg_size;

	if (send(client_sock, &res_msg, sizeof(res_msg), 0) != sizeof(res_msg))
		MS_DBG_STRERROR("sendto failed");

	close(client_sock);
	MS_SAFE_FREE(req->recv_msg);
	MS_SAFE_FREE(req);

	return TRUE;
}

gboolean _ms_dcm_agent_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
	struct sockaddr_un client_addr;
	unsigned int client_addr_len;
	dcmMsg *recv_msg = NULL;
	int sock = -1;
	int client_sock = -1;

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

	recv_msg = calloc(1, sizeof(dcmMsg));
	if (recv_msg == NULL) {
		MS_DBG_ERR("Failed to allocate memory");
		close(client_sock);
		return TRUE;
	}

	if (_ms_dcm_recv_msg(client_sock, recv_msg) < 0) {
		MS_DBG_ERR("_ms_dcm_recv_msg failed ");
		close(client_sock);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	if (getsockopt(client_sock, SOL_SOCKET, SO_PEERCRED, &cr, (socklen_t *) &cl) < 0) {
		MS_DBG_ERR("credential information error");
	}

	if (getuid() != cr.uid)
		recv_msg->uid = cr.uid;

	dcmRequest *dcm_req = NULL;

	MS_MALLOC(dcm_req, sizeof(dcmRequest));
	if (dcm_req == NULL) {
		MS_DBG_ERR("Failed to create request element");
		close(client_sock);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	dcm_req->client_sock = client_sock;
	dcm_req->recv_msg = recv_msg;

	if (g_dcm_request_queue == NULL) {
		MS_DBG_WARN("queue is init");
		g_dcm_request_queue = g_queue_new();
	}

	if (g_queue_get_length(g_dcm_request_queue) >= MAX_DCM_REQUEST) {
		MS_DBG_WARN("Request Queue is full");
		dcmMsg res_msg;
		memset((void *)&res_msg, 0, sizeof(res_msg));

		res_msg.msg_type = recv_msg->msg_type;
		if (res_msg.msg_size != 0) {
			res_msg.msg_size = recv_msg->msg_size;
			strncpy(res_msg.msg, recv_msg->msg, res_msg.msg_size);
		}

		if (send(client_sock, &res_msg, sizeof(dcmMsg), 0) != sizeof(dcmMsg))
			MS_DBG_STRERROR("sendto failed");
		else
			MS_DBG("Sent Refuse msg from %s", recv_msg->msg);

		close(client_sock);
		MS_SAFE_FREE(dcm_req->recv_msg);
		MS_SAFE_FREE(dcm_req);

		return TRUE;
	}

	MS_DBG_SLOG("%s is queued", recv_msg->msg);
	g_queue_push_tail(g_dcm_request_queue, (gpointer)dcm_req);

	if (!g_dcm_queue_work) {
		GSource *src_request = NULL;
		src_request = g_idle_source_new();
		g_source_set_callback(src_request, _ms_dcm_request_to_server, NULL, NULL);
		g_source_attach(src_request, g_main_context_get_thread_default());
		g_dcm_queue_work = 1;
	}

	return TRUE;
}

gboolean _ms_dcm_agent_prepare_tcp_socket(int *sock_fd, unsigned short serv_port)
{
	int sock;

	if (ms_ipc_create_server_socket(MS_PROTOCOL_TCP, serv_port, &sock) < 0) {
		MS_DBG_ERR("_ms_dcm_create_socket failed");
		return FALSE;
	}

	*sock_fd = sock;

	return TRUE;
}

gpointer ms_dcm_agent_start_thread(gpointer data)
{
	int sockfd = -1;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	MS_DBG_FENTER();

	/* Create and bind new TCP socket */
	if (!_ms_dcm_agent_prepare_tcp_socket(&sockfd, MS_DCM_CREATOR_PORT)) {
		MS_DBG_ERR("Failed to create socket");
		return NULL;
	}

	context = g_main_context_new();

	if (context == NULL)
		MS_DBG_ERR("g_main_context_new failed");
	else
		MS_DBG("g_main_context_new success");

	g_dcm_agent_loop = g_main_loop_new(context, FALSE);
	g_main_context_push_thread_default(context);

	/* Create new channel to watch udp socket */
	channel = g_io_channel_unix_new(sockfd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Set callback to be called when socket is readable */
	g_source_set_callback(source, (GSourceFunc)_ms_dcm_agent_read_socket, NULL, NULL);
	g_source_attach(source, context);

	MS_DBG("************************************");
	MS_DBG("*** DCM Agent thread is running ***");
	MS_DBG("************************************");

	g_main_loop_run(g_dcm_agent_loop);

	MS_DBG("DCM Agent thread is shutting down...");

	/*close an IO channel & remove resources */
	g_io_channel_shutdown(channel, FALSE, NULL);
	g_io_channel_shutdown(g_dcm_tcp_channel, FALSE, NULL);
	g_io_channel_unref(channel);
	close(g_dcm_comm_sock);

	g_main_loop_unref(g_dcm_agent_loop);

	close(sockfd);

	return NULL;
}


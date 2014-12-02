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

#define THUMB_SERVER_PATH tzplatform_mkpath(TZ_SYS_BIN,"media-thumbnail-server")

gboolean _ms_thumb_agent_timer();

static GMainLoop *g_thumb_agent_loop = NULL;
static GIOChannel *g_udp_channel = NULL;
static gboolean g_folk_thumb_server = FALSE;
static gboolean g_thumb_server_extracting = FALSE;
static int g_communicate_sock = 0;
static int g_timer_id = 0;
static int g_server_pid = 0;

static GQueue *g_request_queue = NULL;
static int g_queue_work = 0;

typedef struct {
	int client_sock;
	thumbMsg *recv_msg;
} thumbRequest;

#ifdef _USE_UDS_SOCKET_
extern char MEDIA_IPC_PATH[][70];

#endif

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

GMainLoop *
ms_get_thumb_thread_mainloop(void)
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

	if (g_timer_id > 0) {
		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), g_timer_id));
		g_timer_id = 0;
	}

	if (g_thumb_server_extracting) {
		/* Need to inplement when crash happens */
#if 0
		/* Restart thumbnail server */
		if (_ms_thumb_agent_execute_server() < 0) {
			MS_DBG_ERR("starting thumbnail-server failed");
		} else {
			MS_DBG("Thumbnail-server is started");
		}

		thumbMsg msg;
		thumbMsg recv_msg;
		memset((void *)&msg, 0, sizeof(msg));
		memset((void *)&recv_msg, 0, sizeof(recv_msg));

		msg.msg_type = 2; // THUMB_REQUEST_ALL_MEDIA
		msg.org_path[0] = '\0';
		msg.origin_path_size = 1;
		msg.dst_path[0] = '\0';
		msg.dest_path_size = 1;

		/* Command all thumbnail extraction to thumbnail server */
		if (!_ms_thumb_agent_send_msg_to_thumb_server(&msg, &recv_msg)) {
			MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");
		}

		_ms_thumb_create_timer(g_timer_id);
#else
		MS_DBG_ERR("Thumbnail server is dead when processing all-thumbs extraction");
		g_thumb_server_extracting = FALSE;
		g_server_pid = 0;
#endif
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
	g_source_set_callback (timer_src, _ms_thumb_agent_timer, NULL, NULL);
	g_timer_id = g_source_attach (timer_src, g_main_context_get_thread_default());

}

/* This checks if thumbnail server is running */
bool _ms_thumb_check_process()
{
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
	bool ret = FALSE;

	pdir = opendir("/proc");
	if (pdir == NULL) {
		MS_DBG_ERR("err: NO_DIR\n");
		return 0;
	}

	while (!readdir_r(pdir, &pinfo, &result)) {
		if (result == NULL)
			break;

		if (pinfo.d_type != 4 || pinfo.d_name[0] == '.'
		    || pinfo.d_name[0] > 57)
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

	return ret;
}
int
_ms_thumb_create_socket(int sock_type, int *sock)
{
	int sock_fd = 0;

#ifdef _USE_UDS_SOCKET_
	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
#elif defined(_USE_UDS_SOCKET_TCP_)
	if ((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
#else
	if ((sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
#endif
		MS_DBG_ERR("socket failed: %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	if (sock_type == CLIENT_SOCKET) {

		struct timeval tv_timeout = { MS_TIMEOUT_SEC_10, 0 };

		if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
			MS_DBG_ERR("setsockopt failed: %s", strerror(errno));
			close(sock_fd);
			return MS_MEDIA_ERR_SOCKET_INTERNAL;
		}
	} else if (sock_type == SERVER_SOCKET) {

		int n_reuse = 1;

		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &n_reuse, sizeof(n_reuse)) == -1) {
			MS_DBG_ERR("setsockopt failed: %s", strerror(errno));
			close(sock_fd);
			return MS_MEDIA_ERR_SOCKET_INTERNAL;
		}
	}

	*sock = sock_fd;

	return MS_MEDIA_ERR_NONE;
}


int
_ms_thumb_create_udp_socket(int *sock)
{
	int sock_fd = 0;

#ifdef _USE_UDS_SOCKET_
	if ((sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
#else
	if ((sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
#endif
		MS_DBG_ERR("socket failed: %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	struct timeval tv_timeout = { MS_TIMEOUT_SEC_10, 0 };

	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_timeout, sizeof(tv_timeout)) == -1) {
		MS_DBG_ERR("setsockopt failed: %s", strerror(errno));
		close(sock_fd);
		return MS_MEDIA_ERR_SOCKET_INTERNAL;
	}

	*sock = sock_fd;

	return MS_MEDIA_ERR_NONE;
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
		MS_DBG_ERR("recvfrom failed : %s", strerror(errno));
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}
}

int
_ms_thumb_recv_msg(int sock, int header_size, thumbMsg *msg)
{
	int recv_msg_len = 0;
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(header_size);

	if ((recv_msg_len = recv(sock, buf, header_size, 0)) < 0) {
		MS_DBG_ERR("recv failed : %s", strerror(errno));
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);
	//MS_DBG("origin_path_size : %d, dest_path_size : %d", msg->origin_path_size, msg->dest_path_size);

	MS_SAFE_FREE(buf);

	if (msg->origin_path_size <= 0  || msg->origin_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->origin_path_size is invalid %d", msg->origin_path_size );
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	buf = (unsigned char*)malloc(msg->origin_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->origin_path_size, 0)) < 0) {
		MS_DBG_ERR("recv failed : %s", strerror(errno));
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->org_path, (char*)buf, msg->origin_path_size);
	//MS_DBG("original path : %s", msg->org_path);

	MS_SAFE_FREE(buf);

	if (msg->dest_path_size <= 0  || msg->dest_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->dest_path_size is invalid %d", msg->dest_path_size );
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	buf = (unsigned char*)malloc(msg->dest_path_size);

	if ((recv_msg_len = recv(sock, buf, msg->dest_path_size, 0)) < 0) {
		MS_DBG_ERR("recv failed : %s", strerror(errno));
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	strncpy(msg->dst_path, (char*)buf, msg->dest_path_size);
	//MS_DBG("destination path : %s", msg->dst_path);

	MS_SAFE_FREE(buf);
	return MS_MEDIA_ERR_NONE;
}


int
#ifdef _USE_UDS_SOCKET_
_ms_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_un *from_addr, unsigned int *from_size)
#else
_ms_thumb_recv_udp_msg(int sock, int header_size, thumbMsg *msg, struct sockaddr_in *from_addr, unsigned int *from_size)
#endif
{
	int recv_msg_len = 0;
#ifdef _USE_UDS_SOCKET_
	unsigned int from_addr_size = sizeof(struct sockaddr_un);
#else
	unsigned int from_addr_size = sizeof(struct sockaddr_in);
#endif
	unsigned char *buf = NULL;

	buf = (unsigned char*)malloc(sizeof(thumbMsg));

	recv_msg_len = ms_ipc_wait_message(sock, buf, sizeof(thumbMsg), from_addr, &from_addr_size);
	if (recv_msg_len != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_wait_message failed : %s", strerror(errno));
		MS_SAFE_FREE(buf);
		return _media_thumb_get_error();
	}

	memcpy(msg, buf, header_size);
	//MS_DBG("origin_path_size : %d, dest_path_size : %d", msg->origin_path_size, msg->dest_path_size);

	if (msg->origin_path_size <= 0  || msg->origin_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->origin_path_size is invalid %d", msg->origin_path_size );
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	strncpy(msg->org_path, (char*)buf + header_size, msg->origin_path_size);
	//MS_DBG("original path : %s", msg->org_path);

	if (msg->dest_path_size <= 0  || msg->dest_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_SAFE_FREE(buf);
		MS_DBG_ERR("msg->origin_path_size is invalid %d", msg->dest_path_size );
		return MS_MEDIA_ERR_DATA_TAINTED;
	}

	strncpy(msg->dst_path, (char*)buf + header_size + msg->origin_path_size, msg->dest_path_size);
	//MS_DBG("destination path : %s", msg->dst_path);

	MS_SAFE_FREE(buf);
	*from_size = from_addr_size;

	return MS_MEDIA_ERR_NONE;
}

int
_ms_thumb_set_buffer(thumbMsg *req_msg, unsigned char **buf, int *buf_size)
{
	if (req_msg == NULL || buf == NULL) {
		return -1;
	}

	int org_path_len = 0;
	int dst_path_len = 0;
	int size = 0;
	int header_size = 0;

	header_size = sizeof(thumbMsg) - MAX_MSG_SIZE*2;
	org_path_len = strlen(req_msg->org_path) + 1;
	dst_path_len = strlen(req_msg->dst_path) + 1;

	//MS_DBG("Basic Size : %d, org_path : %s[%d], dst_path : %s[%d]", header_size, req_msg->org_path, org_path_len, req_msg->dst_path, dst_path_len);

	size = header_size + org_path_len + dst_path_len;
	*buf = malloc(size);
	memcpy(*buf, req_msg, header_size);
	memcpy((*buf)+header_size, req_msg->org_path, org_path_len);
	memcpy((*buf)+header_size + org_path_len, req_msg->dst_path, dst_path_len);

	*buf_size = size;

	return 0;
}

/*
void _ms_thumb_agent_child_handler(GPid pid, gint status, gpointer user_data)
{
	MS_DBG_WARN("media-thumbnail-server[%d] is shutdown : %d", pid, status);
	g_folk_thumb_server = FALSE;
}
*/
gboolean _ms_thumb_agent_child_handler(gpointer data)
{
	int pid = GPOINTER_TO_INT(data);
	MS_DBG("media-thumbnail-server[%d] is killed", pid);
	return FALSE;
}

gboolean _ms_thumb_agent_recv_msg_from_server()
{
	if (g_communicate_sock <= 0) {
		_ms_thumb_agent_prepare_udp_socket();
	}

	ms_thumb_server_msg recv_msg;
	int recv_msg_size = 0;

	recv_msg_size = ms_ipc_receive_message(g_communicate_sock, & recv_msg, sizeof(ms_thumb_server_msg),  NULL, NULL);
	if (recv_msg_size != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_receive_message failed : %s\n", strerror(errno));
		return FALSE;
	}

	//MS_DBG("Receive : %d(%d)", recv_msg.msg_type, recv_msg_size);
	if (recv_msg.msg_type == MS_MSG_THUMB_SERVER_READY) {
		MS_DBG("Thumbnail server is ready");
	}

	return TRUE;
}

gboolean _ms_thumb_agent_recv_thumb_done_from_server(GIOChannel *src, GIOCondition condition, gpointer data)
{
	int sockfd = -1;

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return FALSE;
	}

	ms_thumb_server_msg recv_msg;
	int recv_msg_size = 0;

	recv_msg_size = ms_ipc_receive_message(sockfd, &recv_msg, sizeof(ms_thumb_server_msg), NULL, NULL);
	if (recv_msg_size != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_ipc_receive_message failed : %s\n", strerror(errno));
		return FALSE;
	}

	MS_DBG("Receive : %d(%d)", recv_msg.msg_type, recv_msg_size);
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
#if 0
	GSource *child_watch_src =  g_child_watch_source_new(pid);
	g_source_set_callback(child_watch_src, _ms_thumb_agent_child_handler, GINT_TO_POINTER(pid), NULL);
	g_source_attach(child_watch_src, g_main_context_get_thread_default());
#endif
	//g_child_watch_add(pid, _ms_thumb_agent_child_handler, NULL);
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
	const char *serv_ip = "127.0.0.1";
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif

	int send_str_len = strlen(recv_msg->org_path);

	if (send_str_len > MAX_MSG_SIZE) {
		MS_DBG_ERR("original path's length exceeds %d(max packet size)", MAX_MSG_SIZE);
		return FALSE;
	}

#if 0
	/* Creaete a datagram/UDP socket */
	if (_ms_thumb_create_udp_socket(&sock) < 0) {
		MS_DBG_ERR("_ms_thumb_create_udp_socket failed");
		return FALSE;
	}
#endif
#ifdef _USE_UDS_SOCKET_
	if (ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sock, MS_THUMB_DAEMON_PORT) < 0) {
#else
	if (ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sock) < 0) {
#endif
		MS_DBG_ERR("ms_ipc_create_client_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	MS_DBG("%s", MEDIA_IPC_PATH[MS_THUMB_DAEMON_PORT]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[MS_THUMB_DAEMON_PORT]);
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
	serv_addr.sin_port = htons(MS_THUMB_DAEMON_PORT);
#endif

	int buf_size = 0;
	int header_size = 0;
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(recv_msg, &buf, &buf_size);

	//MS_DBG("buffer size : %d", buf_size);
	if (sendto(sock, buf, buf_size, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != buf_size) {
		MS_DBG_ERR("sendto failed: %s\n", strerror(errno));
		MS_SAFE_FREE(buf);
		close(sock);
		return FALSE;
	}

	MS_SAFE_FREE(buf);
	MS_DBG("Sending msg to thumbnail server is successful");

#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un client_addr;
#else
	struct sockaddr_in client_addr;
#endif
	unsigned int client_addr_len;
	header_size = sizeof(thumbMsg) - MAX_MSG_SIZE*2;

	if (_ms_thumb_recv_udp_msg(sock, header_size, res_msg, &client_addr, &client_addr_len) < 0) {
		MS_DBG_ERR("_ms_thumb_recv_udp_msg failed");
		close(sock);
		return FALSE;
	}

	MS_DBG("recv %s from thumb daemon is successful", res_msg->dst_path);
	close(sock);

	if (res_msg->msg_type == 2 && g_communicate_sock > 0) { // THUMB_REQUEST_ALL_MEDIA
		/* Create new channel to watch udp socket */
		GSource *source = NULL;
		if (g_udp_channel == NULL)
			g_udp_channel = g_io_channel_unix_new(g_communicate_sock);
		source = g_io_create_watch(g_udp_channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)_ms_thumb_agent_recv_thumb_done_from_server, NULL, NULL);
		g_source_attach(source, g_main_context_get_thread_default());

		g_thumb_server_extracting = TRUE;
	}

	return TRUE;
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
#if 0
		if (kill(g_server_pid, SIGKILL) < 0) {
			MS_DBG_ERR("kill failed : %s", strerror(errno));
		}
#else
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

		/* Command Kill to thumbnail server */
		if (!_ms_thumb_agent_send_msg_to_thumb_server(&msg, &recv_msg)) {
			MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");
		}
#endif
		usleep(200000);
	} else {
		MS_DBG_ERR("g_server_pid is %d. Maybe there's problem in thumbnail-server", g_server_pid);
	}

	return FALSE;
}

#if 0
gboolean _ms_thumb_agent_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un client_addr;
#else
	struct sockaddr_in client_addr;
#endif
	unsigned int client_addr_len;

	thumbMsg recv_msg;
	thumbMsg res_msg;
	int header_size = 0;
	int sock = -1;
	int client_sock = -1;

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	memset((void *)&recv_msg, 0, sizeof(thumbMsg));
	memset((void *)&res_msg, 0, sizeof(res_msg));
	header_size = sizeof(thumbMsg) - MAX_MSG_SIZE*2;

	if ((client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		MS_DBG_ERR("accept failed : %s", strerror(errno));
		return TRUE;
	}

	MS_DBG("Client[%d] is accepted", client_sock);

	if (_ms_thumb_recv_msg(client_sock, header_size, &recv_msg) < 0) {
		MS_DBG_ERR("_ms_thumb_recv_msg failed ");
		close(client_sock);
		return TRUE;
	}

	MS_DBG("Received [%d] %s(%d) from PID(%d) \n", recv_msg.msg_type, recv_msg.org_path, strlen(recv_msg.org_path), recv_msg.pid);

	if (g_folk_thumb_server == FALSE && g_thumb_server_extracting == FALSE) {
		if(_ms_thumb_check_process() == FALSE) { // This logic is temporary
		MS_DBG_WARN("Thumb server is not running.. so start it");
		if (!_ms_thumb_agent_execute_server()) {
			MS_DBG_ERR("_ms_thumb_agent_execute_server is failed");
			return TRUE;
		} else {

			GSource *timer_src = g_timeout_source_new_seconds(MS_TIMEOUT_SEC_20);
			g_source_set_callback (timer_src, _ms_thumb_agent_timer, NULL, NULL);
			g_timer_id = g_source_attach (timer_src, g_main_context_get_thread_default());
		}
		}
	} else {
		if (g_timer_id > 0) {
			g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), g_timer_id));
			//MS_DBG("Timer is recreated");
			GSource *timer_src = g_timeout_source_new_seconds(MS_TIMEOUT_SEC_20);
			g_source_set_callback (timer_src, _ms_thumb_agent_timer, NULL, NULL);
			g_timer_id = g_source_attach (timer_src, g_main_context_get_thread_default());
		}
	}

	if (!_ms_thumb_agent_send_msg_to_thumb_server(&recv_msg, &res_msg)) {
		MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");

		return TRUE;
	}

	strncpy(res_msg.org_path, recv_msg.org_path, recv_msg.origin_path_size);
	res_msg.origin_path_size = recv_msg.origin_path_size;
	res_msg.dest_path_size = strlen(res_msg.dst_path) + 1;

	int buf_size = 0;
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

	//MS_DBG("buffer size : %d", buf_size);

	if (send(client_sock, buf, buf_size, 0) != buf_size) {
		MS_DBG_ERR("sendto failed : %s", strerror(errno));
	} else {
		MS_DBG("Sent %s(%d) \n", res_msg.dst_path, strlen(res_msg.dst_path));
	}

	close(client_sock);
	MS_SAFE_FREE(buf);
	return TRUE;
}
#else
int _ms_thumb_cancel_media(const char *path, int pid)
{
	int ret = -1;
	int i = 0;
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	MS_DBG("Queue length : %d", req_len);

	for (i = 0; i < req_len; i++) {
		thumbRequest *req = NULL;
		req = (thumbRequest *)g_queue_peek_nth(g_request_queue, i);
		if (req == NULL) continue;

		if ((req->recv_msg->pid) == pid && (strncmp(path, req->recv_msg->org_path, strlen(path))) == 0) {
			MS_DBG("Remove %s from queue", req->recv_msg->org_path);
			g_queue_pop_nth(g_request_queue, i);

			close(req->client_sock);
			MS_SAFE_FREE(req->recv_msg);
			MS_SAFE_FREE(req);
			ret = 0;

			break;
		}
	}

	return ret;
}

int _ms_thumb_cancel_all(int pid)
{
	int ret = -1;
	int i = 0;
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	MS_DBG("Queue length : %d", req_len);

	for (i = 0; i < req_len; i++) {
		thumbRequest *req = NULL;
		req = (thumbRequest *)g_queue_peek_nth(g_request_queue, i);
		if (req == NULL) continue;

		if (req->recv_msg->pid == pid) {
			MS_DBG("Remove [%d] %s from queue", req->recv_msg->pid, req->recv_msg->org_path);
			g_queue_pop_nth(g_request_queue, i);
			i--;
			req_len--;

			close(req->client_sock);
			MS_SAFE_FREE(req->recv_msg);
			MS_SAFE_FREE(req);
			ret = 0;
		}
	}

	return ret;
}

void _ms_thumb_cancle_request(thumbRequest *thumb_req)
{
	MS_DBG("");
	int ret = -1;

	if (thumb_req == NULL) return;

	thumbMsg *recv_msg = thumb_req->recv_msg;
	if (recv_msg == NULL) {
		MS_SAFE_FREE(thumb_req);
		return;
	}

	if (recv_msg->msg_type == 3)
		ret = _ms_thumb_cancel_media(recv_msg->org_path, recv_msg->pid);
	else if (recv_msg->msg_type == 4)
		ret = _ms_thumb_cancel_all(recv_msg->pid);

	if (ret == 0) {
		recv_msg->status = 0;  // THUMB_SUCCESS
	}

	if (recv_msg->origin_path_size <= 0  || recv_msg->origin_path_size > MS_FILE_PATH_LEN_MAX) {
		MS_DBG_ERR("recv_msg->origin_path_size is invalid %d", recv_msg->origin_path_size );
		return;
	}

	recv_msg->dest_path_size = recv_msg->origin_path_size;
	strncpy(recv_msg->dst_path, recv_msg->org_path, recv_msg->dest_path_size);
/*
	int buf_size = 0;
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(recv_msg, &buf, &buf_size);

	if (send(thumb_req->client_sock, buf, buf_size, 0) != buf_size) {
		MS_DBG_ERR("sendto failed : %s", strerror(errno));
	} else {
		MS_DBG("Sent response");
	}
*/
	close(thumb_req->client_sock);
	//MS_SAFE_FREE(buf);
	MS_SAFE_FREE(thumb_req->recv_msg);
	MS_SAFE_FREE(thumb_req);

	return;
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

	if (g_folk_thumb_server == FALSE && g_thumb_server_extracting == FALSE) {
		if(_ms_thumb_check_process() == FALSE) { // This logic is temporary
			MS_DBG_WARN("Thumb server is not running.. so start it");
			if (!_ms_thumb_agent_execute_server()) {
				MS_DBG_ERR("_ms_thumb_agent_execute_server is failed");
				g_queue_work = 0;
				return FALSE;
			} else {
				_ms_thumb_create_timer(g_timer_id);
			}
		}
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

	if (req->client_sock <=0 || req->recv_msg == NULL) {
		MS_DBG_ERR("client sock is below 0 or recv msg is NULL");
		MS_SAFE_FREE(req->recv_msg);
		MS_SAFE_FREE(req);
		return TRUE;
	}

	if (recv_msg) {
		if (!_ms_thumb_agent_send_msg_to_thumb_server(recv_msg, &res_msg)) {
			MS_DBG_ERR("_ms_thumb_agent_send_msg_to_thumb_server is failed");

			close(client_sock);
			MS_SAFE_FREE(req->recv_msg);
			MS_SAFE_FREE(req);
			return TRUE;
		}
	} else {
		MS_DBG_ERR("recv_msg is NULL from queue request");
	}

	strncpy(res_msg.org_path, recv_msg->org_path, recv_msg->origin_path_size);
	res_msg.origin_path_size = recv_msg->origin_path_size;
	res_msg.dest_path_size = strlen(res_msg.dst_path) + 1;

	int buf_size = 0;
	unsigned char *buf = NULL;
	_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

	if (send(client_sock, buf, buf_size, 0) != buf_size) {
		MS_DBG_ERR("sendto failed : %s", strerror(errno));
	} else {
		MS_DBG("Sent %s(%d) from %s \n", res_msg.dst_path, strlen(res_msg.dst_path), res_msg.org_path);
	}

	close(client_sock);
	MS_SAFE_FREE(buf);
	MS_SAFE_FREE(req->recv_msg);
	MS_SAFE_FREE(req);

	return TRUE;
}

gboolean _ms_thumb_agent_read_socket(GIOChannel *src,
									GIOCondition condition,
									gpointer data)
{
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un client_addr;
#elif defined(_USE_UDS_SOCKET_TCP_)
	struct sockaddr_un client_addr;
#else
	struct sockaddr_in client_addr;
#endif
	unsigned int client_addr_len;

	thumbMsg *recv_msg = NULL;
	int header_size = 0;
	int sock = -1;
	int client_sock = -1;

	sock = g_io_channel_unix_get_fd(src);
	if (sock < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	header_size = sizeof(thumbMsg) - MAX_MSG_SIZE*2;
	client_addr_len = sizeof(client_addr);

	if ((client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
		MS_DBG_ERR("accept failed : %s", strerror(errno));
		return TRUE;
	}

	MS_DBG("Client[%d] is accepted", client_sock);

	recv_msg = calloc(1, sizeof(thumbMsg));
	if (recv_msg == NULL) {
		MS_DBG_ERR("Failed to allocate memory");
		close(client_sock);
		return TRUE;
	}

	if (_ms_thumb_recv_msg(client_sock, header_size, recv_msg) < 0) {
		MS_DBG_ERR("_ms_thumb_recv_msg failed ");
		close(client_sock);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	MS_DBG("Received [%d] %s(%d) from PID(%d) \n", recv_msg->msg_type, recv_msg->org_path, strlen(recv_msg->org_path), recv_msg->pid);

	thumbRequest *thumb_req = NULL;
	thumb_req = calloc(1, sizeof(thumbRequest));
	if (thumb_req == NULL) {
		MS_DBG_ERR("Failed to create request element");
		close(client_sock);
		MS_SAFE_FREE(recv_msg);
		return TRUE;
	}

	thumb_req->client_sock = client_sock;
	thumb_req->recv_msg = recv_msg;

	if (recv_msg->msg_type == 3 || recv_msg->msg_type == 4) { // THUMB_REQUEST_CANCEL_MEDIA || THUMB_REQUEST_CANCEL_ALL
		_ms_thumb_cancle_request(thumb_req);
		return TRUE;
	}

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
		res_msg.org_path[0] = '\0';
		res_msg.origin_path_size = 0;
		res_msg.dst_path[0] = '\0';
		res_msg.dest_path_size = 0;

		int buf_size = 0;
		unsigned char *buf = NULL;
		_ms_thumb_set_buffer(&res_msg, &buf, &buf_size);

		if (send(client_sock, buf, buf_size, 0) != buf_size) {
			MS_DBG_ERR("sendto failed : %s", strerror(errno));
		} else {
			MS_DBG("Sent Refuse msg from %s \n", recv_msg->org_path);
		}

		close(client_sock);
		MS_SAFE_FREE(buf);
		MS_SAFE_FREE(thumb_req->recv_msg);
		MS_SAFE_FREE(thumb_req);

		return TRUE;
	}

	MS_DBG("%s is queued", recv_msg->org_path);
	g_queue_push_tail(g_request_queue, (gpointer)thumb_req);

	if (!g_queue_work) {
		GSource *src_request = NULL;
		src_request = g_idle_source_new ();
		g_source_set_callback (src_request, _ms_thumb_request_to_server, NULL, NULL);
		//g_source_set_priority(src_request, G_PRIORITY_LOW);
		g_source_attach (src_request, g_main_context_get_thread_default());
		g_queue_work = 1;
	}

	return TRUE;
}
#endif


gboolean _ms_thumb_agent_prepare_tcp_socket(int *sock_fd)
{
	int sock;
	unsigned short serv_port;

#ifdef _USE_UDS_SOCKET_TCP_
	serv_port = MS_THUMB_CREATOR_TCP_PORT;
#else
	serv_port = MS_THUMB_CREATOR_PORT;
#endif

#if 0
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif

	/* Create a TCP socket */
	if (_ms_thumb_create_socket(SERVER_SOCKET, &sock) < 0) {
		MS_DBG_ERR("_ms_thumb_create_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[serv_port]);
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);
#endif

	/* Bind to the local address */
	if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		MS_DBG_ERR("bind failed");
		return FALSE;
	}

	MS_DBG("bind success");

	/* Listening */
	if (listen(sock, SOMAXCONN) < 0) {
		MS_DBG_ERR("listen failed : %s", strerror(errno));
		return FALSE;
	}

	MS_DBG("Listening...");
#endif

#ifdef _USE_UDS_SOCKET_TCP_
	if (ms_ipc_create_server_tcp_socket(MS_PROTOCOL_TCP, serv_port, &sock) < 0) {
#else
	if (ms_ipc_create_server_socket(MS_PROTOCOL_TCP, serv_port, &sock) < 0) {
#endif
		MS_DBG_ERR("_ms_thumb_create_socket failed");
		return FALSE;
	}

	*sock_fd = sock;

	return TRUE;
}

gboolean _ms_thumb_agent_prepare_udp_socket()
{
	int sock;
	unsigned short serv_port;

	serv_port = MS_THUMB_COMM_PORT;

	if (ms_ipc_create_server_socket(MS_PROTOCOL_UDP, serv_port, &sock) < 0) {
		MS_DBG_ERR("ms_ipc_create_server_socket failed");
		return FALSE;
	}
#if 0
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif

	/* Creaete a UDP socket */
	if (_ms_thumb_create_udp_socket(&sock) < 0) {
		MS_DBG_ERR("_ms_thumb_create_udp_socket failed");
		return FALSE;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[serv_port]);
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(serv_port);
#endif

	/* Bind to the local address */
	if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		MS_DBG_ERR("bind failed");
		return FALSE;
	}

	MS_DBG("bind success");
#endif
	g_communicate_sock = sock;

	return TRUE;
}

gpointer ms_thumb_agent_start_thread(gpointer data)
{
	MS_DBG("");
	int sockfd = -1;

    GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	/* Create and bind new TCP socket */
	if (!_ms_thumb_agent_prepare_tcp_socket(&sockfd)) {
		MS_DBG_ERR("Failed to create socket\n");
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
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_shutdown(g_udp_channel,  FALSE, NULL);
	g_io_channel_unref(channel);
	close(g_communicate_sock);

	g_main_loop_unref(g_thumb_agent_loop);

	return NULL;
}

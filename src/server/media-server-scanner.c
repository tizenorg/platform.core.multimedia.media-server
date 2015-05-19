/*
 *  Media Server
 *
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
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
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <glib.h>
#include <vconf.h>
#include <sys/stat.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-types.h"
#include "media-common-utils.h"
#include "media-server-dbg.h"
#include "media-server-socket.h"
#include "media-server-scanner.h"

#include <tzplatform_config.h>

#define MS_NO_REMAIN_TASK 0

#define MEDIA_SERVER_PATH tzplatform_mkpath(TZ_SYS_BIN,"media-scanner")

extern GMainLoop *mainloop;
extern GArray *owner_list;
GMutex *scanner_mutex;

static bool scanner_ready;
static int alarm_id;
static int receive_id;
static int child_pid;



static int _ms_check_remain_task(void)
{
	int remain_task;

	if (owner_list != NULL)
		remain_task = owner_list->len;
	else
		remain_task = MS_NO_REMAIN_TASK;

	return remain_task;
}

ms_db_status_type_t ms_check_scanning_status(void)
{
	int status;

	if(ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &status)) {
		if (status == VCONFKEY_FILEMANAGER_DB_UPDATING) {
			return MS_DB_UPDATING;
		}
	}

	return MS_DB_UPDATED;
}

static gboolean _ms_stop_scanner (gpointer user_data)
{
	int sockfd;
	int task_num;
	GIOChannel *src = user_data;

	g_mutex_lock(scanner_mutex);

	/* check status of scanner */
	/* If some task remain or scanner is running, scanner must not stop*/
	task_num = _ms_check_remain_task();
	if (task_num != MS_NO_REMAIN_TASK) {
		MS_DBG("[%d] task(s) remains", task_num);
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	}

	if (ms_check_scanning_status() == MS_DB_UPDATING) {
		MS_DBG("DB is updating");
		g_mutex_unlock(scanner_mutex);
		return TRUE;
	} else {
		MS_DBG("DB updating is not working");
	}

	/* stop media scanner */
	if (child_pid >0 ) {
		if (kill(child_pid, SIGKILL) < 0) {
			MS_DBG_ERR("kill failed : %s", strerror(errno));
			g_mutex_unlock(scanner_mutex);
			return TRUE;
		}
	}
	/* close socket */
	sockfd = g_io_channel_unix_get_fd(src);
	g_io_channel_shutdown(src, FALSE, NULL);
	g_io_channel_unref(src);
	close(sockfd);

	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), alarm_id));
	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), receive_id));

	return FALSE;
}


static void _ms_add_timeout(guint interval, GSourceFunc func, gpointer data)
{
	MS_DBG("");
	GSource *src;

	src = g_timeout_source_new_seconds(interval);
	g_source_set_callback(src, func, data, NULL);
	alarm_id = g_source_attach(src, g_main_loop_get_context (mainloop));
	g_source_unref(src);
}

int
ms_scanner_start(void)
{
	int pid;

	g_mutex_lock(scanner_mutex);

	if (child_pid > 0) {
		MS_DBG_ERR("media scanner is already started");
		g_mutex_unlock(scanner_mutex);
		return MS_MEDIA_ERR_NONE;
	}

	if((pid = fork()) < 0) {
		MS_DBG_ERR("Fork error\n");
	} else if (pid > 0) {
		/* parent process */
		/* wait until scanner is ready*/
		int ret = MS_MEDIA_ERR_NONE;
		int sockfd = -1;
		int err = -1;
		int n_reuse = 1;
#ifdef _USE_UDS_SOCKET_
		struct sockaddr_un serv_addr;
#else
		struct sockaddr_in serv_addr;
#endif
		unsigned int serv_addr_len = -1;
		int port = MS_SCAN_COMM_PORT;
		ms_comm_msg_s recv_msg;

		GSource *res_source = NULL;
		GIOChannel *res_channel = NULL;
		GMainContext *res_context = NULL;

		/*Create Socket*/
#ifdef _USE_UDS_SOCKET_
		ret = ms_ipc_create_server_socket(MS_PROTOCOL_UDP, MS_SCAN_COMM_PORT, S_IROTH | S_IWOTH | S_IXOTH, &sockfd);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_ipc_create_server_socket failed [%d]",ret);
			g_mutex_unlock(scanner_mutex);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
#else
		ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sockfd);
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_ipc_create_client_socket failed [%d]",ret);
			g_mutex_unlock(scanner_mutex);
			return MS_MEDIA_ERR_SOCKET_CONN;
		}
		/* set socket re-use */
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n_reuse, sizeof(n_reuse)) == -1) {
			MS_DBG_ERR("setsockopt failed: %s", strerror(errno));
			close(sockfd);
			g_mutex_unlock(scanner_mutex);
			return MS_MEDIA_ERR_SOCKET_INTERNAL;
		}
		/*Set server Address*/
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
		serv_addr.sin_port = htons(port);
		/* Bind to the local address */
		if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			MS_DBG_ERR("bind failed [%s]", strerror(errno));
			close(sockfd);
			g_mutex_unlock(scanner_mutex);
			return MS_MEDIA_ERR_SOCKET_BIND;
		}
#endif

		/*Receive Response*/
		serv_addr_len = sizeof(serv_addr);

		err = ms_ipc_wait_message(sockfd, &recv_msg, sizeof(recv_msg), &serv_addr, NULL, FALSE);
		if (err != MS_MEDIA_ERR_NONE) {
			ret = err;
			close(sockfd);
		} else {
			int scanner_status = recv_msg.msg_type;
			if (scanner_status == MS_MSG_SCANNER_READY) {
				MS_DBG("RECEIVE OK [%d] %d", recv_msg.msg_type, pid);
				scanner_ready = true;
				child_pid = pid;

				/* attach result receiving socket to mainloop */
				res_context = g_main_loop_get_context(mainloop);

				/* Create new channel to watch udp socket */
				res_channel = g_io_channel_unix_new(sockfd);
				res_source = g_io_create_watch(res_channel, G_IO_IN);

				/* Set callback to be called when socket is readable */
				g_source_set_callback(res_source, (GSourceFunc)ms_receive_message_from_scanner, NULL, NULL);
				receive_id = g_source_attach(res_source, res_context);
				g_source_unref(res_source);

				_ms_add_timeout(30, (GSourceFunc)_ms_stop_scanner, res_channel);

				ret = MS_MEDIA_ERR_NONE;
			} else {
				MS_DBG_ERR("Receive wrong message from scanner[%d]", scanner_status);
				close(sockfd);
				ret = MS_MEDIA_ERR_SOCKET_RECEIVE;
			}
		}

		g_mutex_unlock(scanner_mutex);

		return ret;
		/* attach socket receive message callback */
	} else if(pid == 0) {
		/* child process */
		MS_DBG_ERR("CHILD PROCESS");
		MS_DBG("EXECUTE MEDIA SCANNER");
		execl(MEDIA_SERVER_PATH, "media-scanner", NULL);
		g_mutex_unlock(scanner_mutex);
	}

	return MS_MEDIA_ERR_NONE;
}

bool ms_get_scanner_status(void)
{
	return scanner_ready;
}

void ms_reset_scanner_status(void)
{
	child_pid = 0;
	scanner_ready = false;

	g_mutex_unlock(scanner_mutex);
}

int ms_get_scanner_pid(void)
{
	return child_pid;
}

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
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <glib.h>
#include <vconf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
GMutex scanner_mutex;

static bool scanner_ready;
static int alarm_id;
static int child_pid;
static int receive_id;

int fifo_fd;

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
	int task_num = MS_NO_REMAIN_TASK;

	g_mutex_lock(&scanner_mutex);

	/* check status of scanner */
	/* If some task remain or scanner is running, scanner must not stop*/
	task_num = _ms_check_remain_task();
	if (task_num != MS_NO_REMAIN_TASK) {
		MS_DBG("[%d] task(s) remains", task_num);
		g_mutex_unlock(&scanner_mutex);
		return TRUE;
	}

	if (ms_check_scanning_status() == MS_DB_UPDATING) {
		MS_DBG("DB is updating");
		g_mutex_unlock(&scanner_mutex);
		return TRUE;
	} else {
		MS_DBG("DB updating is not working");
	}

	/* stop media scanner */
	if (child_pid >0 ) {
		if (kill(child_pid, SIGKILL) < 0) {
			MS_DBG_STRERROR("kill failed");
			g_mutex_unlock(&scanner_mutex);
			return TRUE;
		}
	}
	MS_DBG("KILL SCANNER");

	/* close & delete FIFO */
	int res_fd = -1;
	GIOChannel *res_channel = user_data;

	res_fd = g_io_channel_unix_get_fd(res_channel);
	close(res_fd);

	unlink(MS_SCANNER_FIFO_PATH_RES);
	unlink(MS_SCANNER_FIFO_PATH_REQ);

//	ms_reset_scanner_status();

	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), alarm_id));
	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), receive_id));

	return FALSE;
}

void ms_cleanup_scanner(void)
{
	g_mutex_lock(&scanner_mutex);
	MS_DBG_ERR("_ms_cleanup_scanner START");

	close(fifo_fd);
	MS_DBG_ERR("close fd[%d]", fifo_fd);

	unlink(MS_SCANNER_FIFO_PATH_RES);
	unlink(MS_SCANNER_FIFO_PATH_REQ);

	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), alarm_id));
	g_source_destroy(g_main_context_find_source_by_id(g_main_loop_get_context (mainloop), receive_id));

	g_mutex_unlock(&scanner_mutex);

	MS_DBG_ERR("_ms_cleanup_scanner END");

	return;
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

int ms_scanner_start(void)
{
	int pid;

	g_mutex_lock(&scanner_mutex);

	if (child_pid > 0) {
		MS_DBG_ERR("media scanner is already started");
		g_mutex_unlock(&scanner_mutex);
		return MS_MEDIA_ERR_NONE;
	}

	if((pid = fork()) < 0) {
		MS_DBG_ERR("Fork error\n");
		g_mutex_unlock(&scanner_mutex);
	} else if (pid > 0) {
		/* parent process */
		/* wait until scanner is ready*/
		int ret = MS_MEDIA_ERR_NONE;
		int err = -1;
		int fd = -1;
		ms_comm_msg_s recv_msg;
		int scanner_status = -1;

		err = unlink(MS_SCANNER_FIFO_PATH_RES);
		if (err !=0) {
			MS_DBG_STRERROR("unlink failed");
		}
		err = mkfifo(MS_SCANNER_FIFO_PATH_RES, MS_SCANNER_FIFO_MODE);
		if (err !=0) {
			MS_DBG_STRERROR("mkfifo failed");
			return MS_MEDIA_ERR_INTERNAL;
		}

		fd = open(MS_SCANNER_FIFO_PATH_RES, O_RDWR);
		if (fd < 0) {
			MS_DBG_STRERROR("fifo open failed");
			return MS_MEDIA_ERR_FILE_OPEN_FAIL;
		}

		/* read() is blocked until media scanner sends message */
		err = read(fd, &recv_msg, sizeof(recv_msg));
		if (err < 0) {
			MS_DBG_STRERROR("fifo read failed");
			close(fd);
			return MS_MEDIA_ERR_FILE_READ_FAIL;
		}

		scanner_status = recv_msg.msg_type;
		if (scanner_status == MS_MSG_SCANNER_READY) {
			GSource *res_source = NULL;
			GIOChannel *res_channel = NULL;
			GMainContext *res_context = NULL;

			MS_DBG_ERR("SCANNER is ready");
			scanner_ready = true;
			child_pid = pid;
			/* attach result receiving socket to mainloop */
			res_context = g_main_loop_get_context(mainloop);

			/* Create new channel to watch udp socket */
			res_channel = g_io_channel_unix_new(fd);
			res_source = g_io_create_watch(res_channel, G_IO_IN);

			/* Set callback to be called when socket is readable */
			g_source_set_callback(res_source, (GSourceFunc)ms_receive_message_from_scanner, NULL, NULL);
			receive_id = g_source_attach(res_source, res_context);
			g_source_unref(res_source);

			_ms_add_timeout(30, (GSourceFunc)_ms_stop_scanner, res_channel);

			ret = MS_MEDIA_ERR_NONE;
		} else {
			MS_DBG_ERR("SCANNER is not ready");
			ret = MS_MEDIA_ERR_SCANNER_NOT_READY;
			close(fd);
		}

		fifo_fd = fd;

		g_mutex_unlock(&scanner_mutex);

		return ret;
		/* attach socket receive message callback */
	} else if(pid == 0) {
		/* child process */
		MS_DBG_ERR("CHILD PROCESS");
		MS_DBG("EXECUTE MEDIA SCANNER");
		execl(MEDIA_SERVER_PATH, "media-scanner", NULL);
		g_mutex_unlock(&scanner_mutex);
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

	g_mutex_unlock(&scanner_mutex);
}

int ms_get_scanner_pid(void)
{
	return child_pid;
}

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
 * @file		media-server-main.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#include <dirent.h>
#include <vconf.h>
#include <heynoti.h>

#include "media-common-utils.h"
#include "media-common-drm.h"
#include "media-common-external-storage.h"

#include "media-util.h"
#include "media-scanner-dbg.h"
#include "media-scanner-db-svc.h"
#include "media-scanner-scan.h"
#include "media-scanner-socket.h"

#define APP_NAME "media-scanner"

static int heynoti_id;
extern int mmc_state;

extern GAsyncQueue *storage_queue;
extern GAsyncQueue *scan_queue;
extern GAsyncQueue *reg_queue;
extern GMutex *db_mutex;
extern GMutex *receive_mutex;
bool power_off; /*If this is TRUE, poweroff notification received*/

static GMainLoop *scanner_mainloop = NULL;

bool check_process(void)
{
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
	bool ret = false;
	int find_pid = 0;
	pid_t current_pid = 0;

	current_pid = getpid();

	pdir = opendir("/proc");
	if (pdir == NULL) {
		MSC_DBG_ERR("err: NO_DIR\n");
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
				MSC_DBG_ERR("fgets failed");
			fclose(fp);

			if (strstr(buff, APP_NAME)) {
				find_pid = atoi(pinfo.d_name);
				if (find_pid == current_pid)
					ret = true;
				else {
					ret = false;
					break;
				}
			}
		} else {
			MSC_DBG_ERR("Can't read file [%s]", path);
		}
	}

	closedir(pdir);

	return ret;
}

void init_process()
{

}

static void _power_off_cb(void* data)
{
	ms_comm_msg_s *scan_data;
	ms_comm_msg_s *reg_data;

	MSC_DBG_INFO("++++++++++++++++++++++++++++++++++++++");
	MSC_DBG_INFO("POWER OFF");
	MSC_DBG_INFO("++++++++++++++++++++++++++++++++++++++");

	power_off = true;

	if (scan_queue) {
		/*notify to scannig thread*/
		MS_MALLOC(scan_data, sizeof(ms_comm_msg_s));
		scan_data->pid = POWEROFF;
		g_async_queue_push(scan_queue, GINT_TO_POINTER(scan_data));
	}

	if (reg_queue) {
		/*notify to register thread*/
		MS_MALLOC(reg_data, sizeof(ms_comm_msg_s));
		reg_data->pid = POWEROFF;
		g_async_queue_push(reg_queue, GINT_TO_POINTER(reg_data));
	}

	if (storage_queue) {
		/*notify to register thread*/
		MS_MALLOC(reg_data, sizeof(ms_comm_msg_s));
		reg_data->pid = POWEROFF;
		g_async_queue_push(storage_queue, GINT_TO_POINTER(reg_data));
	}

	if (g_main_loop_is_running(scanner_mainloop)) g_main_loop_quit(scanner_mainloop);
}

void
_msc_mmc_vconf_cb(void *data)
{
	int status = 0;
/*
	ms_comm_msg_s *scan_msg;
	ms_dir_scan_type_t scan_type = MS_SCAN_PART;
*/
	if (!ms_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status)) {
		MSC_DBG_ERR("Get VCONFKEY_SYSMAN_MMC_STATUS failed.");
	}

	MSC_DBG_INFO("VCONFKEY_SYSMAN_MMC_STATUS :%d", status);

	mmc_state = status;
/*
	MS_MALLOC(scan_msg, sizeof(ms_comm_msg_s));

	if (mmc_state == VCONFKEY_SYSMAN_MMC_REMOVED ||
		mmc_state == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {
		scan_type = MS_SCAN_INVALID;
	} else if (mmc_state == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		scan_type = ms_get_mmc_state();
	}

	switch (scan_type) {
		case MS_SCAN_ALL:
			scan_msg->msg_type = MS_MSG_STORAGE_ALL;
			break;
		case MS_SCAN_PART:
			scan_msg->msg_type = MS_MSG_STORAGE_PARTIAL;
			break;
		case MS_SCAN_INVALID:
			scan_msg->msg_type = MS_MSG_STORAGE_INVALID;
			break;
	}

	scan_msg->pid = 0;
	scan_msg->msg_size = strlen(MEDIA_ROOT_PATH_SDCARD);
	ms_strcopy(scan_msg->msg, scan_msg->msg_size+1, "%s", MEDIA_ROOT_PATH_SDCARD);

	MSC_DBG_INFO("ms_get_mmc_state is %d", scan_msg->msg_type);

	g_async_queue_push(storage_queue, GINT_TO_POINTER(scan_msg));
*/
	return;
}

int main(int argc, char **argv)
{
	GThread *storage_scan_thread = NULL;
	GThread *scan_thread = NULL;
	GThread *register_thread = NULL;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	int sockfd = -1;
	int err;

#if 0 /* temporary */
	check_result = check_process();
	if (check_result == false)
		exit(0);
#endif
	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	/*Init main loop*/
	scanner_mainloop = g_main_loop_new(NULL, FALSE);

	/*heynoti for power off*/
	if ((heynoti_id = heynoti_init()) <0) {
		MSC_DBG_INFO("heynoti_init failed");
	} else {
		err = heynoti_subscribe(heynoti_id, POWEROFF_NOTI_NAME, _power_off_cb, NULL);
		if (err < 0)
			MSC_DBG_INFO("heynoti_subscribe failed");

		err = heynoti_attach_handler(heynoti_id);
		if (err < 0)
			MSC_DBG_INFO("heynoti_attach_handler failed");
	}

	/*load functions from plusin(s)*/
	err = msc_load_functions();
	if (err != MS_MEDIA_ERR_NONE) {
		MSC_DBG_ERR("function load failed");
		exit(0);
	}

	/*Init for register file*/
	/*These are a communicator for thread*/
	if (!scan_queue) scan_queue = g_async_queue_new();
	if (!reg_queue) reg_queue = g_async_queue_new();
	if (!storage_queue) storage_queue = g_async_queue_new();

	/*Init mutex variable*/
	if (!db_mutex) db_mutex = g_mutex_new();

	/*prepare socket*/
	/* Create and bind new UDP socket */
	if (ms_ipc_create_server_socket(MS_PROTOCOL_TCP, MS_SCAN_DAEMON_PORT, &sockfd)
		!= MS_MEDIA_ERR_NONE) {
		MSC_DBG_ERR("Failed to create socket\n");
		exit(0);
	} else {
		context = g_main_loop_get_context(scanner_mainloop);

		/* Create new channel to watch udp socket */
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_create_watch(channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)msc_receive_request, NULL, NULL);
		g_source_attach(source, context);
		g_source_unref(source);
	}

	/*create each threads*/
	storage_scan_thread = g_thread_new("storage_scan_thread", (GThreadFunc)msc_storage_scan_thread, NULL);
	scan_thread = g_thread_new("scanner_thread", (GThreadFunc)msc_directory_scan_thread, NULL);
	register_thread = g_thread_new("register_thread", (GThreadFunc)msc_register_thread, NULL);

	/*set vconf callback function*/
	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_STATUS, (vconf_callback_fn) _msc_mmc_vconf_cb, NULL);
	if (err == -1)
		MSC_DBG_ERR("add call back function for event %s fails", VCONFKEY_SYSMAN_MMC_STATUS);

	if (ms_is_mmc_inserted()) {
		mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;
	}

	MSC_DBG_INFO("scanner is ready");

	msc_send_ready();

	MSC_DBG_INFO("*****************************************");
	MSC_DBG_INFO("*** Scanner is running ***");
	MSC_DBG_INFO("*****************************************");

	g_main_loop_run(scanner_mainloop);

	g_thread_join (scan_thread);
	g_thread_join (register_thread);
	g_thread_join (storage_scan_thread);

	if (power_off) {
		g_io_channel_shutdown(channel, FALSE, NULL);
		g_io_channel_unref(channel);
	}

	heynoti_unsubscribe(heynoti_id, POWEROFF_NOTI_NAME, _power_off_cb);
	heynoti_close(heynoti_id);

	if (scan_queue) g_async_queue_unref(scan_queue);
	if (reg_queue) g_async_queue_unref(reg_queue);
	if (storage_queue) g_async_queue_unref(storage_queue);

	/*Clear db mutex variable*/
	if (db_mutex) g_mutex_free (db_mutex);

	/*close socket*/
	close(sockfd);

	/*unload functions*/
	msc_unload_functions();

	MSC_DBG_INFO("SCANNER IS END");

	exit(0);
}


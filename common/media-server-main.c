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
#include <vconf.h>
#include <heynoti.h>
#include <media-util-register.h>

#include "media-server-utils.h"
#include "media-server-external-storage.h"
#include "media-server-db-svc.h"
#include "media-server-inotify.h"
#include "media-server-scan.h"
#include "media-server-socket.h"
#include "media-server-drm.h"
#include "media-server-dbus.h"

#define APP_NAME "media-server"

static int heynoti_id;

extern GAsyncQueue *scan_queue;
extern GAsyncQueue* ret_queue;
extern GMutex *db_mutex;
extern GMutex *list_mutex;
extern GMutex *queue_mutex;
extern GArray *reg_list;
extern int mmc_state;
extern bool power_off; /*If this is TRUE, poweroff notification received*/
static GMainLoop *mainloop = NULL;

bool check_process(pid_t current_pid)
{
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
	bool ret = false;
	int find_pid = 0;

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
			fgets(buff, 128, fp);
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
			MS_DBG_ERR("Can't read file [%s]", path);
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
	ms_scan_data_t *scan_data;

	MS_DBG("++++++++++++++++++++++++++++++++++++++");
	MS_DBG("POWER OFF");
	MS_DBG("++++++++++++++++++++++++++++++++++++++");

	power_off = true;

	if (scan_queue) {
		/*notify to scannig thread*/
		scan_data = malloc(sizeof(ms_scan_data_t));
		scan_data->path = NULL;
		scan_data->scan_type = POWEROFF;
		g_async_queue_push(scan_queue, GINT_TO_POINTER(scan_data));

		/*notify to Inotify thread*/
		mkdir(POWEROFF_DIR_PATH, 0777);
		rmdir(POWEROFF_DIR_PATH);
	}

	if (g_main_loop_is_running(mainloop)) g_main_loop_quit(mainloop);
}

static bool _db_clear(void** handle)
{
	int err;
	int db_status;
	bool need_db_create = false;

	/*update just valid type*/
	err = ms_invalidate_all_items(handle, MS_STORATE_EXTERNAL);
	if (err != MS_ERR_NONE)
		MS_DBG_ERR("ms_change_valid_type fail");

	ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &db_status);
	MS_DBG("finish_phone_init_data  db = %d", db_status);

	if (db_status == VCONFKEY_FILEMANAGER_DB_UPDATING) {
		need_db_create = true;

		err = ms_invalidate_all_items(handle, MS_STORAGE_INTERNAL);
		if (err != MS_ERR_NONE)
			MS_DBG_ERR("ms_change_valid_type fail");
	}

	ms_set_db_status(MS_DB_UPDATED);

	return need_db_create;
}

int main(int argc, char **argv)
{
	GThread *inoti_tid = NULL;
	GThread *scan_tid = NULL;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;

	pid_t current_pid = 0;
	int sockfd = -1;
	int err;
	bool check_result = false;
	bool need_db_create;
	void **handle = NULL;

	ms_scan_data_t *phone_scan_data;
	ms_scan_data_t *mmc_scan_data;

	current_pid = getpid();
	check_result = check_process(current_pid);
	if (check_result == false)
		exit(0);

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	/*Init main loop*/
	mainloop = g_main_loop_new(NULL, FALSE);

	/*inotify setup */
	ms_inoti_init();

	/*heynoti for power off*/
	if ((heynoti_id = heynoti_init()) <0) {
		MS_DBG("heynoti_init failed");
	} else {
		err = heynoti_subscribe(heynoti_id, POWEROFF_NOTI_NAME, _power_off_cb, NULL);
		if (err < 0)
			MS_DBG("heynoti_subscribe failed");

		err = heynoti_attach_handler(heynoti_id);
		if (err < 0)
			MS_DBG("heynoti_attach_handler failed");
	}

	/*load functions from plusin(s)*/
	ms_load_functions();

	/*Init db mutex variable*/
	if (!db_mutex) db_mutex = g_mutex_new();

	/*Init for register file*/
	if (!list_mutex) list_mutex = g_mutex_new();
	if (!queue_mutex) queue_mutex = g_mutex_new();
	if (!reg_list) reg_list = g_array_new(TRUE, TRUE, sizeof(char*));

	/*connect to media db, if conneting is failed, db updating is stopped*/
	ms_connect_db(&handle);

	/*clear previous data of sdcard on media database and check db status for updating*/
	need_db_create = _db_clear(handle);

	ms_dbus_init();

	ms_inoti_add_watch(MS_DB_UPDATE_NOTI_PATH);
	ms_inoti_add_watch_all_directory(MS_STORAGE_INTERNAL);

	/*These are a communicator for thread*/
	if (!scan_queue) scan_queue = g_async_queue_new();
	if (!ret_queue) ret_queue = g_async_queue_new();

	/*prepare socket*/
	/* Create and bind new UDP socket */
	if (!ms_prepare_socket(&sockfd)) {
		MS_DBG_ERR("Failed to create socket\n");
	} else {
		context = g_main_loop_get_context(mainloop);

		/* Create new channel to watch udp socket */
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_create_watch(channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)ms_read_socket, handle, NULL);
		g_source_attach(source, context);
	}

	/*create each threads*/
	inoti_tid = g_thread_create((GThreadFunc) ms_inoti_thread, NULL, TRUE, NULL);
	scan_tid = g_thread_create((GThreadFunc) ms_scan_thread, NULL, TRUE, NULL);

	/*set vconf callback function*/
	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				     (vconf_callback_fn) ms_mmc_vconf_cb, NULL);
	if (err == -1)
		MS_DBG_ERR("add call back function for event %s fails", VCONFKEY_SYSMAN_MMC_STATUS);

	phone_scan_data = malloc(sizeof(ms_scan_data_t));
	mmc_scan_data = malloc(sizeof(ms_scan_data_t));
	MS_DBG("*********************************************************");
	MS_DBG("*** Begin to check tables of file manager in database ***");
	MS_DBG("*********************************************************");

	if (need_db_create) {
		/*insert records*/
		phone_scan_data->path = strdup(MS_ROOT_PATH_INTERNAL);
		phone_scan_data->storage_type = MS_STORAGE_INTERNAL;
		phone_scan_data->scan_type = MS_SCAN_ALL;
		/*push data to fex_dir_scan_cb */
		g_async_queue_push(scan_queue, GINT_TO_POINTER(phone_scan_data));
	}

	if (ms_is_mmc_inserted()) {
		mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;

		if (!ms_drm_insert_ext_memory())
			MS_DBG_ERR("ms_drm_insert_ext_memory failed");

		ms_make_default_path_mmc();
		ms_inoti_add_watch_all_directory(MS_STORATE_EXTERNAL);

		mmc_scan_data->path = strdup(MS_ROOT_PATH_EXTERNAL);
		mmc_scan_data->scan_type = ms_get_mmc_state();
		mmc_scan_data->storage_type = MS_STORATE_EXTERNAL;

		g_async_queue_push(scan_queue, GINT_TO_POINTER(mmc_scan_data));
	}

	/*Active flush */
	malloc_trim(0);

	MS_DBG("*****************************************");
	MS_DBG("*** Server of File Manager is running ***");
	MS_DBG("*****************************************");

	g_main_loop_run(mainloop);

	g_thread_join(inoti_tid);
	g_thread_join(scan_tid);

	/*close an IO channel*/
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);

	heynoti_unsubscribe(heynoti_id, POWEROFF_NOTI_NAME, _power_off_cb);
	heynoti_close(heynoti_id);

	if (scan_queue) g_async_queue_unref(scan_queue);
	if (ret_queue) g_async_queue_unref(ret_queue);
	if (reg_list) g_array_free(reg_list, true);

	/***********
	**remove call back functions
	************/
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				 (vconf_callback_fn) ms_mmc_vconf_cb);

	/*Clear db mutex variable*/
	if (db_mutex) g_mutex_free (db_mutex);

	/*disconnect form media db*/
	if (handle) ms_disconnect_db(&handle);

	/*close socket*/
	close(sockfd);

	/*unload functions*/
	ms_unload_functions();

	exit(0);
}

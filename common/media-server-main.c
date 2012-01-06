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
 * @file		media-server-mainl.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <vconf.h>

#include "media-server-common.h"
#include "media-server-hibernation.h"
#include "media-server-inotify.h"
#include "media-server-scan.h"
#include "media-server-socket.h"
#ifdef THUMB_THREAD
#include "media-server-thumb.h"
#endif

#include <heynoti.h>

#define APP_NAME "media-server"

#ifdef _USE_HIB
int hib_fd = 0;
#endif
extern GAsyncQueue *scan_queue;
extern GAsyncQueue* ret_queue;
extern GMutex *db_mutex;
extern GMutex *list_mutex;
extern GMutex *queue_mutex;
extern GArray *reg_list;

bool check_process(pid_t current_pid)
{
	MS_DBG_START();
	DIR *pdir;
	struct dirent pinfo;
	struct dirent *result = NULL;
	bool ret = false;
	int find_pid = 0;

	pdir = opendir("/proc");
	if (pdir == NULL) {
		MS_DBG("err: NO_DIR\n");
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
				MS_DBG("pinfo->d_name : %s", pinfo.d_name);
				find_pid = atoi(pinfo.d_name);
				MS_DBG(" find_pid : %d", find_pid);
				MS_DBG(" current_pid : %d", current_pid);
				if (find_pid == current_pid)
					ret = true;
				else {
					ret = false;
					break;
				}
			}
		} else {
			MS_DBG("Can't read file [%s]", path);
		}
	}

	closedir(pdir);

	MS_DBG_END();

	return ret;
}

void init_process()
{

}

static bool _db_clear(void)
	{
	int err;
	int db_status;
	int usb_status;
	bool need_db_create = false;

	/*connect to media db, if conneting is failed, db updating is stopped*/
	ms_media_db_open();

	/*update just valid type*/
	err = ms_change_valid_type(MS_MMC, false);
	if (err != MS_ERR_NONE)
		MS_DBG("ms_change_valid_type fail");

	ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &db_status);
	ms_config_get_int(MS_USB_MODE_KEY, &usb_status);

	MS_DBG("finish_phone_init_data  db = %d", db_status);
	MS_DBG("finish_phone_init_data usb = %d", usb_status);

	if (db_status == VCONFKEY_FILEMANAGER_DB_UPDATING
		|| usb_status == MS_VCONFKEY_MASS_STORAGE_MODE) {

		need_db_create = true;

		err = ms_change_valid_type(MS_PHONE, false);
		if (err != MS_ERR_NONE)
			MS_DBG("ms_change_valid_type fail");
	}

	ms_set_db_status(MS_DB_UPDATED);

	/*disconnect form media db*/
	ms_media_db_close();

	return need_db_create;
}

int main(int argc, char **argv)
{
	GThread *inoti_tid;
	GThread *scan_tid;
	GThread *scoket_tid;

	GMainLoop *mainloop = NULL;
	pid_t current_pid = 0;
	bool check_result = false;
	int err;
	bool need_db_create;

	current_pid = getpid();
	check_result = check_process(current_pid);
	if (check_result == false)
		exit(0);

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	/*Init db mutex variable*/
	if (!db_mutex)
		db_mutex = g_mutex_new();

	need_db_create = _db_clear();
	
#ifdef _USE_HIB
	_hibernation_initialize();
#endif

	MS_DBG("MEDIA SERVER START");

	if (!scan_queue)
		scan_queue = g_async_queue_new();
	if (!ret_queue)
		ret_queue = g_async_queue_new();

	/*Init for register file*/
	if (!list_mutex)
		list_mutex = g_mutex_new();
	if (!queue_mutex)
		queue_mutex = g_mutex_new();
	if (!reg_list)
		reg_list = g_array_new(TRUE, TRUE, sizeof(char*));

	ms_inoti_init();

	ms_inoti_add_watch(MS_DB_UPDATE_NOTI_PATH);

	/*inotify setup */
	inoti_tid = g_thread_create((GThreadFunc) ms_inoti_thread, NULL, FALSE, NULL);
	scan_tid = g_thread_create((GThreadFunc) ms_scan_thread, NULL, TRUE, NULL);
	scoket_tid  = g_thread_create((GThreadFunc) ms_socket_thread, NULL, TRUE, NULL);

	mainloop = g_main_loop_new(NULL, FALSE);

	ms_start(need_db_create);

	MS_DBG("*****************************************");
	MS_DBG("*** Server of File Manager is running ***");
	MS_DBG("*****************************************");

	g_main_loop_run(mainloop);

	/*free all associated memory */
	g_main_loop_unref(mainloop);

	if (scan_queue)
		g_async_queue_unref(scan_queue);

	if (ret_queue)
		g_async_queue_unref(ret_queue);

	if(reg_list)
		g_array_free(reg_list, true);

	ms_end();

#ifdef _USE_HIB
	_hibernation_fianalize();
#endif

	exit(0);
}

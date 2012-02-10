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
#include <drm-service.h>

#include "media-server-utils.h"
#include "media-server-external-storage.h"
#include "media-server-db-svc.h"
#include "media-server-inotify.h"
#include "media-server-scan.h"
#include "media-server-socket.h"

#define APP_NAME "media-server"

extern GAsyncQueue *scan_queue;
extern GAsyncQueue* ret_queue;
extern GMutex *db_mutex;
extern GMutex *list_mutex;
extern GMutex *queue_mutex;
extern GArray *reg_list;
extern int mmc_state;

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
	MediaSvcHandle *handle = NULL;

	/*connect to media db, if conneting is failed, db updating is stopped*/
	ms_media_db_open(&handle);

	/*update just valid type*/
	err = ms_change_valid_type(handle, MS_MMC, false);
	if (err != MS_ERR_NONE)
		MS_DBG("ms_change_valid_type fail");

	ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &db_status);
	ms_config_get_int(MS_USB_MODE_KEY, &usb_status);

	MS_DBG("finish_phone_init_data  db = %d", db_status);
	MS_DBG("finish_phone_init_data usb = %d", usb_status);

	if (db_status == VCONFKEY_FILEMANAGER_DB_UPDATING
		|| usb_status == MS_VCONFKEY_MASS_STORAGE_MODE) {

		need_db_create = true;

		err = ms_change_valid_type(handle, MS_PHONE, false);
		if (err != MS_ERR_NONE)
			MS_DBG("ms_change_valid_type fail");
	}

	ms_set_db_status(MS_DB_UPDATED);

	/*disconnect form media db*/
	ms_media_db_close(handle);

	return need_db_create;
}

int main(int argc, char **argv)
{
	GThread *inoti_tid;
	GThread *scan_tid;
	GThread *scoket_tid;

	GMainLoop *mainloop = NULL;
	pid_t current_pid = 0;
	int err;
	bool check_result = false;
	bool need_db_create;

	ms_scan_data_t *phone_scan_data;
	ms_scan_data_t *mmc_scan_data;

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

	/*clear previous data of sdcard on media database and check db status for updating*/
	need_db_create = _db_clear();
	
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

	/*inotify setup */
	ms_inoti_init();

	ms_inoti_add_watch(MS_DB_UPDATE_NOTI_PATH);

	/*create each threads*/
	inoti_tid = g_thread_create((GThreadFunc) ms_inoti_thread, NULL, FALSE, NULL);
	scan_tid = g_thread_create((GThreadFunc) ms_scan_thread, NULL, TRUE, NULL);
	scoket_tid  = g_thread_create((GThreadFunc) ms_socket_thread, NULL, TRUE, NULL);

	/*Init main loop*/
	mainloop = g_main_loop_new(NULL, FALSE);

	/*set vconf callback function*/
	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				     (vconf_callback_fn) ms_mmc_vconf_cb, NULL);
	if (err == -1)
		MS_DBG("add call back function for event %s fails", VCONFKEY_SYSMAN_MMC_STATUS);

	err = vconf_notify_key_changed(VCONFKEY_USB_STORAGE_STATUS,
				     (vconf_callback_fn) ms_usb_vconf_cb, NULL);
	if (err == -1)
		MS_DBG("add call back function for event %s fails", VCONFKEY_USB_STORAGE_STATUS);

	phone_scan_data = malloc(sizeof(ms_scan_data_t));
	mmc_scan_data = malloc(sizeof(ms_scan_data_t));
	MS_DBG("*********************************************************");
	MS_DBG("*** Begin to check tables of file manager in database ***");
	MS_DBG("*********************************************************");

	phone_scan_data->db_type = MS_PHONE;

	if (need_db_create) {
		/*insert records*/
		MS_DBG("Create DB");
		phone_scan_data->scan_type = MS_SCAN_ALL;
	} else {
		MS_DBG("JUST ADD WATCH");
		phone_scan_data->scan_type = MS_SCAN_NONE;
	}

	/*push data to fex_dir_scan_cb */
	g_async_queue_push(scan_queue, GINT_TO_POINTER(phone_scan_data));

	if (ms_is_mmc_inserted()) {
		mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;

		if (drm_svc_insert_ext_memory() == DRM_RESULT_SUCCESS)
			MS_DBG
			    ("fex_db_service_init : drm_svc_insert_ext_memory OK");

		ms_make_default_path_mmc();

		mmc_scan_data->scan_type = ms_get_mmc_state();
		mmc_scan_data->db_type = MS_MMC;

		MS_DBG("ms_get_mmc_state is %d", mmc_scan_data->scan_type);

		g_async_queue_push(scan_queue, GINT_TO_POINTER(mmc_scan_data));
	}

	malloc_trim(0);

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

	/***********
	**remove call back functions
	************/
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				 (vconf_callback_fn) ms_mmc_vconf_cb);
	vconf_ignore_key_changed(VCONFKEY_USB_STORAGE_STATUS,
				 (vconf_callback_fn) ms_usb_vconf_cb);

	/*Clear db mutex variable*/
	g_mutex_free (db_mutex);

	exit(0);
}

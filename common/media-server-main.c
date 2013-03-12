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

#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <vconf.h>
#include <heynoti.h>

#include "media-util.h"
#include "media-server-dbg.h"
#include "media-server-utils.h"
#include "media-server-external-storage.h"
#include "media-server-db-svc.h"
#include "media-server-inotify.h"
#include "media-server-socket.h"
#include "media-server-db.h"
#include "media-server-drm.h"
#include "media-server-dbus.h"
#include "media-server-thumb.h"
#include "media-server-scanner.h"

#define APP_NAME "media-server"

extern GMutex *scanner_mutex;
extern GMutex *db_mutex;
extern int mmc_state;

#if MS_INOTI_ENABLE
extern GAsyncQueue* ret_queue;
extern GMutex *list_mutex;
extern GMutex *queue_mutex;
extern GArray *reg_list;
extern bool power_off; /*If this is TRUE, poweroff notification received*/
#endif
GMainLoop *mainloop = NULL;

bool check_process()
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
	MS_DBG("++++++++++++++++++++++++++++++++++++++");
	MS_DBG("POWER OFF");
	MS_DBG("++++++++++++++++++++++++++++++++++++++");
#if MS_INOTI_ENABLE
	power_off = true;
#endif
	/*Quit Thumbnail Thread*/
	GMainLoop *thumb_mainloop = ms_get_thumb_thread_mainloop();
	if (thumb_mainloop && g_main_is_running(thumb_mainloop)) {
		g_main_loop_quit(thumb_mainloop);
	}

	/*Quit DB Thread*/
	GMainLoop *db_mainloop = ms_db_get_mainloop();
	if(db_mainloop && g_main_loop_is_running(db_mainloop)) {
		g_main_loop_quit(db_mainloop);
	}

	/*Quit Main Thread*/
	if (mainloop && g_main_loop_is_running(mainloop)) {
		g_main_loop_quit(mainloop);
	}

	return;
}

static bool _db_clear(void** handle)
{
	int err;
	int db_status;
	bool need_db_create = false;

	/*update just valid type*/
	err = ms_invalidate_all_items(handle, MS_STORAGE_EXTERNAL);
	if (err != MS_MEDIA_ERR_NONE)
		MS_DBG_ERR("ms_change_valid_type fail");

	ms_config_get_int(MS_SCAN_STATUS_INTERNAL, &db_status);
	MS_DBG("finish_phone_init_data  db = %d", db_status);

	if (db_status == P_VCONF_SCAN_DOING) {
		need_db_create = true;

		err = ms_invalidate_all_items(handle, MS_STORAGE_INTERNAL);
		if (err != MS_MEDIA_ERR_NONE)
			MS_DBG_ERR("ms_change_valid_type fail");
	}

//	ms_set_db_status(MS_DB_UPDATED);

	return need_db_create;
}

void _ms_signal_handler(int n)
{
	MS_DBG("Receive SIGNAL");
	int stat, pid, thumb_pid;
	int scanner_pid;

	thumb_pid = ms_thumb_get_server_pid();
	MS_DBG("Thumbnail server pid : %d", thumb_pid);

	scanner_pid = ms_get_scanner_pid();

	pid = waitpid(-1, &stat, WNOHANG);
	/* check pid of child process of thumbnail thread */
	MS_DBG("[PID %d] signal ID %d", pid, n);

	if (pid == thumb_pid) {
		MS_DBG("Thumbnail server is dead");
		ms_thumb_reset_server_status();
	} else if (pid == scanner_pid) {
		MS_DBG("Scanner is dead");
		ms_reset_scanner_status();
	} else if (pid == -1) {
		MS_DBG("%s", strerror(errno));
	}

	if (WIFEXITED(stat)) {
		MS_DBG("normal termination , exit status : %d", WEXITSTATUS(stat));
	} else if (WIFSIGNALED(stat)) {
		MS_DBG("abnormal termination , signal number : %d", WTERMSIG(stat));
	} else if (WIFSTOPPED(stat)) {
		MS_DBG("child process is stoped, signal number : %d", WSTOPSIG(stat));
	}

	return;
}

static void _ms_new_global_variable(void)
{
#if MS_INOTI_ENABLE
	/*Init for register file*/
	if (!list_mutex) list_mutex = g_mutex_new();
	if (!queue_mutex) queue_mutex = g_mutex_new();
	if (!reg_list) reg_list = g_array_new(TRUE, TRUE, sizeof(char*));
	/*These are a communicator for thread*/
	if (!ret_queue) ret_queue = g_async_queue_new();
#endif
	/*Init mutex variable*/
	if (!db_mutex) db_mutex = g_mutex_new();

	/*media scanner stop/start mutex*/
	if (!scanner_mutex) scanner_mutex = g_mutex_new();
}

static void _ms_free_global_variable(void)
{
#if MS_INOTI_ENABLE
	if (list_mutex) g_mutex_free(list_mutex);
	if (queue_mutex)g_mutex_free(queue_mutex);
	if (reg_list) g_array_free(reg_list, true);
	if (ret_queue) g_async_queue_unref(ret_queue);
#endif
	/*Clear mutex variable*/
	if (db_mutex) g_mutex_free (db_mutex);

	if (scanner_mutex) g_mutex_free(scanner_mutex);
}

int main(int argc, char **argv)
{
#if MS_INOTI_ENABLE
	GThread *inoti_thread = NULL;
#endif
	GThread *db_thread = NULL;
	GThread *thumb_thread = NULL;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int err;
	int heynoti_id;
	bool check_result = false;
	bool need_db_create;
	void **handle = NULL;
	struct sigaction sigset;

	check_result = check_process();
	if (check_result == false)
		exit(0);

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	/*Init main loop*/
	mainloop = g_main_loop_new(NULL, FALSE);
#if MS_INOTI_ENABLE
	/*inotify setup */
	ms_inoti_init();
#endif
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
	err = ms_load_functions();
	if (err != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("function load failed");
		exit(0);
	}

	_ms_new_global_variable();

	/*connect to media db, if conneting is failed, db updating is stopped*/
	ms_connect_db(&handle);

	ms_dbus_init();
#if MS_INOTI_ENABLE
	ms_inoti_add_watch_all_directory(MS_STORAGE_INTERNAL);
#endif
	/*prepare socket*/
	/* Create and bind new UDP socket */
	if (ms_ipc_create_server_socket(MS_PROTOCOL_UDP, MS_SCANNER_PORT, &sockfd)
		!= MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("Failed to create socket");
	} else {
		context = g_main_loop_get_context(mainloop);

		/* Create new channel to watch udp socket */
		channel = g_io_channel_unix_new(sockfd);
		source = g_io_create_watch(channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)ms_read_socket, handle, NULL);
		g_source_attach(source, context);
		g_source_unref(source);
	}

	/*create each threads*/
#if MS_INOTI_ENABLE
	inoti_thread = g_thread_new("inotify_thread", (GThreadFunc)ms_inoti_thread, NULL);
#endif
	db_thread = g_thread_new("db_thread", (GThreadFunc)ms_db_thread, NULL);
	thumb_thread = g_thread_new("thumb_agent_thread", (GThreadFunc)ms_thumb_agent_start_thread, NULL);

	/*set vconf callback function*/
	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_STATUS, (vconf_callback_fn) ms_mmc_vconf_cb, NULL);
	if (err == -1)
		MS_DBG_ERR("add call back function for event %s fails", VCONFKEY_SYSMAN_MMC_STATUS);

	MS_DBG("*********************************************************");
	MS_DBG("*** Begin to check tables of file manager in database ***");
	MS_DBG("*********************************************************");

	/* Add signal handler */
	sigset.sa_handler = _ms_signal_handler;
	if (sigaction(SIGCHLD, &sigset, NULL) < 0) {
		MS_DBG_ERR("sigaction failed [%s]", strerror(errno));
	} else {
		MS_DBG("handler ok");
	}

	/*clear previous data of sdcard on media database and check db status for updating*/
	while(!ms_db_get_thread_status()) {
		MS_DBG("wait db thread");
		sleep(1);
	}

	need_db_create = _db_clear(handle);
	if (need_db_create) {
		/*insert records*/
		ms_send_storage_scan_request(MS_STORAGE_INTERNAL, MS_SCAN_ALL);
	} else {
		ms_send_storage_scan_request(MS_STORAGE_INTERNAL, MS_SCAN_PART);
	}

	if (ms_is_mmc_inserted()) {
		mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;

		if (!ms_drm_insert_ext_memory())
			MS_DBG_ERR("ms_drm_insert_ext_memory failed");

		ms_make_default_path_mmc();
#if MS_INOTI_ENABLE
		ms_inoti_add_watch_all_directory(MS_STORAGE_EXTERNAL);
#endif
		ms_present_mmc_insert();

		ms_send_storage_scan_request(MS_STORAGE_EXTERNAL, ms_get_mmc_state());
	}

	/*Active flush */
	malloc_trim(0);

	MS_DBG("*****************************************");
	MS_DBG("*** Server of File Manager is running ***");
	MS_DBG("*****************************************");

	g_main_loop_run(mainloop);
#if MS_INOTI_ENABLE
	g_thread_join(inoti_thread);
#endif
	g_thread_join(db_thread);
	g_thread_join(thumb_thread);

	/*close an IO channel*/
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);

	heynoti_unsubscribe(heynoti_id, POWEROFF_NOTI_NAME, _power_off_cb);
	heynoti_close(heynoti_id);

	/***********
	**remove call back functions
	************/
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				 (vconf_callback_fn) ms_mmc_vconf_cb);

	_ms_free_global_variable();

	/*disconnect form media db*/
	if (handle) ms_disconnect_db(&handle);

	/*close socket*/
	close(sockfd);

	/*unload functions*/
	ms_unload_functions();

	exit(0);
}

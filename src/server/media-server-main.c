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

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <vconf.h>

#include "media-util.h"
#include "media-common-utils.h"
#include "media-common-external-storage.h"
#include "media-common-db-svc.h"
#include "media-common-system.h"

#include "media-server-dbg.h"
#include "media-server-socket.h"
#include "media-server-db.h"
#include "media-server-thumb.h"
#include "media-server-scanner.h"
#include "media-server-device-block.h"

extern GMutex scanner_mutex;

GMainLoop *mainloop = NULL;
bool power_off; /*If this is TRUE, poweroff notification received*/

static void __ms_check_mediadb(void);
static void __ms_add_signal_handler(void);
static void __ms_remove_event_receiver(void);
static void __ms_add_event_receiver(GIOChannel *channel);
static void __ms_remove_requst_receiver(GIOChannel *channel);
static void __ms_add_requst_receiver(GMainLoop *mainloop, GIOChannel **channel);
static int __ms_check_mmc_status(void);
static int __ms_check_usb_status(void);

static char *priv_lang = NULL;

void _power_off_cb(ms_power_info_s *power_info, void* data)
{
	MS_DBG_ERR("POWER OFF");

	GIOChannel *channel = (GIOChannel *)data;

	power_off = TRUE;

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

	__ms_remove_requst_receiver(channel);

	__ms_remove_event_receiver();

	return;
}

void _ms_signal_handler(int n)
{
	int stat, pid, thumb_pid;
	int scanner_pid;

	thumb_pid = ms_thumb_get_server_pid();
	scanner_pid = ms_get_scanner_pid();

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
		/* check pid of child process of thumbnail thread */
		if (pid == thumb_pid) {
			MS_DBG_ERR("[%d] Thumbnail server is stopped by media-server", pid);
			ms_thumb_reset_server_status();
		} else if (pid == scanner_pid) {
			MS_DBG_ERR("[%d] Scanner is stopped by media-server", pid);
			ms_reset_scanner_status();
		} else {
			MS_DBG_ERR("[%d] is stopped", pid);
		}
	}

	return;
}

static void __ms_new_global_variable(void)
{
	/*Init mutex variable*/
	/*media scanner stop/start mutex*/
	g_mutex_init(&scanner_mutex);
}

static void __ms_free_global_variable(void)
{
	/*Clear mutex variable*/
	g_mutex_clear(&scanner_mutex);
}

void _ms_change_lang_vconf_cb(keynode_t *key, void* data)
{
	char *lang = NULL;
	const char *eng = "en";
	const char *chi = "zh";
	const char *jpn = "ja";
	const char *kor = "ko";
	bool need_update = FALSE;

	if (!ms_config_get_str(VCONFKEY_LANGSET, &lang)) {
		MS_DBG_ERR("Get VCONFKEY_LANGSET failed.");
		return;
	}

	MS_DBG("CURRENT LANGUAGE [%s] [%s]", priv_lang, lang);

	if (MS_STRING_VALID(priv_lang) && MS_STRING_VALID(lang)) {
		if (strcmp(priv_lang, lang) == 0) {
			need_update = FALSE;
		} else if ((strncmp(lang, eng, strlen(eng)) == 0) ||
				(strncmp(lang, chi, strlen(chi)) == 0) ||
				(strncmp(lang, jpn, strlen(jpn)) == 0) ||
				(strncmp(lang, kor, strlen(kor)) == 0)) {
				need_update = TRUE;
		} else {
			if ((strncmp(priv_lang, eng, strlen(eng)) == 0) ||
				(strncmp(priv_lang, chi, strlen(chi)) == 0) ||
				(strncmp(priv_lang, jpn, strlen(jpn)) == 0) ||
				(strncmp(priv_lang, kor, strlen(kor)) == 0)) {
				need_update = TRUE;
			}
		}
	} else {
		need_update = TRUE;
	}

	if (need_update == TRUE) {
		ms_send_storage_scan_request(NULL, INTERNAL_STORAGE_ID, MS_SCAN_META, 0);	/*FIX ME*/
	} else {
		MS_DBG_WARN("language is changed but do not update meta data");
	}

	MS_SAFE_FREE(priv_lang);

	if (MS_STRING_VALID(lang))
		priv_lang = strdup(lang);

	MS_SAFE_FREE(lang);

	return;
}

static int _mkdir(const char *dir, mode_t mode) {
        char tmp[256];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        mkdir(tmp, mode);
                        *p = '/';
                }
        return mkdir(tmp, mode);
}

int main(int argc, char **argv)
{
	GThread *db_thread = NULL;
	GThread *thumb_thread = NULL;
	GIOChannel *channel = NULL;

	power_off = FALSE;

	/*Init main loop*/
	mainloop = g_main_loop_new(NULL, FALSE);

	__ms_new_global_variable();

	if (ms_cynara_initialize() != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("Failed to initialize cynara");
		return -1;
	}

	__ms_add_requst_receiver(mainloop, &channel);

	/* recevie event from other modules */
	__ms_add_event_receiver(channel);

	__ms_add_signal_handler();

	/*create each threads*/
	db_thread = g_thread_new("db_thread", (GThreadFunc)ms_db_thread, NULL);
	thumb_thread = g_thread_new("thumb_agent_thread", (GThreadFunc)ms_thumb_agent_start_thread, NULL);

	/*clear previous data of sdcard on media database and check db status for updating*/
	while(!ms_db_get_thread_status()) {
		MS_DBG_ERR("wait db thread");
		sleep(1);
	}

	/* update media DB */
	__ms_check_mediadb();

	/*Active flush */
	malloc_trim(0);

	/*Read ini file */
	ms_thumb_get_config();

	MS_DBG_ERR("*** Media Server is running ***");

	g_main_loop_run(mainloop);

	g_thread_join(db_thread);
	g_thread_join(thumb_thread);

	ms_cynara_finish();

	__ms_free_global_variable();

	MS_DBG_ERR("*** Media Server is stopped ***");

	return 0;
}

static void __ms_add_requst_receiver(GMainLoop *mainloop, GIOChannel **channel)
{

	int sockfd = -1;
	GSource *source = NULL;
	GMainContext *context = NULL;

	/*prepare socket*/
	/* create dir socket */
	_mkdir("/var/run/media-server",S_IRWXU | S_IRWXG | S_IRWXO);

	/* Create and bind new UDP socket */
	if (ms_ipc_create_server_socket(MS_PROTOCOL_TCP, MS_SCANNER_PORT, &sockfd)
		!= MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("Failed to create socket");
		return;
	} else {
		if (ms_cynara_enable_credentials_passing(sockfd) != MS_MEDIA_ERR_NONE)
			MS_DBG_ERR("Failed to setup credential passing");

		context = g_main_loop_get_context(mainloop);

		/* Create new channel to watch udp socket */
		*channel = g_io_channel_unix_new(sockfd);
		source = g_io_create_watch(*channel, G_IO_IN);

		/* Set callback to be called when socket is readable */
		g_source_set_callback(source, (GSourceFunc)ms_read_socket, *channel, NULL);
		g_source_attach(source, context);
		g_source_unref(source);
	}
}

static void __ms_remove_requst_receiver(GIOChannel *channel)
{
	int fd = -1;

	/*close an IO channel*/
	fd = g_io_channel_unix_get_fd(channel);
	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);

	if (fd > 0) {
		if (close(fd) < 0) {
			MS_DBG_STRERROR("CLOSE ERROR");
		}
	}
}

static void __ms_add_event_receiver(GIOChannel *channel)
{
	int err = 0;
	char *lang = NULL;

	/*set power off callback function*/
	ms_sys_set_poweroff_cb(_power_off_cb,channel);
	ms_sys_set_device_block_event_cb(ms_device_block_changed_cb, NULL);

	if (!ms_config_get_str(VCONFKEY_LANGSET, &lang)) {
		MS_DBG_ERR("Get VCONFKEY_LANGSET failed.");
		return;
	}

	if (MS_STRING_VALID(lang)) {
		MS_DBG("Set language change cb [%s]", lang);

		priv_lang = strdup(lang);
		MS_SAFE_FREE(lang);

		err = vconf_notify_key_changed(VCONFKEY_LANGSET, (vconf_callback_fn) _ms_change_lang_vconf_cb, NULL);
		if (err == -1)
			MS_DBG_ERR("add call back function for event %s fails", VCONFKEY_LANGSET);
	}

	return;
}

static void __ms_remove_event_receiver(void)
{
	ms_sys_unset_device_block_event_cb();
	ms_sys_unset_poweroff_cb();
}

static void __ms_add_signal_handler(void)
{
	struct sigaction sigset;

	/* Add signal handler */
	sigemptyset(&sigset.sa_mask);
	sigaddset(&sigset.sa_mask, SIGCHLD);
	sigset.sa_flags = SA_RESTART |SA_NODEFER;
	sigset.sa_handler = _ms_signal_handler;

	if (sigaction(SIGCHLD, &sigset, NULL) < 0) {
		MS_DBG_STRERROR("sigaction failed");
	}

	signal(SIGPIPE,SIG_IGN);
}

static void __ms_check_mediadb(void)
{
	ms_send_storage_scan_request(MEDIA_ROOT_PATH_INTERNAL, INTERNAL_STORAGE_ID, MS_SCAN_PART, 0);

	/* update external storage */
	__ms_check_mmc_status();
	__ms_check_usb_status();
}


////////////////////////////////////////////////////////////////////
static int __ms_check_mmc_status(void)
{
	int ret = MS_MEDIA_ERR_NONE;
	ms_stg_type_e stg_type = MS_STG_TYPE_MMC;
	GArray *dev_list = NULL;

	ret = ms_sys_get_device_list(stg_type, &dev_list);
	if (ret == MS_MEDIA_ERR_NONE) {
		if (dev_list != NULL) {
			MS_DBG_ERR("MMC FOUND[%d]", dev_list->len);
			int i = 0 ;
			int dev_num = dev_list->len;
			ms_block_info_s *block_info = NULL;

			for (i = 0; i < dev_num; i ++) {
				block_info = (ms_block_info_s *)g_array_index(dev_list , ms_stg_type_e*, i);
				ms_mmc_insert_handler(block_info->mount_path);
			}

			ms_sys_release_device_list(&dev_list);
		} else {
			MS_DBG_ERR("MMC NOT FOUND");
			ms_mmc_remove_handler(NULL);
		}
	} else {
		MS_DBG_ERR("ms_sys_get_device_list failed");
	}

	return MS_MEDIA_ERR_NONE;
}

static int __ms_check_usb_status(void)
{
	int ret = MS_MEDIA_ERR_NONE;
	ms_stg_type_e stg_type = MS_STG_TYPE_USB;
	GArray *dev_list = NULL;

	ret = ms_sys_get_device_list(stg_type, &dev_list);
	if (ret == MS_MEDIA_ERR_NONE) {
		if (dev_list != NULL) {
			MS_DBG_ERR("USB FOUND[%d]", dev_list->len);
			int i = 0 ;
			int dev_num = dev_list->len;
			ms_block_info_s *block_info = NULL;

			for (i = 0; i < dev_num; i ++) {
				block_info = (ms_block_info_s *)g_array_index(dev_list , ms_stg_type_e*, i);
				ms_usb_insert_handler(block_info->mount_path);
			}

			ms_sys_release_device_list(&dev_list);
		} else {
			MS_DBG_ERR("USB NOT FOUND");
		}
	} else {
		MS_DBG_ERR("ms_sys_get_device_list failed");
	}

	return MS_MEDIA_ERR_NONE;
}


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

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dd-display.h>

#include "media-common-utils.h"
#include "media-common-external-storage.h"
#include "media-common-system.h"
#include "media-common-db-svc.h"

#include "media-util.h"
#include "media-scanner-dbg-v2.h"
#include "media-scanner-common-v2.h"
#include "media-scanner-device-block-v2.h"
#include "media-scanner-scan-v2.h"
#include "media-scanner-socket-v2.h"
#include "media-scanner-extract-v2.h"

static GMainLoop *scanner_mainloop2 = NULL;

static void __msc_power_off_cb(ms_power_info_s *power_info, void* data);
static void __msc_add_event_receiver(void *data);
static void __msc_remove_event_receiver(void);

int main(int argc, char **argv)
{
	GThread *storage_scan_thread = NULL;
	GThread *scan_thread = NULL;
	GThread *register_thread = NULL;
	GThread *storage_extract_thread = NULL;
	GThread *folder_extract_thread = NULL;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GMainContext *context = NULL;
	bool power_off_status = FALSE;

	int err = -1;
	int fd = -1;

	MS_DBG_ERR("[No-Error] ========== Scanner start ========");

	__msc_add_event_receiver(NULL);

	/*load functions from plusin(s)*/
	err = ms_load_functions();
	if (err != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("function load failed[%d]", err);
		goto EXIT;
	}

	/*Init main loop*/
	scanner_mainloop2 = g_main_loop_new(NULL, FALSE);

	msc_init_scanner();
	msc_init_extract_thread();
	msc_init_scan_thread();

	/* Create pipe */
	err = unlink(MS_SCANNER_FIFO_PATH_REQ);
	if (err != 0) {
		MS_DBG_STRERROR("[No-Error] unlink failed");
	}

	err = mkfifo(MS_SCANNER_FIFO_PATH_REQ, MS_SCANNER_FIFO_MODE);
	if (err != 0) {
		MS_DBG_STRERROR("mkfifo failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	fd = open(MS_SCANNER_FIFO_PATH_REQ, O_RDWR);
	if (fd < 0) {
		MS_DBG_STRERROR("fifo open failed");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	context = g_main_loop_get_context(scanner_mainloop2);

	/* Create new channel to watch pipe */
	channel = g_io_channel_unix_new(fd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Set callback to be called when pipe is readable */
	g_source_set_callback(source, (GSourceFunc)msc_receive_request, NULL, NULL);
	g_source_attach(source, context);
	g_source_unref(source);

	/*create each threads*/
	storage_scan_thread = g_thread_new("storage_scan_thread", (GThreadFunc)msc_storage_scan_thread, NULL);
	scan_thread = g_thread_new("scanner_thread", (GThreadFunc)msc_directory_scan_thread, NULL);
	register_thread = g_thread_new("register_thread", (GThreadFunc)msc_register_thread, NULL);
	storage_extract_thread = g_thread_new("storage_extract_thread", (GThreadFunc)msc_storage_extract_thread, NULL);
	folder_extract_thread = g_thread_new("folder_extract_thread", (GThreadFunc)msc_folder_extract_thread, NULL);

	if (ms_is_mmc_inserted()) {
		msc_set_mmc_status(MS_STG_INSERTED);
	}

	msc_send_ready();
	MS_DBG_ERR("[No-Error] ========== Scanner is ready ========");

	MS_DBG_ERR("[No-Error] ========== Scanner is running  ========");

	g_main_loop_run(scanner_mainloop2);

	g_thread_join(scan_thread);
	g_thread_join(register_thread);
	g_thread_join(storage_scan_thread);
	g_thread_join(storage_extract_thread);
	g_thread_join(folder_extract_thread);

	g_io_channel_shutdown(channel, FALSE, NULL);
	g_io_channel_unref(channel);

	msc_get_power_status(&power_off_status);
	if (power_off_status) {
		display_unlock_state(LCD_OFF, PM_RESET_TIMER);
	}

	msc_deinit_extract_thread();
	msc_deinit_scan_thread();
	msc_deinit_scanner();

	/*close pipe*/
	close(fd);

	/*unload functions*/
	ms_unload_functions();

	__msc_remove_event_receiver();

EXIT:
	MS_DBG_ERR("[No-Error] ========== Scanner end  ========");

	return 0;
}


static void __msc_power_off_cb(ms_power_info_s *power_info, void* data)
{
	msc_stop_scan_thread();
	msc_stop_extract_thread();

	if (g_main_loop_is_running(scanner_mainloop2)) g_main_loop_quit(scanner_mainloop2);
}

static void __msc_add_event_receiver(void *data)
{
	/*set power off callback function*/
	ms_sys_set_poweroff_cb(__msc_power_off_cb, NULL);
	ms_sys_set_device_block_event_cb(msc_device_block_changed_cb, NULL);
}

static void __msc_remove_event_receiver(void)
{
	ms_sys_unset_device_block_event_cb();
	ms_sys_unset_poweroff_cb();
}


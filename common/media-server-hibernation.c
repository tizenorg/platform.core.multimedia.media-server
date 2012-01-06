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
 * @file		media-server-hibernation.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief		This file defines api utilities of contents manager engines.
 */
#ifdef _USE_HIB

#include <vconf.h>
#include <heynoti.h>

#include "media-server-common.h"
#include "media-server-hibernation.h"
#include "media-server-inotify.h"
#include "media-server-scan.h"
extern int hib_fd;
extern GAsyncQueue *scan_queue;
extern GMutex *db_mutex;

void _hibernation_leave_callback(void *data)
{
	GThread *inoti_tid;
	GThread *scan_tid;
	GMainLoop *mainloop = NULL;

	MS_DBG("hibernation leave callback\n");
	MS_DBG("MEDIA SERVER START[HIB]");

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	if (!scan_queue)
		scan_queue = g_async_queue_new();

	/*Init db mutex variable*/
	if (!db_mutex)
		db_mutex = g_mutex_new();

	ms_inoti_init();

	inoti_tid = g_thread_create((GThreadFunc) ms_inoti_thread, NULL, FALSE, NULL);
	scan_tid = g_thread_create((GThreadFunc) ms_scan_thread, NULL, TRUE, NULL);
#ifdef THUMB_THREAD
	/*create making thumbnail thread*/
	thumb_tid = g_thread_create((GThreadFunc) ms_thumb_thread, NULL, TRUE, NULL);
#endif
	mainloop = g_main_loop_new(NULL, FALSE);

	ms_start();

	MS_DBG("*****************************************");
	MS_DBG("*** Server of File Manager is running ***");
	MS_DBG("*****************************************");

	g_main_loop_run(mainloop);

	/*free all associated memory */
	g_main_loop_unref(mainloop);

	if (scan_queue)
		g_async_queue_unref(scan_queue);

	ms_end();
	exit(0);
}

void _hibernation_enter_callback(void *data)
{

	MS_DBG("[fm-server] hibernation enter callback\n");

	/* IMPORTANT : this is for kill inotify thread 
	 * it is essential for hibernation */
	ms_scan_data_t *scan_data;

	scan_data = malloc(sizeof(ms_scan_data_t));
	scan_data->scan_type = end_thread;

	g_async_queue_push(scan_queue, GINT_TO_POINTER(scan_data));

	mkdir("/opt/media/_HIBERNATION_END", 0777);
	rmdir("/opt/media/_HIBERNATION_END");

	if (scan_queue)
		g_async_queue_unref(scan_queue);

	ms_end();

	if (hib_fd != 0)
		heynoti_close(hib_fd);
}

int _hibernation_initialize(void)
{
	hib_fd = heynoti_init();
	heynoti_subscribe(hib_fd, "HIBERNATION_ENTER",
			  _hibernation_enter_callback, (void *)hib_fd);
	heynoti_subscribe(hib_fd, "HIBERNATION_LEAVE",
			  _hibernation_leave_callback, (void *)hib_fd);
	heynoti_attach_handler(hib_fd);
	return hib_fd;
}

void _hibernation_fianalize(void)
{
	heynoti_close(hib_fd);
}
#endif

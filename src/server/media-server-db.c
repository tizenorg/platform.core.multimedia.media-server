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

#include <unistd.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-types.h"

#include "media-server-dbg.h"
#include "media-server-socket.h"
#include "media-server-db.h"

static GMainLoop *g_db_mainloop = NULL;
static bool db_thread_ready = FALSE;

GMainLoop *ms_db_get_mainloop(void)
{
	return g_db_mainloop;
}
gboolean ms_db_get_thread_status(void)
{
	return db_thread_ready;
}

gboolean ms_db_thread(void *data)
{
	int sockfd = -1;
	int tcp_sockfd = -1;
	int ret = MS_MEDIA_ERR_NONE;
	GSource *source = NULL;
	GIOChannel *channel = NULL;
	GSource *tcp_source = NULL;
	GIOChannel *tcp_channel = NULL;
	GMainContext *context = NULL;
	MediaDBHandle *db_handle = NULL;

	/* Create TCP Socket*/
	ret = ms_ipc_create_server_socket(MS_PROTOCOL_TCP, MS_DB_UPDATE_PORT, &sockfd);
	if(ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("Failed to create socket");
		return FALSE;
	}

	/* Create TCP Socket for batch query*/
	ret = ms_ipc_create_server_socket(MS_PROTOCOL_TCP, MS_DB_BATCH_UPDATE_PORT, &tcp_sockfd);
	if(ret != MS_MEDIA_ERR_NONE) {
		close(sockfd);
		MS_DBG_ERR("Failed to create socket");
		return FALSE;
	}

	context = g_main_context_new();
	/*Init main loop*/
	g_db_mainloop = g_main_loop_new(context, FALSE);
	//context = g_main_loop_get_context(g_db_mainloop);

	/* Create new channel to watch UDP socket */
	channel = g_io_channel_unix_new(sockfd);
	source = g_io_create_watch(channel, G_IO_IN);

	/* Set callback to be called when socket is readable */
	g_source_set_callback(source, (GSourceFunc)ms_read_db_tcp_socket, db_handle, NULL);
	g_source_attach(source, context);

	/* Create new channel to watch TCP socket */
	tcp_channel = g_io_channel_unix_new(tcp_sockfd);
	tcp_source = g_io_create_watch(tcp_channel, G_IO_IN);

	/* Set callback to be called when socket is readable */
	g_source_set_callback(tcp_source, (GSourceFunc)ms_read_db_tcp_batch_socket, db_handle, NULL);
	g_source_attach(tcp_source, context);

	g_main_context_push_thread_default(context);

	MS_DBG("*******************************************");
	MS_DBG("*** Media Server DB thread is running ***");
	MS_DBG("*******************************************");

	db_thread_ready = TRUE;

	g_main_loop_run(g_db_mainloop);

	MS_DBG("*** Media Server DB thread will be closed ***");

	db_thread_ready = FALSE;

	g_io_channel_shutdown(channel,  FALSE, NULL);
	g_io_channel_unref(channel);

	/*close socket*/
	close(sockfd);
	close(tcp_sockfd);

	g_main_loop_unref(g_db_mainloop);

	return FALSE;
}

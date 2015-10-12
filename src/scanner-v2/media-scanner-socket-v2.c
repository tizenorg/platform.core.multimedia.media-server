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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/un.h>
#include <malloc.h>
#include <vconf.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-types.h"
#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-scanner-dbg-v2.h"
#include "media-scanner-scan-v2.h"
#include "media-scanner-socket-v2.h"
#include "media-scanner-extract-v2.h"

extern GAsyncQueue *storage_queue2;
extern GAsyncQueue *scan_queue2;
extern GAsyncQueue *reg_queue2;
extern GMutex scan_req_mutex2;
extern GAsyncQueue *folder_extract_queue;

static int _msc_remove_request(GAsyncQueue *req_queue, ms_comm_msg_s *recv_msg)
{
	char *cancel_path = recv_msg->msg;
	int pid = recv_msg->pid;
	int i = 0;
	int len = g_async_queue_length(req_queue);
	ms_comm_msg_s *msg = NULL;
	GAsyncQueue *temp_scan_queue = NULL;

	MS_DBG_WARN("scan_req_mutex2 is LOCKED");
	g_mutex_lock(&scan_req_mutex2);

	if (len == 0) {
		MS_DBG_WARN("Request is not stacked");
		goto END_REMOVE_REQUEST;
	}

	msc_set_cancel_path(recv_msg->msg);
	msc_set_extract_cancel_path(recv_msg->msg);

	temp_scan_queue = g_async_queue_new();

	for (i = 0; i <len; i++) {
		/*create new queue to compare request*/
		msg = g_async_queue_pop(scan_queue2);
		if ((strcmp(msg->msg, cancel_path) == 0) && (pid == msg->pid)) {
			MS_SAFE_FREE(msg);
		} else {
			g_async_queue_push(temp_scan_queue, GINT_TO_POINTER(msg));
		}
	}
	g_async_queue_unref (scan_queue2);
	scan_queue2 = temp_scan_queue;

END_REMOVE_REQUEST:
	g_mutex_unlock(&scan_req_mutex2);
	MS_DBG_WARN("scan_req_mutex2 is UNLOCKED");

	GAsyncQueue *temp_extract_queue = NULL;
	int len_extract = g_async_queue_length(folder_extract_queue);
	if(len_extract != 0)
	{
		temp_extract_queue = g_async_queue_new();
		for (i = 0; i <len_extract; i++) {
			/*create new queue to compare request*/
			msg = g_async_queue_pop(folder_extract_queue);
			if ((strcmp(msg->msg, cancel_path) == 0) && (pid == msg->pid)) {
				MS_SAFE_FREE(msg);
			} else {
				g_async_queue_push(temp_extract_queue, GINT_TO_POINTER(msg));
			}
		}
		g_async_queue_unref (folder_extract_queue);
		folder_extract_queue = temp_extract_queue;
	}

	return MS_MEDIA_ERR_NONE;
}

gboolean msc_receive_request(GIOChannel *src, GIOCondition condition, gpointer data)
{
	ms_comm_msg_s *recv_msg = NULL;
	int sockfd = MS_SOCK_NOT_ALLOCATE;
	int req_num = MS_MSG_MAX;
	int err = -1;

	sockfd = g_io_channel_unix_get_fd(src);
	if (sockfd < 0) {
		MS_DBG_ERR("sock fd is invalid!");
		return TRUE;
	}

	MS_MALLOC(recv_msg, sizeof(ms_comm_msg_s));
	if (recv_msg == NULL) {
		MS_DBG_ERR("MS_MALLOC failed");
		return TRUE;
	}

	/* read() is blocked until media scanner sends message */
	err = read(sockfd, recv_msg, sizeof(ms_comm_msg_s));
	if (err < 0) {
		MS_DBG_STRERROR("fifo read failed");
		MS_SAFE_FREE(recv_msg);
		return MS_MEDIA_ERR_FILE_READ_FAIL;
	}

	MS_DBG_SLOG("receive msg from [%d] %d, %s", recv_msg->pid, recv_msg->msg_type, recv_msg->msg);

	/* copy from recived data */
	req_num = recv_msg->msg_type;

	switch(req_num){
		case MS_MSG_BULK_INSERT:
		case MS_MSG_BURSTSHOT_INSERT:
			{
				MS_DBG_INFO("BULK INSERT");
				/* request bulk insert*/
				msc_push_scan_request(MS_SCAN_REGISTER, recv_msg);
			}
			break;
		case MS_MSG_DIRECTORY_SCANNING:
		case MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE:
			{
				/* this request from another apps */
				/* set the scan data for scanning thread */
				msc_push_scan_request(MS_SCAN_DIRECTORY, recv_msg);
			}
			break;
		case MS_MSG_STORAGE_ALL:
		case MS_MSG_STORAGE_PARTIAL:
		case MS_MSG_STORAGE_INVALID:
			{
				/* this request from media-server */
				msc_push_scan_request(MS_SCAN_STORAGE, recv_msg);
			}
			break;
		case MS_MSG_DIRECTORY_SCANNING_CANCEL:
			{
				_msc_remove_request(scan_queue2, recv_msg);
				MS_SAFE_FREE(recv_msg);
			}
			break;
		default:
			{
				MS_DBG_ERR("THIS REQUEST IS INVALID %d", req_num);
				MS_SAFE_FREE(recv_msg);
			}
			break;
	}

	/*Active flush */
	malloc_trim(0);

	return TRUE;
}

int msc_send_ready(void)
{
	int res = MS_MEDIA_ERR_NONE;
	ms_comm_msg_s send_msg;
	int fd = -1;
	int err = -1;

	fd = open(MS_SCANNER_FIFO_PATH_RES, O_WRONLY);
	if (fd < 0) {
		MS_DBG_STRERROR("fifo read failed");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	/* send ready message */
	memset(&send_msg, 0, sizeof(send_msg));
	send_msg.msg_type = MS_MSG_SCANNER_READY;

	/* send ready message */
	err = write(fd, &send_msg, sizeof(send_msg));
	if (err < 0) {
		MS_DBG_STRERROR("fifo write failed");
		res = MS_MEDIA_ERR_FILE_READ_FAIL;
	}

	close(fd);

	return res;
}

int msc_send_result(int result, ms_comm_msg_s *res_data)
{
	int res = MS_MEDIA_ERR_NONE;
	ms_comm_msg_s send_msg;
	int fd = -1;
	int err = -1;

	fd = open(MS_SCANNER_FIFO_PATH_RES, O_WRONLY);
	if (fd < 0) {
		MS_DBG_STRERROR("fifo open failed");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	/* send result message */
	memset(&send_msg, 0x0, sizeof(ms_comm_msg_s));
	send_msg.msg_type = MS_MSG_SCANNER_BULK_RESULT;
	send_msg.pid = res_data->pid;
	send_msg.result = result;
	send_msg.msg_size = res_data->msg_size;
	strncpy(send_msg.msg, res_data->msg, send_msg.msg_size);

	/* send ready message */
	err = write(fd, &send_msg, sizeof(send_msg));
	if (err < 0) {
		MS_DBG_STRERROR("fifo write failed");
		res = MS_MEDIA_ERR_FILE_READ_FAIL;
	}

	close(fd);

	return res;
}


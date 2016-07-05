/*
 * Media Utility for DCM
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Ji Yong Min <jiyong.min@samsung.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <unistd.h>
#include <media-server-ipc.h>
#include <media-util.h>
#include <media-util-internal.h>
#include <media-util-dbg.h>
#include <media-util-dcm.h>
#include <media-util-err.h>
#include <media-util-ipc.h>
#include <tzplatform_config.h>

#define DCM_MSG_MAX_SIZE 4096

static GQueue *g_request_queue = NULL;
static GQueue *g_manage_queue = NULL;

typedef struct {
	FaceFunc func;
	void *user_data;
} faceUserData;

typedef struct {
	GIOChannel *channel;
	int msg_type;
	gboolean isCanceled;
	int request_id;
	int source_id;
	uid_t uid;
	char *path;
	faceUserData *userData;
} dcmReq;

enum {
	DCM_REQUEST_MEDIA,
	DCM_REQUEST_ALL_MEDIA,
	DCM_REQUEST_START_FACE_DETECTION,
	DCM_REQUEST_CANCEL_FACE,
};

typedef enum
{
	DCM_PHONE,			/**< Stored only in phone */
	DCM_MMC				/**< Stored only in MMC */
} media_dcm_store_type;

extern char MEDIA_IPC_PATH[][70];

int __media_dcm_check_req_queue(const char *path);
int __media_dcm_pop_manage_queue(const unsigned int request_id, const char *path);
int _media_dcm_send_request();

gboolean __media_dcm_check_cancel(void)
{
	dcmReq *req = NULL;
	req = (dcmReq *)g_queue_peek_head(g_request_queue);

	if (req == NULL) {
		return FALSE;
	} else {
		if (req->isCanceled)
			return FALSE;
		else
			return TRUE;
	}
}

void __media_dcm_shutdown_channel(gboolean only_shutdown)
{
	int len = -1;
	dcmReq *req = (dcmReq *)g_queue_peek_head(g_request_queue);

	len = g_queue_get_length(g_manage_queue);

	if (req != NULL) {
		if (!only_shutdown) {
			if (len == 0 && req->isCanceled == TRUE) {
				MSAPI_DBG("The manage queue will be released");
			} else {
				MSAPI_DBG("There is remain item in the manage queue");
				return;
			}
		}
		GSource *source_id = g_main_context_find_source_by_id(g_main_context_get_thread_default(), req->source_id);
		if (source_id != NULL) {
			g_source_destroy(source_id);
		} else {
			MSAPI_DBG_ERR("G_SOURCE_ID is NULL");
		}

		g_io_channel_shutdown(req->channel, TRUE, NULL);
		g_io_channel_unref(req->channel);
		g_queue_pop_head(g_request_queue);

		MS_SAFE_FREE(req->path);
		MS_SAFE_FREE(req->userData);
		MS_SAFE_FREE(req);

		if(g_manage_queue && len == 0) {
			g_queue_free(g_manage_queue);
			g_manage_queue = NULL;
		}
	}
}

int __media_dcm_pop_req_queue(const char *path)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	if (req_len <= 0) {
		MSAPI_DBG("There is no request in the queue");
	} else {
		__media_dcm_shutdown_channel(TRUE);
	}

	/* Check manage queue */
	if(g_manage_queue) {
		req_len = g_queue_get_length(g_manage_queue);

		if(req_len > 0)
			_media_dcm_send_request();
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_dcm_check_req_queue_for_cancel(unsigned int request_id, const char *path)
{
	int req_len = 0;

	req_len = g_queue_get_length(g_request_queue);

	if (req_len <= 0) {
		MSAPI_DBG("There is no request in the queue");
	} else {
		dcmReq *req = NULL;
		req = (dcmReq *)g_queue_peek_head(g_request_queue);

		if (req != NULL && strncmp(path, req->path, strlen(path)) == 0 && request_id == req->request_id) {
			req->isCanceled = true;
			__media_dcm_shutdown_channel(false);

			return MS_MEDIA_ERR_NONE;
		}
	}

	return MS_MEDIA_ERR_INTERNAL;
}

gboolean __media_dcm_write_socket(GIOChannel *src, GIOCondition condition, gpointer data)
{
	dcmMsg recv_msg;
	int sock = 0;
	int err = MS_MEDIA_ERR_NONE;

	memset((void *)&recv_msg, 0, sizeof(dcmMsg));
	sock = g_io_channel_unix_get_fd(src);

	MSAPI_DBG("__media_dcm_write_socket socket : %d", sock);

	if ((err = recv(sock, &recv_msg, sizeof(dcmMsg), 0)) < 0) {
		MSAPI_DBG_STRERROR("recv failed ");
		if (recv_msg.msg_size > 0) {
			__media_dcm_pop_req_queue(recv_msg.msg);
		} else {
			MSAPI_DBG_ERR("origin path size is wrong.");
		}

		return FALSE;
	}

	MSAPI_DBG("Completed.. %d, %s, %d", recv_msg.msg_type, recv_msg.msg, recv_msg.result);

	if (recv_msg.msg_type != 0) {
		MSAPI_DBG_ERR("Failed to run dcm(%d)", recv_msg.msg_type);
		err = MS_MEDIA_ERR_INTERNAL;
	}

	if (__media_dcm_check_cancel()) {
		MSAPI_DBG_ERR("__media_dcm_check_cancel is true");
		if (data) {
			faceUserData* cb = (faceUserData*)data;
			if (cb->func != NULL) {
				cb->func(recv_msg.msg_type, (int)(recv_msg.result), cb->user_data);
			}
		}
	}

	__media_dcm_pop_req_queue(recv_msg.msg);

	MSAPI_DBG("Done");

	return FALSE;
}

int __media_dcm_check_req_queue(const char *path)
{
	int req_len = 0, i;

	req_len = g_queue_get_length(g_request_queue);

	MSAPI_DBG("Queue length : %d", req_len);
	MSAPI_DBG("Queue path : %s", path);

	if (req_len <= 0) {
		MSAPI_DBG("There is no request in the queue");
	} else {

		for (i = 0; i < req_len; i++) {
			dcmReq *req = NULL;
			req = (dcmReq *)g_queue_peek_nth(g_request_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				MSAPI_DBG("Same Request - %s", path);
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_dcm_pop_manage_queue(const unsigned int request_id, const char *path)
{
	int req_len = 0, i;
	gboolean flag = FALSE;

	req_len = g_queue_get_length(g_manage_queue);

	if (req_len < 0) {
		MSAPI_DBG("There is no request in the queue");
	} else {
		for (i = 0; i < req_len; i++) {
			dcmReq *req = NULL;
			req = (dcmReq *)g_queue_peek_nth(g_manage_queue, i);
			if (req == NULL) continue;

			if (strncmp(path, req->path, strlen(path)) == 0) {
				g_queue_pop_nth(g_manage_queue, i);

				MS_SAFE_FREE(req->path);
				MS_SAFE_FREE(req->userData);
				MS_SAFE_FREE(req);
				flag = TRUE;
				break;
			}
		}
		if (!flag) {
			return __media_dcm_check_req_queue_for_cancel(request_id, path);
		} else {
			__media_dcm_shutdown_channel(FALSE);

			return MS_MEDIA_ERR_NONE;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

int __media_dcm_get_store_type_by_path(const char *full_path)
{
	if (full_path != NULL && MEDIA_ROOT_PATH_INTERNAL != NULL && MEDIA_ROOT_PATH_SDCARD != NULL) {
		if (strncmp(full_path, MEDIA_ROOT_PATH_INTERNAL, strlen(MEDIA_ROOT_PATH_INTERNAL)) == 0) {
			return DCM_PHONE;
		} else if (strncmp(full_path, MEDIA_ROOT_PATH_SDCARD, strlen(MEDIA_ROOT_PATH_SDCARD)) == 0) {
			return DCM_MMC;
		}
	}

	return -1;
}

int _media_dcm_send_request()
{
	int err = MS_MEDIA_ERR_NONE;
	int sock = -1;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	dcmReq *req_manager = NULL;
	sock_info.port = MS_DCM_CREATOR_PORT;

	MSAPI_DBG("_media_dcm_send_request start");

	err = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("ms_ipc_create_client_socket failed");
		return err;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[sock_info.port]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[sock_info.port])));

	GIOChannel *channel = NULL;
	channel = g_io_channel_unix_new(sock);
	int source_id = -1;

	/* Connecting to the dcm service */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_STRERROR("connect");
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	req_manager = (dcmReq *)g_queue_pop_head(g_manage_queue);

	if (req_manager == NULL) {
		MSAPI_DBG_ERR("queue pop fail");
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	GSource *source = NULL;
	source = g_io_create_watch(channel, G_IO_IN);
	g_source_set_callback(source, (GSourceFunc)__media_dcm_write_socket, req_manager->userData, NULL);
	source_id = g_source_attach(source, g_main_context_get_thread_default());

	dcmMsg req_msg;

	memset((void *)&req_msg, 0, sizeof(dcmMsg));

	req_msg.pid = getpid();
	req_msg.msg_type = req_manager->msg_type;
	req_msg.uid = req_manager->uid;
	strncpy(req_msg.msg, req_manager->path, sizeof(req_msg.msg));
	req_msg.msg[strlen(req_msg.msg)] = '\0';
	req_msg.msg_size = strlen(req_msg.msg) + 1;

	if (send(sock, &req_msg, sizeof(req_msg), 0) != sizeof(req_msg)) {
		MSAPI_DBG_STRERROR("send failed");

		g_source_destroy(g_main_context_find_source_by_id(g_main_context_get_thread_default(), source_id));
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref(channel);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	MSAPI_DBG("Sending msg to dcm service is successful");


	if (req_manager->msg_type == DCM_REQUEST_MEDIA/*DCM_REQUEST_INSERT_FACE*/) {
		if (g_request_queue == NULL) {
			g_request_queue = g_queue_new();
		}

		dcmReq *dcm_req = calloc(1, sizeof(dcmReq));
		if (dcm_req == NULL) {
			MSAPI_DBG_ERR("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}

		dcm_req->channel = channel;
		dcm_req->path = strdup(req_manager->path);
		dcm_req->source_id = source_id;
		dcm_req->userData = req_manager->userData;

		g_queue_push_tail(g_request_queue, (gpointer)dcm_req);
	}

	return err;
}

int _media_dcm_request(int msg_type, const char *path, uid_t uid)
{
	int sock = -1;
	struct sockaddr_un serv_addr;
	ms_sock_info_s sock_info;
	int err = MS_MEDIA_ERR_NONE;
	int pid;
	sock_info.port = MS_DCM_CREATOR_PORT;

	err = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sock_info);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("ms_ipc_create_client_socket failed");
		return err;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	sock = sock_info.sock_fd;
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[sock_info.port]), strlen(tzplatform_mkpath(TZ_SYS_RUN, MEDIA_IPC_PATH[sock_info.port])));

	/* Connecting to the dcm service */
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_STRERROR("connect");
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	dcmMsg req_msg;
	dcmMsg recv_msg;

	memset((void *)&req_msg, 0, sizeof(dcmMsg));
	memset((void *)&recv_msg, 0, sizeof(dcmMsg));

	/* Get PID of client*/
	pid = getpid();
	req_msg.pid = pid;

	/* Set requset message */
	req_msg.msg_type = msg_type;
	req_msg.uid = uid;
	if (path != NULL) {
		req_msg.msg_size = strlen(path);
		memcpy(req_msg.msg, path, strlen(path));
		req_msg.msg[strlen(path)+1] = '\0';
	}

	if (req_msg.msg_size > DCM_MSG_MAX_SIZE) {
		MSAPI_DBG_ERR("path's length exceeds %d", DCM_MSG_MAX_SIZE);
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (send(sock, &req_msg, sizeof(dcmMsg), 0) != sizeof(dcmMsg)) {
		MSAPI_DBG_STRERROR("sendto failed");
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_SEND;
	}

	MSAPI_DBG("Sending msg to dcm service is successful");

	if ((err = recv(sock, &recv_msg, sizeof(dcmMsg), 0)) < 0) {
		MSAPI_DBG_ERR("recv failed ");
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	MSAPI_DBG("recv %d from dcm service is successful", recv_msg.msg_type);

	ms_ipc_delete_client_socket(&sock_info);

	return MS_MEDIA_ERR_NONE;
}

int _media_dcm_request_async(int msg_type, const unsigned int request_id, const char *path, faceUserData *userData, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	MSAPI_DBG("Path : %s", path);

	if (g_manage_queue == NULL) {
		MSAPI_DBG("g_manage_queue is NULL");
		if (msg_type == DCM_REQUEST_CANCEL_FACE)
			return MS_MEDIA_ERR_INTERNAL;

		g_manage_queue = g_queue_new();

		dcmReq *dcm_req = NULL;
		dcm_req = calloc(1, sizeof(dcmReq));
		if (dcm_req == NULL) {
			MSAPI_DBG_ERR("Failed to create request element");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}

		dcm_req->msg_type = msg_type;
		dcm_req->path = strdup(path);
		dcm_req->userData = userData;
		dcm_req->isCanceled = FALSE;
		dcm_req->request_id = request_id;
		dcm_req->uid = uid;

		MSAPI_DBG("Enqueue");
		g_queue_push_tail(g_manage_queue, (gpointer)dcm_req);

		/* directly request at first time */
		err = _media_dcm_send_request();

	} else {
		MSAPI_DBG("g_manage_queue is not NULL");
		if (msg_type != DCM_REQUEST_CANCEL_FACE) {
			/* Enqueue */
			dcmReq *dcm_req = NULL;
			dcm_req = calloc(1, sizeof(dcmReq));
			if (dcm_req == NULL) {
				MSAPI_DBG_ERR("Failed to create request element");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}

			dcm_req->msg_type = msg_type;
			dcm_req->path = strdup(path);
			dcm_req->userData = userData;
			dcm_req->isCanceled = FALSE;
			dcm_req->request_id = request_id;
			dcm_req->uid = uid;

			MSAPI_DBG("Enqueue");
			g_queue_push_tail(g_manage_queue, (gpointer)dcm_req);
		} else {
			/* Dequeue */
			MSAPI_DBG("Dequeue");
			err = __media_dcm_pop_manage_queue(request_id, path);
		}
	}

	return err;
}

int dcm_request_extract_all(uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	/* Request for image file to the daemon "Dcm generator" */
	err = _media_dcm_request(DCM_REQUEST_ALL_MEDIA, NULL, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int dcm_request_extract_media(const char *path, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	/* Request for image file to the daemon "Dcm generator" */
	err = _media_dcm_request(DCM_REQUEST_MEDIA, path, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int dcm_request_extract_face_async(const unsigned int request_id, const char *path, FaceFunc func, void *user_data, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;
	int exist = -1;

	if (path == NULL) {
		MSAPI_DBG_ERR("Invalid parameter");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	exist = open(path, O_RDONLY);
	if (exist < 0) {
		MSAPI_DBG_ERR("The path(%s) doesn't exist.", path);
		if (errno == EACCES || errno == EPERM)
			return  MS_MEDIA_ERR_PERMISSION_DENIED;
		else
			return MS_MEDIA_ERR_INVALID_PARAMETER;
	}
	close(exist);

	MSAPI_DBG("Path : %s", path);

	int store_type = -1;
	store_type = __media_dcm_get_store_type_by_path(path);

	if ((store_type != DCM_PHONE) && (store_type != DCM_MMC)) {
		MSAPI_DBG_ERR("The path(%s) is invalid", path);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	MSAPI_DBG("Path : %s", path);

	faceUserData *userData = (faceUserData*)malloc(sizeof(faceUserData));
	if (userData == NULL) {
		MSAPI_DBG_ERR("memory allocation failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}
	userData->func = (FaceFunc)func;
	userData->user_data = user_data;

	/* Request for image file to the daemon "Dcm generator" */
	err = _media_dcm_request_async(DCM_REQUEST_MEDIA/*DCM_REQUEST_INSERT_FACE*/, request_id, path, userData, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int dcm_request_cancel_face(const unsigned int request_id, const char *path)
{
	int err = MS_MEDIA_ERR_NONE;

	/* Request for image file to the daemon "Dcm generator" */
	err = _media_dcm_request_async(DCM_REQUEST_CANCEL_FACE, request_id, path, NULL, request_id);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}


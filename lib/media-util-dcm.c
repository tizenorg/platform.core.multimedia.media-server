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

#include <glib.h>
#include <media-server-ipc.h>
#include <media-util-dbg.h>
#include <media-util-dcm.h>
#include <media-util-err.h>
#include <media-util-ipc.h>

#define DCM_MSG_MAX_SIZE 4096

enum {
	DCM_REQUEST_MEDIA,
	DCM_REQUEST_ALL_MEDIA,
};

extern char MEDIA_IPC_PATH[][70];

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
	strncpy(serv_addr.sun_path, MEDIA_IPC_PATH[sock_info.port], strlen(MEDIA_IPC_PATH[sock_info.port]));

	/* Connecting to the thumbnail server */
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
		MSAPI_DBG_ERR("_media_dcm_recv_msg failed ");
		ms_ipc_delete_client_socket(&sock_info);
		return MS_MEDIA_ERR_SOCKET_RECEIVE;
	}

	MSAPI_DBG("recv %d from dcm service is successful", recv_msg.msg_type);

	ms_ipc_delete_client_socket(&sock_info);

	return MS_MEDIA_ERR_NONE;
}

int dcm_svc_request_extract_all(uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_dcm_request(DCM_REQUEST_ALL_MEDIA, NULL, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

int dcm_svc_request_extract_media(const char *path, uid_t uid)
{
	int err = MS_MEDIA_ERR_NONE;

	/* Request for thumb file to the daemon "Thumbnail generator" */
	err = _media_dcm_request(DCM_REQUEST_MEDIA, path, uid);
	if (err != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("_media_dcm_request failed : %d", err);
		return err;
	}

	return MS_MEDIA_ERR_NONE;
}

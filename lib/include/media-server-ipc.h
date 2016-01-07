/*
 * Media Utility
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

#ifndef _MEDIA_SERVER_IPC_H_
#define _MEDIA_SERVER_IPC_H_

#define MS_TIMEOUT_SEC_3					3		/**< Response from Server time out */
#define MS_TIMEOUT_SEC_10					10		/**< Response from Server time out */
#define MS_TIMEOUT_SEC_20			20		/**< Response from Media server time out */

typedef enum{
	MS_DB_BATCH_UPDATE_PORT = 0,	/**< Media DB batch update */
	MS_SCAN_DAEMON_PORT,		/**< Port of communication between scanner and server */
	MS_SCANNER_PORT,		/**< Directory Scanner */
	MS_DB_UPDATE_PORT,		/**< Media DB Update */
	MS_THUMB_CREATOR_PORT,	/**< Create thumbnail */
	MS_THUMB_COMM_PORT,		/**< Port of communication between creator and server */
	MS_THUMB_DAEMON_PORT, 	/**< Port of Thumbnail server */
	MS_DCM_CREATOR_PORT,	/**< Create DCM service */
	MS_DCM_COMM_PORT, 	/**< Port of communication between creator and server */
	MS_DCM_DAEMON_PORT,	/**< Port of DCM service */
	MS_PORT_MAX,
}ms_msg_port_type_e;

#define MAX_MSG_SIZE				4096*2
#define MAX_FILEPATH_LEN			4096

typedef enum{
	MS_MSG_DB_UPDATE = 0,		/**< Media DB Update */
	MS_MSG_DB_UPDATE_BATCH_START,		/**< Start of media DB update batch */
	MS_MSG_DB_UPDATE_BATCH,				/**< Perform of media DB update batch */
	MS_MSG_DB_UPDATE_BATCH_END,		/**< End of media DB update batch */
	MS_MSG_DIRECTORY_SCANNING,			/**< Non recursive Directory Scan and Media DB Update*/
	MS_MSG_DIRECTORY_SCANNING_NON_RECURSIVE,/**< Recursive Directory Scan and Media DB Update*/
	MS_MSG_BURSTSHOT_INSERT,
	MS_MSG_BULK_INSERT,					/**< Request bulk insert */
	MS_MSG_STORAGE_ALL,
	MS_MSG_STORAGE_PARTIAL,
	MS_MSG_STORAGE_INVALID,
	MS_MSG_THUMB_SERVER_READY,			/**< Ready from thumbnail server */
	MS_MSG_THUMB_EXTRACT_ALL_DONE,		/**< Done of all-thumbnail extracting */
	MS_MSG_SCANNER_READY,				/**< Ready from media scanner */
	MS_MSG_SCANNER_RESULT,				/**< Result of directory scanning */
	MS_MSG_SCANNER_BULK_RESULT,			/**< Request bulk insert */
	MS_MSG_STORAGE_META,				/**< Request updating meta data */
	MS_MSG_DIRECTORY_SCANNING_CANCEL,	/**< Request cancel directory scan*/
	MS_MSG_STORAGE_SCANNER_COMPLETE,	/**< Storage Scanner complete */
	MS_MSG_DIRECTORY_SCANNER_COMPLETE,	/**< Directory Scanner complete */
	MS_MSG_DCM_SERVER_READY,			/**< Ready from dcm server */
	MS_MSG_DCM_EXTRACT_ALL_DONE,		/**< Done of all-dcm extracting */
	MS_MSG_MAX							/**< Invalid msg type */
}ms_msg_type_e;

#define MS_SCANNER_FIFO_PATH_REQ "/tmp/media-scanner-fifo-req"
#define MS_SCANNER_FIFO_PATH_RES "/tmp/media-scanner-fifo-res"
#define MS_SCANNER_FIFO_MODE 0666

typedef struct
{
	int sock_fd;
	int port;
	char *sock_path;
}ms_sock_info_s;

#define MS_UUID_SIZE 37 /* size of uuid + NULL */

typedef struct
{
	ms_msg_type_e msg_type;
	int pid;
	uid_t uid;
	int result;
	size_t msg_size; /*this is size of message below and this does not include the terminationg null byte ('\0'). */
	char storage_id[MS_UUID_SIZE];
	char msg[MAX_MSG_SIZE];
}ms_comm_msg_s;

typedef enum {
	CLIENT_SOCKET,
	SERVER_SOCKET
} ms_socket_type_e;

typedef enum {
	MS_MEDIA_THUMB_LARGE,
	MS_MEDIA_THUMB_SMALL,
} ms_thumb_type_e;

typedef struct {
	ms_msg_type_e msg_type;
} ms_thumb_server_msg;

typedef struct _thumbMsg{
	int msg_type;
	int request_id;
	int status;
	int pid;
	uid_t uid;
	int thumb_size;
	int thumb_width;
	int thumb_height;
	int origin_width;
	int origin_height;
	int origin_path_size;
	int dest_path_size;
	unsigned char *thumb_data;
	char org_path[MAX_FILEPATH_LEN];
	char dst_path[MAX_FILEPATH_LEN];
} thumbMsg;

typedef struct {
	ms_msg_type_e msg_type;
} ms_dcm_server_msg;

typedef struct {
	int msg_type;
	int pid;
	uid_t uid;
	size_t msg_size;
	char msg[MAX_FILEPATH_LEN];
} dcmMsg;

#endif /*_MEDIA_SERVER_IPC_H_*/

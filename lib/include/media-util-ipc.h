/*
 *  Media Utility
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

#ifndef _MEDIA_UTIL_IPC_H_
#define _MEDIA_UTIL_IPC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "media-server-ipc.h"

#define SERVER_IP			"127.0.0.1"

typedef enum {
	MS_PROTOCOL_UDP,
	MS_PROTOCOL_TCP
} ms_protocol_e;

int ms_ipc_create_client_socket(ms_protocol_e protocol, int timeout_sec, ms_sock_info_s* sock_info);
int ms_ipc_create_server_socket(ms_protocol_e protocol, ms_msg_port_type_e port, int *sock_fd);
int ms_ipc_send_msg_to_server(int sockfd, ms_msg_port_type_e port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr);
int ms_ipc_send_msg_to_server_tcp(int sockfd, ms_msg_port_type_e port, ms_comm_msg_s *send_msg, struct sockaddr_un *serv_addr);
int ms_ipc_send_msg_to_client(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_un *client_addr);
int ms_ipc_send_msg_to_client_tcp(int sockfd, ms_comm_msg_s *send_msg, struct sockaddr_un *client_addr);
int ms_ipc_receive_message(int sockfd, void *recv_msg, unsigned int msg_size, struct sockaddr_un *client_addr, unsigned int *size);
int ms_ipc_receive_message_tcp(int client_sock, ms_comm_msg_s *recv_msg);
int ms_ipc_wait_message(int sockfd, void  *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *size);
int ms_ipc_wait_block_message(int sockfd, void  *recv_msg, unsigned int msg_size, struct sockaddr_un *recv_addr, unsigned int *size);
int ms_ipc_delete_client_socket(ms_sock_info_s* sock_info);
int ms_ipc_accept_client_tcp(int serv_sock, int* client_sock);

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_IPC_H_*/

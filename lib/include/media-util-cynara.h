/*
 *  Media Utility
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
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

#ifndef _MEDIA_UTIL_CYNARA_H_
#define _MEDIA_UTIL_CYNARA_H_

#include <stdbool.h>

#define MEDIA_STORAGE_PRIVILEGE    "http://tizen.org/privilege/mediastorage"
#define CONTENT_WRITE_PRIVILEGE    "http://tizen.org/privilege/content.write"

typedef struct {
	uid_t uid;
	pid_t pid;
	char peersec[256];
} ms_peer_credentials;

int ms_cynara_initialize(void);
int ms_cynara_check(const ms_peer_credentials *creds, const char *privilege);
int ms_cynara_receive_untrusted_message(int sockfd, void *recv_msg, unsigned int msg_size,
		struct sockaddr_un *recv_addr, unsigned int *size, ms_peer_credentials *credentials);
int ms_cynara_get_credentials_from_connected_socket(int sockfd, ms_peer_credentials *credentials);
int ms_cynara_enable_credentials_passing(int fd);
void ms_cynara_finish(void);

#endif/* _MEDIA_UTIL_CYNARA_H_ */

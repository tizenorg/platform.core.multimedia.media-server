/*
 * media-server-dcm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jiyong Min <jiyong.min@samsung.com>
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
#include <sys/un.h>
#include <arpa/inet.h>
#include "media-common-types.h"
#include "media-server-ipc.h"

#ifndef _MEDIA_SERVER_DCM_H_
#define _MEDIA_SERVER_DCM_H_

#define MAX_DCM_REQUEST 100

int ms_dcm_get_server_pid();
void ms_dcm_reset_server_status();
gpointer ms_dcm_agent_start_thread(gpointer data);


#endif /*_MEDIA_SERVER_DCM_H_*/


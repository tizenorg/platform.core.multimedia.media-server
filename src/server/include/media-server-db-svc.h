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
 * @file		media-server-db-svc.h
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#ifndef _MEDIA_SERVER_DB_SVC_H_
#define _MEDIA_SERVER_DB_SVC_H_

#include "media-common-types.h"

typedef int (*CONNECT)(void**, uid_t, char **);
typedef int (*DISCONNECT)(void*, char **);
typedef int (*SET_ALL_STORAGE_ITEMS_VALIDITY)(void*, int, int, uid_t,char **);
typedef int (*CHECK_DB)(void*, uid_t uid, char **);

int
ms_load_functions(void);

void
ms_unload_functions(void);

int
ms_connect_db(void ***handle, uid_t uid);

int
ms_disconnect_db(void ***handle);

int
ms_invalidate_all_items(void **handle, ms_storage_type_t store_type, uid_t uid);

int
ms_check_db_upgrade(void **handle, uid_t uid);

#endif /*_MEDIA_SERVER_DB_SVC_H_*/
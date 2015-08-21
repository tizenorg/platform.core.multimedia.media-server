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

#ifndef _MEDIA_UTIL_DB_H_
#define _MEDIA_UTIL_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef void MediaDBHandle;		/**< Handle */

int media_db_connect(MediaDBHandle **handle, uid_t uid, bool needWrite);

int media_db_disconnect(MediaDBHandle *handle);

int media_db_request_update_db(const char *query_str, uid_t uid);

int media_db_request_update_db_batch_start(const char *query_str, uid_t uid);

int media_db_request_update_db_batch(const char *query_str, uid_t uid);

int media_db_request_update_db_batch_end(const char *query_str, uid_t uid);

int media_db_request_update_db_batch_clear(void);

/**
* @}
*/

#ifdef __cplusplus
}
#endif

#endif /*_MEDIA_UTIL_DB_H_*/

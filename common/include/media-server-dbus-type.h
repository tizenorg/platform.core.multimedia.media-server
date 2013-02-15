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
#ifndef _MEDIA_SERVER_DBUS_TYPES_H_
#define _MEDIA_SERVER_DBUS_TYPES_H_

#define MS_DBUS_PATH "/com/mediaserver/dbus/notify"
#define MS_DBUS_INTERFACE "com.mediaserver.dbus.Signal"
#define MS_DBUS_NAME "ms_db_updated"
#define MS_DBUS_MATCH_RULE "type='signal',interface='com.mediaserver.dbus.Signal'"

typedef enum {
	MS_DBUS_DB_UPDATED
} ms_dbus_noti_type_t;

#endif /*_MEDIA_SERVER_DBUS_TYPES_H_*/
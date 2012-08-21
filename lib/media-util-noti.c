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

/**
 * This file defines api utilities of contents manager engines.
 *
 * @file		media-util-noti.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "media-util-dbg.h"
#include "media-util-err.h"
#include "media-util-internal.h"
#include "media-util-noti.h"

static DBusHandlerResult
__message_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	db_update_cb user_cb = user_data;

	/* A Ping signal on the com.burtonini.dbus.Signal interface */
	if (dbus_message_is_signal (message, MS_MEDIA_DBUS_INTERFACE, MS_MEDIA_DBUS_NAME)) {
		DBusError error;
		dbus_uint16_t  noti_type;

		dbus_error_init (&error);
		if (dbus_message_get_args (message, &error, DBUS_TYPE_UINT16, &noti_type, DBUS_TYPE_INVALID)) {
			MSAPI_DBG("noti type: %d\n", noti_type);
			user_cb();
		} else {
			MSAPI_DBG("messgae received, but error getting message: %s\n", error.message);
			dbus_error_free (&error);
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

 int ms_noti_update_complete(void)
{
	int ret;
	int err = MS_MEDIA_ERR_NONE;

	ret = mkdir(MS_MEDIA_UPDATE_NOTI_PATH, 0777);
	if (ret != 0) {
		MSAPI_DBG("%s", strerror(errno));
		err = MS_MEDIA_ERR_OCCURRED;
	}

	return err;
}

int media_db_update_subscribe(db_update_cb user_cb)
{
	DBusConnection *bus;
	DBusError error;

	dbus_g_thread_init();

	dbus_error_init (&error);

	bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		MSAPI_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		return MS_MEDIA_ERR_DBUS_GET;
	}

	dbus_connection_setup_with_g_main (bus, NULL);

	/* listening to messages from all objects as no path is specified */
	dbus_bus_add_match (bus, MS_MEDIA_DBUS_MATCH_RULE, &error);
	if( !dbus_connection_add_filter (bus, __message_filter, user_cb, NULL))
		return MS_MEDIA_ERR_DBUS_ADD_FILTER;

	return MS_MEDIA_ERR_NONE;
}


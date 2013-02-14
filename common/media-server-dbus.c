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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "media-server-dbg.h"
#include "media-server-types.h"
#include "media-server-dbus.h"

void ms_dbus_init(void)
{
	DBusConnection *bus;
	DBusError error;

	/* Get a connection to the session bus */
	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		MS_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		return;
	}

	/* Set up this connection to work in a GLib event loop */
	dbus_connection_setup_with_g_main (bus, NULL);
}

gboolean ms_dbus_send_noti(ms_dbus_noti_type_t data)
{
	MS_DBG("");
	DBusMessage *message;
	DBusConnection *bus;
	DBusError error;
	dbus_uint16_t noti_type = data;

	/* Get a connection to the session bus */
	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus) {
		MS_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		return false;
	}

	/* Create a new signal on the "MS_DBUS_INTERFACE" interface,
	* from the object "MS_DBUS_PATH". */
	message = dbus_message_new_signal (MS_DBUS_PATH, MS_DBUS_INTERFACE, MS_DBUS_NAME);

	/* Append the notification type to the signal */
	dbus_message_append_args (message, DBUS_TYPE_UINT16, &noti_type, DBUS_TYPE_INVALID);

	/* Send the signal */
	dbus_connection_send (bus, message, NULL);

	/* Free the signal now we have finished with it */
	dbus_message_unref (message);

	/* Return TRUE to tell the event loop we want to be called again */
	return true;
}
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

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>

#include "media-util-internal.h"
#include "media-util-dbg.h"
#include "media-util.h"
#include "media-util-noti.h"

static GDBusConnection *g_bus = NULL;
static guint g_handler = 0;
static void *g_data_store = NULL;
static GMutex noti_mutex;
static int ref_count = 0;

#define MS_MEDIA_DBUS_NAME "ms_db_updated"

typedef struct noti_callback_data{
	db_update_cb user_callback;
	void *user_data;
} noti_callback_data;

static bool __gdbus_message_is_signal(const char *iface, const char *signal)
{
	if ((strcmp(iface, MS_MEDIA_DBUS_INTERFACE) == 0) && (strcmp(signal, MS_MEDIA_DBUS_NAME) == 0))
		return TRUE;

	return FALSE;
}

static void __get_message(GVariant *message, db_update_cb user_cb, void *userdata)
{
	gint32 item = -1;
	gint32 pid = 0;
	gint32 update_type = MS_MEDIA_UNKNOWN;
	gint32 content_type = -1;
	char *update_path = NULL;
	char *uuid = NULL;
	char *mime_type = NULL;

	int item_number = g_variant_n_children(message);

	if (item_number == 7)
		g_variant_get(message, "(iiissis)", &item, &pid, &update_type, &update_path, &uuid, &content_type, &mime_type);
	else if (item_number == 5)
		g_variant_get(message, "(iiiss)", &item, &pid, &update_type, &update_path, &uuid);
	else if (item_number == 4)
		g_variant_get(message, "(iiis)", &item, &pid, &update_type, &update_path);

	if (item == MS_MEDIA_ITEM_DIRECTORY)
		content_type = MS_MEDIA_UNKNOWN;

	/* getting data complete */
	user_cb(pid,
			item,
			update_type,
			update_path,
			uuid,
			content_type,
			mime_type,
			userdata);

	MS_SAFE_FREE(update_path);
	MS_SAFE_FREE(uuid);
	MS_SAFE_FREE(mime_type);
}

static void __message_filter(GDBusConnection* connection,
					const gchar* sender_name,
					const gchar* object_path,
					const gchar* interface_name,
					const gchar* signal_name,
					GVariant* parameters,
					gpointer user_data)
{
	if (__gdbus_message_is_signal(interface_name, signal_name)) {
		db_update_cb user_cb = ((noti_callback_data*)user_data)->user_callback;
		void *userdata = ((noti_callback_data*)user_data)->user_data;

		__get_message(parameters, user_cb, userdata);
	}
}

int media_db_update_subscribe(db_update_cb user_cb, void *user_data)
{
	int ret = MS_MEDIA_ERR_NONE;
	GError *error = NULL;
	noti_callback_data *callback_data = NULL;

	g_mutex_lock(&noti_mutex);

	if (g_bus == NULL) {
		g_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!g_bus) {
			MSAPI_DBG ("Failed to connect to the g D-BUS daemon: %s", error->message);
			g_error_free (error);
			ret = MS_MEDIA_ERR_INTERNAL;
			goto ERROR;
		}

		MS_MALLOC(callback_data, sizeof(noti_callback_data));
		if (callback_data == NULL) {
			MSAPI_DBG_ERR("MS_MALLOC failed");
			ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
			goto ERROR;
		}
		callback_data->user_callback = user_cb;
		callback_data->user_data = user_data;

		/* listening to messages from all objects as no path is specified */
		g_handler = g_dbus_connection_signal_subscribe(
						g_bus,
						NULL,
						MS_MEDIA_DBUS_INTERFACE,
						MS_MEDIA_DBUS_NAME,
						MS_MEDIA_DBUS_PATH,
						NULL,
						G_DBUS_SIGNAL_FLAGS_NONE,
						__message_filter,
						callback_data,
						NULL);
		g_data_store = (void *)callback_data;
	}

	ref_count ++;

	g_mutex_unlock(&noti_mutex);

	return MS_MEDIA_ERR_NONE;

ERROR:

	if (g_bus != NULL) {
		g_object_unref(g_bus);
		g_bus = NULL;
	}
	MS_SAFE_FREE(callback_data);

	g_mutex_unlock(&noti_mutex);

	return ret;
}

int media_db_update_unsubscribe(void)
{
	if (g_bus == NULL) {
		return MS_MEDIA_ERR_NONE;
	}

	g_mutex_lock(&noti_mutex);

	if (ref_count == 1) {
		g_dbus_connection_signal_unsubscribe(g_bus, g_handler);
		g_object_unref(g_bus);
		g_bus = NULL;

		/*Release Callback*/
		MS_SAFE_FREE(g_data_store);
		g_data_store = NULL;
	}

	ref_count --;

	g_mutex_unlock(&noti_mutex);

	return MS_MEDIA_ERR_NONE;
}

int media_db_update_send(int pid, /* mandatory */
							media_item_type_e item, /* mandatory */
							media_item_update_type_e update_type, /* mandatory */
							char* path, /* mandatory */
							char* uuid, /* optional */
							media_type_e media_type, /* optional */
							char *mime_type /* optional */
							)
{
	GVariant *message = NULL;
	GDBusConnection *bus = NULL;
	GError *error = NULL;

	bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!bus) {
		MSAPI_DBG ("Failed to get gdbus connection: %s", error->message);
		g_error_free (error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	if (item == MS_MEDIA_ITEM_FILE) {
		MSAPI_DBG("FILE CHANGED");
		if (uuid != NULL && mime_type != NULL) {
			/* fill all datas */
			message = g_variant_new("(iiissis)", item, pid, update_type, path, uuid, media_type, mime_type);
		} else {
			MSAPI_DBG_ERR("uuid or mime_type is NULL");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}
	} else if (item == MS_MEDIA_ITEM_DIRECTORY) {
		MSAPI_DBG("DIRECTORY CHANGED");
		/* fill all datas */
		if(uuid != NULL) {
			message = g_variant_new("(iiiss)", item, pid, update_type, path, uuid);
		} else {
			message = g_variant_new("(iiis)", item, pid, update_type, path);
		}
	} else {
		MSAPI_DBG("this request is wrong");
	}

	gboolean emmiting = g_dbus_connection_emit_signal(
					bus,
					NULL,
					MS_MEDIA_DBUS_PATH,
					MS_MEDIA_DBUS_INTERFACE,
					MS_MEDIA_DBUS_NAME,
					message,
					&error);
	if (!emmiting)
	{
		MSAPI_DBG_ERR("g_dbus_connection_emit_signal failed : %s", error?error->message:"none");
		if (error)
		{
			MSAPI_DBG_ERR("Error in g_dbus_connection_emit_signal");
			g_object_unref(bus);
			g_error_free(error);
		}
		return MS_MEDIA_ERR_INTERNAL;
	}
	MSAPI_DBG("success send notification");

	gboolean flush = FALSE;
	flush = g_dbus_connection_flush_sync(bus, NULL, &error);
	if (!flush)
	{
		MSAPI_DBG_ERR("g_dbus_connection_flush_sync failed");
		if (error)
		{
			MSAPI_DBG_ERR("error : [%s]", error->message);
			g_object_unref(bus);
			g_error_free(error);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	g_object_unref(bus);

	/* Return TRUE to tell the event loop we want to be called again */
	return MS_MEDIA_ERR_NONE;
}

int media_db_update_send_v2(int pid, /* mandatory */
							media_item_type_e item, /* mandatory */
							media_item_update_type_e update_type, /* mandatory */
							char* path, /* mandatory */
							char* uuid, /* optional */
							media_type_e media_type, /* optional */
							char *mime_type /* optional */
							)
{
	GVariant *message = NULL;
	GDBusConnection *bus = NULL;
	GError *error = NULL;

	bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!bus) {
		MSAPI_DBG ("Failed to get gdbus connection: %s", error->message);
		g_error_free (error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	if (item == MS_MEDIA_ITEM_FILE) {
		MSAPI_DBG("FILE CHANGED");
		if (uuid != NULL && mime_type != NULL) {
			/* fill all datas */
			message = g_variant_new("(iiissis)", item, pid, update_type, path, uuid, media_type, mime_type);
		} else {
			MSAPI_DBG_ERR("uuid or mime_type is NULL");
			return MS_MEDIA_ERR_INVALID_PARAMETER;
		}
	} else if (item == MS_MEDIA_ITEM_DIRECTORY) {
		MSAPI_DBG("DIRECTORY CHANGED");
		/* fill all datas */
		if(uuid != NULL) {
			message = g_variant_new("(iiiss)", item, pid, update_type, path, uuid);
		} else {
			message = g_variant_new("(iiis)", item, pid, update_type, path);
		}
	} else {
		MSAPI_DBG("this request is wrong");
	}

	gboolean emmiting = g_dbus_connection_emit_signal(
					bus,
					NULL,
					MS_MEDIA_DBUS_PATH,
					MS_MEDIA_DBUS_INTERFACE,
					MS_MEDIA_DBUS_NAME,
					message,
					&error);
	if (!emmiting)
	{
		MSAPI_DBG_ERR("g_dbus_connection_emit_signal failed : %s", error?error->message:"none");
		if (error)
		{
			MSAPI_DBG_ERR("Error in g_dbus_connection_emit_signal");
			g_object_unref(bus);
			g_error_free(error);
		}
		return MS_MEDIA_ERR_INTERNAL;
	}
	MSAPI_DBG("success send notification");

	gboolean flush = FALSE;
	flush = g_dbus_connection_flush_sync(bus, NULL, &error);
	if (!flush)
	{
		MSAPI_DBG_ERR("g_dbus_connection_flush_sync failed");
		if (error)
		{
			MSAPI_DBG_ERR("error : [%s]", error->message);
			g_object_unref(bus);
			g_error_free(error);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	g_object_unref(bus);

	/* Return TRUE to tell the event loop we want to be called again */
	return MS_MEDIA_ERR_NONE;
}


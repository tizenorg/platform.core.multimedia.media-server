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
#include <gio/gio.h>

#include "media-util-internal.h"
#include "media-util-dbg.h"
#include "media-util.h"
#include "media-util-noti-internal.h"

GArray *handle_list_internal;
static GMutex mutex_internal;

#define MS_MEDIA_DBUS_NAME_INTERNAL "ms_db_updated_internal"

typedef struct internal_noti_cb_data {
	GDBusConnection *gdbus;
	int handler;
	db_update_cb user_callback;
	void *user_data;
} internal_noti_cb_data;

int media_db_update_send_internal(int pid, /* mandatory */
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
		MSAPI_DBG("Failed to get gdbus connection: %s", error->message);
		g_error_free(error);
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
		if (uuid != NULL) {
			message = g_variant_new("(iiiss)", item, pid, update_type, path, uuid);
		} else {
			message = g_variant_new("(iiis)", item, pid, update_type, path);
		}
	} else {
		MSAPI_DBG("this request is wrong");
	}

	if (message == NULL) {
		MSAPI_DBG_ERR("g_dbus_message_new_signal failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	/* Send the signal */
	gboolean emmiting = g_dbus_connection_emit_signal(
					bus,
					NULL,
					MS_MEDIA_DBUS_PATH,
					MS_MEDIA_DBUS_INTERFACE,
					MS_MEDIA_DBUS_NAME_INTERNAL,
					message,
					&error);
	if (!emmiting) {
		MSAPI_DBG_ERR("g_dbus_connection_emit_signal failed : %s", error ? error->message : "none");
		if (error) {
			MSAPI_DBG_ERR("Error in g_dbus_connection_emit_signal");
			g_object_unref(bus);
			g_error_free(error);
		}
		return MS_MEDIA_ERR_INTERNAL;
	}
	MSAPI_DBG_ERR("success send notification");

	g_object_unref(bus);

	/* Return TRUE to tell the event loop we want to be called again */
	return MS_MEDIA_ERR_NONE;
}

static bool __gdbus_message_is_signal(const char *iface, const char *signal)
{
	if ((strcmp(iface, MS_MEDIA_DBUS_INTERFACE) == 0) && (strcmp(signal, MS_MEDIA_DBUS_NAME_INTERNAL) == 0))
		return TRUE;

	return FALSE;
}

static void __get_message_internal(GVariant *message, db_update_cb user_cb, void *userdata)
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

static void __message_filter_internal(GDBusConnection* connection,
					const gchar* sender_name,
					const gchar* object_path,
					const gchar* interface_name,
					const gchar* signal_name,
					GVariant* parameters,
					gpointer user_data)
{
	if (__gdbus_message_is_signal(interface_name, signal_name)) {
		db_update_cb user_cb = ((internal_noti_cb_data*)user_data)->user_callback;
		void *userdata = ((internal_noti_cb_data*)user_data)->user_data;

		__get_message_internal(parameters, user_cb, userdata);
	}
}

int media_db_update_subscribe_internal(MediaNotiHandle *handle, db_update_cb user_cb, void *user_data)
{
	int ret = MS_MEDIA_ERR_NONE;
	int handler = 0;
	internal_noti_cb_data *noti_data = NULL;
	GError *error = NULL;
	GDBusConnection *gdbus = NULL;

	if (gdbus == NULL) {
#if 0
		gchar *address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!address) {
			MSAPI_DBG_ERR("Failed to get the address: %s", error ? error->message : "none");
			g_error_free(error);
			return MS_MEDIA_ERR_INTERNAL;
		}
		MSAPI_DBG("\tType(%s)", address);
		gdbus = g_dbus_connection_new_for_address_sync(address, G_DBUS_CONNECTION_FLAGS_NONE, NULL, NULL, &error);
#endif
		gdbus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!gdbus) {
			MSAPI_DBG_ERR("Failed to connect to the g D-BUS daemon: %s", error ? error->message : "none");
			g_error_free(error);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	MS_MALLOC(noti_data, sizeof(internal_noti_cb_data));
	if (noti_data == NULL) {
		MSAPI_DBG_ERR("MS_MALLOC failed");
		ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
		goto ERROR;
	}
	noti_data->user_callback = user_cb;
	noti_data->user_data = user_data;

	handler = g_dbus_connection_signal_subscribe(
						gdbus,
						NULL,
						MS_MEDIA_DBUS_INTERFACE,
						MS_MEDIA_DBUS_NAME_INTERNAL,
						MS_MEDIA_DBUS_PATH,
						NULL,
						G_DBUS_SIGNAL_FLAGS_NONE,
						__message_filter_internal,
						noti_data,
						NULL);

	noti_data->gdbus = gdbus;
	noti_data->handler = handler;
	*handle = noti_data;

	g_mutex_lock(&mutex_internal);

	if (handle_list_internal == NULL) {
		handle_list_internal = g_array_new(FALSE, FALSE, sizeof(MediaNotiHandle));
		if (handle_list_internal == NULL) {
			g_mutex_unlock(&mutex_internal);
			ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
			goto ERROR;
		}
	}

	g_array_append_val(handle_list_internal, *handle);

	g_mutex_unlock(&mutex_internal);

	return MS_MEDIA_ERR_NONE;

ERROR:

	if (gdbus != NULL) {
		g_object_unref(gdbus);
		gdbus = NULL;
	}

	MS_SAFE_FREE(noti_data);

	return ret;
}

static int _find_handle(MediaNotiHandle handle, int *idx)
{
	unsigned int i;
	int ret = MS_MEDIA_ERR_NONE;
	bool find_flag = false;
	MediaNotiHandle data;

	/*delete all node*/
	if (handle_list_internal != NULL) {
		for (i = 0; i < handle_list_internal->len; i++) {
			data = g_array_index(handle_list_internal , MediaNotiHandle, i);
			MSAPI_DBG_ERR("%x %x", handle, data);
			if (data == handle) {
				MSAPI_DBG("FIND HANDLE");
				*idx = i;
				find_flag = true;
				break;
			}
		}
	} else {
		ret = MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	if (find_flag == false)
		ret = MS_MEDIA_ERR_INVALID_PARAMETER;

	return ret;
}

int media_db_update_unsubscribe_internal(MediaNotiHandle handle, clear_user_data_cb clear_cb)
{
	int idx = -1;
	int ret = MS_MEDIA_ERR_NONE;
	int err;

	if (handle == NULL) {
		MSAPI_DBG_ERR("INVALID PARAMETER");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	g_mutex_lock(&mutex_internal);

	err = _find_handle(handle, &idx);
	if (err == MS_MEDIA_ERR_NONE) {
		GDBusConnection *gdbus = ((internal_noti_cb_data*)handle)->gdbus;
		g_dbus_connection_signal_unsubscribe(gdbus, ((internal_noti_cb_data*)handle)->handler);
		g_object_unref(gdbus);
		g_array_remove_index(handle_list_internal, idx);

		if (clear_cb != NULL)
			clear_cb(((internal_noti_cb_data*)handle)->user_data);

		MS_SAFE_FREE(handle);

		if ((handle_list_internal != NULL) && (handle_list_internal->len == 0)) {
			g_array_free(handle_list_internal, FALSE);
			handle_list_internal = NULL;
		}
	} else {
		MSAPI_DBG_ERR("PARAMETER DOES NOT FIND");
		ret = MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	g_mutex_unlock(&mutex_internal);

	return ret;
}



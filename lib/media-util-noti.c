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

#include "media-util-internal.h"
#include "media-util-dbg.h"
#include "media-util.h"

DBusConnection *g_bus;
void *g_data_store;
GMutex *noti_mutex = NULL;

typedef struct noti_callback_data{
	db_update_cb user_callback;
	void *user_data;
} noti_callback_data;

static void
__free_data_fuction(void *memory)
{
	MS_SAFE_FREE(memory);
}

static DBusHandlerResult
__message_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	db_update_cb user_cb = ((noti_callback_data*)user_data)->user_callback;
	void *userdata = ((noti_callback_data*)user_data)->user_data;

	if (noti_mutex != NULL) {
		g_mutex_lock(noti_mutex);
	} else {
		MSAPI_DBG_ERR("subscribe function is already unreferenced");
	}

	/* A Ping signal on the com.burtonini.dbus.Signal interface */
	if (dbus_message_is_signal (message, MS_MEDIA_DBUS_INTERFACE, MS_MEDIA_DBUS_NAME)) {
		int i = 0;
		DBusError error;
		DBusMessageIter read_iter;
		DBusBasicValue value[7];

		dbus_int32_t item = -1;
		dbus_int32_t pid = 0;
		dbus_int32_t update_type = MS_MEDIA_UNKNOWN;
		dbus_int32_t content_type = -1;
		char *update_path = NULL;
		char *uuid = NULL;
		char *mime_type = NULL;

		dbus_error_init (&error);
		MSAPI_DBG("size [%d]", sizeof(value));
		memset(value, 0x0, sizeof(value));

		/* get data from dbus message */
		dbus_message_iter_init (message, &read_iter);
		while (dbus_message_iter_get_arg_type (&read_iter)  != DBUS_TYPE_INVALID){
	                dbus_message_iter_get_basic (&read_iter, &value[i]);
			dbus_message_iter_next (&read_iter);
			i ++;
		}

		item = value[0].i32;
		pid = value[1].i32;
		update_type = value[2].i32;
		update_path = strdup(value[3].str);
		if (value[4].str != NULL) uuid = strdup(value[4].str);
		content_type = value[5].i32;
		if (value[6].str != NULL) mime_type = strdup(value[6].str);

		/* getting data complete */
		user_cb(pid,
				item,
				update_type,
				update_path,
				uuid,
				content_type,
				mime_type,
				userdata);

		g_mutex_unlock(noti_mutex);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	g_mutex_unlock(noti_mutex);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int media_db_update_subscribe(db_update_cb user_cb, void *user_data)
{
	DBusError error;
	noti_callback_data *callback_data = NULL;

	if (noti_mutex == NULL) {
		noti_mutex = g_mutex_new();
	}

	if (g_bus == NULL) {
		dbus_g_thread_init();

		dbus_error_init (&error);

		g_bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
		if (!g_bus) {
			MSAPI_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
			dbus_error_free (&error);
			return MS_MEDIA_ERR_DBUS_GET;
		}

		dbus_connection_setup_with_g_main (g_bus, NULL);

		callback_data = malloc(sizeof(noti_callback_data));
		MS_MALLOC(callback_data, sizeof(noti_callback_data));
		if (callback_data == NULL) {
			MSAPI_DBG_ERR("MS_MALLOC failed");
			return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
		}
		callback_data->user_callback = user_cb;
		callback_data->user_data = user_data;

		/* listening to messages from all objects as no path is specified */
		dbus_bus_add_match (g_bus, MS_MEDIA_DBUS_MATCH_RULE, &error);
		if( !dbus_connection_add_filter (g_bus, __message_filter, callback_data, __free_data_fuction)) {
			MS_SAFE_FREE(callback_data);
			return MS_MEDIA_ERR_DBUS_ADD_FILTER;
		}
		g_data_store = (void *)callback_data;
	}

	return MS_MEDIA_ERR_NONE;
}

int media_db_update_unsubscribe(void)
{
	g_mutex_lock(noti_mutex);

	if (g_bus != NULL) {
		dbus_connection_remove_filter(g_bus, __message_filter, g_data_store);

		dbus_connection_unref(g_bus);

		g_bus = NULL;
	}

	g_mutex_unlock(noti_mutex);

	if (noti_mutex) g_mutex_free(noti_mutex);
	noti_mutex = NULL;

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
	DBusMessage *message;
	DBusConnection *bus;
	DBusError error;

	/* Get a connection to the session bus */
	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		MSAPI_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		return MS_MEDIA_ERR_DBUS_GET;
	}

	/* Create a new signal on the "MS_DBUS_INTERFACE" interface,
	* from the object "MS_DBUS_PATH". */
	message = dbus_message_new_signal (MS_MEDIA_DBUS_PATH, MS_MEDIA_DBUS_INTERFACE, MS_MEDIA_DBUS_NAME);
	if (message != NULL) {
		if (item == MS_MEDIA_ITEM_FILE) {
			MSAPI_DBG("FILE CHANGED");
			if (uuid != NULL && mime_type != NULL) {
			/* fill all datas */
				dbus_message_append_args (message,
										DBUS_TYPE_INT32, &item,
										DBUS_TYPE_INT32, &pid,
										DBUS_TYPE_INT32, &update_type,
										DBUS_TYPE_STRING, &path,
										DBUS_TYPE_STRING, &uuid,
										DBUS_TYPE_INT32, &media_type,
										DBUS_TYPE_STRING, &mime_type,
										DBUS_TYPE_INVALID);
			} else {
				MSAPI_DBG_ERR("uuid or mime_type is NULL");
				return MS_MEDIA_ERR_INVALID_PARAMETER;
			}
		} else if (item == MS_MEDIA_ITEM_DIRECTORY) {
			MSAPI_DBG("DIRECTORY CHANGED");
			/* fill all datas */
			if(uuid != NULL) {
				dbus_message_append_args (message,
										DBUS_TYPE_INT32, &item,
										DBUS_TYPE_INT32, &pid,
										DBUS_TYPE_INT32, &update_type,
										DBUS_TYPE_STRING, &path,
										DBUS_TYPE_STRING, &uuid,
										DBUS_TYPE_INVALID);
			} else {
				dbus_message_append_args (message,
										DBUS_TYPE_INT32, &item,
										DBUS_TYPE_INT32, &pid,
										DBUS_TYPE_INT32, &update_type,
										DBUS_TYPE_STRING, &path,
										DBUS_TYPE_INVALID);
			}
		} else {
			MSAPI_DBG("this request is wrong");
		}

		/* Send the signal */
		dbus_connection_send (bus, message, NULL);

		/* Free the signal now we have finished with it */
		dbus_message_unref (message);

		MSAPI_DBG_ERR("success send notification");
	} else {
		MSAPI_DBG_ERR("dbus_message_new_signal failed");
	}

	/* Return TRUE to tell the event loop we want to be called again */
	return MS_MEDIA_ERR_NONE;
}


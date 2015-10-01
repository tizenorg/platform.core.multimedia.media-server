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
#include "media-util-noti-internal.h"

GArray *handle_list_internal;
static GMutex mutex_internal;
int ref_count;

#define MS_MEDIA_DBUS_NAME_INTERNAL "ms_db_updated_internal"

typedef struct internal_noti_cb_data{
	DBusConnection *dbus;
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
	DBusMessage *message = NULL;
	DBusConnection *bus = NULL;
	DBusError error;
	unsigned char *path_array = NULL;
	int path_length = strlen(path) + 1;

	/* Get a connection to the session bus */
	dbus_error_init (&error);
	bus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!bus) {
		MSAPI_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	message = dbus_message_new_signal (MS_MEDIA_DBUS_PATH, MS_MEDIA_DBUS_INTERFACE, MS_MEDIA_DBUS_NAME_INTERNAL);
	if (message != NULL) {
		path_array = malloc(sizeof(unsigned char) * path_length);
		memcpy(path_array, path, path_length);

		if (item == MS_MEDIA_ITEM_FILE) {
			MSAPI_DBG("FILE CHANGED");
			if (uuid != NULL && mime_type != NULL) {
			/* fill all datas */
				dbus_message_append_args (message,
										DBUS_TYPE_INT32, &item,
										DBUS_TYPE_INT32, &pid,
										DBUS_TYPE_INT32, &update_type,
										DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &path_array, path_length,
										DBUS_TYPE_STRING, &uuid,
										DBUS_TYPE_INT32, &media_type,
										DBUS_TYPE_STRING, &mime_type,
										DBUS_TYPE_INVALID);
			} else {
				MSAPI_DBG_ERR("uuid or mime_type is NULL");
				MS_SAFE_FREE(path_array);
				dbus_message_unref (message);
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
										DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &path_array, path_length,
										DBUS_TYPE_STRING, &uuid,
										DBUS_TYPE_INVALID);
			} else {
				dbus_message_append_args (message,
										DBUS_TYPE_INT32, &item,
										DBUS_TYPE_INT32, &pid,
										DBUS_TYPE_INT32, &update_type,
										DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &path_array, path_length,
										DBUS_TYPE_INVALID);
			}
		} else {
			MSAPI_DBG("this request is wrong");
		}

		MS_SAFE_FREE(path_array);

		/* Send the signal */
		dbus_connection_send (bus, message, NULL);

		/* Free the signal now we have finished with it */
		dbus_message_unref (message);

		MSAPI_DBG_ERR("success send notification");
	} else {
		MSAPI_DBG_ERR("dbus_message_new_signal failed");
	}

	dbus_connection_unref(bus);

	/* Return TRUE to tell the event loop we want to be called again */
	return MS_MEDIA_ERR_NONE;
}


DBusHandlerResult
__get_message_internal(DBusMessage *message, db_update_cb user_cb, void *userdata)
{
	/* A Ping signal on the com.burtonini.dbus.Signal interface */
	if (dbus_message_is_signal (message, MS_MEDIA_DBUS_INTERFACE, MS_MEDIA_DBUS_NAME_INTERNAL)) {
		int i = 0;
		int current_type = DBUS_TYPE_INVALID;
		DBusError error;
		DBusMessageIter read_iter;
		DBusBasicValue value[6];

		dbus_int32_t item = -1;
		dbus_int32_t pid = 0;
		dbus_int32_t update_type = MS_MEDIA_UNKNOWN;
		dbus_int32_t content_type = -1;
		char *update_path = NULL;
		char *uuid = NULL;
		char *mime_type = NULL;
		void *recevie_path = NULL;
		int path_len = 0;

		dbus_error_init (&error);
		MSAPI_DBG("size [%d]", sizeof(value));
		memset(value, 0x0, sizeof(value));

		/* get data from dbus message */
		dbus_message_iter_init (message, &read_iter);
		while ((current_type = dbus_message_iter_get_arg_type (&read_iter)) != DBUS_TYPE_INVALID){
			if (current_type == DBUS_TYPE_ARRAY) {
				DBusMessageIter sub;
				dbus_message_iter_recurse(&read_iter, &sub);
				dbus_message_iter_get_fixed_array(&sub, &recevie_path, &path_len);
			} else {
		                dbus_message_iter_get_basic (&read_iter, &value[i]);
				i ++;
			}
			dbus_message_iter_next (&read_iter);
		}

		item = value[0].i32;
		pid = value[1].i32;
		update_type = value[2].i32;
		update_path = strndup(recevie_path, path_len);
		if (value[3].str != NULL) uuid = strdup(value[3].str);
		content_type = value[4].i32;
		if (value[5].str != NULL) mime_type = strdup(value[5].str);

		if (item == MS_MEDIA_ITEM_DIRECTORY)
			content_type = MS_MEDIA_UNKNOWN;

		/* getting data complete */
		MSAPI_DBG("update type[%d]", update_type);
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

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult
__message_filter_internal(DBusConnection *connection, DBusMessage *message, void *user_data)
{
	db_update_cb user_cb = ((internal_noti_cb_data*)user_data)->user_callback;
	void *userdata = ((internal_noti_cb_data*)user_data)->user_data;
	DBusHandlerResult ret;

	ret = __get_message_internal(message, user_cb, userdata);

	return ret;
}

int media_db_update_subscribe_internal(MediaNotiHandle *handle, db_update_cb user_cb, void *user_data)
{
	int ret = MS_MEDIA_ERR_NONE;
	internal_noti_cb_data *noti_data = NULL;
	DBusError error;
	DBusConnection *dbus;

	dbus_g_thread_init();

	dbus_error_init (&error);

	dbus = dbus_bus_get_private (DBUS_BUS_SYSTEM, &error);
	if (!dbus) {
		MSAPI_DBG ("Failed to connect to the D-BUS daemon: %s", error.message);
		dbus_error_free (&error);
		ret = MS_MEDIA_ERR_INTERNAL;
		goto ERROR;
	}

	dbus_connection_setup_with_g_main (dbus, NULL);

	MS_MALLOC(noti_data, sizeof(internal_noti_cb_data));
	if (noti_data == NULL) {
		MSAPI_DBG_ERR("MS_MALLOC failed");
		ret = MS_MEDIA_ERR_OUT_OF_MEMORY;
		goto ERROR;
	}
	noti_data->user_callback = user_cb;
	noti_data->user_data = user_data;

	/* listening to messages from all objects as no path is specified */
	dbus_bus_add_match (dbus, MS_MEDIA_DBUS_MATCH_RULE, &error);
	if( !dbus_connection_add_filter (dbus, __message_filter_internal, noti_data, NULL)) {
		dbus_bus_remove_match (dbus, MS_MEDIA_DBUS_MATCH_RULE, NULL);
		ret =  MS_MEDIA_ERR_INTERNAL;
		goto ERROR;
	}

	noti_data->dbus = dbus;
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

	if (dbus != NULL) {
		dbus_connection_close(dbus);
		dbus_connection_unref(dbus);
		dbus = NULL;
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
	if(handle_list_internal != NULL) {
		for (i =0; i < handle_list_internal->len; i++) {
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
	if(err == MS_MEDIA_ERR_NONE) {
		DBusConnection *dbus = ((internal_noti_cb_data*)handle)->dbus;

		dbus_connection_remove_filter(dbus, __message_filter_internal, (internal_noti_cb_data*)handle);
		dbus_bus_remove_match (dbus, MS_MEDIA_DBUS_MATCH_RULE, NULL);
		dbus_connection_close(dbus);
		dbus_connection_unref(dbus);
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



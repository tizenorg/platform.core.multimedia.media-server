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

#include <errno.h>
#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <sys/stat.h>

#include "media-util.h"

#include "media-common-dbg.h"
#include "media-common-types.h"
#include "media-common-system.h"


//////////////////////////////////////////////////////////////////////////////
/// CHECK THE STORATE(MMC, USB)  STATE
//////////////////////////////////////////////////////////////////////////////

#define DEVICED_BUS_NAME       "org.tizen.system.deviced"
#define DEVICED_OBJECT_PATH    "/Org/Tizen/System/DeviceD"
#define DEVICED_INTERFACE_NAME DEVICED_BUS_NAME

#define DEVICED_PATH_BLOCK                  DEVICED_OBJECT_PATH"/Block"
#define DEVICED_PATH_BLOCK_DEVICES          DEVICED_PATH_BLOCK"/Devices"
#define DEVICED_PATH_BLOCK_MANAGER          DEVICED_PATH_BLOCK"/Manager"
#define DEVICED_INTERFACE_BLOCK             DEVICED_INTERFACE_NAME".Block"
#define DEVICED_INTERFACE_BLOCK_MANAGER     DEVICED_INTERFACE_NAME".BlockManager"

#define BLOCK_OBJECT_ADDED      "ObjectAdded"
#define BLOCK_OBJECT_REMOVED    "ObjectRemoved"
#define BLOCK_DEVICE_CHANGED    "DeviceChanged"

#define BLOCK_DEVICE_METHOD "GetDeviceList"
#define BLOCK_DEVICE_USB "scsi"
#define BLOCK_DEVICE_MMC "mmc"
#define BLOCK_DEVICE_ALL "all"

static GDBusConnection *g_usb_bus;

static int g_usb_handler;

typedef struct block_cb_data{
	block_changed_cb usr_cb;
	void *usr_data;
} block_cb_data;

block_cb_data *g_usb_cb_data = NULL;

static void __ms_block_changed(GDBusConnection* connection,
					const gchar* sender_name,
					const gchar* object_path,
					const gchar* interface_name,
					const gchar* signal_name,
					GVariant* parameters,
					gpointer user_data)
{
	const char *devnode = NULL;
	const char *mount_path = NULL;
	GVariant *tmp;
	gsize size = 0;
	block_cb_data *cb_data = (block_cb_data *)user_data;
	void *usr_cb = cb_data->usr_cb;
	void *usr_data = cb_data->usr_data;
	ms_block_info_s *block_info = NULL;

	MS_MALLOC(block_info, sizeof(ms_block_info_s));
	if (block_info == NULL) {
		MS_DBG_ERR("malloc failed");
		return;
	}

	tmp = g_variant_get_child_value(parameters, 0);
	block_info->block_type = g_variant_get_int32(tmp);
	MS_DBG_ERR("block_type : %d", block_info->block_type);

	tmp = g_variant_get_child_value(parameters, 1);
	devnode = g_variant_get_string (tmp, &size);
	MS_DBG_ERR("devnode : %s", devnode);

	tmp = g_variant_get_child_value(parameters, 8);
	mount_path = g_variant_get_string (tmp, &size);
	if (mount_path != NULL) {
		block_info->mount_path = strdup(mount_path);
		MS_DBG_ERR("mount_point : %s", block_info->mount_path);
	}

	tmp = g_variant_get_child_value(parameters, 9);
	block_info->state = g_variant_get_int32 (tmp);
	MS_DBG_ERR("state : %d", block_info->state);

	((block_changed_cb)usr_cb)(block_info, usr_data);
	MS_SAFE_FREE(block_info->mount_path);
	MS_SAFE_FREE(block_info);

	MS_DBG_ERR("user callback done");
}

static int __ms_sys_subscribe_device_block_event(block_changed_cb usr_callback, void *usr_data)
{
	int ret = MS_MEDIA_ERR_NONE;
	GError *error = NULL;

	MS_DBG_FENTER();

	g_usb_cb_data = malloc(sizeof(block_cb_data));
	if (g_usb_cb_data == NULL) {
		MS_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_usb_cb_data->usr_cb = usr_callback;
	g_usb_cb_data->usr_data = usr_data;

	if (g_usb_bus == NULL) {
		g_usb_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!g_usb_bus) {
			MS_DBG_ERR ("Failed to connect to the g D-BUS daemon: %s", error->message);
			g_error_free (error);
			ret = MS_MEDIA_ERR_INTERNAL;
			goto ERROR;
		}
	}

	/* listening to messages from all objects as no path is specified */
	g_usb_handler = g_dbus_connection_signal_subscribe(
					g_usb_bus,
					NULL,
					DEVICED_INTERFACE_BLOCK,
					BLOCK_DEVICE_CHANGED,
					NULL,
					NULL,
					G_DBUS_SIGNAL_FLAGS_NONE,
					__ms_block_changed,
					g_usb_cb_data,
					NULL);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;

ERROR:

	if (g_usb_bus != NULL) {
		g_object_unref(g_usb_bus);
		g_usb_bus = NULL;
	}

	MS_SAFE_FREE(g_usb_cb_data);

	MS_DBG_FLEAVE();

	return ret;
}

int ms_sys_set_device_block_event_cb(block_changed_cb usr_callback, void *usr_data)
{
	int ret = MS_MEDIA_ERR_NONE;

	MS_DBG_FENTER();

	ret = __ms_sys_subscribe_device_block_event(usr_callback, usr_data);

	return ret;
}

int ms_sys_unset_device_block_event_cb(void)
{
	if (g_usb_bus == NULL) {
		return MS_MEDIA_ERR_NONE;
	}

	g_dbus_connection_signal_unsubscribe(g_usb_bus, g_usb_handler);
	g_object_unref(g_usb_bus);
	g_usb_bus = NULL;

	/*Release Callback*/
	MS_SAFE_FREE(g_usb_cb_data);

	return MS_MEDIA_ERR_NONE;
}

#define DBUS_REPLY_TIMEOUT  (-1)
static int __ms_dbus_method_sync(const char *dest, const char *path,
        const char *interface, const char *method, const char *param, GArray **dev_list)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iiiter;
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter iter;
	DBusMessageIter aiter, piter;
	int result;

	int val_int;
	char *val_str;
	bool val_bool;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		MS_DBG_ERR("dbus_bus_get error");
		return MS_MEDIA_ERR_INTERNAL;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		MS_DBG_ERR("dbus_message_new_method_call(%s:%s-%s)",
		path, interface, method);
		return MS_MEDIA_ERR_INTERNAL;
	}

	dbus_message_iter_init_append(msg, &iiiter);
	dbus_message_iter_append_basic(&iiiter, DBUS_TYPE_STRING, &param);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_message_unref(msg);
	if (!reply) {
		MS_DBG_ERR("dbus_connection_send error(%s:%s) %s %s:%s-%s",
		err.name, err.message, dest, path, interface, method);
		dbus_error_free(&err);
		return MS_MEDIA_ERR_INTERNAL;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &aiter);

	result = 0;
	while (dbus_message_iter_get_arg_type(&aiter) != DBUS_TYPE_INVALID) {
		result++;
		MS_DBG("(%d)th block device information", result);

		ms_block_info_s *data = NULL;
		data = malloc(sizeof(ms_block_info_s));

		dbus_message_iter_recurse(&aiter, &piter);
		dbus_message_iter_get_basic(&piter, &val_int);
		MS_DBG("\tType(%d)", val_int);
		data->block_type = val_int;

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tdevnode(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tsyspath(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tfs_usage(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tfs_type(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tfs_version(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tfs_uuid_enc(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_int);
	        MS_DBG("\treadonly(%d)", val_int);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tmount point(%s)", val_str);
		data->mount_path = strdup(val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_int);
		MS_DBG("\tstate(%d)", val_int);
		data->state = val_int;

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_bool);
		MS_DBG("\tprimary(%d)", val_bool ? "true" : "false");

		dbus_message_iter_next(&aiter);

		if (*dev_list == NULL) {
			MS_DBG_ERR("DEV LIST IS NULL");
			*dev_list = g_array_new(FALSE, FALSE, sizeof(ms_block_info_s*));
		}
		g_array_append_val(*dev_list, data);
	}

	dbus_message_unref(reply);

	return result;
}


int ms_sys_get_device_list(ms_stg_type_e stg_type, GArray **dev_list)
{
	int ret;
	const char *dev_params[MS_STG_TYPE_MAX] = { BLOCK_DEVICE_USB,
		BLOCK_DEVICE_MMC,
		BLOCK_DEVICE_ALL,
		};

	ret = __ms_dbus_method_sync(DEVICED_BUS_NAME,
		DEVICED_PATH_BLOCK_MANAGER,
		DEVICED_INTERFACE_BLOCK_MANAGER,
		BLOCK_DEVICE_METHOD,
		dev_params[stg_type], dev_list);
	if (ret < 0) {
		MS_DBG("Failed to send dbus (%d)", ret);
	} else {
		MS_DBG("%d block devices are founded", ret);
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_sys_release_device_list(GArray **dev_list)
{
	if (*dev_list) {
		while((*dev_list)->len != 0) {
			ms_block_info_s *data = NULL;
			data = g_array_index(*dev_list , ms_block_info_s*, 0);
			g_array_remove_index (*dev_list, 0);
			MS_DBG("MOUNT PATH [%s] RELEASED",data->mount_path);
			MS_SAFE_FREE(data->mount_path);
			MS_SAFE_FREE(data);
		}
		g_array_free (*dev_list, FALSE);
		*dev_list = NULL;
	}

	return MS_MEDIA_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////
/// GET ACTIVATE USER ID
//////////////////////////////////////////////////////////////////////////////
#define UID_DBUS_NAME		 "org.freedesktop.login1"
#define UID_DBUS_PATH		 "/org/freedesktop/login1"
#define UID_DBUS_INTERFACE	 UID_DBUS_NAME".Manager"
#define UID_DBUS_METHOD		 "ListUsers"

static int __ms_dbus_get_uid(const char *dest, const char *path, const char *interface, const char *method, uid_t *uid)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iiiter;
	DBusMessage *reply;
	DBusError err;
	DBusMessageIter iter;
	DBusMessageIter aiter, piter;
	int result;

	int val_int;
	char *val_str;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		MS_DBG_ERR("dbus_bus_get error");
		return MS_MEDIA_ERR_INTERNAL;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		MS_DBG_ERR("dbus_message_new_method_call(%s:%s-%s)",
		path, interface, method);
		return MS_MEDIA_ERR_INTERNAL;
	}

	dbus_message_iter_init_append(msg, &iiiter);

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_message_unref(msg);
	if (!reply) {
		MS_DBG_ERR("dbus_connection_send error(%s:%s) %s %s:%s-%s",
		err.name, err.message, dest, path, interface, method);
		dbus_error_free(&err);
		return MS_MEDIA_ERR_INTERNAL;
	}

	dbus_message_iter_init(reply, &iter);
	dbus_message_iter_recurse(&iter, &aiter);

	result = 0;
	while (dbus_message_iter_get_arg_type(&aiter) != DBUS_TYPE_INVALID) {
		result++;
		MS_DBG("(%d)th block device information", result);

		dbus_message_iter_recurse(&aiter, &piter);
		dbus_message_iter_get_basic(&piter, &val_int);
		MS_DBG("\tType(%d)", val_int);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tdevnode(%s)", val_str);

		dbus_message_iter_next(&piter);
		dbus_message_iter_get_basic(&piter, &val_str);
		MS_DBG("\tsyspath(%s)", val_str);

		dbus_message_iter_next(&aiter);
	}

	*uid = (uid_t) val_int;

	return result;
}

int ms_sys_get_uid(uid_t *uid)
{
	int ret;

	ret = __ms_dbus_get_uid(UID_DBUS_NAME,UID_DBUS_PATH, UID_DBUS_INTERFACE, UID_DBUS_METHOD, uid);
	if (ret < 0) {
		MS_DBG("Failed to send dbus (%d)", ret);
	} else {
		MS_DBG("%d get UID[%d]", ret, *uid);
	}

	return MS_MEDIA_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////
/// CHECK THE POWER STATE
//////////////////////////////////////////////////////////////////////////////

#define POWER_DBUS_NAME "ChangeState"
#define POWER_DBUS_PATH "/Org/Tizen/System/DeviceD/PowerOff"
#define POWER_DBUS_INTERFACE "org.tizen.system.deviced.PowerOff"
#define POWER_DBUS_MATCH_RULE "type='signal',interface='org.tizen.system.deviced.PowerOff'"

typedef struct pwoff_cb_data{
	power_off_cb usr_cb;
	void *usr_data;
} pwoff_cb_data;

DBusConnection *g_pwr_dbus;

pwoff_cb_data *g_pwr_cb_data = NULL;

static DBusHandlerResult __poweroff_msg_filter (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	pwoff_cb_data *cb_data = (pwoff_cb_data *)user_data;
	void *usr_cb = cb_data->usr_cb;
	void *usr_data = cb_data->usr_data;

	MS_DBG_FENTER();

	/* A Ping signal on the com.burtonini.dbus.Signal interface */
	if (dbus_message_is_signal (message, POWER_DBUS_INTERFACE, POWER_DBUS_NAME)) {
		int current_type = DBUS_TYPE_INVALID;
		DBusError error;
		DBusMessageIter read_iter;
		DBusBasicValue value;
		power_off_cb cb_func = (power_off_cb)usr_cb;
		ms_power_info_s *power_info = NULL;

		dbus_error_init (&error);

		/* get data from dbus message */
		dbus_message_iter_init (message, &read_iter);
		while ((current_type = dbus_message_iter_get_arg_type (&read_iter)) != DBUS_TYPE_INVALID){
	                dbus_message_iter_get_basic (&read_iter, &value);
			switch(current_type) {
				case DBUS_TYPE_INT32:
					MS_DBG_WARN("value[%d]", value.i32);
					break;
				default:
					MS_DBG_ERR("current type : %d", current_type);
					break;
			}

			if (value.i32 == 2 || value.i32 == 3) {
				MS_DBG_WARN("PREPARE POWER OFF");
				break;
			}

			dbus_message_iter_next (&read_iter);
		}

		if (value.i32 == 2 || value.i32 == 3)
			cb_func(power_info, usr_data);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int ms_sys_set_poweroff_cb(power_off_cb user_callback, void *user_data)
{
	DBusError error;

	/*add noti receiver for power off*/
	dbus_error_init (&error);

	g_pwr_dbus = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!g_pwr_dbus) {
		MS_DBG_ERR ("Failed to connect to the D-BUS daemon: %s", error.message);
		return MS_MEDIA_ERR_INTERNAL;
	}

	dbus_connection_setup_with_g_main (g_pwr_dbus, NULL);

	g_pwr_cb_data = malloc(sizeof(pwoff_cb_data));
	if (g_pwr_cb_data == NULL) {
		MS_DBG_ERR ("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_pwr_cb_data->usr_cb = user_callback;
	g_pwr_cb_data->usr_data = user_data;

	/* listening to messages from all objects as no path is specified */
	dbus_bus_add_match (g_pwr_dbus, POWER_DBUS_MATCH_RULE, &error);
	if( !dbus_connection_add_filter (g_pwr_dbus, __poweroff_msg_filter, g_pwr_cb_data, NULL)) {
		dbus_bus_remove_match (g_pwr_dbus, POWER_DBUS_MATCH_RULE, NULL);
		MS_DBG_ERR("dbus_connection_add_filter failed");
		return MS_MEDIA_ERR_INTERNAL;
	}

	return MS_MEDIA_ERR_NONE;
}

int ms_sys_unset_poweroff_cb(void)
{
	if (g_pwr_dbus == NULL) {
		return MS_MEDIA_ERR_NONE;
	}

	dbus_connection_remove_filter(g_pwr_dbus, __poweroff_msg_filter, g_pwr_cb_data);
	dbus_bus_remove_match (g_pwr_dbus, MS_MEDIA_DBUS_MATCH_RULE, NULL);
	dbus_connection_unref(g_pwr_dbus);
	g_pwr_dbus = NULL;

	MS_SAFE_FREE(g_pwr_cb_data);

        return MS_MEDIA_ERR_NONE;
}


/*
 * Media Server
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
#include <dbus/dbus.h>
#include <sys/stat.h>
#include <systemd/sd-login.h>

#include "media-util.h"

#include "media-common-dbg.h"
#include "media-common-types.h"
#include "media-common-system.h"


//////////////////////////////////////////////////////////////////////////////
/// CHECK THE STORATE(MMC, USB) STATE
//////////////////////////////////////////////////////////////////////////////

#define DEVICED_BUS_NAME       "org.tizen.system.storage"
#define DEVICED_OBJECT_PATH    "/Org/Tizen/System/Storage"
#define DEVICED_INTERFACE_NAME DEVICED_BUS_NAME

#define DEVICED_PATH_BLOCK                  DEVICED_OBJECT_PATH"/Block"
#define DEVICED_PATH_BLOCK_MANAGER          DEVICED_PATH_BLOCK"/Manager"
#define DEVICED_INTERFACE_BLOCK_MANAGER     DEVICED_INTERFACE_NAME".BlockManager"
#define BLOCK_DEVICE_CHANGED    "DeviceChanged"

#define BLOCK_DEVICE_METHOD "GetDeviceList"
#define BLOCK_DEVICE_USB "scsi"
#define BLOCK_DEVICE_MMC "mmc"
#define BLOCK_DEVICE_ALL "all"

static GDBusConnection *g_usb_bus;

static int g_usb_handler;

typedef struct block_cb_data {
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
	const char *mount_uuid = NULL;
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
	devnode = g_variant_get_string(tmp, &size);
	MS_DBG_ERR("devnode : %s", devnode);

	tmp = g_variant_get_child_value(parameters, 6);
	mount_uuid = g_variant_get_string(tmp, &size);
	if (mount_uuid != NULL) {
		block_info->mount_uuid = strdup(mount_uuid);
		MS_DBG_ERR("mount_uuid : %s", block_info->mount_uuid);
	}

	tmp = g_variant_get_child_value(parameters, 8);
	mount_path = g_variant_get_string(tmp, &size);
	if (mount_path != NULL) {
		block_info->mount_path = strdup(mount_path);
		MS_DBG_ERR("mount_point : %s", block_info->mount_path);
	}

	tmp = g_variant_get_child_value(parameters, 9);
	block_info->state = g_variant_get_int32(tmp);
	MS_DBG_ERR("state : %d", block_info->state);

	tmp = g_variant_get_child_value(parameters, 11);
	block_info->flags = g_variant_get_int32(tmp);
	MS_DBG_ERR("flags : %d", block_info->flags);

	((block_changed_cb)usr_cb)(block_info, usr_data);
	MS_SAFE_FREE(block_info->mount_path);
	MS_SAFE_FREE(block_info->mount_uuid);
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
			MS_DBG_ERR("Failed to connect to the g D-BUS daemon: %s", error->message);
			g_error_free(error);
			ret = MS_MEDIA_ERR_INTERNAL;
			goto ERROR;
		}
	}

	/* listening to messages from all objects as no path is specified */
	g_usb_handler = g_dbus_connection_signal_subscribe(
					g_usb_bus,
					NULL,
					DEVICED_INTERFACE_BLOCK_MANAGER,
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

#define DBUS_REPLY_TIMEOUT (-1)
static int __ms_gdbus_method_sync(const char *dest, const char *path, const char *interface, const char *method, const char *param, GArray **dev_list)
{
	GDBusConnection *g_bus = NULL;
	GError *error = NULL;
	GDBusMessage *message = NULL;
	GDBusMessage *reply = NULL;
	GVariant *reply_var = NULL;
	GVariantIter *iter = NULL;
	char *type_str = NULL;
	int val_int[5] = {0, };
	char *val_str[7] = {NULL, };
	gboolean val_bool = FALSE;

	int result = 0;

	MS_DBG_FENTER();

	g_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!g_bus) {
		MS_DBG_ERR("Failed to connect to the g D-BUS daemon: %s", error ? error->message : "none");
		g_error_free(error);
		return MS_MEDIA_ERR_INTERNAL;
	}

	message = g_dbus_message_new_method_call(dest, path, interface, method);
	if (!message) {
		MS_DBG_ERR("g_dbus_message_new_method_call(%s:%s-%s)", path, interface, method);
		g_object_unref(g_bus);
		return MS_MEDIA_ERR_INTERNAL;
	}

	g_dbus_message_set_body(message, g_variant_new("(s)", (gchar*)param));

	reply = g_dbus_connection_send_message_with_reply_sync(g_bus, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, NULL, &error);
	g_object_unref(message);
	if (!reply) {
		MS_DBG_ERR("dbus_connection_send error(%s) %s %s:%s-%s",
		error ? error->message : "none", dest, path, interface, method);
		g_error_free(error);
		g_object_unref(message);
		g_object_unref(g_bus);
		return MS_MEDIA_ERR_INTERNAL;
	}

	reply_var = g_dbus_message_get_body(reply);
	if (!reply_var) {
		MS_DBG_ERR("Failed to get the body of message");
		g_object_unref(reply);
		g_object_unref(g_bus);
		return MS_MEDIA_ERR_INTERNAL;
	}

	type_str = strdup((char*)g_variant_get_type_string(reply_var));
	if (!type_str) {
		MS_DBG_ERR("Failed to get the type-string of message");
		g_variant_unref(reply_var);
		g_object_unref(reply);
		g_object_unref(g_bus);
		return MS_MEDIA_ERR_INTERNAL;
	}

	g_variant_get(reply_var, type_str, &iter);

	while (g_variant_iter_loop(iter, "(issssssisibii)", &val_int[0], &val_str[0], &val_str[1], &val_str[2]
		, &val_str[3], &val_str[4], &val_str[5], &val_int[1], &val_str[6], &val_int[2], &val_bool, &val_int[3], &val_int[4])) {
		int i = 0;
		ms_block_info_s *data = NULL;

		if (val_int[2] == 0) {
			MS_DBG("Block device is unmounted[%d]. So, skip this.", val_int[2]);
		} else {
			result++;
			data = malloc(sizeof(ms_block_info_s));

			data->block_type = val_int[0];
			data->mount_path = strdup(val_str[6]);
			data->state = val_int[2];
			data->mount_uuid = strdup(val_str[5]);
			data->flags = val_int[3];

			if (*dev_list == NULL) {
				MS_DBG_ERR("DEV LIST IS NULL");
				*dev_list = g_array_new(FALSE, FALSE, sizeof(ms_block_info_s*));
			}
			g_array_append_val(*dev_list, data);

			MS_DBG("(%d)th block device information", result);
			MS_DBG("\tType(%d)", val_int[0]);
			MS_DBG("\tdevnode(%s)", val_str[0]);
			MS_DBG("\tsyspath(%s)", val_str[1]);
			MS_DBG("\tfs_usage(%s)", val_str[2]);
			MS_DBG("\tfs_type(%s)", val_str[3]);
			MS_DBG("\tfs_version(%s)", val_str[4]);
			MS_DBG("\tfs_uuid_enc(%s)", val_str[5]);
			MS_DBG("\treadonly(%d)", val_int[1]);
			MS_DBG("\tmount point(%s)", val_str[6]);
			MS_DBG("\tstate(%d)", val_int[2]);
			MS_DBG("\tprimary(%s)", val_bool ? "true" : "false");
			MS_DBG("\tflags(%d)", val_int[3]);
			MS_DBG("\tstorage_id(%d)", val_int[4]);
		}

		for (i = 0; i < 7; i++) {
			MS_SAFE_FREE(val_str[i]);
		}
	}

	g_variant_iter_free(iter);

	g_variant_unref(reply_var);
	g_object_unref(reply);
	g_object_unref(g_bus);
	MS_SAFE_FREE(type_str);

	MS_DBG_FLEAVE();

	return result;
}

int ms_sys_get_device_list(ms_stg_type_e stg_type, GArray **dev_list)
{
	int ret;
	const char *dev_params[MS_STG_TYPE_MAX] = { BLOCK_DEVICE_USB,
		BLOCK_DEVICE_MMC,
		BLOCK_DEVICE_ALL,
		};

	ret = __ms_gdbus_method_sync(DEVICED_BUS_NAME,
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
		while ((*dev_list)->len != 0) {
			ms_block_info_s *data = NULL;
			data = g_array_index(*dev_list , ms_block_info_s*, 0);
			g_array_remove_index(*dev_list, 0);
			MS_DBG("MOUNT PATH [%s] RELEASED", data->mount_path);
			MS_SAFE_FREE(data->mount_path);
			MS_SAFE_FREE(data->mount_uuid);
			MS_SAFE_FREE(data);
		}
		g_array_free(*dev_list, FALSE);
		*dev_list = NULL;
	}

	return MS_MEDIA_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////
/// GET ACTIVATE USER ID
//////////////////////////////////////////////////////////////////////////////
int ms_sys_get_uid(uid_t *uid)
{
	uid_t *list = NULL;
	int users = -1;
	users = sd_get_uids(&list);
	if (users > 0) {
		*uid = list[0];
		MS_SAFE_FREE(list);
	} else {
		MS_DBG_ERR("No login user!.");
		MS_SAFE_FREE(list);
		return MS_MEDIA_ERR_INTERNAL;
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

typedef struct pwoff_cb_data {
	power_off_cb usr_cb;
	void *usr_data;
} pwoff_cb_data;

static GDBusConnection *g_pwr_bus;
static int g_pwr_handler;

pwoff_cb_data *g_pwr_cb_data = NULL;

static void __poweroff_signal_cb(GDBusConnection *connection,
									const gchar *sender_name,
									const gchar *object_path,
									const gchar *interface_name,
									const gchar *signal_name,
									GVariant *parameters,
									gpointer user_data)
{
	pwoff_cb_data *cb_data = (pwoff_cb_data *)user_data;
	void *usr_cb = cb_data->usr_cb;
	void *usr_data = cb_data->usr_data;
	char *type_str = NULL;
	int val_int = 0;

	power_off_cb cb_func = (power_off_cb)usr_cb;
	ms_power_info_s *power_info = NULL;

	MS_DBG_FENTER();

	if (!parameters) {
		MS_DBG_ERR("Error - The body of message is NULL!");
		return ;
	}

	type_str = strdup((char *)g_variant_get_type_string(parameters));
	if (!type_str) {
		MS_DBG_ERR("Failed to get the type-string of message");
		return ;
	}

	if (strcmp(type_str, "(i)") == 0) {
		g_variant_get(parameters, type_str, &val_int);
		MS_DBG_WARN("value[%d]", val_int);
	} else {
		MS_DBG_ERR("current type : %s", type_str);
	}

	if (val_int == 2 || val_int == 3) {
		MS_DBG_WARN("PREPARE POWER OFF");
	}

	if (val_int == 2 || val_int == 3)
		cb_func(power_info, usr_data);

	MS_SAFE_FREE(type_str);
}

int ms_sys_set_poweroff_cb(power_off_cb user_callback, void *user_data)
{
	GError *error = NULL;

	MS_DBG_FENTER();

	/*add noti receiver for power off*/
	if (g_pwr_bus == NULL) {
		g_pwr_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
		if (!g_pwr_bus) {
			MS_DBG_ERR("Failed to connect to the g D-BUS daemon: %s", error ? error->message : "none");
			g_error_free(error);
			return MS_MEDIA_ERR_INTERNAL;
		}
	}

	g_pwr_cb_data = malloc(sizeof(pwoff_cb_data));
	if (g_pwr_cb_data == NULL) {
		MS_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	g_pwr_cb_data->usr_cb = user_callback;
	g_pwr_cb_data->usr_data = user_data;

	/* listening to messages from all objects as no path is specified */
	g_pwr_handler = g_dbus_connection_signal_subscribe(g_pwr_bus,
		NULL,
		POWER_DBUS_INTERFACE,
		POWER_DBUS_NAME,
		NULL,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		__poweroff_signal_cb,
		g_pwr_cb_data,
		NULL);

	MS_DBG_FLEAVE();

	return MS_MEDIA_ERR_NONE;
}

int ms_sys_unset_poweroff_cb(void)
{
	if (g_pwr_bus == NULL) {
		return MS_MEDIA_ERR_NONE;
	}

	g_dbus_connection_signal_unsubscribe(g_pwr_bus, g_pwr_handler);
	g_object_unref(g_pwr_bus);
	g_pwr_bus = NULL;

	MS_SAFE_FREE(g_pwr_cb_data);

	return MS_MEDIA_ERR_NONE;
}


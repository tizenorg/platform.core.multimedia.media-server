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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <gio/gio.h>
#include <dbus/dbus.h>

#include <glib.h>
#include "media-util.h"

#include <tzplatform_config.h>

#define MMC_INFO_SIZE 256
#define BUS_NAME       "org.tizen.system.storage"
#define OBJECT_PATH    "/Org/Tizen/System/Storage"
#define INTERFACE_NAME BUS_NAME
#define DBUS_REPLY_TIMEOUT (-1)

#define PATH_BLOCK                  OBJECT_PATH"/Block"
#define PATH_BLOCK_DEVICES          PATH_BLOCK"/Devices"
#define PATH_BLOCK_MANAGER          PATH_BLOCK"/Manager"
#define INTERFACE_BLOCK             INTERFACE_NAME".Block"
#define INTERFACE_BLOCK_MANAGER     INTERFACE_NAME".BlockManager"

#define DEVICE_METHOD "GetDeviceList"
#define DEVICE_USB "scsi"
#define DEVICE_MMC "mmc"
#define DEVICE_ALL "all"

#define MU_SAFE_FREE(src)		{ if (src) {free(src); src = NULL;} }


GMainLoop * mainloop = NULL;

typedef struct block_info_s{
	char *mount_path;
	int state;
	int block_type;
	int flags;
	char *mount_uuid;
} block_info_s;

int (*svc_connect)				(void ** handle, uid_t uid, char ** err_msg);
int (*svc_disconnect)			(void * handle, char ** err_msg);
int (*svc_check_db)			(void * handle, uid_t uid, char ** err_msg);
int (*svc_get_storage_id)		(void * handle, const char *path, char *storage_id, uid_t uid, char ** err_msg);
int (*svc_get_mmc_info)		(void * handle, char **storage_name, char **storage_path, int *validity, bool *info_exist, char **err_msg);
int (*svc_insert_storage)		(void *handle, const char *storage_id, int storage_type, const char *storage_name, const char *storage_path, uid_t uid, char **err_msg);
int (*svc_delete_storage)		(void * handle, const char *storage_id, const char *storage_name, uid_t uid, char **err_msg);
int (*svc_set_storage_validity)	(void * handle, const char *storage_id, int validity, uid_t uid, char **err_msg);



void callback(media_request_result_s * result, void *user_data)
{
	if (result->result != MEDIA_REQUEST_SCAN_PARTIAL) {
		printf("db updating done\n");
		g_main_loop_quit(mainloop);
	}
}

void print_help()
{
	printf("=======================================================================================\n");
	printf("\n");
	printf("db-update [option] <directory path> \n");
	printf("\n");
	printf("[option]\n");
	printf("	-r : [only directory] update all directory recursivly under <directory path>\n");
	printf("\n");
	printf("db-update --help for check this messages.\n");
	printf("\n");
	printf("A file or directory must exists under %s or %s.\n", tzplatform_getenv(TZ_USER_HOME), tzplatform_mkpath(TZ_SYS_STORAGE, "sdcard"));
	printf("Using %s is allowed SD card is mounted.\n", tzplatform_mkpath(TZ_SYS_STORAGE, "sdcard"));
	printf("\n");
	printf("=======================================================================================\n");
}


static int __get_contents(const char *filename, char *buf)
{
	FILE *fp;

	fp = fopen(filename, "rt");
	if (fp == NULL)
		return -1;
	if (fgets(buf, 255, fp) == NULL)
		printf("fgets failed\n");

	fclose(fp);

	return 0;
}

/*need optimize*/
void __get_mmc_id(char **cid)
{
	const char *path = "/sys/block/mmcblk1/device/cid";
	char mmc_cid[MMC_INFO_SIZE] = {0, };
	memset(mmc_cid, 0x00, sizeof(mmc_cid));

	if (__get_contents(path, mmc_cid) != 0) {
		printf("_get_contents failed");
		*cid = NULL;
	} else {
		*cid = strndup(mmc_cid, strlen(mmc_cid) - 1);
	}
}

int __get_device_list(GArray **dev_list)
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

	g_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!g_bus) {
		g_error_free(error);
		return -1;
	}

	message = g_dbus_message_new_method_call(BUS_NAME, PATH_BLOCK_MANAGER, INTERFACE_BLOCK_MANAGER, DEVICE_METHOD);
	if (!message) {
		g_object_unref(g_bus);
		return -1;
	}

	g_dbus_message_set_body(message, g_variant_new("(s)", (gchar*)DEVICE_MMC));

	reply = g_dbus_connection_send_message_with_reply_sync(g_bus, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, DBUS_REPLY_TIMEOUT, NULL, NULL, &error);
	g_object_unref(message);
	if (!reply) {
		g_error_free(error);
		g_object_unref(message);
		g_object_unref(g_bus);
		return -1;
	}

	reply_var = g_dbus_message_get_body(reply);
	if (!reply_var) {
		g_object_unref(reply);
		g_object_unref(g_bus);
		return -1;
	}

	type_str = strdup((char*)g_variant_get_type_string(reply_var));
	if (!type_str) {
		g_variant_unref(reply_var);
		g_object_unref(reply);
		g_object_unref(g_bus);
		return -1;
	}

	g_variant_get(reply_var, type_str, &iter);

	while (g_variant_iter_loop(iter, "(issssssisibii)", &val_int[0], &val_str[0], &val_str[1], &val_str[2]
		, &val_str[3], &val_str[4], &val_str[5], &val_int[1], &val_str[6], &val_int[2], &val_bool, &val_int[3], &val_int[4])) {
		int i = 0;
		block_info_s *data = NULL;

		if (val_int[2] == 0) {
			/* Do nothing */
		} else {
			data = malloc(sizeof(block_info_s));

			data->block_type = val_int[0];
			data->mount_path = strdup(val_str[6]);
			data->state = val_int[2];
			data->mount_uuid = strdup(val_str[5]);
			data->flags = val_int[3];

			if (*dev_list == NULL) {
				*dev_list = g_array_new(FALSE, FALSE, sizeof(block_info_s*));
			}
			g_array_append_val(*dev_list, data);
		}

		for (i = 0; i < 7; i++) {
			MU_SAFE_FREE(val_str[i]);
		}
	}

	g_variant_iter_free(iter);

	g_variant_unref(reply_var);
	g_object_unref(reply);
	g_object_unref(g_bus);
	MU_SAFE_FREE(type_str);

	return 0;
}

void __release_device_list(GArray **dev_list)
{
	if (*dev_list) {
		while ((*dev_list)->len != 0) {
			block_info_s *data = NULL;
			data = g_array_index(*dev_list , block_info_s*, 0);
			g_array_remove_index(*dev_list, 0);
			MU_SAFE_FREE(data->mount_path);
			MU_SAFE_FREE(data->mount_uuid);
			MU_SAFE_FREE(data);
		}
		g_array_free(*dev_list, FALSE);
		*dev_list = NULL;
	}
}

static void __check_mmc(void)
{
	int ret = 0;
	GArray *dev_list = NULL;
	void *funcHandle = NULL;
	void *db_handle = NULL;
	char *storage_name = NULL;
	char *storage_path = NULL;
	int validity = 0;
	bool info_exist = FALSE;
	char *insert_stg_id = NULL;
	char *err_msg = NULL;

	funcHandle = dlopen("/usr/lib/libmedia-content-plugin.so", RTLD_LAZY);
	if (funcHandle == NULL) {
		funcHandle = dlopen("/usr/lib64/libmedia-content-plugin.so", RTLD_LAZY);
		if (funcHandle == NULL) {
			printf("Error when open plug-in\n");
			return;
		}
	}

	svc_connect			= dlsym(funcHandle, "connect_db");
	svc_disconnect		= dlsym(funcHandle, "disconnect_db");
	svc_get_mmc_info	= dlsym(funcHandle, "get_mmc_info");
	svc_insert_storage	= dlsym(funcHandle, "insert_storage");
	svc_delete_storage	= dlsym(funcHandle, "delete_storage");
	svc_set_storage_validity	= dlsym(funcHandle, "set_storage_validity");

	ret = svc_connect(&db_handle, tzplatform_getuid(TZ_USER_NAME), &err_msg);
	if (ret < 0) {
		printf("Error svc_connect\n");
		dlclose(funcHandle);
		return;
	}

	ret = svc_get_mmc_info(db_handle, &storage_name, &storage_path, &validity, &info_exist, &err_msg);
	if (ret < 0 && strcmp(err_msg, "not found in DB") != 0) {
		printf("Error svc_get_mmc_info\n");
		dlclose(funcHandle);
		return;
	}

	ret = __get_device_list(&dev_list);
	if (ret == 0) {
		if (dev_list != NULL) {
			int i = 0 ;
			int dev_num = dev_list->len;
			block_info_s *block_info = NULL;

			for (i = 0; i < dev_num; i++) {
				block_info = (block_info_s *)g_array_index(dev_list, int *, i);
				if (info_exist) {
					__get_mmc_id(&insert_stg_id);

					if (strncmp(storage_name, insert_stg_id, strlen(insert_stg_id)) == 0 && strncmp(storage_path, block_info->mount_path, strlen(block_info->mount_path)) == 0) {
						if (validity == 0) {
							ret = svc_set_storage_validity(db_handle, "media", 1, tzplatform_getuid(TZ_USER_NAME), &err_msg);
							if (ret < 0)
								printf("Error svc_set_storage_validity\n");
						}
					} else {
						ret = svc_delete_storage(db_handle, "media", storage_name, tzplatform_getuid(TZ_USER_NAME), &err_msg);
						ret = svc_insert_storage(db_handle, "media", 1, insert_stg_id, block_info->mount_path, tzplatform_getuid(TZ_USER_NAME), &err_msg);
					}
					MU_SAFE_FREE(insert_stg_id);
				} else {
					__get_mmc_id(&insert_stg_id);
					ret = svc_insert_storage(db_handle, "media", 1, insert_stg_id, block_info->mount_path, tzplatform_getuid(TZ_USER_NAME), &err_msg);
					MU_SAFE_FREE(insert_stg_id);
				}
			}
			__release_device_list(&dev_list);
		} else {
			if (validity == 1) {
				ret = svc_set_storage_validity(db_handle, "media", 0, tzplatform_getuid(TZ_USER_NAME), &err_msg);
				if (ret < 0)
					printf("Error svc_set_storage_validity\n");
			}
		}
	} else {
		printf("__get_device_list failed\n");
	}

	ret = svc_disconnect(db_handle, &err_msg);
	if (ret < 0)
		printf("Error svc_disconnect\n");

	printf("Check mmc done\n");

	dlclose(funcHandle);
}

static void __check_media_db(void)
{
	void *funcHandle = NULL;
	void *db_handle = NULL;
	char *err_msg = NULL;
	int ret = 0;

	funcHandle = dlopen("/usr/lib/libmedia-content-plugin.so", RTLD_LAZY);
	if (funcHandle == NULL) {
		funcHandle = dlopen("/usr/lib64/libmedia-content-plugin.so", RTLD_LAZY);
		if (funcHandle == NULL) {
			printf("Error when open plug-in\n");
			return;
		}
	}

	svc_connect			= dlsym(funcHandle, "connect_db");
	svc_disconnect		= dlsym(funcHandle, "disconnect_db");
	svc_check_db			= dlsym(funcHandle, "check_db");

	ret = svc_connect(&db_handle, tzplatform_getuid(TZ_USER_NAME), &err_msg);
	if (ret < 0)
		printf("Error svc_connect\n");

	ret = svc_check_db(db_handle, tzplatform_getuid(TZ_USER_NAME), &err_msg);
	if (ret < 0)
		printf("Error svc_check_db\n");

	ret = svc_disconnect(db_handle, &err_msg);
	if (ret < 0)
		printf("Error svc_disconnect\n");

	printf("Check media db done\n");

	dlclose(funcHandle);
}

static int __get_storage_id(const char *path, char *storage_id, uid_t uid)
{
	void *funcHandle = NULL;
	void *db_handle = NULL;
	char *err_msg = NULL;
	int ret = 0;

	if (strncmp(tzplatform_getenv(TZ_USER_HOME), path, strlen(tzplatform_getenv(TZ_USER_HOME))) != 0 && strncmp(tzplatform_getenv(TZ_SYS_STORAGE), path, strlen(tzplatform_getenv(TZ_SYS_STORAGE))) != 0) {
		printf("Not support path[%s]\n", path);
		return -1;
	}

	funcHandle = dlopen("/usr/lib/libmedia-content-plugin.so", RTLD_LAZY);
	if (funcHandle == NULL) {
		funcHandle = dlopen("/usr/lib64/libmedia-content-plugin.so", RTLD_LAZY);
		if (funcHandle == NULL) {
			printf("Error when open plug-in\n");
			return -1;
		}
	}

	svc_connect			= dlsym(funcHandle, "connect_db");
	svc_disconnect		= dlsym(funcHandle, "disconnect_db");
	svc_get_storage_id	= dlsym(funcHandle, "get_storage_id");

	ret = svc_connect(&db_handle, uid, &err_msg);
	if (ret < 0) {
		printf("Error svc_connect\n");
		dlclose(funcHandle);
		return -1;
	}

	ret = svc_get_storage_id(db_handle, path, storage_id, uid, &err_msg);
	if (ret < 0) {
		printf("Error svc_get_storage_id\n");
		dlclose(funcHandle);
		return -1;
	}
	ret = svc_disconnect(db_handle, &err_msg);
	if (ret < 0) {
		printf("Error svc_disconnect\n");
		dlclose(funcHandle);
		return -1;
	}
	printf("Start Scanning for [%s][%s]\n", path, storage_id);

	dlclose(funcHandle);

	return 0;
}

int dir_scan_non_recursive(const char *path)
{
	int ret = 0;
	char storage_id[36+1] = {0,};

	ret = __get_storage_id(path, storage_id, tzplatform_getuid(TZ_USER_NAME));
	if (ret < 0)
		return -1;

	return media_directory_scanning_async(path, storage_id, FALSE, callback, NULL, tzplatform_getuid(TZ_USER_NAME));
}

int dir_scan_recursive(char *path)
{
	int ret = 0;
	char storage_id[36+1] = {0,};

	ret = __get_storage_id(path, storage_id, tzplatform_getuid(TZ_USER_NAME));
	if (ret < 0)
		return -1;

	return media_directory_scanning_async(path, storage_id, TRUE, callback, NULL, tzplatform_getuid(TZ_USER_NAME));
}

typedef enum {
	DIRECTORY_OK,
	FILE_OK,
	NOT_OK,
} check_result;

check_result check_path(char *path)
{
	struct stat buf;
	DIR *dp = NULL;

	/*check the path directory or file*/
	if (stat(path, &buf) == 0) {
		if (S_ISDIR(buf.st_mode)) {
			printf("This is directory\n");
			return DIRECTORY_OK;
		} else {
			dp = opendir(path);
			if (dp == NULL) {
				/*if openning directory is failed, check it exists. */
				if (errno == ENOENT) {
					/* this directory is deleted */
					return DIRECTORY_OK;
				}
			} else {
				closedir(dp);
			}
			print_help();
			return NOT_OK;
		}
	} else {
		printf("stat error");
	}

	return NOT_OK;
}

int main(int argc, char **argv)
{
	int ret;
	int len;
	char *argv1 = NULL;
	char *argv2 = NULL;
	char req_path[1024] = {0, };

	if (argc > 3 || argc < 2) {
		print_help();
		exit(0);
	}

	argv1 = strdup(argv[1]);

	mainloop = g_main_loop_new(NULL, FALSE);

	if (argc == 2) {
		if (strcmp(argv1 , "--help") == 0) {
			print_help();
			exit(0);
		}

		if (strcmp(argv1 , "check_db") == 0) {
			__check_media_db();
			exit(0);
		}

		if (strcmp(argv1 , "check_mmc") == 0) {
			__check_mmc();
			exit(0);
		}

		if (check_path(argv1) == DIRECTORY_OK) {
			len = strlen(argv1);

			if (argv1[len - 1] == '/')
				strncpy(req_path, argv1, len - 1);
			else
				strncpy(req_path, argv1, len);

			ret = dir_scan_non_recursive(req_path);
			if (ret != 0) {
				printf("error : %d\n", ret);
				exit(0);
			}
		} else {
			printf("[%d]invalid path\n", __LINE__);
			print_help();
			exit(0);
		}
	} else if (argc == 3) {
		argv2 = strdup(argv[2]);
		if (strcmp(argv1, "-r") == 0) {
			if (check_path(argv2) == DIRECTORY_OK) {
				len = strlen(argv2);

				if (argv2[len - 1] == '/')
					strncpy(req_path, argv2, len - 1);
				else
					strncpy(req_path, argv2, len);

				ret = dir_scan_recursive(req_path);
				if (ret != 0) {
					printf("error : %d\n", ret);
					exit(0);
				}
			} else {
				printf("[%d]invalid path\n", __LINE__);
				print_help();
				exit(0);
			}
		} else {
			printf("[%d] invalide option\n", __LINE__);
			print_help();
			exit(0);
		}
	}

	g_main_loop_run(mainloop);

	exit(0);
}

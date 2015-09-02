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
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>

#include <locale.h>
#include <libintl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <malloc.h>
#include <vconf.h>
#include <errno.h>
#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>

#include <notification.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-dbg.h"
#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-common-external-storage.h"

#include <pwd.h>
#include <tzplatform_config.h>

#define MMC_INFO_SIZE 256

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

GDBusConnection *g_stg_bus;
int g_stg_added_handler;

static int __get_contents(const char *filename, char *buf)
{
	FILE *fp;

	fp = fopen(filename, "rt");
	if (fp == NULL) {
		MS_DBG_ERR("fp is NULL. file name : %s", filename);
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}
	if (fgets(buf, 255, fp) == NULL)
		MS_DBG_ERR("fgets failed");

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}

/*need optimize*/
int ms_get_mmc_id(char **cid)
{
	const char *path = "/sys/block/mmcblk1/device/cid";
	char mmc_cid[MMC_INFO_SIZE] = {0, };
	memset(mmc_cid, 0x00, sizeof(mmc_cid));

	if (__get_contents(path, mmc_cid) != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("_get_contents failed");
		*cid = NULL;
	} else {
		*cid = strndup(mmc_cid, strlen(mmc_cid) - 1);		//last is carriage return
	}

	return MS_MEDIA_ERR_NONE;
}

#define _GETSYSTEMSTR(ID)	dgettext("sys_string", (ID))

void update_lang(void)
{
	char *lang = NULL;
	char *r = NULL;

	lang = vconf_get_str(VCONFKEY_LANGSET);
	if (lang) {
		setenv("LANG", lang, 1);
		setenv("LC_MESSAGES", lang, 1);
		r = setlocale(LC_ALL, "");
		if (r == NULL) {
			r = setlocale(LC_ALL, vconf_get_str(VCONFKEY_LANGSET));
			MS_DBG_ERR("*****appcore setlocale=%s", r);
		}
		free(lang);
	}
}

int ms_present_mmc_status(ms_sdcard_status_type_t status)
{
	int ret = NOTIFICATION_ERROR_NONE;

	update_lang();

	if (status == MS_SDCARD_INSERTED)
		ret = notification_status_message_post(_GETSYSTEMSTR("IDS_COM_BODY_PREPARING_SD_CARD"));
	else if (status == MS_SDCARD_REMOVED)
		ret = notification_status_message_post(_GETSYSTEMSTR("IDS_COM_BODY_SD_CARD_UNEXPECTEDLY_REMOVED"));

	if(ret != NOTIFICATION_ERROR_NONE)
		return MS_MEDIA_ERR_INTERNAL;
	return MS_MEDIA_ERR_NONE;
}

#define DEVICE_INFO_FILE ".device_info_"

struct linux_dirent {
	int           d_ino;
	long           d_off;
	unsigned short d_reclen;
	char           d_name[];
};

#define BUF_SIZE 1024

int __ms_check_mount_path(const char *mount_path)
{
	DIR *dp = NULL;

	/*check mount path*/
	dp = opendir(mount_path);
	if (dp == NULL) {
		MS_DBG_ERR("Mount path is not exist");
		return MS_MEDIA_ERR_INVALID_PATH;
	}

	closedir(dp);

	return MS_MEDIA_ERR_NONE;
}

int ms_read_device_info(const char *root_path, char **device_uuid)
{
	int ret = MS_MEDIA_ERR_NONE;
	int len = 0;

	int fd = -1;
	int nread = 0;
	char buf[BUF_SIZE] = {0,};
	struct linux_dirent *d;
	int bpos = 0;
	char d_type;
	bool find_uuid = FALSE;

	ret = __ms_check_mount_path(root_path);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_strappend falied");
		goto ERROR;
	}

	fd = open(root_path, O_RDONLY | O_DIRECTORY);
	if (fd == -1) {
		MS_DBG_ERR("open failed [%s]", root_path);
		MS_DBG_STRERROR();
		ret = MS_MEDIA_ERR_INVALID_PATH;
		goto ERROR;
	}

	for ( ; ; ) {
		nread = syscall(SYS_getdents64, fd, buf, BUF_SIZE);
		if (nread == -1) {
			MS_DBG_ERR("getdents");
			MS_DBG_STRERROR();
			ret = MS_MEDIA_ERR_INTERNAL;
			goto ERROR;
		}

		if (nread == 0)
			break;

		for (bpos = 0; bpos < nread;) {
			d = (struct linux_dirent *) (buf + bpos);
			d_type = *(buf + bpos + d->d_reclen - 1);

			if (d_type == DT_REG && (strncmp(d->d_name, DEVICE_INFO_FILE, strlen(DEVICE_INFO_FILE)) == 0)) {
				MS_DBG_ERR("FIND DEV INFO : %s", d->d_name);
				bpos += d->d_reclen;
				find_uuid = TRUE;
				goto FIND_UUID;
			}

			bpos += d->d_reclen;
		}
	}

FIND_UUID:
	if (find_uuid) {
		len = strlen(d->d_name + strlen(DEVICE_INFO_FILE));
		if (len > 0) {
			*device_uuid = strndup(d->d_name + strlen(DEVICE_INFO_FILE), MS_UUID_SIZE -1);
			MS_DBG_ERR("[%s][DEV ID: %s]", root_path, *device_uuid);
		}
	} else {
		ret = MS_MEDIA_ERR_FILE_NOT_EXIST;
		MS_DBG_ERR("[%s]DEV INFO NOT EXIST", root_path);
		goto ERROR;
	}

	if (fd > -1) {
		close(fd);
		fd = -1;
	}

	return MS_MEDIA_ERR_NONE;

ERROR:

	*device_uuid = NULL;
	if (fd > -1) {
		close(fd);
		fd = -1;
	}
	return ret;
}

int ms_write_device_info(const char *root_path, char *device_uuid)
{
	FILE * fp = NULL;
	int len = 0;
	char path[MS_FILE_PATH_LEN_MAX] = {0,};

	len = snprintf(path, MS_FILE_PATH_LEN_MAX -1 , "%s/%s%s", root_path, DEVICE_INFO_FILE,device_uuid);
	if (len < 0) {
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	path[len] = '\0';

	fp = fopen(path, "wt");
	if (fp == NULL) {
		MS_DBG_ERR("fp is NULL. file name : %s", path);
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}


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
#include <locale.h>
#include <libintl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <malloc.h>
#include <vconf.h>
#include <errno.h>
#include <notification.h>

#include "media-util.h"
#include "media-server-ipc.h"
#include "media-common-dbg.h"
#include "media-common-utils.h"
#include "media-common-drm.h"
#include "media-common-external-storage.h"

#include <pwd.h>
#include <tzplatform_config.h>

#define MMC_INFO_SIZE 256
#define PATH_IMAGES tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard/Images")
#define PATH_VIDEOS tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard/Videos")
#define PATH_SOUNDS tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard/Sounds")
#define PATH_DOWNLOADS tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard/Downloads")
#define PATH_CAMERA tzplatform_mkpath(TZ_SYS_STORAGE,"sdcard/Camera")

char default_path[][MS_FILE_NAME_LEN_MAX + 1] = {
		{"/opt/storage/sdcard/Images"},
		{"/opt/storage/sdcard/Videos"},
		{"/opt/storage/sdcard/Sounds"},
		{"/opt/storage/sdcard/Downloads"},
		{"/opt/storage/sdcard/Camera"}
};

#define DIR_NUM       ((int)(sizeof(default_path)/sizeof(default_path[0])))

void
ms_init_default_path(void){

	strcpy (default_path[0], PATH_IMAGES);
	strcpy (default_path[1], PATH_VIDEOS);
	strcpy (default_path[2], PATH_SOUNDS);
	strcpy (default_path[3], PATH_DOWNLOADS);
	strcpy (default_path[4], PATH_CAMERA);
}

void
ms_make_default_path_mmc(void)
{
	int i = 0;
	int ret = 0;
	DIR *dp = NULL;
 	ms_init_default_path();
	for (i = 0; i < DIR_NUM; ++i) {
		dp = opendir(default_path[i]);
		if (dp == NULL) {
			ret = mkdir(default_path[i], 0777);
			if (ret < 0) {
				MS_DBG_ERR("make fail");
			}
			/*this fuction for emulator*/
			/*at the first time, the directroies are made permission 755*/
			ret = chmod(default_path[i], 0777);
			if (ret != 0) {
				MS_DBG_STRERROR("chmod failed");
			}
			ret = chown(default_path[i], tzplatform_getuid(TZ_USER_NAME), tzplatform_getgid(TZ_USER_NAME));
			if (ret != 0) {
				MS_DBG_STRERROR("chown failed");
			}
		} else {
			closedir(dp);
		}
	}
}

int
_ms_update_mmc_info(const char *cid)
{
	bool res;

	if (cid == NULL) {
		MS_DBG_ERR("Parameters are invalid");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	res = ms_config_set_str(MS_MMC_INFO_KEY, cid);
	if (!res) {
		MS_DBG_ERR("fail to get MS_MMC_INFO_KEY");
		return MS_MEDIA_ERR_VCONF_SET_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

bool
_ms_check_mmc_info(const char *cid)
{
	char pre_mmc_info[MMC_INFO_SIZE] = { 0 };
	bool res = false;

	if (cid == NULL) {
		MS_DBG_ERR("Parameters are invalid");
		return false;
	}

	res = ms_config_get_str(MS_MMC_INFO_KEY, pre_mmc_info);
	if (!res) {
		MS_DBG_ERR("fail to get MS_MMC_INFO_KEY");
		return false;
	}

	MS_DBG("Last MMC info	= %s", pre_mmc_info);
	MS_DBG("Current MMC info = %s", cid);

	if (strcmp(pre_mmc_info, cid) == 0) {
		return true;
	}

	return false;
}

static int
_get_contents(const char *filename, char *buf)
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
int
_ms_get_mmc_info(char *cid)
{
	int i;
	int j;
	int len;
	int err = -1;
	bool getdata = false;
	bool bHasColon = false;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char mmcpath[MS_FILE_PATH_LEN_MAX] = { 0 };

	DIR *dp;
	struct dirent ent;
	struct dirent *res = NULL;

	/* mmcblk0 and mmcblk1 is reserved for movinand */
	for (j = 1; j < 3; j++) {
		len = snprintf(mmcpath, MS_FILE_PATH_LEN_MAX, "/sys/class/mmc_host/mmc%d/", j);
		if (len < 0) {
			MS_DBG_ERR("FAIL : snprintf");
			return MS_MEDIA_ERR_INTERNAL;
		}
		else {
			mmcpath[len] = '\0';
		}

		dp = opendir(mmcpath);
		if (dp == NULL) {
			MS_DBG_ERR("dp is NULL");
			return MS_MEDIA_ERR_DIR_OPEN_FAIL;
		}

		while (!readdir_r(dp, &ent, &res)) {
			 /*end of read dir*/
			if (res == NULL)
				break;

			bHasColon = false;
			if (ent.d_name[0] == '.')
				continue;

			if (ent.d_type == DT_DIR) {
				/*ent->d_name is including ':' */
				for (i = 0; i < strlen(ent.d_name); i++) {
					if (ent.d_name[i] == ':') {
						bHasColon = true;
						break;
					}
				}

				if (bHasColon) {
					/*check serial */
					err = ms_strappend(path, sizeof(path), "%s%s/cid", mmcpath, ent.d_name);
					if (err < 0) {
						MS_DBG_ERR("ms_strappend error : %d", err);
						continue;
					}

					if (_get_contents(path, cid) != MS_MEDIA_ERR_NONE)
						break;
					else
						getdata = true;
				}
			}
		}
		closedir(dp);

		if (getdata == true) {
			break;
		}
	}

	return MS_MEDIA_ERR_NONE;
}

ms_dir_scan_type_t
ms_get_mmc_state(void)
{
	char cid[MMC_INFO_SIZE] = { 0 };
	ms_dir_scan_type_t ret = MS_SCAN_ALL;

	/*get new info */
	_ms_get_mmc_info(cid);

	/*check it's same mmc */
	if (_ms_check_mmc_info(cid)) {
		ret = MS_SCAN_PART;
	}

	return ret;
}

int
ms_update_mmc_info(void)
{
	int err;
	char cid[MMC_INFO_SIZE] = { 0 };

	err = _ms_get_mmc_info(cid);

	err = _ms_update_mmc_info(cid);

	/*Active flush */
	if (!malloc_trim(0))
		MS_DBG_ERR("malloc_trim is failed");

	return err;
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

int
ms_present_mmc_status(ms_sdcard_status_type_t status)
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


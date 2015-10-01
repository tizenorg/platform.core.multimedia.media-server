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

#include <vconf.h>

#include "media-util.h"

#include "media-common-utils.h"
#include "media-common-db-svc.h"
#include "media-common-external-storage.h"

#include "media-util-err.h"
#include "media-scanner-dbg-v2.h"
#include "media-scanner-scan-v2.h"
#include "media-scanner-extract-v2.h"
#include "media-scanner-device-block-v2.h"

static void __msc_usb_remove_event(const char *mount_path)
{
	MS_DBG_ERR("===========================================================");
	MS_DBG_ERR("USB REMOVED, mountpath : %s", mount_path);
	MS_DBG_ERR("===========================================================");
	int update_status  = -1;
	int remain_request = 0;
	bool status = FALSE;

	if(!ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &update_status)) {
		MS_DBG_ERR("ms_config_get_int[VCONFKEY_FILEMANAGER_DB_STATUS]");
	}

	if (msc_get_remain_scan_request(MS_SCAN_STORAGE, &remain_request) == MS_MEDIA_ERR_NONE) {
		if (!(remain_request == 0 && update_status == VCONFKEY_FILEMANAGER_DB_UPDATED)) {
			msc_set_blocked_path(mount_path);
		}
		remain_request = 0;
	}


	if (msc_get_dir_scan_status(&status) == MS_MEDIA_ERR_NONE) {
		if (status == TRUE) {
			MS_DBG_ERR("Doing directory scanning. Set cancel path");
			msc_set_cancel_path(mount_path);
		}
		status = FALSE;
	}

	if (msc_get_remain_extract_request(MS_EXTRACT_STORAGE, &remain_request) == MS_MEDIA_ERR_NONE) {
		if (!(remain_request == 0 && update_status == VCONFKEY_FILEMANAGER_DB_UPDATED)) {
			msc_set_extract_blocked_path(mount_path);
		}
		remain_request = 0;
	}

	if (msc_get_dir_scan_status(&status) == MS_MEDIA_ERR_NONE) {
		if (status == true) {
			MS_DBG_ERR("Doing directory extracting. Set cancel path");
			msc_set_extract_cancel_path(mount_path);
		}
		status = FALSE;
	}

	return;
}

int msc_mmc_remove_handler(const char *mount_path)
{
	return MS_MEDIA_ERR_NONE;
}

void _msc_mmc_changed_event(const char *mount_path, ms_stg_status_e mount_status)
{
	/* If scanner is not working, media server executes media scanner and sends request. */
	/* If scanner is working, it detects changing status of SD card. */
	if (mount_status == MS_STG_INSERTED) {
		/*DO NOT THING*/
	} else if (mount_status == MS_STG_REMOVED) {
		msc_set_mmc_status(MS_STG_REMOVED);
		msc_mmc_remove_handler(mount_path);
	}

	return;
}


void msc_device_block_changed_cb(ms_block_info_s *block_info, void *user_data)
{
	if (block_info->block_type == 0) {
		MS_DBG_ERR("GET THE USB EVENT");
		if (block_info->state == MS_STG_INSERTED) {
			/*DO NOT THING*/
		} else {
			__msc_usb_remove_event(block_info->mount_path);
		}
	} else {
		MS_DBG_ERR("GET THE MMC EVENT");
		_msc_mmc_changed_event(block_info->mount_path, block_info->state);
	}
}

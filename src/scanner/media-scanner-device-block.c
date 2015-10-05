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
#include "media-util.h"

#include "media-common-db-svc.h"
#include "media-common-external-storage.h"

#include "media-util-err.h"
#include "media-scanner-dbg.h"
#include "media-scanner-scan.h"
#include "media-scanner-device-block.h"

static void __msc_usb_remove_event(const char *mount_path)
{
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

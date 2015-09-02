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
#include <sys/stat.h>
#include "media-common-db-svc.h"
#include "media-common-external-storage.h"

#include "media-util-err.h"
#include "media-server-dbg.h"
#include "media-server-socket.h"
#include "media-server-device-block.h"

static int __ms_get_added_stroage_path(void **handle, const char *add_path, char **device_id)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *id = NULL;

	*device_id = NULL;

	/* read deive information */
	ret = ms_read_device_info(add_path, &id);
	if (id == NULL) {
		if (ret == MS_MEDIA_ERR_FILE_NOT_EXIST) {
			ret = ms_genarate_uuid(handle, &id);
			ret = ms_write_device_info(add_path , id);
			if (ret == MS_MEDIA_ERR_NONE) {
				*device_id = strdup(id);
			}
		}
	} else {
		*device_id = strdup(id);
	}

	MS_SAFE_FREE(id);

	return ret;
}

int ms_usb_insert_handler(const char *mount_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *storage_id = NULL;
	char *storage_path = NULL;
	void **handle = NULL;
	int validity = 0;
	uid_t uid;
	ms_dir_scan_type_t scan_type = MS_SCAN_ALL;

	ret = ms_load_functions();
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_load_functions failed [%d]", ret);
		return ret;
	}

	ms_sys_get_uid(&uid);
	ms_connect_db(&handle, uid);

	if (mount_path != NULL) {
		MS_DBG_ERR("added path [%s]", mount_path);

		__ms_get_added_stroage_path(handle, mount_path, &storage_id);

		if (storage_id != NULL) {
			/* update storage information into media DB */
			ret = ms_check_storage(handle, storage_id, NULL, &storage_path, &validity);
			if (ret == 0) {
				if (validity == 1) {
					MS_DBG_ERR("This storage is already updated. So ignore this event.");
					ret = MS_MEDIA_ERR_NONE;
					goto ERROR;
				}

				if (strcmp(mount_path, storage_path)) {
					/* update storage path */
					ret = ms_update_storage(handle, storage_id, mount_path, uid);
				}
				scan_type = MS_SCAN_PART;
				ms_set_storage_validity(handle, storage_id, 1, uid);
				if (ms_set_storage_scan_status(handle, storage_id, MEDIA_SCAN_PREPARE, uid) != MS_MEDIA_ERR_NONE){
					MS_DBG_ERR("ms_set_storage_scan_status failed");
				}
			} else {
				/* there is no information of this storage in Media DB */
				ret = ms_insert_storage(handle, storage_id, NULL, mount_path, uid);
			}
		} else {
			MS_DBG_ERR("STORAGE ID IS NUILL");
			ret = MS_MEDIA_ERR_INVALID_PARAMETER;
			goto ERROR;
		}

		/* request to update media db */
		ms_send_storage_otg_scan_request(mount_path, storage_id, scan_type);
	}

ERROR:
	MS_SAFE_FREE(storage_id);
	MS_SAFE_FREE(storage_path);

	ms_disconnect_db(&handle);

	ms_unload_functions();

	return ret;
}

int ms_usb_remove_handler(const char *mount_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	void **handle = NULL;
	char device_id[MS_UUID_SIZE] = {0,};
	uid_t uid;

	if (mount_path != NULL) {
		ret = ms_load_functions();
		if (ret != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_load_functions failed [%d]", ret);
			return ret;
		}

		ms_sys_get_uid(&uid);

		ms_connect_db(&handle, uid);

		while(1) {
			memset(device_id, 0x0, sizeof(device_id));
			ms_get_storage_id(handle, mount_path, device_id);

			MS_DBG_ERR("removed path [%s %s]", mount_path, device_id);

			if (strlen(device_id) == (MS_UUID_SIZE-1)) {
				ms_set_storage_validity(handle, device_id, 0, uid);
				ms_send_storage_otg_scan_request(mount_path, device_id, MS_SCAN_INVALID);
			} else {
				MS_DBG_ERR("Device ID is INVALID");
				break;
			}
		}

		ms_disconnect_db(&handle);

		ms_unload_functions();
	}

	return ret;
}

static ms_dir_scan_type_t __ms_get_mmc_scan_type(const char *cid, const char *storage_name, const char *storage_path)
{
	ms_dir_scan_type_t scan_type = MS_SCAN_ALL;

	MS_DBG("Last MMC id = [%s] MMC path = [%s]", storage_name, storage_path);
	MS_DBG("Current MMC info = [%s]", cid);

	if(storage_name != NULL) {
		if (strcmp(storage_name, cid) == 0) {
			scan_type = MS_SCAN_PART;
		}
	}

	if(scan_type == MS_SCAN_PART)
		MS_DBG("MMC Scan type [MS_SCAN_PART]");
	else
		MS_DBG("MMC Scan type [MS_SCAN_ALL]");

	return scan_type;
}

static int __ms_get_mmc_info(void **handle, char **storage_name, char **storage_path, int *validity, bool *info_exist)
{
	int ret = MS_MEDIA_ERR_NONE;

	ret = ms_get_mmc_info(handle, storage_name, storage_path, validity, info_exist);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("[No-Error] ms_get_mmc_info failed. Maybe storage info not in media db [%d]", ret);
	}

	return ret;
}

static int __ms_insert_mmc_info(void **handle, const char *storage_name, const char *storage_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	uid_t uid;

	ms_sys_get_uid(&uid);

	ret = ms_insert_storage(handle, MMC_STORAGE_ID, storage_name, storage_path, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_insert_storage failed [%d]", ret);
		ret = MS_MEDIA_ERR_INTERNAL;
	}

	return ret;
}

static int __ms_update_mmc_info(void **handle, const char *old_storage_name, const char *new_storage_name, const char *new_storage_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	uid_t uid;

	ms_sys_get_uid(&uid);

	ret = ms_delete_storage(handle, MMC_STORAGE_ID, old_storage_name, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_delete_storage failed [%d]", ret);
	}

	ret = ms_insert_storage(handle, MMC_STORAGE_ID, new_storage_name, new_storage_path, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_insert_storage failed [%d]", ret);
		ret = MS_MEDIA_ERR_INTERNAL;
	}

	return ret;
}

int ms_mmc_insert_handler(const char *mount_path)
{
	/*FIX ME*/
	int ret = MS_MEDIA_ERR_NONE;
	ms_dir_scan_type_t scan_type = MS_SCAN_ALL;
	char *storage_name = NULL;
	char *storage_path = NULL;
	int validity = NULL;
	bool info_exist = FALSE;
	char *cid = NULL;
	uid_t uid;

	void **db_handle = NULL;

	ret = ms_load_functions();
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_load_functions failed [%d]", ret);
		return MS_MEDIA_ERR_INTERNAL;
	}

	ms_sys_get_uid(&uid);

	ret = ms_connect_db(&db_handle, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_connect_db failed [%d]", ret);
		ms_unload_functions();
		return MS_MEDIA_ERR_INTERNAL;
	}

	ms_present_mmc_status(MS_SDCARD_INSERTED);

	ms_get_mmc_id(&cid);
	__ms_get_mmc_info(db_handle, &storage_name, &storage_path, &validity, &info_exist);
	ms_make_default_path_mmc(storage_path);

	if (info_exist == TRUE) {
		scan_type = __ms_get_mmc_scan_type(cid, storage_name, storage_path);
		if(scan_type == MS_SCAN_ALL) /*FIX ME. path should be compared*/
			__ms_update_mmc_info(db_handle, storage_name, cid, mount_path);
	} else {
		__ms_insert_mmc_info(db_handle, cid, mount_path);
	}

	ms_set_storage_validity(db_handle, MMC_STORAGE_ID, 1, uid);

	ms_disconnect_db(&db_handle);
	ms_unload_functions();

	MS_SAFE_FREE(cid);
	MS_SAFE_FREE(storage_name);
	MS_SAFE_FREE(storage_path);

	ms_send_storage_scan_request(mount_path, MMC_STORAGE_ID, scan_type);

	return MS_MEDIA_ERR_NONE;
}

int ms_mmc_remove_handler(const char *mount_path)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *storage_path = NULL;
	void **handle = NULL;
	int validity = 0;
	uid_t uid;

	ret = ms_load_functions();
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_load_functions failed [%d]", ret);
		return MS_MEDIA_ERR_INTERNAL;
	}

	ms_sys_get_uid(&uid);

	ret = ms_connect_db(&handle, uid);
	if (ret != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR("ms_connect_db failed [%d]", ret);
		ms_unload_functions();
		return MS_MEDIA_ERR_INTERNAL;
	}

	if (mount_path == NULL) {
		ret = ms_check_storage(handle, MMC_STORAGE_ID, NULL, &storage_path, &validity);
	} else {
		storage_path = strdup(mount_path);
	}

	if (storage_path != NULL) {
		MS_DBG_ERR("mmc_path[%s]", storage_path);

		ms_set_storage_validity(handle, MMC_STORAGE_ID, 0, uid);

		ms_send_storage_scan_request(storage_path, MMC_STORAGE_ID, MS_SCAN_INVALID);

		MS_SAFE_FREE(storage_path);
	}

	ms_disconnect_db(&handle);
	ms_unload_functions();

	return MS_MEDIA_ERR_NONE;
}

void _ms_mmc_changed_event(const char *mount_path, ms_stg_status_e mount_status)
{
	/* If scanner is not working, media server executes media scanner and sends request. */
	/* If scanner is working, it detects changing status of SD card. */
	if (mount_status == MS_STG_INSERTED) {
		ms_mmc_insert_handler(mount_path);
	} else if (mount_status == MS_STG_REMOVED) {
		/*remove added watch descriptors */
		ms_present_mmc_status(MS_SDCARD_REMOVED);
		ms_mmc_remove_handler(mount_path);
	}

	return;
}

void _ms_usb_changed_event(const char *mount_path, ms_stg_status_e mount_status)
{
	if (mount_status == MS_STG_INSERTED) {
		ms_usb_insert_handler(mount_path);
	} else if (mount_status == MS_STG_REMOVED) {
		/*remove added watch descriptors */
		ms_usb_remove_handler(mount_path);
	}

	return;
}

void ms_device_block_changed_cb(const char *mount_path, int block_type, ms_stg_status_e mount_status, void *user_data)
{
	if (block_type == 0) {
		MS_DBG_ERR("GET THE USB EVENT");
		_ms_usb_changed_event(mount_path, mount_status);
	} else {
		MS_DBG_ERR("GET THE MMC EVENT");
		_ms_mmc_changed_event(mount_path, mount_status);
	}
}

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

/**
 * This file defines api utilities of contents manager engines.
 *
 * @file		media-server-common.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */
#include <media-svc.h>
#include <media-thumbnail.h>
#include <audio-svc-error.h>
#include <audio-svc.h>
#include <db-util.h>
#include <pmapi.h>
#include <vconf.h>
#include <drm-service.h>
#ifdef PROGRESS
#include <quickpanel.h>
#endif
#include <aul/aul.h>
#include <mmf/mm_file.h>

#include "media-server-global.h"
#include "media-server-common.h"
#include "media-server-inotify.h"

#ifdef FMS_PERF
#include <sys/time.h>
#define MILLION 1000000L
#endif

#define CONTENT_TYPE_NUM 4
#define MUSIC_MIME_NUM 28
#define SOUND_MIME_NUM 1
#define MIME_TYPE_LENGTH 255
#define MIME_LENGTH 50
#define _3GP_FILE ".3gp"
#define _MP4_FILE ".mp4"
#define MMC_INFO_SIZE 256

int mmc_state = 0;
int ums_mode = 0;
int current_usb_mode = 0;

extern GAsyncQueue *scan_queue;

struct timeval g_mmc_start_time;
struct timeval g_mmc_end_time;

typedef struct {
	char content_type[15];
	int category_by_mime;
} fex_content_table_t;

static const fex_content_table_t content_category[CONTENT_TYPE_NUM] = {
	{"audio", MS_CATEGORY_SOUND},
	{"image", MS_CATEGORY_IMAGE},
	{"video", MS_CATEGORY_VIDEO},
	{"application", MS_CATEGORY_ETC},
};

static const char music_mime_table[MUSIC_MIME_NUM][MIME_LENGTH] = {
	/*known mime types of normal files*/
	"mpeg",
	"ogg",
	"x-ms-wma",
	"x-flac",
	"mp4",
	/* known mime types of drm files*/
	"mp3",
	"x-mp3", /*alias of audio/mpeg*/
	"x-mpeg", /*alias of audio/mpeg*/
	"3gpp",
	"x-ogg", /*alias of  audio/ogg*/
	"vnd.ms-playready.media.pya:*.pya", /*playready*/
	"wma",
	"aac",
	"x-m4a", /*alias of audio/mp4*/
	/* below mimes are rare*/
	"x-vorbis+ogg",
	"x-flac+ogg",
	"x-matroska",
	"ac3",
	"mp2",
	"x-ape",
	"x-ms-asx",
	"vnd.rn-realaudio",

	"x-vorbis", /*alias of audio/x-vorbis+ogg*/
	"vorbis", /*alias of audio/x-vorbis+ogg*/
	"x-oggflac",
	"x-mp2", /*alias of audio/mp2*/
	"x-pn-realaudio", /*alias of audio/vnd.rn-realaudio*/
	"vnd.m-realaudio", /*alias of audio/vnd.rn-realaudio*/
};

static const char sound_mime_table[SOUND_MIME_NUM][MIME_LENGTH] = {
	"x-smaf",
};

int ms_update_valid_type(char *path)
{
	MS_DBG_START();

	int res = MS_ERR_NONE;
	int err;
	int category = MS_CATEGORY_UNKNOWN;

	MS_DBG("%s", path);

	err = ms_get_category_from_mime(path, &category);
	if (err < 0) {
		MS_DBG("ms_get_category_from_mime fails");
		return err;
	}

	/*if music, call mp_svc_set_item_valid() */
	if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		/*check exist in Music DB, If file is not exist, insert data in DB. */
		err = audio_svc_check_item_exist(path);
		if (err == AUDIO_SVC_ERROR_DB_NO_RECORD) {
			MS_DBG("not exist in Music DB. insert data");
#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(path, category);
#else
			err = ms_media_db_insert(path, category, true);
#endif 
			if (err != MS_ERR_NONE) {
				MS_DBG ("ms_media_db_insert() error %d", err);
				res = err;
			}
		}
		else if (err == AUDIO_SVC_ERROR_NONE) {
			/*if meta data of file exist, change valid field to "1" */
			MS_DBG("Item exist");

			err = audio_svc_set_item_valid(path, true);
			if (err != AUDIO_SVC_ERROR_NONE)
				MS_DBG("audio_svc_set_item_valid() error : %d", err);
		}
		else
		{
			MS_DBG("audio_svc_check_item_exist() error : %d", err);
			res = MS_ERR_DB_OPERATION_FAIL;
		}
	}
	/*if file is image file, call mb_svc_set_db_item_valid() */
	else if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
		ms_store_type_t store_type;
		minfo_store_type mb_stroage;

		store_type = ms_get_store_type_by_full(path);
		mb_stroage = (store_type == MS_MMC) ? MINFO_MMC : MINFO_PHONE;

		err = minfo_set_item_valid(mb_stroage, path, true);
		if (err != MB_SVC_ERROR_NONE) {
			MS_DBG("not exist in Media DB. insert data");

			ms_update_valid_type_end();

#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(path, category);
#else
			err = ms_media_db_insert(path, category, true);
#endif 
			if (err != MS_ERR_NONE)
				MS_DBG("ms_media_db_insert() error : %d", err);

			ms_update_valid_type_start();
		}
		else {
			MS_DBG("Item exist");
		}
	}

	if (category & MS_CATEGORY_DRM) {
		DRM_RESULT drm_res;

		ms_inoti_add_ignore_file(path);
		drm_res = drm_svc_register_file(path);
		if (drm_res != DRM_RESULT_SUCCESS) {
			MS_DBG("drm_svc_register_file error : %d", drm_res);
			res = MS_ERR_DRM_REGISTER_FAIL;
		}
	}

	MS_DBG_END();

	return res;
}

static int _ms_set_power_mode(ms_db_status_type_t status)
{
	int res = MS_ERR_NONE;
	int err;

	switch (status) {
	case MS_DB_UPDATING:
		err = pm_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
		if (err != 0)
			res = MS_ERR_UNKNOWN_ERROR;
		break;
	case MS_DB_UPDATED:
		err = pm_unlock_state(LCD_OFF, STAY_CUR_STATE);
		if (err != 0)
			res = MS_ERR_UNKNOWN_ERROR;
		break;
	default:
		MS_DBG("Unacceptable type : %d", status);
		break;
	}

	return res;
}

int ms_set_db_status(ms_db_status_type_t status)
{
	int res = MS_ERR_NONE;
	int err = 0;

	if (status == MS_DB_UPDATING) {
		if (ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS, VCONFKEY_FILEMANAGER_DB_UPDATING))
			  res = MS_ERR_VCONF_SET_FAIL;
	} else if (status == MS_DB_UPDATED) {
		if(ms_config_set_int(VCONFKEY_FILEMANAGER_DB_STATUS,  VCONFKEY_FILEMANAGER_DB_UPDATED))
			  res = MS_ERR_VCONF_SET_FAIL;
	}

	err = _ms_set_power_mode(status);
	if (err != MS_ERR_NONE) {
		MS_DBG("_ms_set_power_mode fail");
		res = err;
	}

	return res;
}

GMutex * db_mutex;

int ms_db_init(bool need_db_create)
{
	MS_DBG_START();

	ms_scan_data_t *phone_scan_data;
	ms_scan_data_t *mmc_scan_data;

	phone_scan_data = malloc(sizeof(ms_scan_data_t));
	mmc_scan_data = malloc(sizeof(ms_scan_data_t));
	MS_DBG("*********************************************************");
	MS_DBG("*** Begin to check tables of file manager in database ***");
	MS_DBG("*********************************************************");

	fex_make_default_path();

	phone_scan_data->db_type = MS_PHONE;

	if (need_db_create) {
		/*insert records */
		MS_DBG("Create DB");

		phone_scan_data->scan_type = MS_SCAN_ALL;
	} else {
		MS_DBG("JUST ADD WATCH");
		phone_scan_data->scan_type = MS_SCAN_NONE;
	}

	/*push data to fex_dir_scan_cb */
	g_async_queue_push(scan_queue, GINT_TO_POINTER(phone_scan_data));

	if (ms_is_mmc_supported()) {
		mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;

#ifdef _WITH_DRM_SERVICE_
		if (drm_svc_insert_ext_memory() == DRM_RESULT_SUCCESS)
			MS_DBG
			    ("fex_db_service_init : drm_svc_insert_ext_memory OK");
#endif

		fex_make_default_path_mmc();

		mmc_scan_data->scan_type = ms_get_mmc_state();
		mmc_scan_data->db_type = MS_MMC;

		MS_DBG("ms_get_mmc_state is %d", mmc_scan_data->scan_type);

		g_async_queue_push(scan_queue, GINT_TO_POINTER(mmc_scan_data));
	} 

	MS_DBG_END();
	return 0;
}

int ms_media_db_open(void)
{
	int err;

	/*Lock mutex for openning db*/
	g_mutex_lock(db_mutex);

	err = minfo_init();
	if (err != MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_init() error : %d", err);

		g_mutex_unlock(db_mutex);

		return MS_ERR_DB_CONNECT_FAIL;
	}

	err = audio_svc_open();
	if (err != AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_open() error : %d", err);
		err = minfo_finalize();
		if (err != MB_SVC_ERROR_NONE)
			MS_DBG("minfo_finalize() error: %d", err);

		g_mutex_unlock(db_mutex);

		return MS_ERR_DB_CONNECT_FAIL;
	}
	MS_DBG("connect Media DB");

	g_mutex_unlock(db_mutex);

	return MS_ERR_NONE;
}

int ms_media_db_close(void)
{
	int err;

	err = audio_svc_close();
	if (err != AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_close() error : %d", err);
		return MS_ERR_DB_DISCONNECT_FAIL;
	}

	err = minfo_finalize();
	if (err != MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_finalize() error: %d", err);
		return MS_ERR_DB_DISCONNECT_FAIL;
	}

	MS_DBG("Disconnect Media DB");

	return MS_ERR_NONE;
}

int
_ms_update_mmc_info(const char *cid)
{
	MS_DBG_START();
	bool res;

	if (cid == NULL) {
		MS_DBG("Parameters are invalid");
		return MS_ERR_ARG_INVALID;
	}

	res = ms_config_set_str(MS_MMC_INFO_KEY, cid);
	if (!res) {
		MS_DBG("fail to get MS_MMC_INFO_KEY");
		return MS_ERR_VCONF_SET_FAIL;
	}

	MS_DBG_END();

	return MS_ERR_NONE;
}

bool
_ms_check_mmc_info(const char *cid)
{
	MS_DBG_START();

	char pre_mmc_info[MMC_INFO_SIZE] = { 0 };
	bool res = false;

	if (cid == NULL) {
		MS_DBG("Parameters are invalid");
		return false;
	}

	res = ms_config_get_str(MS_MMC_INFO_KEY, pre_mmc_info);
	if (!res) {
		MS_DBG("fail to get MS_MMC_INFO_KEY");
		return false;
	}

	MS_DBG("Last MMC info	= %s", pre_mmc_info);
	MS_DBG("Current MMC info = %s", cid);

	if (strcmp(pre_mmc_info, cid) == 0) {
		return true;
	}

	MS_DBG_END();
	return false;
}

static int _get_contents(const char *filename, char *buf)
{
	FILE *fp;

	MS_DBG("%s", filename);

	fp = fopen(filename, "rt");
	if (fp == NULL) {
		MS_DBG("fp is NULL");
		return MS_ERR_FILE_OPEN_FAIL;
	}
	fgets(buf, 255, fp);

	fclose(fp);

	return MS_ERR_NONE;
}

/*need optimize*/
int _ms_get_mmc_info(char *cid)
{
	MS_DBG_START();

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
			MS_DBG("FAIL : snprintf");
			return MS_ERR_UNKNOWN_ERROR;
		}
		else {
			mmcpath[len] = '\0';
		}

		dp = opendir(mmcpath);

		if (dp == NULL) {
			MS_DBG("dp is NULL");
			{
				return MS_ERR_DIR_OPEN_FAIL;
			}
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
						MS_DBG("FAIL : ms_strappend");
						continue;
					}

					if (_get_contents(path, cid) != MS_ERR_NONE)
						break;
					else
						getdata = true;
					MS_DBG("MMC serial : %s", cid);
				}
			}
		}
		closedir(dp);

		if (getdata == true) {
			break;
		}
	}
	
	MS_DBG_END();

	return MS_ERR_NONE;
}

int ms_update_mmc_info(void)
{
	int err;
	char cid[MMC_INFO_SIZE] = { 0 };

	err = _ms_get_mmc_info(cid);

	err = _ms_update_mmc_info(cid);

	/*Active flush */
	if (malloc_trim(0))
		MS_DBG("SUCCESS malloc_trim");

	return err;
}

void ms_mmc_removed_handler(void)
{
	ms_scan_data_t *mmc_scan_data;

	mmc_scan_data = malloc(sizeof(ms_scan_data_t));

	mmc_scan_data->scan_type = MS_SCAN_VALID;
	mmc_scan_data->db_type = MS_MMC;

	g_async_queue_push(scan_queue, GINT_TO_POINTER(mmc_scan_data));

	/*remove added watch descriptors */
	ms_inoti_remove_watch_recursive(MS_MMC_ROOT_PATH);

	ms_inoti_delete_mmc_ignore_file();

	if (drm_svc_extract_ext_memory() == DRM_RESULT_SUCCESS)
		MS_DBG("drm_svc_extract_ext_memory OK");
}

void ms_mmc_vconf_cb(void *data)
{
	MS_DBG_START();

	int status = 0;
	ms_scan_data_t *scan_data;

	MS_DBG("Received MMC noti from vconf : %d", status);

	if (!ms_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &status)) {
		MS_DBG("........Get VCONFKEY_SYSMAN_MMC_STATUS failed........");
	}

	MS_DBG("ms_config_get_int : VCONFKEY_SYSMAN_MMC_STATUS END = %d",
	       status);

	mmc_state = status;

	if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF)
		return;

	if (mmc_state == VCONFKEY_SYSMAN_MMC_REMOVED ||
		mmc_state == VCONFKEY_SYSMAN_MMC_INSERTED_NOT_MOUNTED) {
		ms_mmc_removed_handler();
	} 
	else if (mmc_state == VCONFKEY_SYSMAN_MMC_MOUNTED) {
		scan_data = malloc(sizeof(ms_scan_data_t));
#ifdef _WITH_DRM_SERVICE_
		if (drm_svc_insert_ext_memory() == DRM_RESULT_SUCCESS)
			MS_DBG("drm_svc_insert_ext_memory OK");
#endif
		fex_make_default_path_mmc();

		scan_data->scan_type = ms_get_mmc_state();
		scan_data->db_type = MS_MMC;

		MS_DBG("ms_get_mmc_state is %d", scan_data->scan_type);

		g_async_queue_push(scan_queue, GINT_TO_POINTER(scan_data));
	}
	
	MS_DBG_END();
	
	return;
}

#ifdef FMS_PERF

void ms_check_start_time(struct timeval *start_time)
{
	gettimeofday(start_time, NULL);
}

void ms_check_end_time(struct timeval *end_time)
{
	gettimeofday(end_time, NULL);
}

void ms_check_time_diff(struct timeval *start_time, struct timeval *end_time)
{
	struct timeval time;
	long difftime;

	time.tv_sec = end_time->tv_sec - start_time->tv_sec;
	time.tv_usec = end_time->tv_usec - start_time->tv_usec;
	difftime = MILLION * time.tv_sec + time.tv_usec;
	MS_DBG("The function_to_time took %ld microseconds or %f seconds.",
	       difftime, difftime / (double)MILLION);
}

#endif

void ms_usb_vconf_cb(void *data)
{
	MS_DBG_START();

	int status = 0;

	MS_DBG("Received usb noti from vconf : %d", status);

	if (!ms_config_get_int(VCONFKEY_USB_STORAGE_STATUS, &status)) {
		MS_DBG
		    ("........Get VCONFKEY_USB_STORAGE_STATUS failed........");
	}

	MS_DBG("ms_config_get_int : VCONFKEY_USB_STORAGE_STATUS END = %d",
	       status);
	current_usb_mode = status;

	if (current_usb_mode == VCONFKEY_USB_STORAGE_STATUS_OFF) {
		if (ums_mode != VCONFKEY_USB_STORAGE_STATUS_OFF) {
			ms_scan_data_t *int_scan;

			fex_make_default_path();

			int_scan = malloc(sizeof(ms_scan_data_t));

			int_scan->db_type = MS_PHONE;
			int_scan->scan_type = MS_SCAN_PART;

			/*push data to fex_dir_scan_cb */
			g_async_queue_push(scan_queue, GINT_TO_POINTER(int_scan));

			if (ms_is_mmc_supported()) {
				ms_scan_data_t *ext_scan;

				/*prepare to insert drm data and delete previous drm datas */
				if (drm_svc_insert_ext_memory() ==
				    DRM_RESULT_SUCCESS)
					MS_DBG("drm_svc_insert_ext_memory OK");

				ext_scan = malloc(sizeof(ms_scan_data_t));
				mmc_state = VCONFKEY_SYSMAN_MMC_MOUNTED;

				ext_scan->db_type = MS_MMC;
				ext_scan->scan_type = MS_SCAN_PART;

				/*push data to fex_dir_scan_cb */
				g_async_queue_push(scan_queue, GINT_TO_POINTER(ext_scan));
			}
		}
		ums_mode = VCONFKEY_USB_STORAGE_STATUS_OFF;
		ms_config_set_int(MS_USB_MODE_KEY, MS_VCONFKEY_NORMAL_MODE);
	} 
	else {
		if (ums_mode == VCONFKEY_USB_STORAGE_STATUS_OFF) {
			MS_DBG("VCONFKEY_USB_STORAGE_STATUS : %d", current_usb_mode);
			ms_scan_data_t *int_scan;

			ums_mode = current_usb_mode;

			int_scan = malloc(sizeof(ms_scan_data_t));			
			int_scan->scan_type = MS_SCAN_VALID;
			int_scan->db_type = MS_PHONE;

			g_async_queue_push(scan_queue, GINT_TO_POINTER(int_scan));

			ms_inoti_remove_watch_recursive(MS_PHONE_ROOT_PATH);

			if (ums_mode == VCONFKEY_USB_STORAGE_STATUS_UMS_MMC_ON) {

				ms_scan_data_t *ext_scan;

				ext_scan = malloc(sizeof(ms_scan_data_t));
				ext_scan->scan_type = MS_SCAN_VALID;
				ext_scan->db_type = MS_MMC;

				g_async_queue_push(scan_queue, GINT_TO_POINTER(ext_scan));
				
				ms_inoti_remove_watch_recursive(MS_MMC_ROOT_PATH);

				/*notify to drm-server */
				if (drm_svc_extract_ext_memory() == DRM_RESULT_SUCCESS)
					MS_DBG("drm_svc_extract_ext_memory OK");
			}

			/*delete all drm contect from drm server */
			if (drm_svc_unregister_all_contents() == DRM_RESULT_SUCCESS)
				MS_DBG("drm_svc_unregister_all_contents OK");

			ms_config_set_int(MS_USB_MODE_KEY, MS_VCONFKEY_MASS_STORAGE_MODE);
		}
	}

	MS_DBG_END();
	return;
}

bool ms_is_mmc_supported(void)
{
	MS_DBG_START();

	/*TODO-Q: simulator support? */
	if (0)
		/* if (strcmp(FEXPLORER_MMC_ROOT_PATH, "/mnt/mmc") == 0) //simulator */
	{
		return true;
	} else {
		int data = -1;
		ms_config_get_int(VCONFKEY_SYSMAN_MMC_STATUS, &data);
		MS_DBG("%s is  %d ", VCONFKEY_SYSMAN_MMC_STATUS, data);
		if (data != VCONFKEY_SYSMAN_MMC_MOUNTED)
			return false;
		else
			return true;
	}

	MS_DBG_END();
	return false;
}

ms_dir_scan_type_t ms_get_mmc_state(void)
{
	char cid[MMC_INFO_SIZE] = { 0 };
	ms_dir_scan_type_t ret = MS_SCAN_ALL;

	/*get new info */
	_ms_get_mmc_info(cid);

	/*check it's same mmc */
	if (_ms_check_mmc_info(cid)) {
		MS_DBG("Detected same MMC! but needs to update the changes...");
		ret = MS_SCAN_PART;
	}

	return ret;
}

GAsyncQueue* soc_queue;
GArray *reg_list;
GMutex *list_mutex;
GMutex *queue_mutex;

void _ms_insert_reg_list(const char *path)
{
	char *reg_path = strdup(path);

	g_mutex_lock(list_mutex);

	g_array_append_val(reg_list, reg_path);

	g_mutex_unlock(list_mutex);
}

bool _ms_find_reg_list(const char *path)
{
	int i;
	int len = reg_list->len;
	char *data;
	bool ret = false;

	g_mutex_lock(list_mutex);
	MS_DBG("array length : %d", len);

	for(i = 0; i < len; i++) {
		data = g_array_index (reg_list, char*, i);
		if(!strcmp(data, path))
			ret = true;
	}

	g_mutex_unlock(list_mutex);

	return ret;
}

void _ms_delete_reg_list(const char *path)
{
	int i;
	int len = reg_list->len;
	char *data;

	MS_DBG("Delete : %s", path);
	g_mutex_lock(list_mutex);

	for(i = 0; i < len; i++) {
		data = g_array_index (reg_list, char*, i);
		MS_DBG("%s", data);
		if(!strcmp(data, path))	{
			MS_DBG("Delete complete : %s", data);
			free(data);
			g_array_remove_index(reg_list, i);
			break;
		}
	}

	g_mutex_unlock(list_mutex);
}

void ms_register_start(void)
{
	minfo_add_media_start(MS_REGISTER_COUNT);
	audio_svc_insert_item_start(MS_REGISTER_COUNT);
}

void ms_register_end(void)
{
	minfo_add_media_end();
	audio_svc_insert_item_end();
}

int ms_register_file(const char *path, GAsyncQueue* queue)
{
	MS_DBG_START();
	MS_DBG("[%d]register file : %s", syscall(__NR_gettid), path);

	int err;
	int category = MS_CATEGORY_UNKNOWN;

	if (path == NULL) {
		MS_DBG("path == NULL");
		return MS_ERR_ARG_INVALID;
	}

	g_mutex_lock(queue_mutex);
	/*first request for this file*/
	if(!_ms_find_reg_list(path)) {
		/*insert registering file list*/
		_ms_insert_reg_list(path);
	} else {
		MS_DBG("______________________ALREADY INSERTING");
		if(queue != NULL) {
			MS_DBG("Need reply");
			soc_queue = queue;
		}
		g_mutex_unlock(queue_mutex);
		return MS_ERR_NOW_REGISTER_FILE;
	}
	g_mutex_unlock(queue_mutex);

	err = ms_get_category_from_mime(path, &category);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error : %d", err);
		goto FREE_RESOURCE;
	}

	if (category <= MS_CATEGORY_ETC) {
		MS_DBG("This is not media contents");
		err = MS_ERR_NOT_MEDIA_FILE;
		goto FREE_RESOURCE;
	} else {
#ifdef THUMB_THREAD
			err = ms_media_db_insert_with_thumb(path, category);
#else
			err = ms_media_db_insert(path, category, false);
#endif 
		if (err != MS_ERR_NONE) {
			MS_DBG("ms_media_db_insert error : %d", err);

			/*if music, call mp_svc_set_item_valid() */
			if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
				/*check exist in Music DB, If file is not exist, insert data in DB. */
				err = audio_svc_check_item_exist(path);
				if (err == AUDIO_SVC_ERROR_NONE) {
					MS_DBG("Audio Item exist");
					err = MS_ERR_NONE;
				}
			} else if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
				Mitem *mi = NULL;

				/*get an item based on its url. */
				err = minfo_get_item(path, &mi);
				if (err == MB_SVC_ERROR_NONE) {
					MS_DBG("Visual Item exist");
					err = MS_ERR_NONE;
				}

				minfo_destroy_mtype_item(mi);
			}
		}

		if (category & MS_CATEGORY_DRM) {
			MS_DBG("THIS IS DRM FILE");
			DRM_RESULT res;

			ms_inoti_add_ignore_file(path);
			res = drm_svc_register_file(path);
			if (res != DRM_RESULT_SUCCESS) {
				MS_DBG("drm_svc_register_file error : %d", res);
				err = MS_ERR_DRM_REGISTER_FAIL;
			}
		}
	}

FREE_RESOURCE:
	g_mutex_lock(queue_mutex);
	
	_ms_delete_reg_list(path);

	if (soc_queue != NULL) {
		MS_DBG("%d", err);
		g_async_queue_push(soc_queue, GINT_TO_POINTER(err+MS_ERR_MAX));
		MS_DBG("Return OK");
	}
	soc_queue = NULL;
	g_mutex_unlock(queue_mutex);
	MS_DBG_END();
	return err;
}

int ms_register_scanfile(const char *path)
{
	MS_DBG_START();
	MS_DBG("register scanfile : %s", path);

	int err = MS_ERR_NONE;
	bool res = false;
	int category = MS_CATEGORY_UNKNOWN;

	if (path == NULL) {
		MS_DBG("path == NULL");
		return MS_ERR_ARG_INVALID;
	}

	err = ms_get_category_from_mime(path, &category);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error : %d", err);
		return err;
	}

	if (category <= MS_CATEGORY_ETC) {
		MS_DBG("This is not media contents");
		return MS_ERR_NOT_MEDIA_FILE;
	}

	err = ms_media_db_insert(path, category, true);
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_media_db_insert error : %d", err);
	}

	if (category & MS_CATEGORY_DRM) {
		MS_DBG("THIS IS DRM FILE");
		DRM_RESULT res;

		ms_inoti_add_ignore_file(path);

		res = drm_svc_register_file(path);
		if (res != DRM_RESULT_SUCCESS) {
			MS_DBG("drm_svc_register_file error : %d", err);
			err = MS_ERR_DRM_REGISTER_FAIL;
		}
	}

	MS_DBG_END();
	return err;
}


int ms_start(bool need_db_create)
{
	int err = 0;

	err = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				     (vconf_callback_fn) ms_mmc_vconf_cb, NULL);
	if (err == -1)
		MS_DBG("add call back function for event %s fails",
		       VCONFKEY_SYSMAN_MMC_STATUS);

	err = vconf_notify_key_changed(VCONFKEY_USB_STORAGE_STATUS,
				     (vconf_callback_fn) ms_usb_vconf_cb, NULL);
	if (err == -1)
		MS_DBG("add call back function for event %s fails",
		       VCONFKEY_USB_STORAGE_STATUS);

	ms_db_init(need_db_create);

	malloc_trim(0);

	return err;
}

void ms_end(void)
{
	/***********
	**remove call back functions
	************/
	vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_STATUS,
				 (vconf_callback_fn) ms_mmc_vconf_cb);
	vconf_ignore_key_changed(VCONFKEY_USB_STORAGE_STATUS,
				 (vconf_callback_fn) ms_usb_vconf_cb);

	/*Clear db mutex variable*/
	g_mutex_free (db_mutex);
}

int ms_get_full_path_from_node(ms_dir_scan_info * const node, char *ret_path)
{
	int err = 0;
	ms_dir_scan_info *cur_node;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char tmp_path[MS_FILE_PATH_LEN_MAX] = { 0 };

	cur_node = node;
	MS_DBG("%s", cur_node->name);

	while (1) {
		err = ms_strappend(path, sizeof(path), "/%s%s", cur_node->name, tmp_path);
		if (err < 0) {
			MS_DBG("ms_strappend error : %d", err);
			return err;
		}

		strncpy(tmp_path, path, MS_FILE_PATH_LEN_MAX);

		if (cur_node->parent == NULL)
			break;

		cur_node = cur_node->parent;
		memset(path, 0, MS_FILE_PATH_LEN_MAX);
	}

	strncpy(ret_path, path, MS_FILE_PATH_LEN_MAX);

	return err;
}

ms_store_type_t ms_get_store_type_by_full(const char *path)
{
	if (strncmp(path + 1, MS_PHONE_ROOT_PATH, strlen(MS_PHONE_ROOT_PATH)) == 0) {
		return MS_PHONE;
	} else
	    if (strncmp(path + 1, MS_MMC_ROOT_PATH, strlen(MS_MMC_ROOT_PATH)) == 0) {
		return MS_MMC;
	} else
		return MS_ERR_INVALID_FILE_PATH;
}

int
ms_strappend(char *res, const int size, const char *pattern,
	     const char *str1, const char *str2)
{
	int len = 0;
	int real_size = size - 1;

	if (!res ||!pattern || !str1 ||!str2 )
		return MS_ERR_ARG_INVALID;

	if (real_size < (strlen(str1) + strlen(str2)))
		return MS_ERR_OUT_OF_RANGE;

	len = snprintf(res, real_size, pattern, str1, str2);
	if (len < 0) {
		MS_DBG("MS_ERR_ARG_INVALID");
		return MS_ERR_ARG_INVALID;
	}

	res[len] = '\0';

	return MS_ERR_NONE;
}

int
ms_strcopy(char *res, const int size, const char *pattern, const char *str1)
{
	int len = 0;
	int real_size = size - 1;

	if (!res || !pattern || !str1)
		return MS_ERR_ARG_INVALID;

	if (real_size < strlen(str1))
		return MS_ERR_OUT_OF_RANGE;

	len = snprintf(res, real_size, pattern, str1);
	if (len < 0) {
		MS_DBG("MS_ERR_ARG_INVALID");
		return MS_ERR_ARG_INVALID;
	}

	res[len] = '\0';

	return MS_ERR_NONE;
}

#ifdef _WITH_SETTING_AND_NOTI_
bool ms_config_get_int(const char *key, int *value)
{
	int err;

	if (!key || !value) {
		MS_DBG("Arguments key or value is NULL");
		return false;
	}

	err = vconf_get_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MS_DBG("Unexpected error code: %d", err);

	return false;
}

bool ms_config_set_int(const char *key, int value)
{
	int err;

	if (!key) {
		MS_DBG("Arguments key is NULL");
		return false;
	}

	err = vconf_set_int(key, value);
	if (err == 0)
		return true;
	else if (err == -1)
		return false;
	else
		MS_DBG("Unexpected error code: %d", err);

	return false;
}

bool ms_config_get_str(const char *key, char *value)
{
	char *res;
	if (!key || !value) {
		MS_DBG("Arguments key or value is NULL");
		return false;
	}

	res = vconf_get_str(key);
	if (res) {
		strncpy(value, res, strlen(res) + 1);
		return true;
	}

	return false;
}

bool ms_config_set_str(const char *key, const char *value)
{
	int err;

	if (!key || !value) {
		MS_DBG("Arguments key or value is NULL");
		return false;
	}

	err = vconf_set_str(key, value);
	if (err == 0)
		return true;
	else
		MS_DBG("fail to vconf_set_str %d", err);

	return false;
}
#endif				/* _WITH_SETTING_AND_NOTI_ */

bool fex_make_default_path(void)
{
	int i = 0;
	int ret = 0;
	bool create_flag = false;
	DIR *dp = NULL;

	char default_path[10][256] = {
		{"/opt/media/Images and videos"},
		{"/opt/media/Images and videos/Wallpapers"},
		{"/opt/media/Sounds and music"},
		{"/opt/media/Sounds and music/Music"},
		{"/opt/media/Sounds and music/Ringtones"},
		{"/opt/media/Sounds and music/Alerts"},
		{"/opt/media/Sounds and music/FM Radio"},
		{"/opt/media/Sounds and music/Voice recorder"},
		{"/opt/media/Downloads"},
		{"/opt/media/Camera shots"}
	};

	for (i = 0; i < 10; ++i) {
		dp = opendir(default_path[i]);
		if (dp == NULL) {
			ret = mkdir(default_path[i], 0777);
			if (ret < 0) {
				MS_DBG("make fail");
			} else {
				create_flag = true;
				ms_inoti_add_watch(default_path[i]);
			}
		} else {
			closedir(dp);
		}
	}

	return create_flag;
}

bool fex_make_default_path_mmc(void)
{
	MS_DBG_START();

	int i = 0;
	int ret = 0;
	bool create_flag = false;
	DIR *dp = NULL;

	char default_path[10][256] = {
		{"/opt/storage/sdcard/Images and videos"},
		{"/opt/storage/sdcard/Images and videos/Wallpapers"},
		{"/opt/storage/sdcard/Sounds and music"},
		{"/opt/storage/sdcard/Sounds and music/Music"},
		{"/opt/storage/sdcard/Sounds and music/Ringtones"},
		{"/opt/storage/sdcard/Sounds and music/Alerts"},
		{"/opt/storage/sdcard/Sounds and music/FM Radio"},
		{"/opt/storage/sdcard/Sounds and music/Voice recorder"},
		{"/opt/storage/sdcard/Downloads"},
		{"/opt/storage/sdcard/Camera shots"}
	};

	for (i = 0; i < 10; ++i) {
		dp = opendir(default_path[i]);
		if (dp == NULL) {
			ret = mkdir(default_path[i], 0777);
			if (ret < 0) {
				MS_DBG("make fail");
			} else {
				create_flag = true;
				ms_inoti_add_watch(default_path[i]);
			}
		} else {
			closedir(dp);
		}
	}

	MS_DBG_END();

	return create_flag;
}

static int _ms_get_mime_by_drm_info(const char *path, char *mime)
{
#ifdef _WITH_DRM_SERVICE_
	int res = MS_ERR_NONE;
	drm_content_info_t contentInfo = { 0 };

	if (path == NULL || mime == NULL)
		return MS_ERR_ARG_INVALID;

	res = drm_svc_get_content_info(path, &contentInfo);
	if (res != DRM_RESULT_SUCCESS) {
		MS_DBG("drm_svc_get_content_info() fails. ");
		return res;
	}

	strncpy(mime, contentInfo.contentType, MS_DRM_CONTENT_TYPE_LENGTH);

	return res;
#else
	return MS_ERR_NONE;
#endif
}

int ms_get_category_from_mime(const char *path, int *category)
{
	int i = 0;
	int err = 0;
	char mimetype[MIME_TYPE_LENGTH];

	if (path == NULL || category == NULL)
		return MS_ERR_ARG_INVALID;

	*category = MS_CATEGORY_UNKNOWN;

	/*get content type and mime type from file. */
	/*in case of drm file. */
	if (drm_svc_is_drm_file(path) == DRM_TRUE) {
		DRM_FILE_TYPE drm_type = DRM_FILE_TYPE_NONE;
		drm_type = drm_svc_get_drm_type(path);
		if (drm_type == DRM_FILE_TYPE_NONE) {
			*category = MS_CATEGORY_UNKNOWN;
			return err;
		} 
		else {
			err =  _ms_get_mime_by_drm_info(path, mimetype);
			if (err < 0) {
				*category = MS_CATEGORY_UNKNOWN;
				return err;
			}
			*category |= MS_CATEGORY_DRM;
		}
	} 
	else {
		/*in case of normal files */
		if (aul_get_mime_from_file(path, mimetype, sizeof(mimetype)) < 0) {
			MS_DBG("aul_get_mime_from_file fail");
			*category = MS_CATEGORY_UNKNOWN;
			return MS_ERR_ARG_INVALID;
		}
	}

	MS_DBG("mime type : %s", mimetype);

	/*categorize from mimetype */
	for (i = 0; i < CONTENT_TYPE_NUM; i++) {
		if (strstr(mimetype, content_category[i].content_type) != NULL) {
			*category = (*category | content_category[i].category_by_mime);
			break;
		}
	}

	/*in application type, exitst sound file ex) x-smafs */
	if (*category & MS_CATEGORY_ETC) {
		int prefix_len = strlen(content_category[0].content_type);

		for (i = 0; i < SOUND_MIME_NUM; i++) {
			if (strstr(mimetype + prefix_len, sound_mime_table[i]) != NULL) {
				*category ^= MS_CATEGORY_ETC;
				*category |= MS_CATEGORY_SOUND;
				break;
			}
		}
	}

	/*check music file in soun files. */
	if (*category & MS_CATEGORY_SOUND) {
		int prefix_len = strlen(content_category[0].content_type) + 1;

		MS_DBG("mime_type : %s", mimetype + prefix_len);

		for (i = 0; i < MUSIC_MIME_NUM; i++) {
			if (strcmp(mimetype + prefix_len, music_mime_table[i]) == 0) {
				*category ^= MS_CATEGORY_SOUND;
				*category |= MS_CATEGORY_MUSIC;
				break;
			}
		}
	} else if (*category & MS_CATEGORY_VIDEO) {
		/*some video files don't have video stream. in this case it is categorize as music. */
		char *ext;
		/*"3gp" and "mp4" must check video stream and then categorize in directly. */
		ext = strrchr(path, '.');
		if (ext != NULL) {
			if ((strncasecmp(ext, _3GP_FILE, 4) == 0) || (strncasecmp(ext, _MP4_FILE, 5) == 0)) {
				int audio = 0;
				int video = 0;

				err = mm_file_get_stream_info(path, &audio, &video);
				if (err == 0) {
					if (audio > 0 && video == 0) {
						*category ^= MS_CATEGORY_VIDEO;
						*category |= MS_CATEGORY_MUSIC;
					}
				}
			}
		}
	}

	MS_DBG("category_from_ext : %d", *category);

	return err;
}

void ms_check_db_updating(void)
{
	int vconf_value = 0;

	/*wait if phone init is not finished. */
	while (1) {
		ms_config_get_int(VCONFKEY_FILEMANAGER_DB_STATUS, &vconf_value);

		if (vconf_value != VCONFKEY_FILEMANAGER_DB_UPDATED) {
			MS_DBG("iNoti waits till init_phone finishes...");
			sleep(2);
		} else {
			return;
		}
	}
}

/****************************************************************************************************
LIBMEDIA_INFO
*****************************************************************************************************/

#ifdef _WITH_MP_PB_
void ms_update_valid_type_start(void)
{
	audio_svc_set_item_valid_start(MS_VALID_COUNT);
	minfo_set_item_valid_start(MS_VALID_COUNT);
}

void ms_update_valid_type_end(void)
{
	audio_svc_set_item_valid_end();
	minfo_set_item_valid_end();
}

int ms_change_valid_type(ms_store_type_t store_type, bool validation)
{
	MS_DBG_START();
	int res = MS_ERR_NONE;
	int err;

	audio_svc_storage_type_e audio_storage;
	minfo_store_type visual_storage;

	audio_storage = (store_type == MS_PHONE) 
		? AUDIO_SVC_STORAGE_PHONE : AUDIO_SVC_STORAGE_MMC;
	visual_storage = (store_type == MS_PHONE) 
		? MINFO_PHONE : MINFO_MMC;

	err = audio_svc_set_db_valid(audio_storage, validation);
	if (err < AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_set_db_valid error :%d", err);
		res = MS_ERR_DB_OPERATION_FAIL;
	}

	err = minfo_set_db_valid(visual_storage, validation);
	if (err < MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_set_db_valid : error %d", err);
		res = MS_ERR_DB_OPERATION_FAIL;
	}

	MS_DBG_END();

	return res;
}

#ifdef THUMB_THREAD
int ms_media_db_insert_with_thumb(const char *path, int category)
{
	MS_DBG_START();
	MS_DBG("%s", path);

	int ret = MS_ERR_NONE;
	int err;
	ms_store_type_t store_type;
	audio_svc_category_type_e audio_category;
	minfo_file_type visual_category;
	audio_svc_storage_type_e storage;

	if (category & MS_CATEGORY_VIDEO ||category & MS_CATEGORY_IMAGE) {
		visual_category = (category & MS_CATEGORY_IMAGE)
			? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		err = minfo_add_media(path, visual_category);
		if (err < 0) {
			MS_DBG(" minfo_add_media error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		} else {
			char thumb_path[1024];

			err = thumbnail_request_from_db(path, thumb_path, sizeof(thumb_path));
			if (err < 0) {
				MS_DBG("thumbnail_request_from_db falied: %d", err);
			} else {
				MS_DBG("thumbnail_request_from_db success: %s", thumb_path);
			}
		}
		
		MS_DBG("IMAGE");
	} 
	else if (category & MS_CATEGORY_SOUND || category & MS_CATEGORY_MUSIC) {
		store_type = ms_get_store_type_by_full(path);

		storage = (store_type == MS_MMC)
			? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		audio_category = (category & MS_CATEGORY_SOUND)
			? AUDIO_SVC_CATEGORY_SOUND : AUDIO_SVC_CATEGORY_MUSIC;

		err = audio_svc_insert_item(storage, path, audio_category);
		if (err < 0) {
			MS_DBG("mp_svc_insert_item fails error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		}
		MS_DBG("SOUND");
	}

	MS_DBG_END();
	return ret;
}
#endif

int ms_media_db_insert(const char *path, int category, bool bulk)
{
	MS_DBG_START();
	MS_DBG("%s", path);

	int ret = MS_ERR_NONE;
	int err;
	ms_store_type_t store_type;
	audio_svc_category_type_e audio_category;
	minfo_file_type visual_category;
	audio_svc_storage_type_e storage;

	if (category & MS_CATEGORY_VIDEO ||category & MS_CATEGORY_IMAGE) {
		visual_category = (category & MS_CATEGORY_IMAGE)
			? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		if(bulk)
			err = minfo_add_media_batch(path, visual_category);
		else
			err = minfo_add_media(path, visual_category);
		if (err < 0) {
			MS_DBG(" minfo_add_media error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		} 
#ifndef THUMB_THREAD		
		else {
			char thumb_path[1024];

			err = thumbnail_request_from_db(path, thumb_path, sizeof(thumb_path));
			if (err < 0) {
				MS_DBG("thumbnail_request_from_db falied: %d", err);
			} else {
				MS_DBG("thumbnail_request_from_db success: %s", thumb_path);
			}
#endif			
		}

		MS_DBG("IMAGE");
	}
	else if (category & MS_CATEGORY_SOUND || category & MS_CATEGORY_MUSIC) {
		store_type = ms_get_store_type_by_full(path);

		storage = (store_type == MS_MMC)
			? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		audio_category = (category & MS_CATEGORY_SOUND)
			? AUDIO_SVC_CATEGORY_SOUND : AUDIO_SVC_CATEGORY_MUSIC;

		err = audio_svc_insert_item(storage, path, audio_category);
		if (err < 0) {
			MS_DBG("mp_svc_insert_item fails error %d", err);
			ret = MS_ERR_DB_INSERT_RECORD_FAIL;
		}
		MS_DBG("SOUND");
	}

	MS_DBG_END();
	return ret;
}

int ms_check_file_exist_in_db(const char *path)
{
	int err;
	int category;
	Mitem *mi = NULL;

	/*get an item based on its url. */
	err = minfo_get_item(path, &mi);
	if (err != MS_ERR_NONE) {
		err = audio_svc_check_item_exist(path);
		if (err != MS_ERR_NONE)
			category = MS_CATEGORY_UNKNOWN;
		else
			category = MS_CATEGORY_MUSIC;
	} else {
		category = MS_CATEGORY_IMAGE;
	}

	minfo_destroy_mtype_item(mi);
	MS_DBG("Category : %d", category);

	return category;
}

int ms_media_db_delete(const char *path)
{
	MS_DBG_START();
	int ret = MS_ERR_NONE;
	int category;
	ms_ignore_file_info *ignore_file;

	category = ms_check_file_exist_in_db(path);

	if (category & MS_CATEGORY_VIDEO || category & MS_CATEGORY_IMAGE) {
		ret = minfo_delete_media(path);
		if (ret != MS_ERR_NONE) {
			MS_DBG("minfo_delete_media error : %d", ret);
			return ret;
		}
		MS_DBG("VIDEO or IMAGE");
	} else if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		ret = audio_svc_delete_item_by_path(path);
		if (ret != MS_ERR_NONE) {
			MS_DBG("audio_svc_delete_item_by_path error : %d", ret);
			return ret;
		}
		MS_DBG("MUSIC or SOUND");
	}
#ifdef _WITH_DRM_SERVICE_
	drm_svc_unregister_file(path, false);
#endif

	ignore_file = ms_inoti_find_ignore_file(path);
	if (ignore_file != NULL)
		ms_inoti_delete_ignore_file(ignore_file);

	MS_DBG_END();

	return ret;
}

void ms_media_db_move_start(void)
{
	audio_svc_move_item_start(MS_MOVE_COUNT);
}

void ms_media_db_move_end(void)
{
       audio_svc_move_item_end();
}

int
ms_media_db_move(ms_store_type_t src_store, ms_store_type_t dst_store,
		 const char *src_path, const char *dst_path)
{
	MS_DBG_START();

	int category = MS_CATEGORY_UNKNOWN;
	minfo_file_type visual_category;
	audio_svc_storage_type_e dst_storage;
	audio_svc_storage_type_e src_storage;
	int ret = 0;

	ret = ms_get_category_from_mime(dst_path, &category);
	if (ret != MS_ERR_NONE) {
		MS_DBG("ms_get_category_from_mime error %d", ret);
		return ret;
	}

	MS_DBG("category = %d", category);

	if (category & MS_CATEGORY_IMAGE || category & MS_CATEGORY_VIDEO) {
		visual_category = (category & MS_CATEGORY_IMAGE) ? MINFO_ITEM_IMAGE : MINFO_ITEM_VIDEO;
		ret = minfo_move_media(src_path, dst_path, visual_category);
		if (ret != MB_SVC_ERROR_NONE) {
			MS_DBG("minfo_move_media error : %d", ret);
			return ret;
		}
		MS_DBG("VISUAL");
	} else if (category & MS_CATEGORY_MUSIC || category & MS_CATEGORY_SOUND) {
		src_storage = (src_store == MS_MMC) ? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;
		dst_storage = (dst_store == MS_MMC) ? AUDIO_SVC_STORAGE_MMC : AUDIO_SVC_STORAGE_PHONE;

		ret = audio_svc_move_item(src_storage, src_path, dst_storage, dst_path);
		if (ret < 0) {
			MS_DBG("mp_svc_move_item error : %d", ret);
			if (ret == AUDIO_SVC_ERROR_DB_NO_RECORD) {
				MS_DBG(" This is a new file, Add DB");
				/*if source file does not exist in DB, it is new file. */

				ret = ms_register_file(dst_path, NULL);

				if (ret != MS_ERR_NONE) {
					MS_DBG("ms_register_file error : %d", ret);
					return MS_ERR_DB_INSERT_RECORD_FAIL;
				}
			}

			return ret;
		}
		MS_DBG("AUDIO");
	}

	MS_DBG_END();

	return ret;
}

bool ms_delete_all_record(ms_store_type_t store_type)
{
	MS_DBG_START();
	int err = 0;
	minfo_store_type visual_storage;
	audio_svc_storage_type_e audio_storage;

	visual_storage = (store_type == MS_PHONE) ? MINFO_PHONE : MINFO_MMC;
	audio_storage = (store_type == MS_PHONE) ? AUDIO_SVC_STORAGE_PHONE : AUDIO_SVC_STORAGE_MMC;

	/* To reset media db when differnet mmc is inserted. */
	err = audio_svc_delete_all(audio_storage);
	if (err != AUDIO_SVC_ERROR_NONE) {
		MS_DBG("audio_svc_delete_all error : %d\n", err);
#if 0 /*except temporary*/
		return false;
#endif
	}

	err = minfo_delete_invalid_media_records(visual_storage);
	if (err != MB_SVC_ERROR_NONE) {
		MS_DBG("minfo_delete_invalid_media_records error : %d\n", err);
		return false;
	}

	MS_DBG_END();

	return true;
}
#endif/*_WITH_MP_PB_*/

#ifdef PROGRESS
void ms_create_quickpanel(struct quickpanel *ms_quickpanel)
{
	MS_DBG_START();
	int type_id;
	int ret;

	struct quickpanel_type *qp_type;

	ret = quickpanel_get_type_id(NULL,
				   "/opt/apps/com.samsung.myfile/res/icons/default/small/com.samsung.myfile.png",
				   0);
	MS_DBG("return value of quickpanel_get_type_id : %d", ret);

	ms_quickpanel->type = ret;
	ms_quickpanel->priv_id = getpid();
	ms_quickpanel->group_id = 0;
	ms_quickpanel->title = "Media scanning";
	ms_quickpanel->content = NULL;
	ms_quickpanel->progress = 0;
	ms_quickpanel->args = NULL;
	ms_quickpanel->args_group = NULL;
	ms_quickpanel->evt = QP_E_ONGOING;

	ret = quickpanel_insert(ms_quickpanel);
	MS_DBG("return value of quickpanel_insert : %d", ret);

	MS_DBG_END();
}

void ms_update_progress(struct quickpanel *ms_quickpanel, double progress)
{
	MS_DBG_START();

	MS_DBG("%lf", progress)
	    quickpanel_update_progress(ms_quickpanel->type,
				       ms_quickpanel->priv_id, progress);

	MS_DBG_END();
}

void ms_delete_quickpanel(struct quickpanel *ms_quickpanel)
{
	MS_DBG_START();
	int ret = 0;

	ret = quickpanel_delete(ms_quickpanel->type, ms_quickpanel->priv_id);
	MS_DBG("return value of quickpanel_delete : %d", ret);

	MS_DBG_END();
}
#endif /*PROGRESS*/


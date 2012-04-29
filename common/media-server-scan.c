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
 * @file		media-server-scan.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <vconf.h>

#include "media-server-utils.h"
#include "media-server-db-svc.h"
#include "media-server-external-storage.h"
#include "media-server-scan-internal.h"
#include "media-server-scan.h"

#ifdef PROGRESS
#include <quickpanel.h>
#endif

GAsyncQueue *scan_queue;
extern int mmc_state;

#ifdef FMS_PERF
extern struct timeval g_mmc_start_time;
extern struct timeval g_mmc_end_time;
#endif

static void _insert_array(GArray *garray, ms_scan_data_t *insert_data)
{
	ms_scan_data_t *data;
	bool insert_ok = false;
	int len = garray->len;
	int i;

	MS_DBG("the length of array : %d", len);
	MS_DBG("db_type : %d", insert_data->db_type);
	MS_DBG("scan_type : %d", insert_data->scan_type);

	for (i=0; i < len; i++)
	{
		data = g_array_index(garray, ms_scan_data_t*, i);

		if (data->db_type == insert_data->db_type) {
			if(data->scan_type > insert_data->scan_type) {
				g_array_remove_index (garray, i);
				g_array_insert_val(garray, i, insert_data);
				insert_ok =  true;
			}
		}
	}

	if (insert_ok == false)
		g_array_append_val(garray, insert_data);
}

gboolean ms_scan_thread(void *data)
{
	MS_DBG_START();

	ms_scan_data_t *scan_data = NULL;
	ms_scan_data_t *insert_data;
	GArray *garray = NULL;
	bool res;
	int length;
	int err;
	void *handle = NULL;

#ifdef PROGRESS
	struct quickpanel *ms_quickpanel = NULL;
#endif

	/*create array for processing overlay data*/
	garray = g_array_new (FALSE, FALSE, sizeof (ms_scan_data_t *));
	if (garray == NULL) {
		MS_DBG("g_array_new error");
		return false;
	}

	while (1) {
		length  = g_async_queue_length(scan_queue);

		/*updating requests remain*/
		if (garray->len != 0 && length == 0) {
			scan_data = g_array_index(garray, ms_scan_data_t*, 0);
			g_array_remove_index (garray, 0);
		} else if (length != 0) {
			insert_data = g_async_queue_pop(scan_queue);
			_insert_array(garray, insert_data);
			continue;
		} else if (garray->len == 0 && length == 0) {
			/*Threre is no request, Wait until pushung new request*/
			insert_data = g_async_queue_pop(scan_queue);
			_insert_array(garray, insert_data);
			continue;
		}

		if (scan_data->scan_type != MS_SCAN_VALID) {
			/*connect to media db, if conneting is failed, db updating is stopped*/
			err = ms_connect_db(&handle);
			if (err != MS_ERR_NONE)
				continue;

			/*start db updating */
			if (scan_data->scan_type != MS_SCAN_NONE ) {
				int status;

				ms_set_db_status(MS_DB_UPDATING);
				/*check UMS status and change configuration value*/
				ms_config_get_int(MS_USB_MODE_KEY, &status);
				if (status == MS_VCONFKEY_MASS_STORAGE_MODE) {
					ms_config_get_int(VCONFKEY_USB_STORAGE_STATUS, &status);
					if(status == VCONFKEY_USB_STORAGE_STATUS_OFF) {
						ms_config_set_int(MS_USB_MODE_KEY, MS_VCONFKEY_NORMAL_MODE);
					}
				}
#ifdef PROGRESS
				ms_quickpanel = malloc(sizeof(struct quickpanel));
				ms_create_quickpanel(ms_quickpanel);
#endif /*PROGRESS*/
			}

			if (scan_data->scan_type == MS_SCAN_ALL) {
				res = ms_delete_all_items(handle, scan_data->db_type);
				if (res != true) {
					MS_DBG("ms_delete_all_record fails");
				}
			}

#ifdef FMS_PERF
			if (scan_data->db_type == MS_MMC) {
				ms_check_start_time(&g_mmc_start_time);
			}
#endif
			/*call for bundle commit*/
			ms_register_start(handle);

#ifdef PROGRESS
			/*add inotify watch and insert data into media db */
			_ms_dir_scan(handle, scan_data, ms_quickpanel);

			if (ms_quickpanel != NULL) {
				ms_delete_quickpanel(ms_quickpanel);
				free(ms_quickpanel);
				ms_quickpanel = NULL;
			}
#else
			/*add inotify watch and insert data into media db */
			_ms_dir_scan(handle, scan_data);
#endif
			/*call for bundle commit*/
			ms_register_end(handle);

#ifdef FMS_PERF
			if (scan_data->db_type == MS_MMC) {
				ms_check_end_time(&g_mmc_end_time);
				ms_check_time_diff(&g_mmc_start_time, &g_mmc_end_time);
			}
#endif

			/*set vconf key mmc loading for indicator */
			if (scan_data->scan_type != MS_SCAN_NONE )
				ms_set_db_status(MS_DB_UPDATED);

			/*disconnect form media db*/
			ms_disconnect_db(handle);

			/*Active flush */
			sqlite3_release_memory(-1);

			if (scan_data->db_type == MS_MMC) {
				ms_update_mmc_info();
			}
		} else  {
			/*connect to media db, if conneting is failed, db updating is stopped*/
			err = ms_connect_db(&handle);
			if (err != MS_ERR_NONE)
				continue;

			/*update just valid type*/
			err = ms_invalidate_all_items(handle, scan_data->db_type);
			if (err != MS_ERR_NONE)
				MS_DBG("ms_change_valid_type fail");

			ms_set_db_status(MS_DB_UPDATED);

			/*disconnect form media db*/
			ms_disconnect_db(handle);

			/*Active flush */
			sqlite3_release_memory(-1);
		}

		free(scan_data);
	}			/*thread while*/

	if(garray) g_array_free (garray, TRUE);

	MS_DBG_END();

	return true;
}

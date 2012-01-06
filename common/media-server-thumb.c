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
 * @file		media-server-thumb.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <media-svc.h>
#include <media-thumbnail.h>
#include <vconf.h>

#include "media-server-common.h"
#include "media-server-thumb.h"

#ifdef THUMB_THREAD
extern int mmc_state;
extern int current_usb_mode;

static int _ite_fn(Mitem * item, void *user_data)
{
	GList **list = (GList **) user_data;
	*list = g_list_append(*list, item);

	return 0;
}

gboolean ms_thumb_thread(void *data)
{
	int err = -1;
	int i = 0;
	Mitem *mitem = NULL;
	GList *p_list = NULL;
	minfo_item_filter item_filter;
	char thumb_path[1024];

	memset(&item_filter, 0x00, sizeof(minfo_item_filter));

	item_filter.file_type = MINFO_ITEM_ALL;
	item_filter.sort_type = MINFO_MEDIA_SORT_BY_DATE_ASC;
	item_filter.start_pos = -1;
	item_filter.end_pos = -1;
	item_filter.with_meta = FALSE;
	item_filter.favorite = MINFO_MEDIA_FAV_ALL;

	ms_media_db_open();

	MS_DBG("data : %d", *(minfo_folder_type*)data);

	err =  minfo_get_all_item_list(*(minfo_folder_type*)data, item_filter,  _ite_fn, &p_list);
	if (err < 0) {
		MS_DBG("minfo_get_all_item_list error : %d", err);
		return err;
	}

	for (i = 0; i < g_list_length(p_list); i++) {
		//check usb in out
		if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF
			    || ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (*(minfo_folder_type*)data == MINFO_CLUSTER_TYPE_LOCAL_MMC))) {
		    MS_DBG("BREAK");
			break;
		}
		mitem = (Mitem *) g_list_nth_data(p_list, i);

		if (mitem) {
			MS_DBG("Start to extracting thumbnails [%s] ( %s )", mitem->uuid, mitem->file_url);
                     err = minfo_extract_thumbnail(mitem->uuid, mitem->type);
			if (err < 0) {
				MS_DBG("thumbnail_request_from_db falied: %d", err);
			} else {
				MS_DBG("thumbnail_request_from_db success: %s", thumb_path);
			}

			minfo_destroy_mtype_item(mitem);
		} else {
			MS_DBG("mitem[%d] is NULL", i);
		}
	}

	ms_media_db_close();

	if (p_list != NULL) {
		g_list_free(p_list);
		p_list = NULL;
	}
}
#endif

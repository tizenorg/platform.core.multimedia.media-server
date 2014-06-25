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
 * @file		media-server-db-svc.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief       This file implements main database operation.
 */

#include <dlfcn.h>

#include "media-util.h"

#include "media-common-utils.h"
#include "media-common-drm.h"
#include "media-server-dbg.h"
#include "media-server-db-svc.h"

#include <tzplatform_config.h>

#define CONFIG_PATH tzplatform_mkpath(TZ_SYS_DATA,"data-media/file-manager-service/plugin-config")
#define EXT ".so"
#define EXT_LEN 3

GArray *so_array;
void ***func_array;
int lib_num;

void **func_handle = NULL; /*dlopen handle*/

enum func_list {
	eCONNECT,
	eDISCONNECT,
	eSET_ALL_VALIDITY,
	eFUNC_MAX
};

static int
_ms_token_data(char *buf, char **name)
{
	int len;
	char* pos = NULL;

	pos = strstr(buf, EXT);
	if (pos == NULL) {
		MS_DBG_ERR("This is not shared object library.");
		name = NULL;
		return -1;
	} else {
		len = pos - buf + EXT_LEN;
		*name = strndup(buf, len);
		MS_DBG("%s", *name);
	}

	return 0;
}

static bool
_ms_load_config()
{
	int ret;
	FILE *fp;
	char *so_name = NULL;
	char buf[256] = {0};

	fp = fopen(CONFIG_PATH, "rt");
	if (fp == NULL) {
		MS_DBG_ERR("fp is NULL");
		return MS_MEDIA_ERR_FILE_OPEN_FAIL;
	}
	while(1) {
		if (fgets(buf, 256, fp) == NULL) {
			MS_DBG_ERR("fgets failed");
			break;
		}

		ret = _ms_token_data(buf, &so_name);
		if (ret == 0) {
			/*add to array*/
			g_array_append_val(so_array, so_name);
			so_name = NULL;
		}
	}

	fclose(fp);

	return MS_MEDIA_ERR_NONE;
}

int
ms_load_functions(void)
{
	int lib_index = 0, func_index;
	char func_list[eFUNC_MAX][40] = {
		"connect",
		"disconnect",
		"set_all_storage_items_validity",
		};
	/*init array for adding name of so*/
	so_array = g_array_new(FALSE, FALSE, sizeof(char*));

	/*load information of so*/
	_ms_load_config();

	if(so_array->len == 0) {
		MS_DBG("There is no information for functions");
		return MS_MEDIA_ERR_DYNAMIC_LINK;
	}

	/*the number of functions*/
	lib_num = so_array->len;

	MS_DBG("The number of information of so : %d", lib_num);
	MS_MALLOC(func_handle, sizeof(void*) * lib_num);
	if (func_handle == NULL) {
		MS_DBG_ERR("malloc failed");
		return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
	}

	while(lib_index < lib_num) {
		/*get handle*/
		MS_DBG("[name of so : %s]", g_array_index(so_array, char*, lib_index));
		func_handle[lib_index] = dlopen(g_array_index(so_array, char*, lib_index), RTLD_LAZY);
		if (!func_handle[lib_index]) {
			MS_DBG_ERR("%s", dlerror());
			MS_SAFE_FREE(func_handle);
			return MS_MEDIA_ERR_DYNAMIC_LINK;
		}
		lib_index++;
	}

	dlerror();    /* Clear any existing error */

	/*allocate for array of functions*/
	MS_MALLOC(func_array, sizeof(void*) * lib_num);
	if (func_array == NULL) {
		MS_DBG_ERR("malloc failed");
		MS_SAFE_FREE(func_handle);
		return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
	}

	for(lib_index = 0 ; lib_index < lib_num; lib_index ++) {
		MS_MALLOC(func_array[lib_index], sizeof(void*) * eFUNC_MAX);
		if (func_array[lib_index] == NULL) {
			int index;

			for (index = 0; index < lib_index; index ++) {
				MS_SAFE_FREE(func_array[index]);
			}
			MS_SAFE_FREE(func_array);
			MS_SAFE_FREE(func_handle);

			MS_DBG_ERR("malloc failed");
			return MS_MEDIA_ERR_ALLOCATE_MEMORY_FAIL;
		}
	}

	/*add functions to array */
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		for (func_index = 0; func_index < eFUNC_MAX ; func_index++) {
			func_array[lib_index][func_index] = dlsym(func_handle[lib_index], func_list[func_index]);
			if (func_array[lib_index][func_index] == NULL) {
				int index;

				for (index = 0; index < lib_index; index ++) {
					MS_SAFE_FREE(func_array[index]);
				}
				MS_SAFE_FREE(func_array);
				MS_SAFE_FREE(func_handle);

				MS_DBG_ERR("dlsym failed");
				return MS_MEDIA_ERR_DYNAMIC_LINK;
			}
		}
	}

	return MS_MEDIA_ERR_NONE;
}

void
ms_unload_functions(void)
{
	int lib_index;

	for (lib_index = 0; lib_index < lib_num; lib_index ++)
		dlclose(func_handle[lib_index]);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
			if (func_array[lib_index]) {
				MS_SAFE_FREE(func_array[lib_index]);
			}
	}

	MS_SAFE_FREE (func_array);
	MS_SAFE_FREE (func_handle);
	if (so_array) g_array_free(so_array, TRUE);
}

int
ms_connect_db(void ***handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	MS_MALLOC(*handle, sizeof (void*) * lib_num);

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((CONNECT)func_array[lib_index][eCONNECT])(&((*handle)[lib_index]), &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_CONNECT_FAIL;
		}
	}

	MS_DBG("connect Media DB");

	return MS_MEDIA_ERR_NONE;
}

int
ms_disconnect_db(void ***handle)
{
	int lib_index;
	int ret;
	char * err_msg = NULL;

	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((DISCONNECT)func_array[lib_index][eDISCONNECT])((*handle)[lib_index], &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
		}
	}

	MS_SAFE_FREE(*handle);

	MS_DBG("Disconnect Media DB");

	return MS_MEDIA_ERR_NONE;
}

int
ms_invalidate_all_items(void **handle, ms_storage_type_t store_type)
{
	int lib_index;
	int res = MS_MEDIA_ERR_NONE;
	int ret;
	char *err_msg = NULL;
	MS_DBG("");
	for (lib_index = 0; lib_index < lib_num; lib_index++) {
		ret = ((SET_ALL_STORAGE_ITEMS_VALIDITY)func_array[lib_index][eSET_ALL_VALIDITY])(handle[lib_index], store_type, false, &err_msg); /*dlopen*/
		if (ret != 0) {
			MS_DBG_ERR("error : %s [%s]", g_array_index(so_array, char*, lib_index), err_msg);
			MS_SAFE_FREE(err_msg);
			res = MS_MEDIA_ERR_DB_UPDATE_FAIL;
		}
	}
	MS_DBG("");
	return res;
}


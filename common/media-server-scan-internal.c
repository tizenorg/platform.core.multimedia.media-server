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
 * @file		media-server-scan-internal.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <vconf.h>

#include "media-server-utils.h"
#include "media-server-db-svc.h"
#include "media-server-inotify.h"
#include "media-server-scan-internal.h"

extern int mmc_state;
bool power_off;

typedef struct ms_scan_data {
	char *name;
	struct ms_scan_data *next;
} ms_scan_data;

ms_scan_data *first_scan_node;

static int _ms_scan_get_next_path_from_current_node(int find_folder,
				   ms_dir_scan_info **current_root,
				   ms_dir_scan_info **real_root, char **path, int *depth)
{
	int err = MS_ERR_NONE;
	char get_path[FAT_FILEPATH_LEN_MAX] = { 0 };

	if (find_folder == 0) {
		if ((*current_root)->Rbrother != NULL) {
			*current_root = (*current_root)->Rbrother;
		} else {
			while (1) {
				if ((*current_root)->parent == *real_root
				    || (*current_root)->parent == NULL) {
					*current_root = NULL;
					*depth = 0;
					return MS_ERR_NONE;
				} else if ((*current_root)->parent->Rbrother == NULL) {
					*current_root = (*current_root)->parent;
					(*depth) --;
				} else {
					*current_root = (*current_root)->parent->Rbrother;
					(*depth) --;
					break;
				}
			}
		}
		(*depth) --;
	}

	err = ms_get_full_path_from_node(*current_root, get_path, *depth);
	if (err != MS_ERR_NONE)
		return MS_ERR_INVALID_DIR_PATH;

	*path = strdup(get_path);

	return err;
}

static int _ms_scan_add_node(ms_dir_scan_info * const node, int depth)
{
	int err;
	char full_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_scan_data *current_dir = NULL;
	ms_scan_data *prv_node = NULL;
	ms_scan_data *last_node = NULL;

	err = ms_get_full_path_from_node(node, full_path, depth);
	if (err != MS_ERR_NONE)
		return MS_ERR_INVALID_DIR_PATH;

	last_node = first_scan_node;
	while (last_node != NULL) {
		last_node = last_node->next;
	}

	/*find same folder */
	if (first_scan_node != NULL) {
		last_node = first_scan_node;
		while (last_node != NULL) {
			if (strcmp(full_path, last_node->name) == 0) {
				return MS_ERR_NONE;
			}
			prv_node = last_node;
			last_node = last_node->next;
		}
	}

	current_dir = malloc(sizeof(ms_scan_data));
	current_dir->name = strdup(full_path);
	current_dir->next = NULL;

	if (first_scan_node == NULL) {
		first_scan_node = current_dir;
	} else {
		/*if next node of current node is NULL, it is the lastest node. */
		prv_node->next = current_dir;
	}
	MS_DBG("scan path : %s", full_path);

	return MS_ERR_NONE;
}

static void _ms_dir_check(ms_scan_data_t * scan_data)
{
	int err = 0;
	int depth = 0;
	int find_folder = 0;
	char get_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char *path = NULL;
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result;
	ms_storage_type_t storage_type = scan_data->storage_type;

	ms_dir_scan_info *root;
	ms_dir_scan_info *tmp_root = NULL;
	ms_dir_scan_info *cur_node = NULL; /*current node*/
	ms_dir_scan_info *prv_node = NULL; /*previous node*/
	ms_dir_scan_info *next_node = NULL;

	root = malloc(sizeof(ms_dir_scan_info));
	if (root == NULL) {
		MS_DBG_ERR("malloc fail");
		return;
	}

	root->name = strdup(scan_data->path);
	if (root->name == NULL) {
		MS_DBG_ERR("strdup fail");
		MS_SAFE_FREE(root);
		return;
	}

	root->parent = NULL;
	root->Rbrother = NULL;
	root->next = NULL;
	tmp_root = root;
	prv_node = root;

	path = malloc(sizeof(char) * MS_FILE_PATH_LEN_MAX);

	err = ms_get_full_path_from_node(tmp_root, path, depth);
	if (err != MS_ERR_NONE) {
		return;
	}

	_ms_scan_add_node(root, depth);

	while (1) {
		/*check poweroff status*/
		if (power_off) {
			MS_DBG("Power off");
			goto FREE_RESOURCES;
		}

		/*check SD card in out*/
		if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORATE_EXTERNAL))
			goto FREE_RESOURCES;

		depth ++;
		dp = opendir(path);
		if (dp == NULL) {
			MS_DBG_ERR("%s folder opendir fails", path);
			goto NEXT_DIR;
		}

		while (!readdir_r(dp, &entry, &result)) {
			/*check poweroff status*/
			if (power_off) {
				MS_DBG("Power off");
				goto FREE_RESOURCES;
			}

			if (result == NULL)
				break;

			if (entry.d_name[0] == '.')
				continue;

			/*check SD card in out*/
			if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORATE_EXTERNAL)) {
				goto FREE_RESOURCES;
			}

			if (entry.d_type & DT_DIR) {
				DIR *tmp_dp = NULL;
				err = ms_strappend(get_path, sizeof(get_path), "%s/%s",path, entry.d_name);
				if (err != MS_ERR_NONE) {
					MS_DBG_ERR("ms_strappend error");
					continue;
				}

				tmp_dp = opendir(get_path);
				if (tmp_dp == NULL) {
					MS_DBG_ERR("%s folder opendir fails", get_path);
					continue;
				}
				else
					closedir(tmp_dp);

				cur_node = malloc(sizeof(ms_dir_scan_info));
				if (cur_node == NULL) {
					MS_DBG_ERR("malloc fail");

					goto FREE_RESOURCES;
				}

				cur_node->name = strdup(entry.d_name);
				cur_node->Rbrother = NULL;
				cur_node->next = NULL;

				/*1. 1st folder */
				if (find_folder == 0) {
					cur_node->parent = tmp_root;
					tmp_root = cur_node;
				} else {
					cur_node->parent = tmp_root->parent;
					prv_node->Rbrother = cur_node;
				}
				prv_node->next = cur_node;

				/*add watch */
				_ms_scan_add_node(cur_node, depth);

				/*change previous */
				prv_node = cur_node;
				find_folder++;
			}
		}
NEXT_DIR:
		MS_SAFE_FREE(path);
		if (dp) closedir(dp);
		dp = NULL;

		err = _ms_scan_get_next_path_from_current_node(find_folder, &tmp_root, &root, &path, &depth);
		if (err != MS_ERR_NONE)
			break;

		if (tmp_root == NULL)
			break;

		find_folder = 0;
	}

FREE_RESOURCES:
	/*free allocated memory */
	MS_SAFE_FREE(path);
	if (dp) closedir(dp);
	dp = NULL;

	cur_node = root;
	while (cur_node != NULL) {
		next_node = cur_node->next;
		MS_SAFE_FREE(cur_node->name);
		MS_SAFE_FREE(cur_node);
		cur_node = next_node;
	}
}

void _ms_dir_scan(void **handle, ms_scan_data_t * scan_data)
{
	int err = 0;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_scan_data *node;
	DIR *dp = NULL;
	ms_storage_type_t storage_type = scan_data->storage_type;
	ms_dir_scan_type_t scan_type = scan_data->scan_type;

	err = ms_strcopy(path, sizeof(path), "%s", scan_data->path);
	if (err != MS_ERR_NONE) {
		MS_DBG_ERR("error : %d", err );
	}

	/*Add inotify watch */
	if (scan_type != MS_SCAN_INVALID)
		_ms_dir_check(scan_data);

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_type == MS_SCAN_ALL || scan_type == MS_SCAN_PART) {
		struct dirent entry;
		struct dirent *result = NULL;

		node = first_scan_node;

		while (node != NULL) {
			/*check poweroff status*/
			if (power_off) {
				MS_DBG("Power off");
				goto STOP_SCAN;
			}

			/*check SD card in out */
			if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORATE_EXTERNAL)) {
			    	MS_DBG("Directory scanning is stopped");
				goto STOP_SCAN;
			}

			dp = opendir(node->name);
			if (dp != NULL) {
				while (!readdir_r(dp, &entry, &result)) {
					/*check poweroff status*/
					if (power_off) {
						MS_DBG("Power off");
						goto STOP_SCAN;
					}

					if (result == NULL)
						break;

					if (entry.d_name[0] == '.')
						continue;

					/*check SD card in out */
					if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORATE_EXTERNAL)) {
						MS_DBG("Directory scanning is stopped");
						goto STOP_SCAN;
					}

					if (entry.d_type & DT_REG) {
						err = ms_strappend(path, sizeof(path), "%s/%s", node->name, entry.d_name);
						if (err < 0) {
							MS_DBG_ERR("error : %d", err);
							continue;
						}

						if (scan_type == MS_SCAN_PART)
							err = ms_validate_item(handle,path);
						else
							err = ms_insert_item_batch(handle, path);

						if (err < 0) {
							MS_DBG_ERR("failed to update db : %d , %d\n", err, scan_type);
							continue;
						}
					}
				}
			} else {
				MS_DBG_ERR("%s folder opendir fails", node->name);
			}
			if (dp) closedir(dp);
			dp = NULL;

			if (node == NULL) {
				MS_DBG_ERR("DB updating is done");
				break;
			}
			node = node->next;
		}		/*db update while */
	} else if ( scan_type == MS_SCAN_INVALID) {
		/*In this case, update just validation record*/
		/*update just valid type*/
		err = ms_invalidate_all_items(handle, storage_type);
		if (err != MS_ERR_NONE)
			MS_DBG_ERR("error : %d", err);
	}
STOP_SCAN:
	if (dp) closedir(dp);

	/*delete all node*/
	node = first_scan_node;

	while (node != NULL) {
		if (node->name) free(node->name);
		MS_SAFE_FREE(node);
	}

	first_scan_node = NULL;

	sync();

	return;
}

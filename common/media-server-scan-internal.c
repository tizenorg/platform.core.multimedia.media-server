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
#include <audio-svc-error.h>
#include <audio-svc.h>
#include <media-svc.h>
#include "media-server-common.h"
#include "media-server-inotify.h"
#include "media-server-scan-internal.h"

#ifdef PROGRESS
#include <quickpanel.h>
#endif

extern int mmc_state;
extern int current_usb_mode;
extern ms_dir_data *first_inoti_node;

#ifdef PROGRESS
#define SIZE_OF_PBARRAY 100
#endif

int _ms_get_path_from_current_node(int find_folder,
				   ms_dir_scan_info **current_root,
				   ms_dir_scan_info **real_root, char **path)
{
	MS_DBG_START();

	int err = 0;
	char get_path[FAT_FILEPATH_LEN_MAX + 1] = { 0 };

	if (find_folder == 0) {
		if ((*current_root)->Rbrother != NULL) {
			*current_root = (*current_root)->Rbrother;
		} else {
			while (1) {
				if ((*current_root)->parent == *real_root
				    || (*current_root)->parent == NULL) {
					*current_root = NULL;
					MS_DBG_END();
					return 0;
				} else if ((*current_root)->parent->Rbrother ==
					   NULL) {
					*current_root = (*current_root)->parent;
				} else {
					*current_root =
					    (*current_root)->parent->Rbrother;
					break;
				}
			}
		}
	}

	err = ms_get_full_path_from_node(*current_root, get_path);

	*path = strdup(get_path);

	MS_DBG_END();

	return err;
}

#ifdef PROGRESS
void _ms_dir_check(char *start_path, ms_store_type_t db_type, unsigned short *file_count)
#else
void _ms_dir_check(char *start_path, ms_store_type_t db_type)
#endif
{
	MS_DBG_START();

	int err = 0;
	int find_folder = 0;
	char get_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char *path;
	DIR *dp = NULL;
	struct dirent entry;
	struct dirent *result;

	ms_dir_scan_info *root;
	ms_dir_scan_info *tmp_root = NULL;
	ms_dir_scan_info *cur_node = NULL; /*current node*/
	ms_dir_scan_info *prv_node = NULL; /*previous node*/
	ms_dir_scan_info *next_node = NULL;

	root = malloc(sizeof(ms_dir_scan_info));
	if (root == NULL) {
		MS_DBG("malloc fail");
		return;
	}

	root->name = strdup(start_path);
	if (root->name  == NULL) {
		MS_DBG("strdup fail");
		free(root);
		return;
	}

	root->parent = NULL;
	root->Rbrother = NULL;
	root->next = NULL;
	tmp_root = root;
	prv_node = root;

	err = ms_get_full_path_from_node(tmp_root, get_path);
	MS_DBG("full_path : %s", get_path);

	path = strdup(get_path);
	if (path == NULL) {
		MS_DBG("strdup fail");
		free(root);
		return;
	}

	ms_inoti_add_watch_with_node(root);

	while (1) {
		dp = opendir(path);
		if (dp == NULL) {
			MS_DBG("%s folder opendir fails", path);
			goto NEXT_DIR;
		}

		while (!readdir_r(dp, &entry, &result)) {
			if (result == NULL)
				break;

			if (entry.d_name[0] == '.')
				continue;

			/*check usb in out*/
			if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF
			    ||(( mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (db_type == MS_MMC))) {

				goto FREE_RESOURCES;
			}

			if (entry.d_type & DT_DIR) {
				DIR *tmp_dp = NULL;
				err = ms_strappend(get_path, sizeof(get_path), "%s/%s",path, entry.d_name);
				if (err != MS_ERR_NONE) {
					MS_DBG("ms_strappend error");
					continue;
				}

				tmp_dp = opendir(get_path);
				if (tmp_dp == NULL) {
					MS_DBG("%s folder opendir fails", get_path);
					continue;
				}
				else
					closedir(tmp_dp);
				
				cur_node = malloc(sizeof(ms_dir_scan_info));
				if (cur_node == NULL) {
					MS_DBG("malloc fail");

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
				ms_inoti_add_watch_with_node(cur_node);

				/*change previous */
				prv_node = cur_node;
				find_folder++;
			}
#ifdef PROGRESS
			else if (entry.d_type & DT_REG) {
				(*file_count)++;
			}
#endif
		}
NEXT_DIR:
		if (dp) closedir(dp);
		if (path) free(path);
		dp = NULL;
		path = NULL;

		err = _ms_get_path_from_current_node(find_folder, &tmp_root, &root, &path);
		if (err < 0)
			break;

		if (tmp_root == NULL)
			break;

		find_folder = 0;
	}

FREE_RESOURCES:
	/*free allocated memory */
	if (path) free(path);
	if (dp) closedir(dp);

	cur_node = root;
	while (cur_node != NULL) {
		next_node = cur_node->next;
		free(cur_node->name);
		free(cur_node);
		cur_node = next_node;
	}

	MS_DBG_END();
}

#ifdef PROGRESS
void _ms_dir_scan(ms_scan_data_t * scan_data, struct quickpanel *ms_quickpanel)
#else
void _ms_dir_scan(ms_scan_data_t * scan_data)
#endif
{
	MS_DBG_START();
	int err = 0;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_dir_data *node;
	DIR *dp = NULL;
#ifdef PROGRESS
	int i;
	unsigned short file_count = 0;
	unsigned short update_count = 0;
	unsigned short proress_array[SIZE_OF_PBARRAY] = { 0 };
#endif

	if (scan_data->db_type == MS_PHONE)
		err = ms_strcopy(path, sizeof(path), "%s", MS_PHONE_ROOT_PATH);
	else
		err =  ms_strcopy(path, sizeof(path), "%s", MS_MMC_ROOT_PATH);

	if (err < MS_ERR_NONE) {
		MS_DBG("fail ms_strcopy");
	}

#ifdef PROGRESS
	/*Add inotify watch */
	_ms_dir_check(path, scan_data->db_type,&file_count);
#else
	/*Add inotify watch */
	_ms_dir_check(path, scan_data->db_type);
#endif

#ifdef PROGRESS
	for (i = 0; i < SIZE_OF_PBARRAY; i++) {
		proress_array[i] = ((i + 1) * file_count) / SIZE_OF_PBARRAY;
		if (proress_array[i] == 0)
			proress_array[i] = 1;
	}
	i = 0;
#endif

	/*if scan type is not MS_SCAN_NONE, check data in db. */
	if (scan_data->scan_type == MS_SCAN_ALL
	    || scan_data->scan_type == MS_SCAN_PART) {
		struct dirent entry;
		struct dirent *result = NULL;

		node = first_inoti_node;
#ifdef PROGRESS
		int progress = 0;
		int pre_progress = 0;
#endif
		if (scan_data->scan_type == MS_SCAN_PART) {
			/*enable bundle commit*/
			ms_update_valid_type_start();
		}

		while (node != NULL) {
			/*check usb in out */
			if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF
			    || ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (scan_data->db_type == MS_MMC))) {
			    	MS_DBG("Directory scanning is stopped");
				goto STOP_SCAN;
			}
			if (node->db_updated != true) {
				dp = opendir(node->name);
				if (dp != NULL) {
					while (!readdir_r(dp, &entry, &result)) {
						if (result == NULL)
							break;

						if (entry.d_name[0] == '.')
							continue;

						/*check usb in out */
						if (current_usb_mode != VCONFKEY_USB_STORAGE_STATUS_OFF
						    || ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (scan_data->db_type == MS_MMC))) {
							MS_DBG("Directory scanning is stopped");
							goto STOP_SCAN;
						}

						if (entry.d_type & DT_REG) {
							MS_DBG("THIS IS FEX_DIR_SCAN_CB");
#ifdef PROGRESS
							/*update progress bar */
							update_count++;
							if (ms_quickpanel != NULL) {
								if (proress_array[i] == update_count) {
									while(1) {
										ms_update_progress(ms_quickpanel, ((double)i) / 100);
										i++;
										if (proress_array[i] != update_count)
											break;
									}
								}
							}
#endif/*PROGRESS*/
							err = ms_strappend(path, sizeof(path), "%s/%s", node->name, entry.d_name);
							if (err < 0) {
								MS_DBG("FAIL : ms_strappend");
								continue;
							}

							if (scan_data->scan_type == MS_SCAN_PART)	
								err = ms_update_valid_type(path);
							else
								err = ms_register_scanfile(path);

							if (err < 0) {
								MS_DBG("failed to update db : %d , %d\n", err, scan_data->scan_type);
								continue;
							}
						}
					}
				} else {
					MS_DBG("%s folder opendir fails", node->name);
				}
				if (dp) closedir(dp);
				dp = NULL;

				if (node == NULL) {
					MS_DBG("");
					MS_DBG("DB updating is done");
					MS_DBG("");
					break;
				}
				node->db_updated = true;
			}
			node = node->next;
		}		/*db update while */

		if (scan_data->scan_type == MS_SCAN_PART) {
			/*disable bundle commit*/
			ms_update_valid_type_end();

			audio_svc_delete_invalid_items(scan_data->db_type);
			minfo_delete_invalid_media_records(scan_data->db_type);
		}
	} else {
		node = first_inoti_node;

		while (node != NULL) {
			node->db_updated = true;
			node = node->next;
		}
	}
STOP_SCAN:
	if (dp) closedir(dp);

	sync();

	MS_DBG_END();

	return;
}

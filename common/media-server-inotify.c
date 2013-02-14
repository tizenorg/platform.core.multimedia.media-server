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
 * @file		media-server-inotify.c
 * @author	Yong Yeon Kim(yy9875.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <errno.h>
#include <dirent.h>
#include <malloc.h>
#include <vconf.h>

#include "media-util.h"
#include "media-server-dbg.h"
#include "media-server-utils.h"
#include "media-server-db-svc.h"
#include "media-server-inotify-internal.h"
#include "media-server-inotify.h"

#if MS_INOTI_ENABLE

bool power_off;
extern int inoti_fd;
extern int mmc_state;
ms_inoti_dir_data *first_inoti_node;
ms_ignore_file_info *latest_ignore_file;

int _ms_inoti_directory_scan_and_register_file(void **handle, char *dir_path)
{
	struct dirent ent;
	struct dirent *res = NULL;
	DIR *dp = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int err;

	if (dir_path == NULL)
		return MS_MEDIA_ERR_INVALID_PATH;

	dp = opendir(dir_path);
	if (dp == NULL) {
		MS_DBG_ERR("Fail to open dir %s", dir_path);
		return MS_MEDIA_ERR_DIR_OPEN_FAIL;
	}

	ms_inoti_add_watch(dir_path);

	while (!readdir_r(dp, &ent, &res)) {
		if (res == NULL)
			break;

		if (ent.d_name[0] == '.')
			continue;

		err = ms_strappend(path, sizeof(path), "%s/%s", dir_path, ent.d_name);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_strappend error : %d", err);
			continue;
		}

		/*in case of directory */
		if (ent.d_type == DT_DIR) {
			_ms_inoti_directory_scan_and_register_file(handle, path);
		} else {
			err = ms_register_file(handle, path, NULL);
			if (err != MS_MEDIA_ERR_NONE) {
				MS_DBG_ERR("ms_register_file error : %d", err);
				continue;
			}
		}
	}

	closedir(dp);

	return 0;
}

int _ms_inoti_scan_renamed_folder(void **handle, char *org_path, char *chg_path)
{
	int err = -1;
	struct dirent ent;
	struct dirent *res = NULL;

	DIR *dp = NULL;
	char path_from[MS_FILE_PATH_LEN_MAX] = { 0 };
	char path_to[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_storage_type_t src_storage = 0;
	ms_storage_type_t des_storage = 0;

	if (org_path == NULL || chg_path == NULL) {
		MS_DBG_ERR("Parameter is wrong");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	dp = opendir(chg_path);
	if (dp == NULL) {
		MS_DBG_ERR("Fail to open dir %s", chg_path);
		return MS_MEDIA_ERR_DIR_OPEN_FAIL;
	} else {
		MS_DBG("Modify added watch");
		ms_inoti_modify_watch(org_path, chg_path);
	}

	while (!readdir_r(dp, &ent, &res)) {
		if (res == NULL)
			break;

		if (ent.d_name[0] == '.')
			continue;

		err = ms_strappend(path_from, sizeof(path_from), "%s/%s", org_path, ent.d_name);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_strappend error : %d", err);
			continue;
		}

		err =  ms_strappend(path_to, sizeof(path_to), "%s/%s", chg_path, ent.d_name);
		if (err != MS_MEDIA_ERR_NONE) {
			MS_DBG_ERR("ms_strappend error : %d", err);
			continue;
		}

		/*in case of directory */
		if (ent.d_type == DT_DIR) {
			_ms_inoti_scan_renamed_folder(handle, path_from, path_to);
		}

		/*in case of file */
		if (ent.d_type == DT_REG) {
			src_storage = ms_get_storage_type_by_full(path_from);
			des_storage = ms_get_storage_type_by_full(path_to);

			if ((src_storage != MS_MEDIA_ERR_INVALID_PATH)
			    && (des_storage != MS_MEDIA_ERR_INVALID_PATH))
				ms_move_item(handle, src_storage, des_storage, path_from, path_to);
			else {
				MS_DBG_ERR("src_storage : %s", src_storage);
				MS_DBG_ERR("des_storage : %s", des_storage);
			}
		}
	}

	closedir(dp);

	return 0;
}

int ms_inoti_add_ignore_file(const char *path)
{
	ms_ignore_file_info *new_node;

	new_node = ms_inoti_find_ignore_file(path);
	if (new_node != NULL)
		return MS_MEDIA_ERR_NONE;

	new_node = malloc(sizeof(ms_ignore_file_info));
	new_node->path = strdup(path);

	/*first created file */
	if (latest_ignore_file == NULL) {
		latest_ignore_file = malloc(sizeof(ms_ignore_file_info));
		new_node->previous = NULL;
	} else {
		latest_ignore_file->next = new_node;
		new_node->previous = latest_ignore_file;
	}
	new_node->next = NULL;

	latest_ignore_file = new_node;

	return MS_MEDIA_ERR_NONE;
}

int ms_inoti_delete_ignore_file(ms_ignore_file_info * delete_node)
{
	if (delete_node->previous != NULL)
		delete_node->previous->next = delete_node->next;
	if (delete_node->next != NULL)
		delete_node->next->previous = delete_node->previous;

	if (delete_node == latest_ignore_file) {
		latest_ignore_file = delete_node->previous;
	}

	MS_SAFE_FREE(delete_node->path);
	MS_SAFE_FREE(delete_node);


	return MS_MEDIA_ERR_NONE;
}

ms_ignore_file_info *ms_inoti_find_ignore_file(const char *path)
{
	ms_ignore_file_info *node = NULL;
	
	node = latest_ignore_file;
	while (node != NULL) {
		if (strcmp(node->path, path) == 0) {
			return node;
		}

		node = node->previous;
	}

	return NULL;
}

void ms_inoti_delete_mmc_ignore_file(void)
{
	ms_ignore_file_info *prv_node = NULL;
	ms_ignore_file_info *cur_node = NULL;
	ms_ignore_file_info *del_node = NULL;

	if (latest_ignore_file != NULL) {
		cur_node = latest_ignore_file;
		while (cur_node != NULL) {
			if (strstr(cur_node->path, MEDIA_ROOT_PATH_SDCARD) != NULL) {
				if (prv_node != NULL) {
					prv_node->previous = cur_node->previous;
				}

				if (cur_node == latest_ignore_file)
					latest_ignore_file = latest_ignore_file->previous;

				del_node = cur_node;
			} else {
				prv_node = cur_node;
			}

			cur_node = cur_node->previous;

			if (del_node != NULL) {
				MS_SAFE_FREE(del_node->path);
				MS_SAFE_FREE(del_node);
			}
		}
	}

	/*active flush */
	malloc_trim(0);
}

int ms_inoti_init(void)
{
	inoti_fd = inotify_init();
	if (inoti_fd < 0) {
		perror("inotify_init");
		MS_DBG_ERR("inotify_init failed");
		return inoti_fd;
	}

	return MS_MEDIA_ERR_NONE;
}

void ms_inoti_add_watch(char *path)
{
	ms_inoti_dir_data *current_dir = NULL;
	ms_inoti_dir_data *prv_node = NULL;
	ms_inoti_dir_data *last_node = NULL;

	/*find same folder */
	if (first_inoti_node != NULL) {
		last_node = first_inoti_node;
		while (last_node != NULL) {
			if (strcmp(path, last_node->name) == 0) {
				MS_DBG("watch is already added: %s", path);
				return;
			}
			prv_node = last_node;
			last_node = last_node->next;
		}
	}

	/*there is no same path. */
	current_dir = malloc(sizeof(ms_inoti_dir_data));
	current_dir->wd = inotify_add_watch(inoti_fd, path,
			      IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
			      IN_MOVED_FROM | IN_MOVED_TO);

	if (current_dir->wd > 0) {
		current_dir->name = strdup(path);
		current_dir->next = NULL;

		if (first_inoti_node == NULL) {
			first_inoti_node = current_dir;
		} else {
			/*if next node of current node is NULL, it is the lastest node. */
			prv_node->next = current_dir;
		}
		MS_DBG("add watch : %s", path);
	} else {
		MS_DBG_ERR("inotify_add_watch failed");
		MS_SAFE_FREE(current_dir);
	}
}

int ms_inoti_add_watch_with_node(ms_dir_scan_info * const node, int depth)
{
	int err;
	char full_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_inoti_dir_data *current_dir = NULL;
	ms_inoti_dir_data *prv_node = NULL;
	ms_inoti_dir_data *last_node = NULL;

	err = ms_get_full_path_from_node(node, full_path, depth);
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_INVALID_PATH;

	/*find same folder */
	if (first_inoti_node != NULL) {
		last_node = first_inoti_node;
		while (last_node != NULL) {
			if (strcmp(full_path, last_node->name) == 0) {
				return MS_MEDIA_ERR_NONE;
			}
			prv_node = last_node;
			last_node = last_node->next;
		}
	}

	/*there is no same path. */
	current_dir = malloc(sizeof(ms_inoti_dir_data));
	current_dir->wd = inotify_add_watch(inoti_fd, full_path,
			      IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
			      IN_MOVED_FROM | IN_MOVED_TO);
	if( current_dir->wd > 0) {
		current_dir->name = strdup(full_path);
		current_dir->next = NULL;

		if (first_inoti_node == NULL) {
			first_inoti_node = current_dir;
		} else {
			/*if next node of current node is NULL, it is the lastest node. */
			prv_node->next = current_dir;
		}
		MS_DBG("add watch : %s", full_path);
	} else {
		MS_DBG_ERR("inotify_add_watch  failed : %d", current_dir->wd);
		MS_SAFE_FREE(current_dir);
	}

	return MS_MEDIA_ERR_NONE;
}

void ms_inoti_remove_watch_recursive(char *path)
{
	ms_inoti_dir_data *prv_node = NULL;
	ms_inoti_dir_data *cur_node = NULL;
	ms_inoti_dir_data *del_node = NULL;

	if (first_inoti_node != NULL) {
		cur_node = first_inoti_node;
		while (cur_node != NULL) {
			if (strstr(cur_node->name, path) != NULL) {
				if (prv_node != NULL) {
					prv_node->next =
					    cur_node->next;
				}

				if (cur_node == first_inoti_node)
					first_inoti_node =
					    first_inoti_node->next;

				del_node = cur_node;
			} else {
				prv_node = cur_node;
			}

			cur_node = cur_node->next;

			if (del_node != NULL) {
				MS_SAFE_FREE(del_node->name);
				MS_SAFE_FREE(del_node);
			}
		}
	}

	/*active flush */
	 malloc_trim(0);
}

void ms_inoti_remove_watch(char *path)
{
	ms_inoti_dir_data *del_node = NULL;
	ms_inoti_dir_data *prv_node = NULL;

	if (strcmp(first_inoti_node->name, path) == 0) {
		del_node = first_inoti_node;
		first_inoti_node = first_inoti_node->next;
	} else {
		/*find same folder */
		if (first_inoti_node != NULL) {
			del_node = first_inoti_node;
			while (del_node != NULL) {
				MS_DBG("current node %s", del_node->name);
				if (strcmp(path, del_node->name) == 0) {
					MS_DBG("find delete node: %s", del_node->name);
					if (prv_node != NULL) {
						MS_DBG("previous_node : %s", prv_node->name);
						prv_node->next = del_node->next;
					}
					/*free deleted node */
					MS_SAFE_FREE(del_node->name);
					MS_SAFE_FREE(del_node);
					break;
				}
				prv_node = del_node;
				del_node = del_node->next;
			}
		}
	}

	/*active flush */
	malloc_trim(0);
}

void ms_inoti_modify_watch(char *path_from, char *path_to)
{
	bool find = false;
	ms_inoti_dir_data *mod_node;

	if (strcmp(first_inoti_node->name, path_from) == 0) {
		mod_node = first_inoti_node;
	} else {
		/*find same folder */
		if (first_inoti_node != NULL) {
			mod_node = first_inoti_node;
			while (mod_node != NULL) {
				/*find previous directory*/
				if (strcmp(path_from, mod_node->name) == 0) { 
					/*change path of directory*/
					/*free previous name of node */
					MS_SAFE_FREE(mod_node->name);

					/*add new name */
					mod_node->name = strdup(path_to);

					/*active flush */
					malloc_trim(0);

					find = true;
					break;
				}
				mod_node = mod_node->next;
			}
		}
	}

	/*this is new directory*/
	if (find == false) {
		ms_inoti_add_watch(path_to);
	}
}

#define STOP_INOTI "stop_inoti"

gboolean ms_inoti_thread(void *data)
{
	uint32_t i;
	int length;
	int err;
	int prev_mask = 0;
	int prev_wd = -1;
	bool res;
	char name[MS_FILE_NAME_LEN_MAX + 1] = { 0 };
	char prev_name[MS_FILE_NAME_LEN_MAX + 1] = { 0 };
	char buffer[INOTI_BUF_LEN] = { 0 };
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	struct inotify_event *event;
	void **handle = NULL;

	MS_DBG("START INOTIFY");

	err = ms_connect_db(&handle);
	if (err != MS_MEDIA_ERR_NONE) {
		MS_DBG_ERR(" INOTIFY : sqlite3_open: ret = %d", err);
		return false;
	}

	while (1) {
		i = 0;
		length = read(inoti_fd, buffer, sizeof(buffer) - 1);

		if (length < 0 || length > sizeof(buffer)) {	/*this is error */
			continue;
		}

		while (i < length && i < INOTI_BUF_LEN) {
			/*check poweroff status*/
			if(power_off) {
				MS_DBG("power off");
				goto _POWEROFF;
			}

			/*it's possible that ums lets reset phone data... */
			event = (struct inotify_event *)&buffer[i];

			if (strcmp(event->name, POWEROFF_DIR_NAME) == 0) {
				MS_DBG("power off");
				goto _POWEROFF;
			} else if (strcmp(event->name, STOP_INOTI) == 0) {
				MS_DBG("stop inotify thread");
				goto _POWEROFF;
			} else if (event->name[0] == '.') {
				/*event of hidden folder is ignored */
				goto NEXT_INOTI_EVENT;
			} else if (event->wd < 1) {
				/*this is error */
				MS_DBG_ERR("invalid wd : %d", event->wd);
				goto NEXT_INOTI_EVENT;
			}

			/*start of one event */
			if (event->len && (event->len <= MS_FILE_NAME_LEN_MAX + 1)) {
				/*Add for fixing prevent defect 2011-02-15 */
				err = ms_strcopy(name, sizeof(name), "%s", event->name);
				if (err != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strcopy error : %d", err);
					goto NEXT_INOTI_EVENT;
				}

				/*get full path of file or directory */
				res = _ms_inoti_get_full_path(event->wd, name, path, sizeof(path));
				if (res == false) {
					MS_DBG_ERR("_ms_inoti_get_full_path error");
					goto NEXT_INOTI_EVENT;
				}

				MS_DBG("INOTIFY[%d : %s]", event->wd, name);
				if (event->mask & IN_ISDIR) {
					MS_DBG("DIRECTORY INOTIFY");
					
					if (event->mask & IN_MOVED_FROM) {
						MS_DBG("MOVED_FROM");

						prev_mask = event->mask;
						prev_wd = event->wd;

						err = ms_strcopy(prev_name, sizeof(prev_name), "%s", event->name);
						if (err != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("ms_strcopy fail");
							goto NEXT_INOTI_EVENT;
						}
					} 
					else if (event->mask & IN_MOVED_TO) {
						MS_DBG("MOVED_TO");

						char full_path_from[MS_FILE_PATH_LEN_MAX] = { 0 };

						res = _ms_inoti_get_full_path(prev_wd, prev_name, full_path_from, sizeof(full_path_from));
						if (res == false) {
							MS_DBG_ERR("_ms_inoti_get_full_path error");
							goto NEXT_INOTI_EVENT;
						}
						/*enable bundle commit*/
						ms_move_start(handle);

						/*need update file information under renamed directory */
						_ms_inoti_scan_renamed_folder(handle, full_path_from, path);

						/*disable bundle commit*/
						ms_move_end(handle);

						prev_mask = prev_wd = 0;	/*reset */
					}
					else if (event->mask & IN_CREATE) {
						MS_DBG("CREATE");

						_ms_inoti_directory_scan_and_register_file(handle, path);
						prev_mask = event->mask;
					}
					else if (event->mask & IN_DELETE) {
						MS_DBG("DELETE");

						ms_inoti_remove_watch(path);
					}
				}
				else {
					MS_DBG("FILE INOTIFY");

					if (event->mask & IN_MOVED_FROM) {
						MS_DBG("MOVED_FROM");

						err = ms_delete_item(handle, path);
						if (err != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("ms_media_db_delete fail error : %d", err);
						}
					}
					else if (event->mask & IN_MOVED_TO) {
						MS_DBG("MOVED_TO");

						err = ms_register_file(handle, path, NULL);
						if (err != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("ms_register_file error : %d", err);
						}
					}
					else if (event->mask & IN_CREATE) {
						MS_DBG("CREATE");

						_ms_inoti_add_create_file_list(event->wd, name);
					}
					else if (event->mask & IN_DELETE) {
						MS_DBG("DELETE");
						
						err = ms_delete_item(handle, path);
						if (err != MS_MEDIA_ERR_NONE) {
							MS_DBG_ERR("ms_media_db_delete error : %d", err);
						}
					}
					else if (event->mask & IN_CLOSE_WRITE) {
						MS_DBG("CLOSE_WRITE");
						ms_create_file_info *node;

						node = _ms_inoti_find_create_file_list (event->wd, name);
						if (node != NULL || ((prev_mask & IN_ISDIR) & IN_CREATE)) {
							err = ms_register_file(handle, path, NULL);
							if (err != MS_MEDIA_ERR_NONE) {
								MS_DBG_ERR("ms_register_file error : %d", err);
							}
							if (node != NULL)
								_ms_inoti_delete_create_file_list(node);
						}
						else {
							ms_ignore_file_info  *ignore_file;

							ignore_file = ms_inoti_find_ignore_file(path);
							if (ignore_file == NULL) {
								/*in case of replace */
								MS_DBG("This case is replacement or changing meta data.");
								err = ms_refresh_item(handle, path);
								if (err != MS_MEDIA_ERR_NONE) {
									MS_DBG_ERR("ms_refresh_item error : %d", err);
									goto NEXT_INOTI_EVENT;
								}
							} else {
								/*This is ignore case*/
							}
						}
						prev_mask = prev_wd = 0;	/*reset */
					}
				}
			} /*end of one event */
			else {
				/*This is ignore case*/
				if (event->mask & IN_IGNORED) {
					MS_DBG("This case is ignored");
				}
			}
 NEXT_INOTI_EVENT:	;
			i += INOTI_EVENT_SIZE + event->len;
		}

		/*Active flush */
		malloc_trim(0);
	}
_POWEROFF:
	ms_inoti_remove_watch_recursive(MEDIA_ROOT_PATH_INTERNAL);
	ms_inoti_remove_watch_recursive(MEDIA_ROOT_PATH_SDCARD);

	close(inoti_fd);

	if (handle) ms_disconnect_db(&handle);

	return false;
}

int _ms_get_path_from_current_node(int find_folder,
				   ms_dir_scan_info **current_root,
				   ms_dir_scan_info **real_root, char **path, int *depth)
{
	int err = MS_MEDIA_ERR_NONE;
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
					return MS_MEDIA_ERR_NONE;
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
	if (err != MS_MEDIA_ERR_NONE)
		return MS_MEDIA_ERR_INVALID_PATH;

	*path = strdup(get_path);

	return err;
}

void ms_inoti_add_watch_all_directory(ms_storage_type_t storage_type)
{
	int err = 0;
	int depth = 0;
	int find_folder = 0;
	char get_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	char *path = NULL;
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
		MS_DBG_ERR("malloc fail");
		return;
	}

	if (storage_type == MS_STORAGE_INTERNAL)
		root->name = strdup(MEDIA_ROOT_PATH_INTERNAL);
	else
		root->name = strdup(MEDIA_ROOT_PATH_SDCARD);
	if (root->name  == NULL) {
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
	if (err != MS_MEDIA_ERR_NONE) {
		MS_SAFE_FREE(path);
		MS_SAFE_FREE(root);
		return;
	}

	ms_inoti_add_watch_with_node(root, depth);

	while (1) {
		/*check poweroff status*/
		if (power_off) {
			MS_DBG("Power off");
			goto FREE_RESOURCES;
		}

		/*check SD card in out*/
		if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORAGE_EXTERNAL))
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
			if ((mmc_state != VCONFKEY_SYSMAN_MMC_MOUNTED) && (storage_type == MS_STORAGE_EXTERNAL)) {
				goto FREE_RESOURCES;
			}

			if (entry.d_type & DT_DIR) {
				DIR *tmp_dp = NULL;
				err = ms_strappend(get_path, sizeof(get_path), "%s/%s",path, entry.d_name);
				if (err != MS_MEDIA_ERR_NONE) {
					MS_DBG_ERR("ms_strappend error");
					continue;
				}

				tmp_dp = opendir(get_path);
				if (tmp_dp == NULL) {
					MS_DBG_ERR("%s folder opendir fails", get_path);
					MS_DBG("error : %d, %s", errno ,strerror(errno));
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
				ms_inoti_add_watch_with_node(cur_node, depth);

				/*change previous */
				prv_node = cur_node;
				find_folder++;
			}
		}
NEXT_DIR:
		if (dp) closedir(dp);
		MS_SAFE_FREE(path);
		dp = NULL;
		path = NULL;

		err = _ms_get_path_from_current_node(find_folder, &tmp_root, &root, &path, &depth);
		if (err != MS_MEDIA_ERR_NONE)
			break;

		if (tmp_root == NULL)
			break;

		find_folder = 0;
	}

FREE_RESOURCES:
	/*free allocated memory */
	if (dp) closedir(dp);
	MS_SAFE_FREE(path);

	cur_node = root;
	while (cur_node != NULL) {
		next_node = cur_node->next;
		MS_SAFE_FREE(cur_node->name);
		MS_SAFE_FREE(cur_node);
		cur_node = next_node;
	}
}

#endif
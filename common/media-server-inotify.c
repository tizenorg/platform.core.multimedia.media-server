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
#include "media-server-global.h"
#include "media-server-common.h"
#include "media-server-inotify-internal.h"
#include "media-server-inotify.h"

extern int inoti_fd;
ms_dir_data *first_inoti_node;
ms_ignore_file_info *latest_ignore_file;

int _ms_inoti_directory_scan_and_register_file(char *dir_path)
{
	MS_DBG_START();
	struct dirent ent;
	struct dirent *res = NULL;
	DIR *dp = NULL;
	char path[MS_FILE_PATH_LEN_MAX] = { 0 };
	int err;

	if (dir_path == NULL)
		return MS_ERR_INVALID_DIR_PATH;

	dp = opendir(dir_path);
	if (dp == NULL) {
		MS_DBG("Fail to open dir");
		return MS_ERR_DIR_OPEN_FAIL;
	}

	ms_inoti_add_watch(dir_path);

	while (!readdir_r(dp, &ent, &res)) {
		if (res == NULL)
			break;

		if (ent.d_name[0] == '.')
			continue;

		err = ms_strappend(path, sizeof(path), "%s/%s", dir_path, ent.d_name);
		if (err != MS_ERR_NONE) {
			MS_DBG("ms_strappend error : %d", err);
			continue;
		}

		/*in case of directory */
		if (ent.d_type == DT_DIR) {
			_ms_inoti_directory_scan_and_register_file(path);
		} else {

			err = ms_register_file(path, NULL);
			if (err != MS_ERR_NONE) {
				MS_DBG("ms_register_file error : %d", err);
				continue;
			}
		}
	}

	closedir(dp);

	MS_DBG_END();

	return 0;
}

int _ms_inoti_scan_renamed_folder(char *org_path, char *chg_path)
{
	if (org_path == NULL || chg_path == NULL) {
		MS_DBG("Parameter is wrong");
		return MS_ERR_ARG_INVALID;
	}

	MS_DBG_START();

	int err = -1;
	struct dirent ent;
	struct dirent *res = NULL;

	DIR *dp = NULL;
	char path_from[MS_FILE_PATH_LEN_MAX] = { 0 };
	char path_to[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_store_type_t src_storage = 0;
	ms_store_type_t des_storage = 0;

	dp = opendir(chg_path);
	if (dp == NULL) {
		MS_DBG("Fail to open dir");
		return MS_ERR_DIR_OPEN_FAIL;
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
		if (err != MS_ERR_NONE) {
			MS_DBG("ms_strappend error : %d", err);
			continue;
		}

		err =  ms_strappend(path_to, sizeof(path_to), "%s/%s", chg_path, ent.d_name);
		if (err != MS_ERR_NONE) {
			MS_DBG("ms_strappend error : %d", err);
			continue;
		}

		/*in case of directory */
		if (ent.d_type == DT_DIR) {
			_ms_inoti_scan_renamed_folder(path_from, path_to);
		}

		/*in case of file */
		if (ent.d_type == DT_REG) {
			src_storage = ms_get_store_type_by_full(path_from);
			des_storage = ms_get_store_type_by_full(path_to);

			if ((src_storage != MS_ERR_INVALID_FILE_PATH)
			    && (des_storage != MS_ERR_INVALID_FILE_PATH))
				ms_media_db_move(src_storage, des_storage, path_from, path_to);
			else {
				MS_DBG("ms_get_store_type_by_full error");
			}
		}
	}

	closedir(dp);

	MS_DBG_END();
	return 0;
}

int ms_inoti_add_ignore_file(const char *path)
{
	ms_ignore_file_info *new_node;

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

	return MS_ERR_NONE;
}

int ms_inoti_delete_ignore_file(ms_ignore_file_info * delete_node)
{
	MS_DBG("");
	if (delete_node->previous != NULL)
		delete_node->previous->next = delete_node->next;
	if (delete_node->next != NULL)
		delete_node->next->previous = delete_node->previous;

	if (delete_node == latest_ignore_file) {
		latest_ignore_file = delete_node->previous;
	}

	free(delete_node->path);
	free(delete_node);

	MS_DBG("");

	return MS_ERR_NONE;
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
	MS_DBG_START();
	ms_ignore_file_info *prv_node = NULL;
	ms_ignore_file_info *cur_node = NULL;
	ms_ignore_file_info *del_node = NULL;

	if (latest_ignore_file != NULL) {
		cur_node = latest_ignore_file;
		while (cur_node != NULL) {
			if (strstr(cur_node->path, MS_MMC_ROOT_PATH) != NULL) {
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
				free(del_node->path);
				free(del_node);
				del_node = NULL;
			}
		}
	}

	/*active flush */
	malloc_trim(0);

	MS_DBG_END();
}

int ms_inoti_init(void)
{
	inoti_fd = inotify_init();
	if (inoti_fd < 0) {
		perror("inotify_init");
		MS_DBG("inotify_init failed");
		return inoti_fd;
	}

	return MS_ERR_NONE;
}

void ms_inoti_add_watch(char *path)
{
	MS_DBG("");
	ms_dir_data *current_dir = NULL;
	ms_dir_data *prv_node = NULL;
	ms_dir_data *last_node = NULL;

	/*find same folder */
	if (first_inoti_node != NULL) {
		MS_DBG("find same folder");
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

	MS_DBG("start add watch");

	/*there is no same path. */
	current_dir = malloc(sizeof(ms_dir_data));
	current_dir->name = strdup(path);
	current_dir->wd = inotify_add_watch(inoti_fd, path,
			      IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
			      IN_MOVED_FROM | IN_MOVED_TO);
	current_dir->next = NULL;

	if (first_inoti_node == NULL) {
		first_inoti_node = current_dir;
	} else {
		/*if next node of current node is NULL, it is the lastest node. */
		MS_DBG("last_node : %s", prv_node->name);
		prv_node->next = current_dir;
	}

	MS_DBG("add watch : %s", path);
}

int ms_inoti_add_watch_with_node(ms_dir_scan_info * const node)
{
	char full_path[MS_FILE_PATH_LEN_MAX] = { 0 };
	ms_dir_data *current_dir = NULL;
	ms_dir_data *last_node = NULL;

	ms_get_full_path_from_node(node, full_path);

	/*find same folder */
	if (first_inoti_node != NULL) {
		last_node = first_inoti_node;
		while (last_node->next != NULL) {
			if (strcmp(full_path, last_node->name) == 0) {
				MS_DBG("watch is already added: %s", full_path);
				return MS_ERR_NONE;
			}
			last_node = last_node->next;
		}
	}

	/*there is no same path. */
	current_dir = malloc(sizeof(ms_dir_data));
	current_dir->name = strdup(full_path);
	current_dir->wd =
	    inotify_add_watch(inoti_fd, full_path,
			      IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
			      IN_MOVED_FROM | IN_MOVED_TO);
	current_dir->next = NULL;
	current_dir->db_updated = false;

	if (first_inoti_node == NULL) {
		first_inoti_node = current_dir;
	} else {
		/*if next node of current node is NULL, it is the lastest node. */
		MS_DBG("last_node : %s", last_node->name);
		last_node->next = current_dir;
	}
	MS_DBG("add watch : %s", full_path);

	return MS_ERR_NONE;
}

void ms_inoti_remove_watch_recursive(char *path)
{
	MS_DBG_START();
	ms_dir_data *prv_node = NULL;
	ms_dir_data *cur_node = NULL;
	ms_dir_data *del_node = NULL;

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
				free(del_node->name);
				free(del_node);
				del_node = NULL;
			}
		}
	}

	/*active flush */
	 malloc_trim(0);

	MS_DBG_END();
}

void ms_inoti_remove_watch(char *path)
{
	ms_dir_data *del_node = NULL;
	ms_dir_data *prv_node = NULL;

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
					free(del_node->name);
					free(del_node);
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
	ms_dir_data *mod_node;

	if (strcmp(first_inoti_node->name, path_from) == 0) {
		mod_node = first_inoti_node;
	} else {
		/*find same folder */
		if (first_inoti_node != NULL) {
			mod_node = first_inoti_node;
			while (mod_node->next != NULL) {
				if (strcmp(path_from, mod_node->name) == 0) {
					MS_DBG("find change node: %s",
					       mod_node->name);
					break;
				}
				mod_node = mod_node->next;
			}
		}
	}

	/*free previous name of node */
	free(mod_node->name);
	mod_node->name = NULL;

	/*add new name */
	mod_node->name = strdup(path_to);

	/*active flush */
	malloc_trim(0);
}


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

	MS_DBG("START INOTIFY");

	err = ms_media_db_open();
	if (err != MS_ERR_NONE) {
		MS_DBG(" INOTIFY : sqlite3_open: ret = %d", err);
		return false;
	}

	while (1) {
		i = 0;
		length = read(inoti_fd, buffer, sizeof(buffer) - 1);

		if (length < 0 || length > sizeof(buffer)) {	/*this is error */
			MS_DBG("fail read");
			perror("read");
			continue;
		}

		while (i < length && i < INOTI_BUF_LEN) {
			/*it's possible that ums lets reset phone data... */
			event = (struct inotify_event *)&buffer[i];

			/*stop this threadfor hibernation */
			if (strcmp(event->name, "_HIBERNATION_END") == 0) {
				/*db close before capture hibernatio image */
				err = ms_media_db_close();
				if (err != MS_ERR_NONE) {
					MS_DBG("failed ms_media_db_close : ret = (%d)", err);
				}
				return false;
			} else if(strcmp(event->name, "_FILEOPERATION_END") == 0) {
				/*file operation is end*/
				/* announce db is updated*/
				ms_set_db_status(MS_DB_UPDATED);
				rmdir("/opt/media/_FILEOPERATION_END");
				goto NEXT_INOTI_EVENT;
			} else if (event->name[0] == '.') {
				/*event of hidden folder is ignored */
				MS_DBG("Ignore : First character of event->name includes invalid character");
				goto NEXT_INOTI_EVENT;
			} else if (event->wd < 1) {
				/*this is error */
				MS_DBG("invalid wd : %d", event->wd);
				goto NEXT_INOTI_EVENT;
			}

			/*start of one event */
			if (event->len && (event->len <= MS_FILE_NAME_LEN_MAX)) {
				/*Add for fixing prevent defect 2011-02-15 */
				err = ms_strcopy(name, sizeof(name), "%s", event->name);
				if (err != MS_ERR_NONE) {
					MS_DBG("ms_strcopy error : %d", err);
					goto NEXT_INOTI_EVENT;
				}

				/*get full path of file or directory */
				res = _ms_inoti_get_full_path(event->wd, name, path, sizeof(path));
				if (res == false) {
					MS_DBG("_ms_inoti_get_full_path error");
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
						if (err != MS_ERR_NONE) {
							MS_DBG("ms_strcopy fail");
							goto NEXT_INOTI_EVENT;
						}
					} 
					else if (event->mask & IN_MOVED_TO) {
						MS_DBG("MOVED_TO");

						char full_path_from[MS_FILE_PATH_LEN_MAX] = { 0 };

						res = _ms_inoti_get_full_path(prev_wd, prev_name, full_path_from, sizeof(full_path_from));
						if (res == false) {
							MS_DBG("_ms_inoti_get_full_path error");
							goto NEXT_INOTI_EVENT;
						}
						/*enable bundle commit*/
						ms_media_db_move_start();

						/*need update file information under renamed directory */
						_ms_inoti_scan_renamed_folder(full_path_from, path);

						/*disable bundle commit*/
						ms_media_db_move_end();

						if (_fex_is_default_path(prev_name)) {
							if (strstr(path, MS_PHONE_ROOT_PATH)) {
								fex_make_default_path();
							} else {
								fex_make_default_path_mmc();
							}
						}
						prev_mask = prev_wd = 0;	/*reset */
					}
					else if (event->mask & IN_CREATE) {
						MS_DBG("CREATE");

						_ms_inoti_directory_scan_and_register_file(path);
						prev_mask = event->mask;
					}
					else if (event->mask & IN_DELETE) {
						MS_DBG("DELETE");

						ms_inoti_remove_watch(path);

						if (_fex_is_default_path(name)) {
							if (strstr(path, MS_PHONE_ROOT_PATH)) {
								fex_make_default_path();
							} else {
								fex_make_default_path_mmc();
							}
						}
					}
				}
				else {
					MS_DBG("FILE INOTIFY");

					if (event->mask & IN_MOVED_FROM) {
						MS_DBG("MOVED_FROM");

						err = ms_media_db_delete(path);
						if (err != MS_ERR_NONE) {
							MS_DBG("ms_media_db_delete fail error : %d", err);
						}
					}
					else if (event->mask & IN_MOVED_TO) {
						MS_DBG("MOVED_TO");

						err = ms_register_file(path, NULL);
						if (err != MS_ERR_NONE) {
							MS_DBG("ms_register_file error : %d", err);
						}
					}
					else if (event->mask & IN_CREATE) {
						MS_DBG("CREATE");

						_ms_inoti_add_create_file_list(event->wd, name);
					}
					else if (event->mask & IN_DELETE) {
						MS_DBG("DELETE");
						
						err = ms_media_db_delete(path);
						if (err != MS_ERR_NONE) {
							MS_DBG("ms_media_db_delete error : %d", err);
						}
					}
					else if (event->mask & IN_CLOSE_WRITE) {
						MS_DBG("CLOSE_WRITE");

						ms_create_file_info *node;
						node = _ms_inoti_find_create_file_list (event->wd, name);

						if (node != NULL || ((prev_mask & IN_ISDIR) & IN_CREATE)) {

							err = ms_register_file(path, NULL);
							if (err != MS_ERR_NONE) {
								MS_DBG("ms_register_file error : %d", err);
							}
							if (node != NULL)
								_ms_inoti_delete_create_file_list(node);
						}
						else {
							/*in case of replace */
							MS_DBG("This case is replacement or changing meta data.");
							ms_ignore_file_info  *ignore_file;

							ignore_file = ms_inoti_find_ignore_file(path);
							if (ignore_file == NULL) {
								err = ms_media_db_delete(path);
								if (err != MS_ERR_NONE) {
									MS_DBG("ms_media_db_delete error : %d", err);
								}
								/*update = delete + regitster */
								err = ms_register_file(path, NULL);
								if (err != MS_ERR_NONE) {
									MS_DBG("ms_register_file error : %d", err);
									goto NEXT_INOTI_EVENT;
								}
							} else {
								MS_DBG(" Ignore this file");
							}
						}
						prev_mask = prev_wd = 0;	/*reset */
					}
				}
			} /*end of one event */
			else {
				MS_DBG("Event length is zero or over MS_FILE_NAME_LEN_MAX");
				if (event->mask & IN_IGNORED) {
					MS_DBG("This case is ignored");
				}
			}
 NEXT_INOTI_EVENT:	;
			i += INOTI_EVENT_SIZE + event->len;
		}
		/*Active flush */
		sqlite3_release_memory(-1);
		malloc_trim(0);
	}

	ms_inoti_remove_watch(MS_DB_UPDATE_NOTI_PATH);

	ms_inoti_remove_watch_recursive(MS_PHONE_ROOT_PATH);
	ms_inoti_remove_watch_recursive(MS_MMC_ROOT_PATH);

	close(inoti_fd);

	err = ms_media_db_close();
	if (err != MS_ERR_NONE) {
		MS_DBG("ms_media_db_close error : %d", err);
		return false;
	}
	MS_DBG("Disconnect MEDIA DB");

	return false;
}

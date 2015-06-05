/*
 *  Media Utility
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
 * This file defines api utilities of Media DB.
 *
 * @file		media-util-db.c
 * @author	Haejeong Kim(backto.kim@samsung.com)
 * @version	1.0
 * @brief
 */
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <db-util.h>
#include <grp.h>
#include <pwd.h>
#include <sys/smack.h>
#include "media-server-ipc.h"
#include "media-util-dbg.h"
#include "media-util-internal.h"
#include "media-util.h"

#define GLOBAL_USER	0 //#define 	tzplatform_getenv(TZ_GLOBAL) //TODO
#define BUFSIZE 4096

#define QUERY_ATTACH "attach database '%s' as Global"
#define QUERY_CREATE_VIEW_ALBUM "CREATE temp VIEW album as select distinct * from (select  * from main.album m union select * from Global.album g)"
#define QUERY_CREATE_VIEW_FOLDER "CREATE temp VIEW folder as select distinct * from (select  * from main.folder m union select * from Global.folder g)"
#define QUERY_CREATE_VIEW_PLAYLIST "CREATE temp VIEW playlist as select distinct * from (select  * from main.playlist m union select * from Global.playlist g)"
#define QUERY_CREATE_VIEW_PLAYLIST_VIEW "CREATE temp VIEW playlist_view as select distinct * from (select  * from main.playlist_view m union select * from Global.playlist_view g)"
#define QUERY_CREATE_VIEW_TAG_MAP "CREATE temp VIEW tag_map as select distinct * from (select  * from main.tag_map m union select * from Global.tag_map g)"
#define QUERY_CREATE_VIEW_BOOKMARK "CREATE temp VIEW bookmark as select distinct * from (select  * from main.bookmark m union select * from Global.bookmark g)"
#define QUERY_CREATE_VIEW_MEDIA "CREATE temp VIEW media as select distinct * from (select  * from main.media m union select * from Global.media g)"
#define QUERY_CREATE_VIEW_PLAYLIST_MAP "CREATE temp VIEW playlist_map as select distinct * from (select  * from main.playlist_map m union select * from Global.playlist_map g)"
#define QUERY_CREATE_VIEW_TAG "CREATE temp VIEW tag as select distinct * from (select  * from main.tag m union select * from Global.tag g)"
#define QUERY_CREATE_VIEW_TAG_VIEW "CREATE temp VIEW tag_view as select distinct * from (select  * from main.tag_view m union select * from Global.tag_view g)"

#define QUERY_DETACH "detach database Global"
#define QUERY_DROP_VIEW_ALBUM "DROP VIEW album"
#define QUERY_DROP_VIEW_FOLDER "DROP VIEW folder"
#define QUERY_DROP_VIEW_PLAYLIST "DROP VIEW playlist"
#define QUERY_DROP_VIEW_PLAYLIST_VIEW "DROP VIEW playlist_view"
#define QUERY_DROP_VIEW_TAG_MAP "DROP VIEW tag_map"
#define QUERY_DROP_VIEW_BOOKMARK "DROP VIEW bookmark"
#define QUERY_DROP_VIEW_MEDIA "DROP VIEW media"
#define QUERY_DROP_VIEW_PLAYLIST_MAP "DROP VIEW playlist_map"
#define QUERY_DROP_VIEW_TAG "DROP VIEW tag"
#define QUERY_DROP_VIEW_TAG_VIEW "DROP VIEW tag_view"

static __thread char **sql_list = NULL;
static __thread int g_list_idx = 0;

static int __media_db_busy_handler(void *pData, int count);
static int __media_db_connect_db_with_handle(sqlite3 **db_handle, uid_t uid);
static int __media_db_disconnect_db_with_handle(sqlite3 *db_handle);
static int __media_db_request_update(ms_msg_type_e msg_type, const char *request_msg, uid_t uid);

void __media_db_destroy_sql_list()
{
	int i = 0;

	for (i = 0; i < g_list_idx; i++) {
		MS_SAFE_FREE(sql_list[i]);
	}

	MS_SAFE_FREE(sql_list);
	g_list_idx = 0;
}

static int __media_db_busy_handler(void *pData, int count)
{
	usleep(50000);

	MSAPI_DBG("media_db_busy_handler called : %d", count);

	return 100 - count;
}
int _xsystem(const char *argv[])
{
	int status = 0;
	pid_t pid;
	pid = fork();
	switch (pid) {
	case -1:
		perror("fork failed");
		return -1;
	case 0:
		/* child */
		execvp(argv[0], (char *const *)argv);
		_exit(-1);
	default:
		/* parent */
		break;
	}
	if (waitpid(pid, &status, 0) == -1)
	{
		perror("waitpid failed");
		return -1;
	}
	if (WIFSIGNALED(status))
	{
		perror("signal");
		return -1;
	}
	if (!WIFEXITED(status))
	{
		/* shouldn't happen */
		perror("should not happen");
		return -1;
	}
	return WEXITSTATUS(status);
}

static char* __media_get_media_DB(uid_t uid)
{
	char *result_psswd = NULL;
	struct group *grpinfo = NULL;
	char * dir = NULL;
	if(uid == getuid())
	{
		result_psswd = strdup(MEDIA_DB_NAME);
		grpinfo = getgrnam("users");
		if(grpinfo == NULL) {
			MSAPI_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
	}
	else
	{
		struct passwd *userinfo = getpwuid(uid);
		if(userinfo == NULL) {
			MSAPI_DBG_ERR("getpwuid(%d) returns NULL !", uid);
			return NULL;
		}
		grpinfo = getgrnam("users");
		if(grpinfo == NULL) {
			MSAPI_DBG_ERR("getgrnam(users) returns NULL !");
			return NULL;
		}
		// Compare git_t type and not group name
		if (grpinfo->gr_gid != userinfo->pw_gid) {
			MSAPI_DBG_ERR("UID [%d] does not belong to 'users' group!", uid);
			return NULL;
		}
		asprintf(&result_psswd, "%s/.applications/dbspace/.media.db", userinfo->pw_dir);
	}

	dir = strrchr(result_psswd, '/');
	if(!dir)
		return result_psswd;

	//Control if db exist create otherwise
	if(access(dir + 1, F_OK)) {
		int ret;
		mkdir(dir + 1, S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH);
		ret = chown(dir + 1, uid, grpinfo->gr_gid);
		if (ret == -1) {
			char buf[BUFSIZE];
			strerror_r(errno, buf, sizeof(buf));
			MSAPI_DBG_ERR("FAIL : chown %s %d.%d, because %s", dir + 1, uid, grpinfo->gr_gid, buf);
		}
	}

	return result_psswd;
}

static int __media_db_update_directly(sqlite3 *db_handle, const char *sql_str)
{
	int ret = MS_MEDIA_ERR_NONE;
	char *zErrMsg = NULL;

	MSAPI_DBG_INFO("SQL = [%s]", sql_str);

	ret = sqlite3_exec(db_handle, sql_str, NULL, NULL, &zErrMsg);

	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("DB Update Fail SQL:%s [%s], err[%d]", sql_str, zErrMsg, ret);
		if (ret == SQLITE_BUSY)
			ret = MS_MEDIA_ERR_DB_BUSY_FAIL;
		else
			ret = MS_MEDIA_ERR_DB_UPDATE_FAIL;
	} else {
		MSAPI_DBG("DB Update Success");
	}

	if (zErrMsg)
		sqlite3_free (zErrMsg);

	return ret;
}

static int __media_db_connect_db_with_handle(sqlite3 **db_handle,uid_t uid)
{
	int i;
	int ret = MS_MEDIA_ERR_NONE;
	char *mediadb_path = NULL;
	char *mediadbjournal_path = NULL;

	/*Init DB*/
	const char *tbls[46] = {
		"CREATE TABLE IF NOT EXISTS album (album_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, artist TEXT, album_art TEXT, unique(name, artist));",
		"CREATE TABLE IF NOT EXISTS bookmark (bookmark_id INTEGER PRIMARY KEY AUTOINCREMENT, media_uuid TEXT NOT NULL, marked_time INTEGER DEFAULT 0, thumbnail_path  TEXT, unique(media_uuid, marked_time));",
		"CREATE TABLE IF NOT EXISTS folder (folder_uuid TEXT PRIMARY KEY, path TEXT NOT NULL UNIQUE, name TEXT NOT NULL, modified_time INTEGER DEFAULT 0, name_pinyin TEXT, storage_type INTEGER, unique(path, name, storage_type));",
		"CREATE TABLE IF NOT EXISTS media (media_uuid TEXT PRIMARY KEY, path TEXT NOT NULL UNIQUE, file_name TEXT NOT NULL, media_type INTEGER, mime_type TEXT, size INTEGER DEFAULT 0, added_time INTEGER DEFAULT 0, modified_time INTEGER DEFAULT 0, folder_uuid TEXT NOT NULL, thumbnail_path TEXT, title TEXT, album_id INTEGER DEFAULT 0, album TEXT, artist TEXT, album_artist TEXT, genre TEXT, composer TEXT, year TEXT, recorded_date TEXT, copyright TEXT, track_num TEXT, description TEXT, bitrate INTEGER DEFAULT -1, bitpersample INTEGER DEFAULT 0, samplerate INTEGER DEFAULT -1, channel INTEGER DEFAULT -1, duration INTEGER DEFAULT -1, longitude DOUBLE DEFAULT 0, latitude DOUBLE DEFAULT 0, altitude DOUBLE DEFAULT 0, width INTEGER DEFAULT -1, height INTEGER DEFAULT -1, datetaken TEXT, orientation INTEGER DEFAULT -1, burst_id TEXT, played_count INTEGER DEFAULT 0, last_played_time INTEGER DEFAULT 0, last_played_position INTEGER DEFAULT 0, rating INTEGER DEFAULT 0, favourite INTEGER DEFAULT 0, author TEXT, provider TEXT, content_name TEXT, category TEXT, location_tag TEXT, age_rating TEXT, keyword TEXT, is_drm INTEGER DEFAULT 0, storage_type INTEGER, timeline INTEGER DEFAULT 0, weather TEXT, sync_status INTEGER DEFAULT 0, file_name_pinyin TEXT, title_pinyin TEXT, album_pinyin TEXT, artist_pinyin TEXT, album_artist_pinyin TEXT, genre_pinyin TEXT, composer_pinyin TEXT, copyright_pinyin TEXT, description_pinyin TEXT, author_pinyin TEXT, provider_pinyin TEXT, content_name_pinyin TEXT, category_pinyin TEXT, location_tag_pinyin TEXT, age_rating_pinyin TEXT, keyword_pinyin TEXT, validity INTEGER DEFAULT 1, unique(path, file_name) );",
		"CREATE TABLE IF NOT EXISTS playlist ( playlist_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, thumbnail_path TEXT );",
		"CREATE TABLE IF NOT EXISTS playlist_map ( _id INTEGER PRIMARY KEY AUTOINCREMENT, playlist_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, play_order INTEGER NOT NULL );",
		"CREATE TABLE IF NOT EXISTS tag ( tag_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE );",
		"CREATE TABLE IF NOT EXISTS tag_map ( _id INTEGER PRIMARY KEY AUTOINCREMENT, tag_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, unique(tag_id, media_uuid) );",
		"CREATE INDEX folder_folder_uuid_idx on folder (folder_uuid);",
		"CREATE INDEX folder_uuid_idx on media (folder_uuid);",
		"CREATE INDEX media_album_idx on media (album);",
		"CREATE INDEX media_artist_idx on media (artist);",
		"CREATE INDEX media_author_idx on media (author);",
		"CREATE INDEX media_category_idx on media (category);",
		"CREATE INDEX media_composer_idx on media (composer);",
		"CREATE INDEX media_content_name_idx on media (content_name);",
		"CREATE INDEX media_file_name_idx on media (file_name);",
		"CREATE INDEX media_genre_idx on media (genre);",
		"CREATE INDEX media_location_tag_idx on media (location_tag);",
		"CREATE INDEX media_media_type_idx on media (media_type);",
		"CREATE INDEX media_media_uuid_idx on media (media_uuid);",
		"CREATE INDEX media_modified_time_idx on media (modified_time);",
		"CREATE INDEX media_path_idx on media (path);",
		"CREATE INDEX media_provider_idx on media (provider);",
		"CREATE INDEX media_title_idx on media (title);",
		"CREATE VIEW IF NOT EXISTS playlist_view AS "
		"SELECT "
		"p.playlist_id, p.name, p.thumbnail_path, media_count, pm._id as pm_id, pm.play_order, m.media_uuid, path, file_name, media_type, mime_type, size, added_time, modified_time, m.thumbnail_path, description, rating, favourite, author, provider, content_name, category, location_tag, age_rating, keyword, is_drm, storage_type, longitude, latitude, altitude, width, height, datetaken, orientation, title, album, artist, genre, composer, year, recorded_date, copyright, track_num, bitrate, duration, played_count, last_played_time, last_played_position, samplerate, channel FROM playlist AS p "
		"INNER JOIN playlist_map AS pm "
		"INNER JOIN media AS m "
		"INNER JOIN (SELECT count(playlist_id) as media_count, playlist_id FROM playlist_map group by playlist_id) as cnt_tbl "
		"ON (p.playlist_id=pm.playlist_id AND pm.media_uuid = m.media_uuid AND cnt_tbl.playlist_id=pm.playlist_id AND m.validity=1) "
		"UNION "
		"SELECT "
		"playlist_id, name, thumbnail_path, 0, 0, -1, NULL, NULL, -1, -1, -1, -1, -1, NULL, NULL, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, -1, -1, -1, -1, -1, -1, -1, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, -1, -1, -1, -1, -1, -1, NULL, -1 FROM playlist "
		"WHERE playlist_id "
		"NOT IN (select playlist_id from playlist_map);",
		"CREATE VIEW IF NOT EXISTS tag_view AS "
		"SELECT "
		"t.tag_id, t.name, media_count, tm._id as tm_id, m.media_uuid, path, file_name, media_type, mime_type, size, added_time, modified_time, thumbnail_path, description, rating, favourite, author, provider, content_name, category, location_tag, age_rating, keyword, is_drm, storage_type, longitude, latitude, altitude, width, height, datetaken, orientation, title, album, artist, genre, composer, year, recorded_date, copyright, track_num, bitrate, duration, played_count, last_played_time, last_played_position, samplerate, channel FROM tag AS t "
		"INNER JOIN tag_map AS tm "
		"INNER JOIN media AS m "
		"INNER JOIN (SELECT count(tag_id) as media_count, tag_id FROM tag_map group by tag_id) as cnt_tbl "
		"ON (t.tag_id=tm.tag_id AND tm.media_uuid = m.media_uuid AND cnt_tbl.tag_id=tm.tag_id AND m.validity=1)"
		"UNION "
		"SELECT "
		"tag_id, name, 0, 0,  NULL, NULL, -1, -1, -1, -1, -1, NULL, NULL, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, -1, -1, -1, -1, -1, -1, -1, -1, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, -1, -1, -1, -1, -1, -1, NULL, -1 FROM tag "
		"WHERE tag_id "
		"NOT IN (select tag_id from tag_map); ",
		"CREATE TRIGGER album_cleanup DELETE ON media BEGIN DELETE FROM album WHERE (SELECT count(*) FROM media WHERE album_id=old.album_id)=1 AND album_id=old.album_id;END;",
		"CREATE TRIGGER bookmark_cleanup DELETE ON media BEGIN DELETE FROM bookmark WHERE media_uuid=old.media_uuid;END;",
		"CREATE TRIGGER folder_cleanup DELETE ON media BEGIN DELETE FROM folder WHERE (SELECT count(*) FROM media WHERE folder_uuid=old.folder_uuid)=1 AND folder_uuid=old.folder_uuid;END;",
		"CREATE TRIGGER playlist_map_cleanup DELETE ON media BEGIN DELETE FROM playlist_map WHERE media_uuid=old.media_uuid;END;",
		"CREATE TRIGGER playlist_map_cleanup_1 DELETE ON playlist BEGIN DELETE FROM playlist_map WHERE playlist_id=old.playlist_id;END;",
		"CREATE TRIGGER tag_map_cleanup DELETE ON media BEGIN DELETE FROM tag_map WHERE media_uuid=old.media_uuid;END;",
		"CREATE TRIGGER tag_map_cleanup_1 DELETE ON tag BEGIN DELETE FROM tag_map WHERE tag_id=old.tag_id;END;",
		NULL
	};

	mediadb_path = __media_get_media_DB(uid);

	if (access(mediadb_path, F_OK)) {
		if (MS_MEDIA_ERR_NONE == db_util_open_with_options(mediadb_path, db_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
			for (i = 0; tbls[i] != NULL; i++) {
				ret = __media_db_update_directly(*db_handle, tbls[i]);
			}
			if(smack_setlabel(__media_get_media_DB(uid), "User", SMACK_LABEL_ACCESS)){
				MSAPI_DBG_ERR("failed chsmack -a \"User\" %s", mediadb_path);
			} else {
				MSAPI_DBG_ERR("chsmack -a \"User\" %s", mediadb_path);
			}
			asprintf(&mediadbjournal_path, "%s-journal", mediadb_path);
			if(smack_setlabel(mediadbjournal_path, "User", SMACK_LABEL_ACCESS)){
				MSAPI_DBG_ERR("failed chsmack -a \"User\" %s", mediadbjournal_path);
			} else {
				MSAPI_DBG_ERR("chsmack -a \"User\" %s", mediadbjournal_path);
			}
		} else {
			MSAPI_DBG_ERR("Failed to create table %s\n",__media_get_media_DB(uid));
			return MS_MEDIA_ERR_DB_CONNECT_FAIL;
		}
	}

	ret = db_util_open(__media_get_media_DB(uid), db_handle, DB_UTIL_REGISTER_HOOK_METHOD);
	if (SQLITE_OK != ret) {

		MSAPI_DBG_ERR("error when db open");
		*db_handle = NULL;
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	if (*db_handle == NULL) {
		MSAPI_DBG_ERR("*db_handle is NULL");
		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	/*Register busy handler*/
	ret = sqlite3_busy_handler(*db_handle, __media_db_busy_handler, NULL);

	if (SQLITE_OK != ret) {

		if (*db_handle) {
			MSAPI_DBG_ERR("[error when register busy handler] %s\n", sqlite3_errmsg(*db_handle));
		}

		db_util_close(*db_handle);
		*db_handle = NULL;

		return MS_MEDIA_ERR_DB_CONNECT_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

static int __media_db_disconnect_db_with_handle(sqlite3 *db_handle)
{
	int ret = MS_MEDIA_ERR_NONE;

	ret = db_util_close(db_handle);

	if (SQLITE_OK != ret) {
		MSAPI_DBG_ERR("error when db close");
		MSAPI_DBG_ERR("Error : %s", sqlite3_errmsg(db_handle));
		db_handle = NULL;
		return MS_MEDIA_ERR_DB_DISCONNECT_FAIL;
	}

	return MS_MEDIA_ERR_NONE;
}

static int __media_db_request_update(ms_msg_type_e msg_type, const char *request_msg, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;
	int err = -1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	unsigned int serv_addr_len = -1;
	int port = MS_DB_UPDATE_PORT;

	if(msg_type == MS_MSG_DB_UPDATE)
		port = MS_DB_UPDATE_PORT;
	else
		port = MS_SCANNER_PORT;

	if(!MS_STRING_VALID(request_msg))
	{
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if(request_msg_size >= MAX_MSG_SIZE)
	{
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

//	MSAPI_DBG("querysize[%d] query[%s]", request_msg_size, request_msg);

	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	strncpy(send_msg.msg, request_msg, request_msg_size);
	send_msg.uid = uid;

	/*Create Socket*/
#ifdef _USE_UDS_SOCKET_
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sockfd, port);
#else
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_UDP, MS_TIMEOUT_SEC_10, &sockfd);
#endif
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	ret = ms_ipc_send_msg_to_server(sockfd, port, &send_msg, &serv_addr);
	if (ret != MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("ms_ipc_send_msg_to_server failed : %d", ret);
		close(sockfd);
		return ret;
	}


	/*Receive Response*/
	ms_comm_msg_s recv_msg;
	serv_addr_len = sizeof(serv_addr);
	memset(&recv_msg, 0x0, sizeof(ms_comm_msg_s));

	/* connected socket*/
	err = ms_ipc_wait_message(sockfd, &recv_msg, sizeof(recv_msg), &serv_addr, NULL,TRUE);
	if (err != MS_MEDIA_ERR_NONE) {
		ret = err;
	} else {
		MSAPI_DBG("RECEIVE OK [%d]", recv_msg.result);
		ret = recv_msg.result;
	}

	close(sockfd);

	return ret;
}

static int g_tcp_client_sock = -1;

static int __media_db_get_client_tcp_sock()
{
	return g_tcp_client_sock;
}

#ifdef _USE_UDS_SOCKET_
extern char MEDIA_IPC_PATH[][70];
#elif defined(_USE_UDS_SOCKET_TCP_)
extern char MEDIA_IPC_PATH[][50];
#endif

static int __media_db_prepare_tcp_client_socket()
{
	int ret = MS_MEDIA_ERR_NONE;
	int sockfd = -1;
#ifdef _USE_UDS_SOCKET_
	struct sockaddr_un serv_addr;
#elif defined(_USE_UDS_SOCKET_TCP_)
	struct sockaddr_un serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	int port = MS_DB_BATCH_UPDATE_PORT;

	/*Create TCP Socket*/
#ifdef _USE_UDS_SOCKET_
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd, 0);
#elif defined(_USE_UDS_SOCKET_TCP_)
	ret = ms_ipc_create_client_tcp_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd, MS_DB_BATCH_UPDATE_TCP_PORT);
#else
	ret = ms_ipc_create_client_socket(MS_PROTOCOL_TCP, MS_TIMEOUT_SEC_10, &sockfd);
#endif
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	/*Set server Address*/
	memset(&serv_addr, 0, sizeof(serv_addr));
#ifdef _USE_UDS_SOCKET_
	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[port]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[port]);
#elif defined(_USE_UDS_SOCKET_TCP_)
	serv_addr.sun_family = AF_UNIX;
	MSAPI_DBG("%s", MEDIA_IPC_PATH[MS_DB_BATCH_UPDATE_TCP_PORT]);
	strcpy(serv_addr.sun_path, MEDIA_IPC_PATH[MS_DB_BATCH_UPDATE_TCP_PORT]);
#else
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serv_addr.sin_port = htons(port);
#endif

	/* Connecting to the media db server */
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		MSAPI_DBG_ERR("connect error : %s", strerror(errno));
		close(sockfd);
		return MS_MEDIA_ERR_SOCKET_CONN;
	}

	g_tcp_client_sock = sockfd;

	MSAPI_DBG("Connected successfully");

	return 0;
}

static int __media_db_close_tcp_client_socket()
{
	close(g_tcp_client_sock);
	g_tcp_client_sock = -1;

	return 0;
}

static int __media_db_request_batch_update(ms_msg_type_e msg_type, const char *request_msg, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	int request_msg_size = 0;
	int sockfd = -1;

	if(!MS_STRING_VALID(request_msg))
	{
		MSAPI_DBG_ERR("invalid query");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	request_msg_size = strlen(request_msg);
	if(request_msg_size >= MAX_MSG_SIZE)
	{
		MSAPI_DBG_ERR("Query is Too long. [%d] query size limit is [%d]", request_msg_size, MAX_MSG_SIZE);
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	MSAPI_DBG("querysize[%d] query[%s]", request_msg_size, request_msg);
	ms_comm_msg_s send_msg;
	memset((void *)&send_msg, 0, sizeof(ms_comm_msg_s));

	send_msg.msg_type = msg_type;
	send_msg.msg_size = request_msg_size;
	send_msg.uid = uid;
	strncpy(send_msg.msg, request_msg, request_msg_size);

	sockfd = __media_db_get_client_tcp_sock();
	if (sockfd <= 0) {
		return  MS_MEDIA_ERR_SOCKET_CONN;
	}

	/* Send request */
	if (send(sockfd, &send_msg, sizeof(send_msg), 0) != sizeof(send_msg)) {
		MSAPI_DBG_ERR("send failed : %s", strerror(errno));
		__media_db_close_tcp_client_socket(sockfd);
		return MS_MEDIA_ERR_SOCKET_SEND;
	} else {
		MSAPI_DBG("Sent successfully");
	}

	/*Receive Response*/
	int recv_msg_size = -1;
	int recv_msg = -1;
	if ((recv_msg_size = recv(sockfd, &recv_msg, sizeof(recv_msg), 0)) < 0) {
		MSAPI_DBG_ERR("recv failed : %s", strerror(errno));

		__media_db_close_tcp_client_socket(sockfd);
		if (errno == EWOULDBLOCK) {
			MSAPI_DBG_ERR("Timeout. Can't try any more");
			return MS_MEDIA_ERR_SOCKET_RECEIVE_TIMEOUT;
		} else {
			MSAPI_DBG_ERR("recv failed : %s", strerror(errno));
			return MS_MEDIA_ERR_SOCKET_RECEIVE;
		}
	}

	MSAPI_DBG("RECEIVE OK [%d]", recv_msg);
	ret = recv_msg;

	return ret;
}

int media_db_connect(MediaDBHandle **handle, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;
	sqlite3 * db_handle = NULL;

	MSAPI_DBG_FUNC();

	ret = __media_db_connect_db_with_handle(&db_handle,uid);
	MSAPI_RETV_IF(ret != MS_MEDIA_ERR_NONE, ret);

	*handle = db_handle;
	return MS_MEDIA_ERR_NONE;
}

int media_db_disconnect(MediaDBHandle *handle)
{
	sqlite3 * db_handle = (sqlite3 *)handle;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(db_handle == NULL, MS_MEDIA_ERR_INVALID_PARAMETER, "Handle is NULL");

	return __media_db_disconnect_db_with_handle(db_handle);
}

int media_db_request_update_db(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_update(MS_MSG_DB_UPDATE, query_str ,uid);

	return ret;
}

int media_db_request_update_db_batch_start(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_prepare_tcp_client_socket();

	if (ret < MS_MEDIA_ERR_NONE) {
		MSAPI_DBG_ERR("__media_db_prepare_tcp_client_socket failed : %d", ret);
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_START, query_str, uid);

	return ret;
}

int media_db_request_update_db_batch(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH, query_str, uid);

	return ret;
}

int media_db_request_update_db_batch_end(const char *query_str, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_DBG_FUNC();

	if (!MS_STRING_VALID(query_str)) {
		MSAPI_DBG_ERR("Invalid Query");
		__media_db_close_tcp_client_socket();
		return ret;
	}

	ret = __media_db_request_batch_update(MS_MSG_DB_UPDATE_BATCH_END, query_str, uid);

	__media_db_close_tcp_client_socket();

	return ret;
}

int media_db_request_directory_scan(const char *directory_path, uid_t uid)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(directory_path), MS_MEDIA_ERR_INVALID_PARAMETER, "Directory Path is NULL");

	ret = __media_db_request_update(MS_MSG_DIRECTORY_SCANNING, directory_path, uid);

	return ret;
}

int media_db_update_db(MediaDBHandle *handle, const char *query_str)
{
	sqlite3 * db_handle = (sqlite3 *)handle;
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(db_handle == NULL, MS_MEDIA_ERR_INVALID_PARAMETER, "Handle is NULL");
	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	ret = __media_db_update_directly(db_handle, query_str);

	return ret;
}

int media_db_update_db_batch_start(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	if (g_list_idx != 0) {
		MSAPI_DBG_ERR("Current idx is not 0");
		ret = MS_MEDIA_ERR_DB_SERVER_BUSY_FAIL;
	} else {
		sql_list = (char**)malloc(sizeof(char*));
		MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");
		sql_list[g_list_idx++] = strdup(query_str);
		MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");
	}

	return ret;
}

int media_db_update_db_batch(const char *query_str)
{
	int ret = MS_MEDIA_ERR_NONE;

	MSAPI_RETVM_IF(!MS_STRING_VALID(query_str), MS_MEDIA_ERR_INVALID_PARAMETER, "Invalid Query");

	sql_list = (char**)realloc(sql_list, (g_list_idx + 1) * sizeof(char*));
	MSAPI_RETVM_IF(sql_list == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");

	sql_list[g_list_idx++] = strdup(query_str);
	MSAPI_RETVM_IF(sql_list[g_list_idx - 1] == NULL, MS_MEDIA_ERR_OUT_OF_MEMORY, "Out of memory");

	return ret;
}

int media_db_update_db_batch_end(MediaDBHandle *handle, const char *query_str)
{
	sqlite3 * db_handle = (sqlite3 *)handle;
	int ret = MS_MEDIA_ERR_NONE;

	if (db_handle == NULL || (!MS_STRING_VALID(query_str))) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Handle is NULL");
		return MS_MEDIA_ERR_INVALID_PARAMETER;
	}

	sql_list = (char**)realloc(sql_list, (g_list_idx + 1) * sizeof(char*));
	if (sql_list == NULL) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Out of memory");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	sql_list[g_list_idx++] = strdup(query_str);
	if (sql_list[g_list_idx - 1] == NULL) {
		__media_db_destroy_sql_list();
		MSAPI_DBG_ERR("Out of memory");
		return MS_MEDIA_ERR_OUT_OF_MEMORY;
	}

	int i = 0;
	char *current_sql = NULL;
	for (i = 0; i < g_list_idx; i++) {
		current_sql = sql_list[i];
		ret = __media_db_update_directly(db_handle, current_sql);
		if (ret < 0) {
			if (i == 0) {
				/* This is fail of "BEGIN" */
				MSAPI_DBG_ERR("Query failed : %s", current_sql);
				break;
			} else if (i == g_list_idx - 1) {
				/* This is fail of "COMMIT" */
				MSAPI_DBG_ERR("Query failed : %s", current_sql);
				break;
			} else {
				MSAPI_DBG_ERR("Query failed : %s, but keep going to run remaining queries", current_sql);
			}
		}
	}

	__media_db_destroy_sql_list();

	return ret;
}

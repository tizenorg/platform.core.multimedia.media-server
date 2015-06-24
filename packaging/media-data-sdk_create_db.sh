#!/bin/sh

source /etc/tizen-platform.conf

mkdir -p $TZ_SYS_DB
mkdir -p  $TZ_SYS_DATA

#Create DB file
sqlite3 ${TZ_SYS_DB}/.media.db 'PRAGMA journal_mode = PERSIST;

    CREATE TABLE IF NOT EXISTS album (album_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, artist TEXT, album_art TEXT);
    CREATE TABLE IF NOT EXISTS bookmark (bookmark_id INTEGER PRIMARY KEY AUTOINCREMENT, media_uuid TEXT NOT NULL, marked_time INTEGER DEFAULT 0, thumbnail_path TEXT, unique(media_uuid, marked_time));
    CREATE TABLE IF NOT EXISTS folder (folder_uuid TEXT PRIMARY KEY, path TEXT NOT NULL, name TEXT NOT NULL, modified_time INTEGER DEFAULT 0, name_pinyin TEXT, storage_type INTEGER, storage_uuid TEXT, folder_order INTEGER DEFAULT 0, parent_folder_uuid TEXT, validity INTEGER DEFAULT 1, unique(path, name));
    CREATE TABLE IF NOT EXISTS media (media_uuid TEXT PRIMARY KEY, path TEXT NOT NULL UNIQUE, file_name TEXT NOT NULL, media_type INTEGER, mime_type TEXT, size INTEGER DEFAULT 0, added_time INTEGER DEFAULT 0, modified_time INTEGER DEFAULT 0, folder_uuid TEXT NOT NULL, thumbnail_path TEXT, title TEXT, album_id INTEGER DEFAULT 0, album TEXT, artist TEXT, album_artist TEXT, genre TEXT, composer TEXT, year TEXT, recorded_date TEXT, copyright TEXT, track_num TEXT, description TEXT, bitrate INTEGER DEFAULT -1, bitpersample INTEGER DEFAULT 0, samplerate INTEGER DEFAULT -1, channel INTEGER DEFAULT -1, duration INTEGER DEFAULT -1, longitude DOUBLE DEFAULT 0, latitude DOUBLE DEFAULT 0, altitude DOUBLE DEFAULT 0, exposure_time TEXT, fnumber DOUBLE DEFAULT 0, iso INTEGER DEFAULT -1, model TEXT, width INTEGER DEFAULT -1, height INTEGER DEFAULT -1, datetaken TEXT, orientation INTEGER DEFAULT -1, burst_id TEXT, played_count INTEGER DEFAULT 0, last_played_time INTEGER DEFAULT 0, last_played_position INTEGER DEFAULT 0, rating INTEGER DEFAULT 0, favourite INTEGER DEFAULT 0, author TEXT, provider TEXT, content_name TEXT, category TEXT, location_tag TEXT, age_rating TEXT, keyword TEXT, is_drm INTEGER DEFAULT 0, storage_type INTEGER, timeline INTEGER DEFAULT 0, weather TEXT, sync_status INTEGER DEFAULT 0, file_name_pinyin TEXT, title_pinyin TEXT, album_pinyin TEXT, artist_pinyin TEXT, album_artist_pinyin TEXT, genre_pinyin TEXT, composer_pinyin TEXT, copyright_pinyin TEXT, description_pinyin TEXT, author_pinyin TEXT, provider_pinyin TEXT, content_name_pinyin TEXT, category_pinyin TEXT, location_tag_pinyin TEXT, age_rating_pinyin TEXT, keyword_pinyin TEXT, storage_uuid TEXT, validity INTEGER DEFAULT 1, unique(path, file_name));
    CREATE TABLE IF NOT EXISTS playlist (playlist_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, name_pinyin TEXT, thumbnail_path TEXT);
    CREATE TABLE IF NOT EXISTS playlist_map (_id INTEGER PRIMARY KEY AUTOINCREMENT, playlist_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, play_order INTEGER NOT NULL);
    CREATE TABLE IF NOT EXISTS storage (storage_uuid TEXT PRIMARY KEY, storage_name TEXT, storage_path TEXT NOT NULL, storage_account TEXT, storage_type INTEGER DEFAULT 0, scan_status INTEGER DEFAULT 0, validity INTEGER DEFAULT 1, unique(storage_name, storage_path, storage_account));
    CREATE TABLE IF NOT EXISTS tag (tag_id  INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, name_pinyin TEXT);
    CREATE TABLE IF NOT EXISTS tag_map (_id INTEGER PRIMARY KEY AUTOINCREMENT, tag_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, unique(tag_id, media_uuid));
    CREATE VIEW IF NOT EXISTS media_view AS SELECT * from media;
    CREATE VIEW IF NOT EXISTS playlist_view AS SELECT playlist.playlist_id, playlist.name, playlist.thumbnail_path AS p_thumbnail_path, media_count IS NOT NULL AS media_count, playlist_map._id AS pm_id, playlist_map.play_order, media.media_uuid, media.path, media.file_name, media.media_type, media.mime_type, media.size, media.added_time, media.modified_time, media.thumbnail_path, media.title, media.album, media.artist, media.album_artist, media.genre, media.composer, media.year, media.recorded_date, media.copyright, media.track_num, media.description, media.bitrate, media.bitpersample, media.samplerate, media.channel, media.duration, media.longitude, media.latitude, media.altitude, media.exposure_time, media.fnumber, media.iso, media.model, media.width, media.height, media.datetaken, media.orientation, media.burst_id, media.played_count, media.last_played_time, media.last_played_position, media.rating, media.favourite, media.author, media.provider, media.content_name, media.category, media.location_tag, media.age_rating, media.keyword, media.is_drm, media.storage_type, media.timeline, media.weather, media.sync_status, media.storage_uuid FROM playlist                                      LEFT OUTER JOIN playlist_map ON playlist.playlist_id = playlist_map.playlist_id                 LEFT OUTER JOIN media ON (playlist_map.media_uuid = media.media_uuid AND media.validity=1)      LEFT OUTER JOIN (SELECT count(playlist_id) as media_count, playlist_id FROM playlist_map group by playlist_id) as cnt_tbl ON (cnt_tbl.playlist_id=playlist_map.playlist_id AND media.validity=1);
    CREATE VIEW IF NOT EXISTS tag_view AS SELECT tag.tag_id , tag.name, media_count IS NOT NULL AS media_count, tag_map._id AS tm_id, media.media_uuid, media.path, media.file_name, media.media_type, media.mime_type, media.size, media.added_time, media.modified_time, media.thumbnail_path, media.title, media.album, media.artist, media.album_artist, media.genre, media.composer, media.year, media.recorded_date, media.copyright, media.track_num, media.description, media.bitrate, media.bitpersample, media.samplerate, media.channel, media.duration, media.longitude, media.latitude, media.altitude, media.exposure_time, media.fnumber, media.iso, media.model, media.width, media.height, media.datetaken, media.orientation, media.burst_id, media.played_count, media.last_played_time, media.last_played_position, media.rating, media.favourite, media.author, media.provider, media.content_name, media.category, media.location_tag, media.age_rating, media.keyword, media.is_drm, media.storage_type, media.timeline, media.weather, media.sync_status, media.storage_uuid FROM tag                                     LEFT OUTER JOIN tag_map ON tag.tag_id=tag_map.tag_id                            LEFT OUTER JOIN media ON (tag_map.media_uuid = media.media_uuid AND media.validity=1)           LEFT OUTER JOIN (SELECT count(tag_id) as media_count, tag_id FROM tag_map group by tag_id) as cnt_tbl ON (cnt_tbl.tag_id=tag_map.tag_id AND media.validity=1);
    CREATE INDEX folder_uuid_idx on media (folder_uuid);
    CREATE INDEX media_album_idx on media (album);
    CREATE INDEX media_artist_idx on media (artist);
    CREATE INDEX media_author_idx on media (author);
    CREATE INDEX media_category_idx on media (category);
    CREATE INDEX media_composer_idx on media (composer);
    CREATE INDEX media_content_name_idx on media (content_name);
    CREATE INDEX media_file_name_idx on media (file_name);
    CREATE INDEX media_genre_idx on media (genre);
    CREATE INDEX media_location_tag_idx on media (location_tag);
    CREATE INDEX media_media_type_idx on media (media_type);
    CREATE INDEX media_modified_time_idx on media (modified_time);
    CREATE INDEX media_provider_idx on media (provider);
    CREATE INDEX media_timeline_idx on media (timeline);
    CREATE INDEX media_title_idx on media (title);
    CREATE TRIGGER album_cleanup DELETE ON media BEGIN DELETE FROM album WHERE (SELECT count(*) FROM media WHERE album_id=old.album_id)=1 AND album_id=old.album_id;END;
    CREATE TRIGGER bookmark_cleanup DELETE ON media BEGIN DELETE FROM bookmark WHERE media_uuid=old.media_uuid;END;
    CREATE TRIGGER playlist_map_cleanup DELETE ON media BEGIN DELETE FROM playlist_map WHERE media_uuid=old.media_uuid;END;
    CREATE TRIGGER playlist_map_cleanup_1 DELETE ON playlist BEGIN DELETE FROM playlist_map WHERE playlist_id=old.playlist_id;END;
    CREATE TRIGGER storage_folder_cleanup DELETE ON storage BEGIN DELETE FROM folder WHERE storage_uuid=old.storage_uuid;END;
    CREATE TRIGGER tag_map_cleanup DELETE ON media BEGIN DELETE FROM tag_map WHERE media_uuid=old.media_uuid;END;
    CREATE TRIGGER tag_map_cleanup_1 DELETE ON tag BEGIN DELETE FROM tag_map WHERE tag_id =old.tag_id ;END;
'

#Change permission
chmod 664 ${TZ_SYS_DB}/.media.db
chmod 664 ${TZ_SYS_DB}/.media.db-journal
chmod 666 ${TZ_SYS_DATA}/data-media/Images

#Change owner
chown -R :$TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/data-media/*

#Change group (6017: db_filemanager 5000: app)
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DB}
chgrp 6017 ${TZ_SYS_DB}/.media.db
chgrp 6017 ${TZ_SYS_DB}/.media.db-journal
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/file-manager-service/
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/file-manager-service/.thumb
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/file-manager-service/.thumb/phone
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/file-manager-service/.thumb/phone/.[a-z0-9]*.*
chgrp $TZ_SYS_USER_GROUP ${TZ_SYS_DATA}/file-manager-service/.thumb/mmc


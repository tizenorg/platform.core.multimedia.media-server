#!/bin/sh

source /etc/tizen-platform.conf

mkdir -p $TZ_SYS_DB
mkdir -p  $TZ_SYS_DATA

#Create DB file
sqlite3 ${TZ_SYS_DB}/.media.db 'PRAGMA journal_mode = PERSIST;

	CREATE TABLE album (album_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, artist TEXT, album_art TEXT);
    CREATE TABLE bookmark (bookmark_id INTEGER PRIMARY KEY AUTOINCREMENT, media_uuid TEXT NOT NULL, marked_time INTEGER DEFAULT 0, thumbnail_path TEXT, unique(media_uuid, marked_time));
    CREATE TABLE folder (folder_uuid TEXT PRIMARY KEY, path TEXT NOT NULL, name TEXT NOT NULL, modified_time INTEGER DEFAULT 0, name_pinyin TEXT, storage_type INTEGER, folder_order INTEGER DEFAULT 0, parent_folder_uuid TEXT, validity INTEGER DEFAULT 1, unique(path, name));
    CREATE TABLE media (media_uuid TEXT PRIMARY KEY, path TEXT NOT NULL UNIQUE, file_name TEXT NOT NULL, media_type INTEGER, mime_type TEXT, size INTEGER DEFAULT 0, added_time INTEGER DEFAULT 0, modified_time INTEGER DEFAULT 0, folder_uuid TEXT NOT NULL, thumbnail_path TEXT, title TEXT, album_id INTEGER DEFAULT 0, album TEXT, artist TEXT, album_artist TEXT, genre TEXT, composer TEXT, year TEXT, recorded_date TEXT, copyright TEXT, track_num TEXT, description TEXT, bitrate INTEGER DEFAULT -1, bitpersample INTEGER DEFAULT 0, samplerate INTEGER DEFAULT -1, channel INTEGER DEFAULT -1, duration INTEGER DEFAULT -1, longitude DOUBLE DEFAULT 0, latitude DOUBLE DEFAULT 0, altitude DOUBLE DEFAULT 0, exposure_time TEXT, fnumber DOUBLE DEFAULT 0, iso INTEGER DEFAULT -1, model TEXT, width INTEGER DEFAULT -1, height INTEGER DEFAULT -1, datetaken TEXT, orientation INTEGER DEFAULT -1, burst_id TEXT, played_count INTEGER DEFAULT 0, last_played_time INTEGER DEFAULT 0, last_played_position INTEGER DEFAULT 0, rating INTEGER DEFAULT 0, favourite INTEGER DEFAULT 0, author TEXT, provider TEXT, content_name TEXT, category TEXT, location_tag TEXT, age_rating TEXT, keyword TEXT, is_drm INTEGER DEFAULT 0, storage_type INTEGER, timeline INTEGER DEFAULT 0, weather TEXT, sync_status INTEGER DEFAULT 0, file_name_pinyin TEXT, title_pinyin TEXT, album_pinyin TEXT, artist_pinyin TEXT, album_artist_pinyin TEXT, genre_pinyin TEXT, composer_pinyin TEXT, copyright_pinyin TEXT, description_pinyin TEXT, author_pinyin TEXT, provider_pinyin TEXT, content_name_pinyin TEXT, category_pinyin TEXT, location_tag_pinyin TEXT, age_rating_pinyin TEXT, keyword_pinyin TEXT, storage_uuid TEXT, validity INTEGER DEFAULT 1, unique(path, file_name));
    CREATE TABLE playlist (playlist_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, name_pinyin TEXT, thumbnail_path TEXT);
    CREATE TABLE playlist_map (_id INTEGER PRIMARY KEY AUTOINCREMENT, playlist_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, play_order INTEGER NOT NULL);
    CREATE TABLE storage (storage_uuid TEXT PRIMARY KEY, storage_name TEXT, storage_path TEXT NOT NULL, storage_account TEXT, storage_type INTEGER DEFAULT 0, scan_status INTEGER DEFAULT 0, validity INTEGER DEFAULT 1, unique(storage_name, storage_path, storage_account));
    CREATE TABLE tag (tag_id  INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, name_pinyin TEXT);
    CREATE TABLE tag_map (_id INTEGER PRIMARY KEY AUTOINCREMENT, tag_id INTEGER NOT NULL, media_uuid TEXT NOT NULL, unique(tag_id, media_uuid));
    CREATE VIEW media_view AS SELECT * from media;
    CREATE VIEW playlist_view AS SELECT playlist.playlist_id, playlist.name, playlist.thumbnail_path AS p_thumbnail_path, media_count IS NOT NULL AS media_count, playlist_map._id AS pm_id, playlist_map.play_order, media.media_uuid, media.path, media.file_name, media.media_type, media.mime_type, media.size, media.added_time, media.modified_time, media.thumbnail_path, media.title, media.album, media.artist, media.album_artist, media.genre, media.composer, media.year, media.recorded_date, media.copyright, media.track_num, media.description, media.bitrate, media.bitpersample, media.samplerate, media.channel, media.duration, media.longitude, media.latitude, media.altitude, media.exposure_time, media.fnumber, media.iso, media.model, media.width, media.height, media.datetaken, media.orientation, media.burst_id, media.played_count, media.last_played_time, media.last_played_position, media.rating, media.favourite, media.author, media.provider, media.content_name, media.category, media.location_tag, media.age_rating, media.keyword, media.is_drm, media.storage_type, media.timeline, media.weather, media.sync_status, media.storage_uuid FROM playlist                                      LEFT OUTER JOIN playlist_map ON playlist.playlist_id = playlist_map.playlist_id                 LEFT OUTER JOIN media ON (playlist_map.media_uuid = media.media_uuid AND media.validity=1)      LEFT OUTER JOIN (SELECT count(playlist_id) as media_count, playlist_id FROM playlist_map group by playlist_id) as cnt_tbl ON (cnt_tbl.playlist_id=playlist_map.playlist_id AND media.validity=1);
    CREATE VIEW tag_view AS SELECT tag.tag_id , tag.name, media_count IS NOT NULL AS media_count, tag_map._id AS tm_id, media.media_uuid, media.path, media.file_name, media.media_type, media.mime_type, media.size, media.added_time, media.modified_time, media.thumbnail_path, media.title, media.album, media.artist, media.album_artist, media.genre, media.composer, media.year, media.recorded_date, media.copyright, media.track_num, media.description, media.bitrate, media.bitpersample, media.samplerate, media.channel, media.duration, media.longitude, media.latitude, media.altitude, media.exposure_time, media.fnumber, media.iso, media.model, media.width, media.height, media.datetaken, media.orientation, media.burst_id, media.played_count, media.last_played_time, media.last_played_position, media.rating, media.favourite, media.author, media.provider, media.content_name, media.category, media.location_tag, media.age_rating, media.keyword, media.is_drm, media.storage_type, media.timeline, media.weather, media.sync_status, media.storage_uuid FROM tag                                     LEFT OUTER JOIN tag_map ON tag.tag_id=tag_map.tag_id                            LEFT OUTER JOIN media ON (tag_map.media_uuid = media.media_uuid AND media.validity=1)           LEFT OUTER JOIN (SELECT count(tag_id) as media_count, tag_id FROM tag_map group by tag_id) as cnt_tbl ON (cnt_tbl.tag_id=tag_map.tag_id AND media.validity=1);
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
    INSERT INTO media VALUES("60aea677-4742-408e-b5f7-f2628062d06d","@TZ_SYS_DATA@/data-media/Images/Default.jpg","Default.jpg",0,"image/jpeg",632118,3023047,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-a19569ad296e9655d1fbf216f195f801.jpg","Default",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,720,280,"2011:10:20 18:41:26",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("84b3a64a-51ef-4b3a-bbaa-c527fbbcfa42","@TZ_SYS_DATA@/data-media/Images/data-media/Images/image1.jpg","image1.jpg",0,"image/jpeg",751750,3023268,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-f5052c8428c4a22231d6ece0c63b74bd.jpg","image1",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:08 15:59:47",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("51d282b7-06d7-4bce-aebc-59e46b15f7e7","@TZ_SYS_DATA@/data-media/Images/image13.jpg","image13.jpg",0,"image/jpeg",549310,3023473,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-825ded447a3ce04d14d737f93d7cee26.jpg","image13",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:59:11",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("dd6d7c0b-273d-47f5-8a37-30ebac9ac3a3","@TZ_SYS_DATA@/data-media/Images/image4.jpg","image4.jpg",0,"image/jpeg",609139,3023702,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-8ea059905f24eea065a7998dc5ff1f7e.jpg","image4",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","                               ",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:44:45",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("21141e5d-e6da-45e2-93d6-5fb5c5870adb","@TZ_SYS_DATA@/data-media/Images/image2.jpg","image2.jpg",0,"image/jpeg",254304,3023930,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-93d14e2e94dfbccc9f38a14c4be6a780.jpg","image2",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:08 14:54:05",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("2ec6f64b-c982-4c6a-9a70-1323d27f4aa5","@TZ_SYS_DATA@/data-media/Images/image9.jpg","image9.jpg",0,"image/jpeg",1168466,3024130,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-e82b0d23bfecbddaad1b98be7674b96e.jpg","image9",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2011:10:20 18:43:39",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("db1c184c-6f31-43b4-b924-8c00ac5b6197","@TZ_SYS_DATA@/data-media/Images/Home_default.jpg","Home_default.jpg",0,"image/jpeg",554116,3024507,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-66784c0b912f077f0a8de56a2f56161e.jpg","Home_default",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,720,1280,"2012:02:07 14:44:33",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("1b25f4d9-ffff-4b83-8dbf-187a0a809985","@TZ_SYS_DATA@/data-media/Images/image15.jpg","image15.jpg",0,"image/jpeg",484926,3024713,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-de79768105a730492b3b28ca33ff89f4.jpg","image15",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 15:00:59",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("b3ee9659-22bb-423d-99d0-ed35240545a8","@TZ_SYS_DATA@/data-media/Images/image12.jpg","image12.jpg",0,"image/jpeg",519167,3024908,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-e32fd6fd44abe296c14de2407bab1f93.jpg","image12",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:50:16",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("0d905e54-4583-4de8-80c1-ee4117e06ddf","@TZ_SYS_DATA@/data-media/Images/image11.jpg","image11.jpg",0,"image/jpeg",656364,3025097,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-773fffb8f086e5954f15407b41c2635d.jpg","image11",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,720,1280,"2012:02:07 14:49:37",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("bc1eae57-376e-4ff3-b6a0-edcff8c7a1b1","@TZ_SYS_DATA@/data-media/Images/image5.jpg","image5.jpg",0,"image/jpeg",773064,3025291,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-c468b6d8820bfc0d9311d76f6575251a.jpg","image5",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:45:14",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("eec305ed-ccb4-4abb-93f9-f087c823b1d4","@TZ_SYS_DATA@/data-media/Images/image6.jpg","image6.jpg",0,"image/jpeg",682883,3025500,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-10555c13cdfe5a763a69e08489de3c70.jpg","image6",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2011:10:20 18:39:26",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("53101826-d64e-4d26-b448-acc1d94511a5","@TZ_SYS_DATA@/data-media/Images/image14.jpg","image14.jpg",0,"image/jpeg",452386,3025742,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-6f7e6adae30603c45be7db083610d0a3.jpg","image14",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:59:44",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("dbf546ed-e984-45de-a85e-b1d750005036","@TZ_SYS_DATA@/data-media/Images/image8.jpg","image8.jpg",0,"image/jpeg",930876,3026032,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-5876701a15ec16bd0226ed00044cad92.jpg","image8",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2011:10:21 10:01:44",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("696835a1-eaec-4602-8310-5e7ad9f4429b","@TZ_SYS_DATA@/data-media/Images/image10.jpg","image10.jpg",0,"image/jpeg",1236980,3027125,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-70a952ceff9175115b8c3fd044cdf978.jpg","image10",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2011:10:20 18:43:24",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("4a76c208-ca95-4b2f-9de1-86c82a45506d","@TZ_SYS_DATA@/data-media/Images/image16.jpg","image16.jpg",0,"image/jpeg",1443416,3027413,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-aa5afe63b8aaa41079f9f37297d0763f.jpg","image16",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,720,1280,"2011:10:20 18:42:06",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("83b9fd32-cd53-4ba7-84b8-c3d507f4701d","@TZ_SYS_DATA@/data-media/Images/image7.jpg","image7.jpg",0,"image/jpeg",1073428,3027698,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-fc4557b53139ca8f35a3f13cea24ed13.jpg","image7",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,720,1280,"2011:10:20 18:41:51",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO media VALUES("a2b3cbf8-82e3-456f-b912-74b0a646da6e","@TZ_SYS_DATA@/data-media/Images/image3.jpg","image3.jpg",0,"image/jpeg",738597,3027977,1337008628,"baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/file-manager-service/.thumb/phone/.jpg-03bdfd7e4d43c736819639b84a590b5f.jpg","image3",0,"Unknown","Unknown","Unknown","Unknown","Unknown",NULL,"Unknown","Unknown","Unknown",0,0,0,0,-200.0,-200.0,-200.0,1280,720,"2012:02:07 14:46:13",1,NULL,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,1);
	INSERT INTO folder VALUES("baeb79e5-a9da-4667-aeaf-6b98830e4ce8","@TZ_SYS_DATA@/data-media/Images/","Images",1337008628,0);
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

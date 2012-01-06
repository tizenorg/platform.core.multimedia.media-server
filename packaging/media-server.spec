Name:       media-server
Summary:    Media Server.
Version:    0.1.18
Release:    1
Group:      utils
License:    Samsung
Source0:    %{name}-%{version}.tar.gz

Requires(post): /usr/bin/sqlite3
Requires(post): /usr/bin/vconftool

BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(drm-service)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(mm-fileinfo)
BuildRequires:  pkgconfig(libmedia-info)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(quickpanel)

%description
Media service server..

%package devel
Summary:   Media server headers and libraries
Group:     utils
Requires:  %{name} = %{version}-%{release}

%description devel
Media server headers and libraries (development)

%prep
%setup -q

%build

%autogen

LDFLAGS="$LDFLAGS -Wl,--rpath=%{prefix}/lib -Wl,--hash-style=both -Wl,--as-needed "; export LDFLAGS

%configure --prefix=/usr

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post

if  [ ! -f /opt/dbspace/.filemanager.db ]
then
	sqlite3 /opt/dbspace/.filemanager.db 'PRAGMA journal_mode = PERSIST;

	CREATE TABLE mmc_info (serial text, date text, manfid text);
'
fi

if  [ ! -f /opt/dbspace/.media.db ]
then
	sqlite3 /opt/dbspace/.media.db 'PRAGMA journal_mode = PERSIST;

	CREATE TABLE audio_media (audio_id integer primary key autoincrement, path text unique, basename text, thumbnail_path text, title text, title_key text, album text, album_id integer, artist text, artist_id integer, genre text, author text, year integer default -1, copyright text, description text, format text, bitrate integer default -1, track_num integer default -1, duration integer default -1, rating integer default 0, played_count integer default 0, last_played_time integer default -1, added_time integer, rated_time integer, album_rating integer default 0, modified_date integer default 0, size integer default 0, mime_type text, is_ringtone INTEGER default 0, is_music INTEGER default 1, is_alarm INTEGER default 0, is_notification INTEGER default 0, is_podcast INTEGER default 0, bookmark INTEGER default 0, category INTEGER default 0, valid integer default 0, folder_id integer default -1, storage_type integer, content_type integer default 3);
	INSERT INTO "audio_media" VALUES(1,"/opt/media/Sounds and music/Ringtones/Basic_Cuisine.mp3","Basic_Cuisine.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Cuisine","Cuisine","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,15804,0,0,0,1303362789,NULL,0,1300757280,253889,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(2,"/opt/media/Sounds and music/Ringtones/Global_Rhythm of the rain.mp3","Global_Rhythm of the rain.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Rhythm of the rain","Rhythm of the rain","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,36780,0,0,0,1303362789,NULL,0,1257745924,589696,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(3,"/opt/media/Sounds and music/Ringtones/Global_Imagine.mp3","Global_Imagine.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Imagine","Imagine","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,40620,0,0,0,1303362789,NULL,0,1257745880,651136,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(4,"/opt/media/Sounds and music/Ringtones/Global_Avemaria.mp3","Global_Avemaria.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Avemaria","Avemaria","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,40489,0,0,0,1303362789,NULL,0,1257745780,649088,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(5,"/opt/media/Sounds and music/Ringtones/Global_Sunshine.mp3","Global_Sunshine.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Sunshine","Sunshine","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,33280,0,0,0,1303362789,NULL,0,1257745966,534400,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(6,"/opt/media/Sounds and music/Ringtones/Global_Do it now.mp3","Global_Do it now.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Do it now","Do it now","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,37250,0,0,0,1303362789,NULL,0,1257759968,598016,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(7,"/opt/media/Sounds and music/Ringtones/Global_Brain wave.mp3","Global_Brain wave.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Brain wave","Brain wave","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,36022,0,0,0,1303362789,NULL,0,1257745594,577408,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(8,"/opt/media/Sounds and music/Ringtones/General_Crossingwalk.mp3","General_Crossingwalk.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Crossingwalk","Crossingwalk","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,13296,0,0,0,1303362789,NULL,0,1296193320,213765,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(9,"/opt/media/Sounds and music/Ringtones/General_One fine day.mp3","General_One fine day.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","One fine day","One fine day","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,0,14497,0,0,0,1303362790,NULL,0,1276820740,233611,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(10,"/opt/media/Sounds and music/Ringtones/Global_Vocalise.mp3","Global_Vocalise.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Vocalise","Vocalise","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,44617,0,0,0,1303362790,NULL,0,1257746032,714624,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(11,"/opt/media/Sounds and music/Ringtones/Basic_Basic tone.mp3","Basic_Basic tone.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Basic tone","Basic tone","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,3500,0,0,0,1303362790,NULL,0,1271843334,57216,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(12,"/opt/media/Sounds and music/Ringtones/Basic_Minimal tone.mp3","Basic_Minimal tone.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Minimal tone","Minimal tone","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,5355,0,0,0,1303362790,NULL,0,1301545764,86706,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(13,"/opt/media/Sounds and music/Ringtones/General_Emotive sensation.mp3","General_Emotive sensation.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Emotive sensation","Emotive sensation","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,19226,0,0,0,1303362790,NULL,0,1300950722,308642,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(14,"/opt/media/Sounds and music/Ringtones/General_Just spinning.mp3","General_Just spinning.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Just spinning","Just spinning","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,19356,0,0,0,1303362790,NULL,0,1300950870,310919,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(15,"/opt/media/Sounds and music/Ringtones/Global_You'\''re my love song.mp3","Global_You'\''re my love song.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","You'\''re my love song","You'\''re my love song","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,32104,0,0,0,1303362790,NULL,0,1257746058,513920,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(16,"/opt/media/Sounds and music/Ringtones/General_City drivin.mp3","General_City drivin.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","City drivin","City drivin","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,18076,0,0,0,1303362790,NULL,0,1300692200,290857,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(17,"/opt/media/Sounds and music/Ringtones/Basic_Single tone.mp3","Basic_Single tone.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Single tone","Single tone","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","80kbps 44.1kHz 1ch",80000,-1,3683,0,0,0,1303362790,NULL,0,1300950830,38849,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(18,"/opt/media/Sounds and music/Ringtones/Basic_Popple tone.mp3","Basic_Popple tone.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Popple tone","Popple tone","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,16613,0,0,0,1303362790,NULL,0,1300757442,267870,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(19,"/opt/media/Sounds and music/Ringtones/Global_Ringing to you.mp3","Global_Ringing to you.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Ringing to you","Ringing to you","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,31895,0,0,0,1303362790,NULL,0,1272851802,513253,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(20,"/opt/media/Sounds and music/Ringtones/Global_Tell me.mp3","Global_Tell me.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Tell me","Tell me","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,32052,0,0,0,1303362790,NULL,0,1257745984,513920,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(21,"/opt/media/Sounds and music/Ringtones/Basic_Crossing tone.mp3","Basic_Crossing tone.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Crossing tone","Crossing tone","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,7314,0,0,0,1303362790,NULL,0,1300757250,118808,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(22,"/opt/media/Sounds and music/Ringtones/Global_The secret only 4U.mp3","Global_The secret only 4U.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","The secret only 4U","The secret only 4U","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,41482,0,0,0,1303362790,NULL,0,1257746014,665472,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(23,"/opt/media/Sounds and music/Ringtones/Global_On my mind.mp3","Global_On my mind.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","On my mind","On my mind","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,29675,0,0,0,1303362790,NULL,0,1298452646,475008,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(24,"/opt/media/Sounds and music/Ringtones/General_Ring a ring.mp3","General_Ring a ring.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Ring a ring","Ring a ring","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,20062,0,0,0,1303362790,NULL,0,1300757470,322017,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(25,"/opt/media/Sounds and music/Ringtones/Global_Drawing the night.mp3","Global_Drawing the night.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Drawing the night","Drawing the night","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,32078,0,0,0,1303362790,NULL,0,1257745832,513920,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(26,"/opt/media/Sounds and music/Ringtones/Global_Anymore.mp3","Global_Anymore.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Anymore","Anymore","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,32156,0,0,0,1303362790,NULL,0,1257745756,516096,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(27,"/opt/media/Sounds and music/Ringtones/Global_Hypnotize.mp3","Global_Hypnotize.mp3","","Hypnotize","Hypnotize","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,49319,0,0,0,1303362791,NULL,0,1257745856,790400,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(28,"/opt/media/Sounds and music/Ringtones/General_Digital cloud.mp3","General_Digital cloud.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Digital cloud","Digital cloud","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,20297,0,0,0,1303362791,NULL,0,1300692232,326802,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(29,"/opt/media/Sounds and music/Ringtones/General_Mis en scene.mp3","General_Mis en scene.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Mis en scene","Mis en scene","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,30667,0,0,0,1303362791,NULL,0,1300757424,491708,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(30,"/opt/media/Sounds and music/Ringtones/General_Reverie.mp3","General_Reverie.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Reverie","Reverie","Samsung",1,"Samsung",1,"Ringtone","Unknown",-1,"","","128kbps 44.1kHz 2ch",128000,-1,25391,0,0,0,1303362791,NULL,0,1300757454,407280,"audio/mpeg",0,1,0,0,0,0,1,1,1,0,3);
	INSERT INTO "audio_media" VALUES(31,"/opt/media/Sounds and music/Music/Over the horizon.mp3","Over the horizon.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Over the horizon","Over the horizon","Samsung",1,"Samsung",1,"Rock","Unknown",2011,"","Samsung","128kbps 44.1kHz 2ch",128000,-1,185521,0,0,0,1303362791,NULL,0,1303267153,3179820,"audio/mpeg",0,1,0,0,0,0,0,1,2,0,3);
	INSERT INTO "audio_media" VALUES(32,"/opt/media/Sounds and music/Alerts/Glittering Light.mp3","Glittering Light.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Glittering light","Glittering light","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 2ch",96000,-1,2560,0,0,0,1303362791,NULL,0,1298442334,31744,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(33,"/opt/media/Sounds and music/Alerts/Bubbles.mp3","Bubbles.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Bubbles","Bubbles","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,574,0,0,0,1303362791,NULL,0,1298442388,8944,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(34,"/opt/media/Sounds and music/Alerts/Chirp Chrip.mp3","Chirp Chrip.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Chirp Chirp","Chirp Chirp","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,1253,0,0,0,1303362791,NULL,0,1298540588,17095,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(35,"/opt/media/Sounds and music/Alerts/Pianissimo.mp3","Pianissimo.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Pianissimo","Pianissimo","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,3474,0,0,0,1303362791,NULL,0,1298442184,43739,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(36,"/opt/media/Sounds and music/Alerts/Cloud.mp3","Cloud.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Cloud","Cloud","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,1933,0,0,0,1303362791,NULL,0,1282182420,24448,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(37,"/opt/media/Sounds and music/Alerts/Charming bell.mp3","Charming bell.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Charming bell","Charming bell","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,2481,0,0,0,1303362791,NULL,0,1298442370,31828,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(38,"/opt/media/Sounds and music/Alerts/Good News.mp3","Good News.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Good news","Good news","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 2ch",96000,-1,1645,0,0,0,1303362791,NULL,0,1298442316,20773,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(39,"/opt/media/Sounds and music/Alerts/Knock.mp3","Knock.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Knock","Knock","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,1018,0,0,0,1303362791,NULL,0,1298442242,14273,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(40,"/opt/media/Sounds and music/Alerts/Transparent piano.mp3","Transparent piano.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Transparent piano","Transparent piano","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,3160,0,0,0,1303362791,NULL,0,1300842836,38912,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(41,"/opt/media/Sounds and music/Alerts/Stepping stones.mp3","Stepping stones.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Stepping stones","Stepping stones","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,1854,0,0,0,1303362791,NULL,0,1282182694,22528,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(42,"/opt/media/Sounds and music/Alerts/Harmonics.mp3","Harmonics.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Harmonics","Harmonics","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,4597,0,0,0,1303362792,NULL,0,1298442284,57219,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(43,"/opt/media/Sounds and music/Alerts/A toy watch.mp3","A toy watch.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","A toy watch","A toy watch","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,3030,0,0,0,1303362792,NULL,0,1282181478,36736,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(44,"/opt/media/Sounds and music/Alerts/Good morning.mp3","Good morning.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Good morning","Good morning","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,5694,0,0,0,1303362792,NULL,0,1282181594,69632,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(45,"/opt/media/Sounds and music/Alerts/Opener.mp3","Opener.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Opener","Opener","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,1671,0,0,0,1303362792,NULL,0,1298442202,22110,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(46,"/opt/media/Sounds and music/Alerts/Starry night.mp3","Starry night.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Starry night","Starry night","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,5015,0,0,0,1303362792,NULL,0,1298442082,62234,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(47,"/opt/media/Sounds and music/Alerts/On time.mp3","On time.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","On time","On time","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,2560,0,0,0,1303362792,NULL,0,1282182136,32640,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(48,"/opt/media/Sounds and music/Alerts/Postman.mp3","Postman.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Postman","Postman","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,2220,0,0,0,1303362792,NULL,0,1298442162,28693,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(49,"/opt/media/Sounds and music/Alerts/Pure Bell.mp3","Pure Bell.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Pure bell","Pure bell","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 2ch",96000,0,2063,0,0,0,1303362792,NULL,0,1298442768,25916,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(50,"/opt/media/Sounds and music/Alerts/Haze.mp3","Haze.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Haze","Haze","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","Samsung","96kbps 44.1kHz 1ch",96000,-1,1071,0,0,0,1303362792,NULL,0,1282182480,14208,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	INSERT INTO "audio_media" VALUES(51,"/opt/media/Sounds and music/Alerts/Sherbet.mp3","Sherbet.mp3","/opt/data/file-manager-service/.thumb/phone/.mp3-2b1eb83d4d987ad847b9d56d044321a4.png","Sherbet","Sherbet","Samsung",1,"Samsung",1,"Alert tone","Unknown",-1,"","","96kbps 44.1kHz 1ch",96000,-1,1436,0,0,0,1303362792,NULL,0,1298442114,19289,"audio/mpeg",0,1,0,0,0,0,1,1,3,0,3);
	
	CREATE TABLE audio_folder (_id integer primary key autoincrement, path text, folder_name text, storage_type integer);
	INSERT INTO "audio_folder" VALUES(1,"/opt/media/Sounds and music/Ringtones","Ringtones",0);
	INSERT INTO "audio_folder" VALUES(2,"/opt/media/Sounds and music/Music","Music",0);
	INSERT INTO "audio_folder" VALUES(3,"/opt/media/Sounds and music/Alerts","Alerts",0);

	CREATE TABLE album_art (album_id INTEGER PRIMARY KEY, _data TEXT);
	INSERT INTO "album_art" VALUES(1,"");

	CREATE TABLE albums (album_id INTEGER PRIMARY KEY AUTOINCREMENT, album_key TEXT NOT NULL UNIQUE, album TEXT NOT NULL);
	INSERT INTO "albums" VALUES(1,"Samsung","Samsung");

	CREATE TABLE artists (artist_id INTEGER PRIMARY KEY AUTOINCREMENT,artist_key TEXT NOT NULL UNIQUE,artist TEXT NOT NULL);
	INSERT INTO "artists" VALUES(1,"Samsung","Samsung");

	CREATE TABLE audio_genres (_id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL);
	INSERT INTO "audio_genres" VALUES(1,"Ringtone");
	INSERT INTO "audio_genres" VALUES(2,"Rock");
	INSERT INTO "audio_genres" VALUES(3,"Alert tone");

	CREATE TABLE audio_genres_map (_id INTEGER PRIMARY KEY AUTOINCREMENT,audio_id INTEGER NOT NULL,genre_id INTEGER NOT NULL);
	INSERT INTO "audio_genres_map" VALUES(1,1,1);
	INSERT INTO "audio_genres_map" VALUES(2,2,1);
	INSERT INTO "audio_genres_map" VALUES(3,3,1);
	INSERT INTO "audio_genres_map" VALUES(4,4,1);
	INSERT INTO "audio_genres_map" VALUES(5,5,1);
	INSERT INTO "audio_genres_map" VALUES(6,6,1);
	INSERT INTO "audio_genres_map" VALUES(7,7,1);
	INSERT INTO "audio_genres_map" VALUES(8,8,1);
	INSERT INTO "audio_genres_map" VALUES(9,9,1);
	INSERT INTO "audio_genres_map" VALUES(10,10,1);
	INSERT INTO "audio_genres_map" VALUES(11,11,1);
	INSERT INTO "audio_genres_map" VALUES(12,12,1);
	INSERT INTO "audio_genres_map" VALUES(13,13,1);
	INSERT INTO "audio_genres_map" VALUES(14,14,1);
	INSERT INTO "audio_genres_map" VALUES(15,15,1);
	INSERT INTO "audio_genres_map" VALUES(16,16,1);
	INSERT INTO "audio_genres_map" VALUES(17,17,1);
	INSERT INTO "audio_genres_map" VALUES(18,18,1);
	INSERT INTO "audio_genres_map" VALUES(19,19,1);
	INSERT INTO "audio_genres_map" VALUES(20,20,1);
	INSERT INTO "audio_genres_map" VALUES(21,21,1);
	INSERT INTO "audio_genres_map" VALUES(22,22,1);
	INSERT INTO "audio_genres_map" VALUES(23,23,1);
	INSERT INTO "audio_genres_map" VALUES(24,24,1);
	INSERT INTO "audio_genres_map" VALUES(25,25,1);
	INSERT INTO "audio_genres_map" VALUES(26,26,1);
	INSERT INTO "audio_genres_map" VALUES(27,27,1);
	INSERT INTO "audio_genres_map" VALUES(28,28,1);
	INSERT INTO "audio_genres_map" VALUES(29,29,1);
	INSERT INTO "audio_genres_map" VALUES(30,30,1);
	INSERT INTO "audio_genres_map" VALUES(31,31,2);
	INSERT INTO "audio_genres_map" VALUES(32,32,3);
	INSERT INTO "audio_genres_map" VALUES(33,33,3);
	INSERT INTO "audio_genres_map" VALUES(34,34,3);
	INSERT INTO "audio_genres_map" VALUES(35,35,3);
	INSERT INTO "audio_genres_map" VALUES(36,36,3);
	INSERT INTO "audio_genres_map" VALUES(37,37,3);
	INSERT INTO "audio_genres_map" VALUES(38,38,3);
	INSERT INTO "audio_genres_map" VALUES(39,39,3);
	INSERT INTO "audio_genres_map" VALUES(40,40,3);
	INSERT INTO "audio_genres_map" VALUES(41,41,3);
	INSERT INTO "audio_genres_map" VALUES(42,42,3);
	INSERT INTO "audio_genres_map" VALUES(43,43,3);
	INSERT INTO "audio_genres_map" VALUES(44,44,3);
	INSERT INTO "audio_genres_map" VALUES(45,45,3);
	INSERT INTO "audio_genres_map" VALUES(46,46,3);
	INSERT INTO "audio_genres_map" VALUES(47,47,3);
	INSERT INTO "audio_genres_map" VALUES(48,48,3);
	INSERT INTO "audio_genres_map" VALUES(49,49,3);
	INSERT INTO "audio_genres_map" VALUES(50,50,3);
	INSERT INTO "audio_genres_map" VALUES(51,51,3);

	CREATE TABLE audio_playlists (_id integer primary key autoincrement, name text, thumbnail_uri TEXT, _data TEXT, date_added INTEGER, date_modified INTEGER);

	CREATE TABLE audio_playlists_map (_id integer primary key autoincrement, playlist_id integer, audio_id integer, play_order INTEGER);

	DELETE FROM sqlite_sequence;
	INSERT INTO "sqlite_sequence" VALUES("audio_folder",3);
	INSERT INTO "sqlite_sequence" VALUES("audio_genres",3);
	INSERT INTO "sqlite_sequence" VALUES("artists",1);
	INSERT INTO "sqlite_sequence" VALUES("albums",1);
	INSERT INTO "sqlite_sequence" VALUES("audio_media",51);
	INSERT INTO "sqlite_sequence" VALUES("audio_genres_map",51);

	CREATE INDEX titlekey_index on audio_media(title_key);
	CREATE INDEX albumkey_index on albums(album_key);
	CREATE TRIGGER albumart_cleanup1 DELETE ON albums BEGIN DELETE FROM album_art WHERE album_id = old.album_id;END;
	CREATE INDEX artistkey_index on artists(artist_key);
	CREATE TRIGGER audio_genres_cleanup DELETE ON audio_genres BEGIN DELETE FROM audio_genres_map WHERE genre_id = old._id;END;
	CREATE TRIGGER audio_playlists_map_cleanup_1 DELETE ON audio_media BEGIN DELETE FROM audio_playlists_map WHERE audio_id = old.audio_id;END;
	CREATE VIEW audio_meta AS SELECT audio_id AS _id, path AS _data, basename AS _display_name, size AS _size, mime_type, added_time AS date_added, modified_date, title, title_key, duration, artist_id, author AS composer, album_id, track_num, year, is_ringtone, is_music, is_alarm, is_notification, is_podcast, bookmark FROM audio_media where valid = 1;
	CREATE VIEW audio as SELECT * FROM audio_meta LEFT OUTER JOIN artists ON audio_meta.artist_id=artists.artist_id LEFT OUTER JOIN albums ON audio_meta.album_id=albums.album_id;
	CREATE VIEW album_info AS SELECT audio.album_id AS _id, album, album_key, MIN(year) AS minyear, MAX(year) AS maxyear, artist, artist_id, artist_key, count(*) AS numsongs,album_art._data AS album_art FROM audio LEFT OUTER JOIN album_art ON audio.album_id=album_art.album_id WHERE is_music=1 GROUP BY audio.album_id;
	CREATE VIEW artist_info AS SELECT artist_id AS _id, artist, artist_key, COUNT(DISTINCT album) AS number_of_albums, COUNT(*) AS number_of_tracks FROM audio WHERE is_music=1 GROUP BY artist_key;
	CREATE VIEW artists_albums_map AS SELECT DISTINCT artist_id, album_id FROM audio_meta;
	CREATE VIEW searchhelpertitle AS SELECT * FROM audio ORDER BY title_key;
	CREATE VIEW search AS SELECT _id,artist AS mime_type,artist,NULL AS album, NULL AS title,artist AS text1,NULL AS text2,number_of_albums AS data1, number_of_tracks AS data2,artist_key AS match, "content://media/external/audio/artists/"||_id AS suggest_intent_data, 1 AS grouporder FROM artist_info WHERE (artist!="Unknown") UNION ALL SELECT _id,album AS mime_type,artist,album,NULL AS title, album AS text1,artist AS text2,NULL AS data1,NULL AS data2, artist_key|| " " ||album_key AS match, "content://media/external/audio/albums/"||_id AS suggest_intent_data, 2 AS grouporder FROM album_info WHERE (album!="Unknown") UNION ALL SELECT searchhelpertitle._id AS _id,mime_type,artist,album,title, title AS text1,artist AS text2,NULL AS data1,NULL AS data2, artist_key||" "||album_key||" "||title_key AS match, "content://media/external/audio/media/"||searchhelpertitle._id AS suggest_intent_data, 3 AS grouporder FROM searchhelpertitle WHERE (title != "");

	CREATE TABLE visual_folder(_id INTEGER, path VARCHAR(256), folder_name VARCHAR(256), modified_date INT, web_account_id VARCHAR(256), storage_type INT, sns_type INT, lock_status INT, web_album_id VARCHAR(256), valid INT, primary key (path, folder_name, storage_type) );

	CREATE TABLE image_meta(_id INTEGER, media_id INT, longitude DOUBLE, latitude DOUBLE, description VARCHAR(256), width INT, height INT, orientation INT, datetaken INT, primary key (media_id) );

	CREATE TABLE visual_media(_id INTEGER, path VARCHAR(256), folder_id INT, display_name VARCHAR(256), content_type INT, rating INT, modified_date INT, thumbnail_path VARCHAR(256), http_url VARCHAR(256), valid INT, primary key (path, folder_id, display_name) );

	CREATE TABLE video_bookmark(_id INTEGER, media_id INT, marked_time INT, thumbnail_path VARCHAR(256), primary key ( media_id, marked_time) );

	CREATE TABLE video_meta(_id INTEGER, media_id INT, album VARCHAR(256), artist VARCHAR(256), title VARCHAR(256), description VARCHAR(256), youtube_category VARCHAR(256), last_played_time INT, duration INT, longitude DOUBLE, latitude DOUBLE, width INT, height INT, datetaken INT, primary key ( media_id) );

	CREATE TABLE web_streaming(_id INTEGER, folder_id INT, title VARCHAR(256), duration INT, url VARCHAR(256), thumb_path VARCHAR(256), primary key (_id) );

	CREATE TABLE visual_tag_map(_id INTEGER, media_id INT, tag_id INT, primary key ( media_id, tag_id) );
	CREATE TABLE visual_tag(_id INTEGER, tag_name VARCHAR(256), primary key ( tag_name) );

	INSERT INTO "visual_folder" VALUES(5000001,"/opt/media/Images and videos/My photo clips","My photo clips",1298917858,"",0,0,0,"",1);
	INSERT INTO "visual_folder" VALUES(5000002,"/opt/media/Images and videos/My video clips","My video clips",1298917858,"",0,0,0,"",1);
	INSERT INTO "visual_folder" VALUES(5000003,"/opt/media/Images and videos/Wallpapers","Wallpapers",1298917858,"",0,0,0,"",1);

	INSERT INTO "image_meta" VALUES(1,1,1000.0,1000.0,"No description", 480, 800, 1, 1275064496);
	INSERT INTO "image_meta" VALUES(2,2,1000.0,1000.0,"", 800, 480, 0, 0);
	INSERT INTO "image_meta" VALUES(3,3,1000.0,1000.0,"", 800, 480, 0, 0);
	INSERT INTO "image_meta" VALUES(4,4,1000.0,1000.0,"No description", 480, 800, 1, 1281973593);
	INSERT INTO "image_meta" VALUES(5,5,1000.0,1000.0,"No description", 480, 800, 1, 1281973895);
	INSERT INTO "image_meta" VALUES(6,7,1000.0,1000.0,"", 480, 800, 0, 0);
	INSERT INTO "image_meta" VALUES(7,8,1000.0,1000.0,"", 480, 800, 0, 0);

	INSERT INTO "visual_media" VALUES(1,"/opt/media/Images and videos/My photo clips/5_photo.JPG", 5000001,"5_photo.JPG",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.JPG-8ddcdba99df472b448908d21d13854ad.png","",1);
	INSERT INTO "visual_media" VALUES(2,"/opt/media/Images and videos/My photo clips/2_photo.jpg", 5000001,"2_photo.jpg",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.jpg-1bf63a4205303cb894a1564d603d85e8.png","",1);
	INSERT INTO "visual_media" VALUES(3,"/opt/media/Images and videos/My photo clips/1_photo.jpg", 5000001,"1_photo.jpg",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.jpg-d78db813538100657fe67bb739b391ce.png","",1);
	INSERT INTO "visual_media" VALUES(4,"/opt/media/Images and videos/My photo clips/4_photo.jpg", 5000001,"4_photo.jpg",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.jpg-4681c28ed65b83f59c5af7f84be4487a.png","",1);
	INSERT INTO "visual_media" VALUES(5,"/opt/media/Images and videos/My photo clips/3_photo.jpg", 5000001,"3_photo.jpg",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.jpg-871f45a1520706c878122ff456b67d1a.png","",1);
	INSERT INTO "visual_media" VALUES(6,"/opt/media/Images and videos/My video clips/Helicopter.mp4", 5000002,"Helicopter.mp4",2,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.mp4-0c401c836306fcb51ff1ffa0f6bd5289.png","",1);
	INSERT INTO "visual_media" VALUES(7,"/opt/media/Images and videos/Wallpapers/Default.png", 5000003,"Default.png",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.png-e860d4acbabab8388203316f9193bf1d.png","",1);
	INSERT INTO "visual_media" VALUES(8,"/opt/media/Images and videos/Wallpapers/Home_default.png", 5000003,"Home_default.png",1,0,1298917858,"/opt/data/file-manager-service/.thumb/phone/.png-7fb48e238a87f86506fc9670004327f4.png","",1);

	INSERT INTO "video_meta" VALUES(1, 6, "Unknown", "Unknown","Unknown", "Unknown","", 0, 99800, 1000.0, 1000.0, 640, 352, 0 );

	CREATE VIEW mediainfo_folder AS select _id, path, folder_name as name, storage_type from visual_folder where valid=1 union select _id, path, folder_name, storage_type       from audio_folder;

	CREATE VIEW item_view AS select _id as item_id, path as file_path, display_name,thumbnail_path as   thumbnail,modified_date as date_modified, valid,folder_id,content_type from visual_media where 1 union select  audio_id ,path ,title ,thumbnail_path, modified_date, valid,folder_id,content_type from audio_media where 1;'
fi

vconftool set -t int db/filemanager/dbupdate "1"
vconftool set -t int memory/filemanager/Mmc "0" -i

if  [ -f /opt/dbspace/.filemanager.db ]
then
	chown root:6017 /opt/dbspace/.filemanager.db
	chown root:6017 /opt/dbspace/.filemanager.db-journal
fi

if  [ -f /opt/dbspace/.media.db ]
then
	chown root:6017 /opt/dbspace/.media.db
        chown root:6017 /opt/dbspace/.media.db-journal
fi


%files
%defattr(-,root,root,-)
%{_bindir}/media-server
/etc/rc.d/init.d/mediasvr
/etc/rc.d/rc3.d/S60mediasvr
/etc/rc.d/rc5.d/S60mediasvr
/usr/lib/libmedia-server.so.0
/usr/lib/libmedia-server.so.0.0.0

%files devel
/usr/include/media-server/media-server-api.h
/usr/lib/libmedia-server.so
/usr/lib/pkgconfig/libmedia-server.pc

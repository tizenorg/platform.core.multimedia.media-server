ACLOCAL_AMFLAGS = -I m4

SUBDIRS = .

AM_CPPFLAGS = $(FMS_DEBUG_FLAGS)

AM_LDFLAGS=-Wl,--as-needed -Wl,--hash-style=both

### pkgconfig ###
pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = libmedia-utils.pc

nor_directory = /etc/rc.d/rc3.d
hib_directory = /etc/rc.d/rc5.d

install-data-hook:
	mkdir $(DESTDIR)/usr/local/bin/ -p
	cp -a $(CURDIR)/reset_mediadb.sh $(DESTDIR)/usr/local/bin/
	mkdir $(DESTDIR)$(nor_directory) -p
	ln -s ../init.d/mediasvr S99mediasvr
	mv ./S99mediasvr $(DESTDIR)$(nor_directory)
	mkdir $(DESTDIR)$(hib_directory) -p
	ln -s ../init.d/mediasvr S99mediasvr
	mv ./S99mediasvr $(DESTDIR)$(hib_directory)

# init.d script
fmsvcdir = /etc/rc.d/init.d
dist_fmsvc_DATA = ./data/mediasvr

### libmedia-utils.la ###
lib_LTLIBRARIES = libmedia-utils.la
libmedia_utils_la_SOURCES = lib/media-util-noti.c \
                            lib/media-util-ipc.c \
                            lib/media-util-db.c \
                            lib/media-util-register.c

libmedia_utils_la_CFLAGS = -I${srcdir}/lib/include \
                              $(GLIB_CFLAGS) \
			      $(DLOG_CFLAGS) \
			      $(DBUS_CFLAGS) \
			      $(SQLITE3_CFLAGS) \
			      $(DB_UTIL_CFLAGS) \
			      $(PHONESTATUS_CFLAGS)

libmedia_utils_la_LIBADD  =   $(GLIB_LIBS) \
                              $(DLOG_LIBS) \
                              $(DBUS_LIBS) \
			      $(SQLITE3_LIBS) \
			      $(DB_UTIL_LIBS) \
			      $(PHONESTATUS_LIBS)


### file-manager-server ###
bin_PROGRAMS = media-server \
		media-scanner \
		mediadb-update

media_server_SOURCES = common/media-server-dbus.c \
		       common/media-server-drm.c \
                       common/media-server-utils.c \
                       common/media-server-external-storage.c \
                       common/media-server-db-svc.c \
                       common/media-server-inotify-internal.c \
                       common/media-server-inotify.c \
                       common/media-server-db.c \
                       common/media-server-socket.c \
                       common/media-server-thumb.c \
                       common/media-server-scanner.c \
                       common/media-server-main.c 

media_server_CFLAGS = -I${srcdir}/common/include \
                      -I${srcdir}/lib/include \
                             $(GTHREAD_CFLAGS) \
                             $(GLIB_CFLAGS) \
                             $(PHONESTATUS_CFLAGS) \
                             $(DLOG_CFLAGS) \
                             $(DRM_SERVICE_CFLAGS) \
			     $(AUL_CFLAG)\
			     $(LIBPMCONTROL_CFLAGS) \
			     $(HEYNOTI_CFLAGS) \
			     $(DBUS_CFLAGS) \
                             $(STATUS_CFLAGS)

media_server_LDADD = libmedia-utils.la \
			      $(GLIB_LIBS) \
                              $(GTHREAD_LIBS) \
                              $(PHONESTATUS_LIBS) \
                              $(DLOG_LIBS) \
                              $(DRM_SERVICE_LIBS) \
			      $(AUL_LIBS) \
			      $(LIBPMCONTROL_LIBS) \
			      $(HEYNOTI_LIBS) \
			      $(DBUS_LIBS) \
                              -ldl \
                              $(STATUS_LIBS)

media_scanner_SOURCES = common/scanner/media-scanner-drm.c \
                       common/scanner/media-scanner-utils.c \
                       common/scanner/media-scanner-db-svc.c \
                       common/scanner/media-scanner-scan.c \
                       common/scanner/media-scanner-socket.c \
                       common/scanner/media-scanner.c

media_scanner_CFLAGS = -I${srcdir}/common/scanner/include \
                       -I${srcdir}/lib/include \
                       -I${srcdir}/common/include \
                             $(GTHREAD_CFLAGS) \
                             $(GLIB_CFLAGS) \
                             $(PHONESTATUS_CFLAGS) \
                             $(DLOG_CFLAGS) \
                             $(DRM_SERVICE_CFLAGS) \
                             $(AUL_CFLAG)\
                             $(LIBPMCONTROL_CFLAGS) \
                             $(HEYNOTI_CFLAGS) \
                             $(DBUS_CFLAGS)

media_scanner_LDADD = libmedia-utils.la \
                              $(GLIB_LIBS) \
                              $(GTHREAD_LIBS) \
                              $(PHONESTATUS_LIBS) \
                              $(DLOG_LIBS) \
                              $(DRM_SERVICE_LIBS) \
                              $(AUL_LIBS) \
                              $(LIBPMCONTROL_LIBS) \
                              $(THUMB_GEN_LIBS) \
                              $(HEYNOTI_LIBS) \
                              $(DBUS_LIBS) \
                              -ldl #this is for using dlsym

mediadb_update_SOURCES = common/mediadb-update.c

mediadb_update_CFLAGS = -I${srcdir}/lib/include \
                             $(GTHREAD_CFLAGS) \
                             $(GLIB_CFLAGS)

mediadb_update_LDADD = libmedia-utils.la \
                              $(GLIB_LIBS) \
                              $(GTHREAD_LIBS)

### includeheaders ###
includeheadersdir = $(includedir)/media-utils
includeheaders_HEADERS = lib/include/media-util-noti.h \
                         lib/include/media-util-register.h \
                         lib/include/media-util-err.h \
                         lib/include/media-util-db.h \
                         lib/include/media-util-ipc.h \
                         lib/include/media-util.h \
                         lib/include/media-server-ipc.h

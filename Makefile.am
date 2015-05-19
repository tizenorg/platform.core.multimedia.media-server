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
	mkdir $(DESTDIR)$(nor_directory) -p
	ln -s ../init.d/mediasvr S46mediasvr
	mv ./S46mediasvr $(DESTDIR)$(nor_directory)
	mkdir $(DESTDIR)$(hib_directory) -p
	ln -s ../init.d/mediasvr S46mediasvr
	mv ./S46mediasvr $(DESTDIR)$(hib_directory)

# init.d script
fmsvcdir = /etc/rc.d/init.d
dist_fmsvc_DATA = ./data/mediasvr

### libmedia-utils.la ###
lib_LTLIBRARIES = libmedia-utils.la
libmedia_utils_la_SOURCES = lib/media-util-noti.c \
                            lib/media-util-ipc.c \
                            lib/media-util-db.c \
                            lib/media-util-register.c \
                            lib/media-util-cynara.c

libmedia_utils_la_CFLAGS = -I${srcdir}/lib/include \
                              $(GLIB_CFLAGS) \
			      $(DLOG_CFLAGS) \
			      $(DBUS_CFLAGS) \
			      $(SQLITE3_CFLAGS) \
			      $(DB_UTIL_CFLAGS) \
			      $(PHONESTATUS_CFLAGS) \
			      $(TZ_PLATFORM_CONFIG_CFLAGS) \
			      $(LIBSMACK_CFLAGS) \
			      $(CYNARA_CLIENT_CFLAGS) \
			      $(CYNARA_SESSION_CFLAGS)

libmedia_utils_la_LIBADD  =   $(GLIB_LIBS) \
                              $(DLOG_LIBS) \
                              $(DBUS_LIBS) \
			      $(SQLITE3_LIBS) \
			      $(DB_UTIL_LIBS) \
			      $(PHONESTATUS_LIBS) \
			      $(TZ_PLATFORM_CONFIG_LIBS) \
			      $(LIBSMACK_LIBS) \
			      $(CYNARA_CLIENT_LIBS) \
			      $(CYNARA_SESSION_LIBS)


### file-manager-server ###
bin_PROGRAMS = media-server \
		media-scanner \
		mediadb-update

media_server_SOURCES = src/common/media-common-drm.c \
                       src/common/media-common-utils.c \
                       src/common/media-common-external-storage.c \
                       src/server/media-server-db-svc.c \
                       src/server/media-server-db.c \
                       src/server/media-server-socket.c \
                       src/server/media-server-thumb.c \
                       src/server/media-server-scanner.c \
                       src/server/media-server-main.c

media_server_CFLAGS = -I${srcdir}/src/common/include \
		      -I${srcdir}/src/server/include \
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

media_scanner_SOURCES = src/common/media-common-utils.c \
                	src/common/media-common-external-storage.c \
		       	src/common/media-common-drm.c \
                       	src/scanner/media-scanner-db-svc.c \
                       	src/scanner/media-scanner-scan.c \
                       	src/scanner/media-scanner-socket.c \
                       	src/scanner/media-scanner.c

media_scanner_CFLAGS = -I${srcdir}/lib/include \
                       -I${srcdir}/src/common/include \
		       -I${srcdir}/src/scanner/include \
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
                              -ldl \
			      $(STATUS_LIBS)

mediadb_update_SOURCES = src/mediadb-update.c

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
                         lib/include/media-util-cynara.h \
                         lib/include/media-util.h \
                         lib/include/media-server-ipc.h

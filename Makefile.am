ACLOCAL_AMFLAGS = -I m4

SUBDIRS = . 

AM_CPPFLAGS = $(FMS_DEBUG_FLAGS)

AM_LDFLAGS=-Wl,--as-needed --hash-style=both

### pkgconfig ###
pkgconfigdir = $(libdir)/pkgconfig
dist_pkgconfig_DATA = libmedia-utils.pc

nor_directory = /etc/rc.d/rc3.d
hib_directory = /etc/rc.d/rc5.d

install-data-hook:
	mkdir $(DESTDIR)$(nor_directory) -p
	ln -s ../init.d/mediasvr S48mediasvr
	mv ./S48mediasvr $(DESTDIR)$(nor_directory)
	mkdir $(DESTDIR)$(hib_directory) -p
	ln -s ../init.d/mediasvr S48mediasvr
	mv ./S48mediasvr $(DESTDIR)$(hib_directory)

# init.d script
fmsvcdir = /etc/rc.d/init.d
dist_fmsvc_DATA = ./data/mediasvr

### libmedia-utils.la ###
lib_LTLIBRARIES = libmedia-utils.la
libmedia_utils_la_SOURCES = lib/media-util-noti.c \
                            lib/media-util-register.c

libmedia_utils_la_CFLAGS = -I${srcdir}/lib/include \
			      $(MEDIA_INFO_CFLAGS) \
                              $(GLIB_CFLAGS) \
			      $(DLOG_CFLAGS)

libmedia_utils_la_LIBADD  =  $(MEDIA_INFO_LIBS) \
                              $(GLIB_LIBS) \
                              $(DLOG_LIBS)


### file-manager-server ###
bin_PROGRAMS = media-server

media_server_SOURCES = common/media-server-utils.c \
                       common/media-server-db-svc.c \
                       common/media-server-external-storage.c \
                       common/media-server-inotify-internal.c \
                       common/media-server-inotify.c \
                       common/media-server-scan-internal.c \
                       common/media-server-scan.c \
                       common/media-server-socket.c \
                       common/media-server-main.c 

media_server_CFLAGS = -I${srcdir}/common/include \
                      -I${srcdir}/lib/include \
                             $(GTHREAD_CFLAGS) \
                             $(GLIB_CFLAGS) \
			     $(MM_FILEINFO_CFLAGS) \
                             $(MEDIA_INFO_CFLAGS) \
                             $(PHONESTATUS_CFLAGS) \
                             $(UTIL_CFLAGS) \
                             $(DLOG_CFLAGS) \
                             $(DRM_SERVICE_CFLAGS) \
                             $(DB_UTIL_CFLAGS) \
			     $(AUL_CFLAG)\
			     $(DBUS_CFLAGS)\
			     $(LIBPMCONTROL_CFLAGS)\
                             $(THUMB_GEN_CFLAGS)
#                             $(LIBQUICKPANEL_CFLAGS)

media_server_LDADD = $(GLIB_LIBS) \
                              $(GTHREAD_LIBS) \
                              $(MM_FILEINFO_LIBS) \
			      $(MEDIA_INFO_LIBS) \
                              $(PHONESTATUS_LIBS) \
                              $(UTIL_LIBS) \
                              $(DLOG_LIBS) \
                              $(DRM_SERVICE_LIBS) \
                              $(DB_UTIL_LIBS) \
			      $(AUL_LIBS)\
			      $(DBUS_LIBS)\
			      $(LIBPMCONTROL_LIBS)\
			      $(THUMB_GEN_LIBS)
#                              $(LIBQUICKPANEL_LIBS)

### includeheaders ###
includeheadersdir = $(includedir)/media-utils
includeheaders_HEADERS = lib/include/media-util-noti.h \
                         lib/include/media-util-register.h \
                         lib/include/media-util-err.h

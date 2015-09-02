ACLOCAL_AMFLAGS = -I m4

SUBDIRS = .

AM_CPPFLAGS = $(FMS_DEBUG_FLAGS)

AM_LDFLAGS=-Wl,--as-needed -Wl,--hash-style=both


######################################################
#common flags for binary
######################################################
COMMON_CFLAGS = -I${srcdir}/lib/include \
                -I${srcdir}/src/common/include \
		$(GLIB_CFLAGS) \
		$(GTHREAD_CFLAGS) \
		$(DLOG_CFLAGS) \
		$(INIPARSER_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(LIBPMCONTROL_CFLAGS) \
		$(SECURITY_CFLAGS) \
		$(SMACK_CFLAGS)

COMMON_CFLAGS += $(STATUS_CFLAGS)

COMMON_LDADD =	libmedia-utils.la \
		$(GLIB_LIBS) \
		$(GTHREAD_LIBS) \
		$(DLOG_LIBS) \
		$(INIPARSER_LIBS) \
		$(DBUS_LIBS) \
		$(LIBPMCONTROL_LIBS) \
		$(SECURITY_LIBS) \
		$(SMACK_LIBS) \
		-ldl

COMMON_LDADD += $(STATUS_LIBS)

######################################################
#pkgconfig
######################################################
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

######################################################
#libmedia utils
######################################################
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
			      $(GIO_CFLAGS) \
			      $(SQLITE3_CFLAGS) \
			      $(DB_UTIL_CFLAGS) \
			      $(PHONESTATUS_CFLAGS) \
			      $(TZ_PLATFORM_CONFIG_CFLAGS) \
			      $(LIBSMACK_CFLAGS) \
			      $(CYNARA_CLIENT_CFLAGS) \
			      $(CYNARA_SESSION_CFLAGS) \
			      $(CYNARA_CREDS_SOCKET_CFLAGS)

libmedia_utils_la_LIBADD  =   $(GLIB_LIBS) \
                              $(DLOG_LIBS) \
                              $(DBUS_LIBS) \
			      $(GIO_LIBS) \
			      $(SQLITE3_LIBS) \
			      $(DB_UTIL_LIBS) \
			      $(PHONESTATUS_LIBS) \
			      $(TZ_PLATFORM_CONFIG_LIBS) \
			      $(LIBSMACK_LIBS) \
			      $(CYNARA_CLIENT_LIBS) \
			      $(CYNARA_SESSION_LIBS) \
			      $(CYNARA_CREDS_SOCKET_LIBS)

bin_PROGRAMS = media-server \
		media-scanner \
		mediadb-update
######################################################
#media server
######################################################
media_server_SOURCES = src/common/media-common-utils.c \
                       src/common/media-common-system.c \
		       src/common/media-common-external-storage.c \
                       src/common/media-common-db-svc.c \
                       src/server/media-server-db.c \
                       src/server/media-server-socket.c \
                       src/server/media-server-thumb.c \
                       src/server/media-server-scanner.c \
		       src/server/media-server-device-block.c \
                       src/server/media-server-main.c

media_server_CFLAGS = -I${srcdir}/src/server/include \
			$(COMMON_CFLAGS) \
                        $(PHONESTATUS_CFLAGS)

media_server_CFLAGS += -fPIE

media_server_LDFLAGS = -pie

media_server_LDADD = $(COMMON_LDADD) \
		     $(PHONESTATUS_LIBS)

######################################################
#media scanner v1
######################################################
media_scanner_SOURCES = src/common/media-common-utils.c \
			src/common/media-common-system.c \
			src/common/media-common-external-storage.c \
			src/common/media-common-db-svc.c \
                        src/scanner/media-scanner-device-block.c \
			src/scanner/media-scanner-scan.c \
			src/scanner/media-scanner-socket.c \
			src/scanner/media-scanner.c

media_scanner_CFLAGS = -I${srcdir}/src/scanner/include \
			$(COMMON_CFLAGS)

media_scanner_CFLAGS += -fPIE

media_scanner_LDFLAGS = -pie

media_scanner_LDADD = $(COMMON_LDADD)

######################################################
#mediadb_udpate
######################################################
mediadb_update_SOURCES = src/mediadb-update.c

mediadb_update_CFLAGS = -I${srcdir}/lib/include \
			$(GTHREAD_CFLAGS) \
			$(GLIB_CFLAGS)

mediadb_update_CFLAGS += -fPIE

mediadb_update_LDFLAGS = -pie

mediadb_update_LDADD = libmedia-utils.la \
			$(GLIB_LIBS) \
			$(GTHREAD_LIBS) \
			-ldl

### includeheaders ###
includeheadersdir = $(includedir)/media-utils
includeheaders_HEADERS = lib/include/media-util-noti-common.h \
                         lib/include/media-util-noti.h \
                         lib/include/media-util-register.h \
                         lib/include/media-util-err.h \
                         lib/include/media-util-db.h \
                         lib/include/media-util-ipc.h \
                         lib/include/media-util-cynara.h \
                         lib/include/media-util.h \
                         lib/include/media-server-ipc.h

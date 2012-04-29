ifeq ($(ARCH),target)
	PKG_CONFIG_PATH=/usr/lib/pkgconfig
	export PKG_CONFIG_PATH
	CC=arm-linux-gcc -Wall
	CXX=arm-linux-g++ -Wall
else
        PKG_CONFIG_PATH=/usr/lib/pkgconfig
        export PKG_CONFIG_PATH
	CC=gcc -Wall
	CXX=gcc -Wall
endif

TS10=utc_ms_noti_update_complete
TS11=utc_media_file_register

LIBS = `pkg-config --libs glib-2.0 libmedia-utils`
LIBS +=$(TET_ROOT)/lib/tet3/tcm_s.o
LIBS +=-L$(TET_ROOT)/lib/tet3 -ltcm_s
LIBS +=-L$(TET_ROOT)/lib/tet3/ -lapi_s

INCS = -I. `pkg-config --cflags glib-2.0 libmedia-utils`
INCS += -I$(TET_ROOT)/inc/tet3
INCS += -I/usr/include

CFLAGS = $(INCS)
CC += $(CFLAGS)
LDFLAGS = $(LIBS)

all : $(TS10) $(TS11)

$(TS10): $(TS10).c
	$(CC) -o $(TS10) $(TS10).c $(LDFLAGS)
$(TS11): $(TS11).c
	$(CC) -o $(TS11) $(TS11).c $(LDFLAGS)

clean :
	rm -rf *~ *.o $(TS10)
	rm -rf *~ *.o $(TS11)

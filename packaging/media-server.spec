Name:       media-server
Summary:    Media Server.
Version:	0.1.53
Release:    1
Group:      Services
License:    Apache-2.0
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
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(libmedia-service)
BuildRequires:  pkgconfig(media-thumbnail)
BuildRequires:  pkgconfig(pmapi)

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
%make_install
chmod +x %{buildroot}/etc/rc.d/init.d/mediasvr

rm -rf %{buildroot}/etc/rc.d/rc3.d/S48mediasvr
rm -rf %{buildroot}/etc/rc.d/rc5.d/S48mediasvr

%post

ln -s ../init.d/mediasvr /etc/rc.d/rc3.d/S48mediasvr
ln -s ../init.d/mediasvr /etc/rc.d/rc5.d/S48mediasvr


vconftool set -t int db/filemanager/dbupdate "1"
vconftool set -t int memory/filemanager/Mmc "0" -i

vconftool set -t int db/Apps/mediaserver/usbmode "0"
vconftool set -t string db/Apps/mediaserver/mmc_info ""

%files
%{_bindir}/media-server
/etc/rc.d/init.d/mediasvr
/usr/lib/lib*.so.*

%files devel
/usr/include/media-utils/*.h
/usr/lib/libmedia-utils.so
/usr/lib/pkgconfig/libmedia-utils.pc

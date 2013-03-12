Name:       media-server
Summary:    File manager service server.
Version: 0.2.26
Release:    1
Group:      utils
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    media-server.service

Requires(post): /usr/bin/vconftool
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(drm-client)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(notification)

%description
Description: File manager service server


%package -n libmedia-utils
Summary:   media server runtime library.
Group:     TO_BE/FILLED_IN

%description -n libmedia-utils
Description : media server runtime library.


%package -n libmedia-utils-devel
Summary:   media server development library.
Group:     Development/Libraries
Requires:  libmedia-utils = %{version}-%{release}

%description -n libmedia-utils-devel
Description: media server development library.

%prep
%setup -q

%build

%autogen
%configure --prefix=%{_prefix} --disable-static

make %{?jobs:-j%jobs}

%install
%make_install

mkdir -p %{buildroot}/usr/lib/systemd/system/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}/usr/lib/systemd/system/media-server.service
ln -s ../media-server.service %{buildroot}/usr/lib/systemd/system/multi-user.target.wants/media-server.service

#License
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/LICENSE.APLv2.0 %{buildroot}/%{_datadir}/license/%{name}

%post
vconftool set -t int db/filemanager/dbupdate "1" -f
vconftool set -t int memory/filemanager/Mmc "0" -i -f
vconftool set -t string db/private/mediaserver/mmc_info "" -f
vconftool set -t int file/private/mediaserver/scan_internal "1" -f
vconftool set -t int file/private/mediaserver/scan_directory "1" -f

%files
%manifest media-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-server
%{_bindir}/media-scanner
%{_bindir}/mediadb-update
%attr(755,-,-) %{_sysconfdir}/rc.d/init.d/mediasvr
/etc/rc.d/rc3.d/S99mediasvr
/etc/rc.d/rc5.d/S99mediasvr
/usr/local/bin/reset_mediadb.sh
/usr/lib/systemd/system/media-server.service
/usr/lib/systemd/system/multi-user.target.wants/media-server.service
#License
%{_datadir}/license/%{name}

%files -n libmedia-utils
%manifest libmedia-utils.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so
%{_libdir}/libmedia-utils.so.0
%{_libdir}/libmedia-utils.so.0.0.0

%files -n libmedia-utils-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/libmedia-utils.pc
%{_includedir}/media-utils/*.h

%changelog
* Mon Oct 15 2012 Hyunjun Ko <zzoon.ko@samsnug.com> - 0.1.97
- Fix a bug in db thread

* Wed Oct 10 2012 Hyunjun Ko <zzoon.ko@samsnug.com> - 0.1.96
- Some apis are added : media_db_request_update_db_batch / start / end

* Mon Sep 10 2012 Haejeong Kim <backto.kim@samsnug.com> - 0.1.86
- Make new thread for DB write. Only Media server can update db

* Mon Aug 06 2012 Yong Yeon Kim <yy9875.kim@samsnug.com> - 0.1.86
- add notification subscribe function for application
- fix bug : once validity checking time, call insert_item_batch two times.
- add MS_SAFE_FREE Macro, modify check value after using snprintf by secure coding guide
- change macro name MS_PHONE_ROOT_PATH, MS_MMC_ROOT_PATH
- make reference directory list by each thread

* Tue Jul 03 2012 Yong Yeon Kim <yy9875.kim@samsnug.com> - 0.1.80
- manage db handle by plug-in

* Wed Jun 27 2012 Yong Yeon Kim <yy9875.kim@samsnug.com> - 0.1.79
- If item exists in media db, return directly

* Tue Jun 26 2012 Yong Yeon Kim <yy9875.kim@samsnug.com> - 0.1.78
- change modified file updating routine (delete & insert -> refresh)
- modify return error type of media_file_register


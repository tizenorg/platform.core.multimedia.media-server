Name:       media-server
Summary:    File manager service server.
Version: 0.1.90
Release:    1
Group:      utils
License:    Samsung
Source0:    %{name}-%{version}.tar.gz

Requires(post): /usr/bin/vconftool
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(drm-client)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(dbus-glib-1)

%description
Description: File manager service server


%package -n libmedia-utils
Summary:   media server runtime library.
Group:     TO_BE/FILLED_IN
Requires:  %{name} = %{version}-%{release}

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
rm -rf %{buildroot}
%make_install

%post
vconftool set -t int db/filemanager/dbupdate "1"
vconftool set -t int memory/filemanager/Mmc "0" -i

vconftool set -t string db/private/mediaserver/mmc_info ""


%files
%defattr(-,root,root,-)
%{_bindir}/media-server
%attr(755,-,-) %{_sysconfdir}/rc.d/init.d/mediasvr
/etc/rc.d/rc3.d/S99mediasvr
/etc/rc.d/rc5.d/S99mediasvr
/usr/local/bin/reset_mediadb.sh

%files -n libmedia-utils
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so
%{_libdir}/libmedia-utils.so.0
%{_libdir}/libmedia-utils.so.0.0.0

%files -n libmedia-utils-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/libmedia-utils.pc
%{_includedir}/media-utils/*.h

%changelog
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


Name:       media-server
Summary:    File manager service server.
Version: 0.2.42
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
/etc/rc.d/rc3.d/S46mediasvr
/etc/rc.d/rc5.d/S46mediasvr
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


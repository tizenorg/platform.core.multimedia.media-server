Name:       media-server
Summary:    File manager service server.
Version: 0.2.46
Release:    1
Group:      Multimedia/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    media-server.service
Source1001: 	media-server.manifest

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
Group:     Multimedia/Libraries

%description -n libmedia-utils
Description : media server runtime library.


%package -n libmedia-utils-devel
Summary:   media server development library.
Group:     Development/Multimedia
Requires:  libmedia-utils = %{version}-%{release}

%description -n libmedia-utils-devel
Description: media server development library.

%prep
%setup -q
cp %{SOURCE1001} .

%build

%autogen
%configure --prefix=%{_prefix} --disable-static

make %{?jobs:-j%jobs}

%install
%make_install

mkdir -p %{buildroot}/usr/lib/systemd/system/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}/usr/lib/systemd/system/media-server.service
ln -s ../media-server.service %{buildroot}/usr/lib/systemd/system/multi-user.target.wants/media-server.service

%post
vconftool set -t int db/filemanager/dbupdate "1" -f
vconftool set -t int memory/filemanager/Mmc "0" -i -f
vconftool set -t string db/private/mediaserver/mmc_info "" -f
vconftool set -t int file/private/mediaserver/scan_internal "1" -f
vconftool set -t int file/private/mediaserver/scan_directory "1" -f

%post -n libmedia-utils -p /sbin/ldconfig

%postun -n libmedia-utils -p /sbin/ldconfig

%files
%manifest %{name}.manifest
#%manifest media-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-server
%{_bindir}/media-scanner
%{_bindir}/mediadb-update
%exclude %attr(755,-,-) %{_sysconfdir}/rc.d/init.d/mediasvr
%exclude /etc/rc.d/rc3.d/S46mediasvr
%exclude /etc/rc.d/rc5.d/S46mediasvr
/usr/lib/systemd/system/media-server.service
/usr/lib/systemd/system/multi-user.target.wants/media-server.service
%license LICENSE.APLv2.0


%files -n libmedia-utils
%manifest %{name}.manifest
%license LICENSE.APLv2.0
#%manifest libmedia-utils.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so.0
%{_libdir}/libmedia-utils.so.0.0.0

%files -n libmedia-utils-devel
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so
%{_libdir}/pkgconfig/libmedia-utils.pc
%{_includedir}/media-utils/*.h


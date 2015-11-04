Name:       media-server
Summary:    File manager service server.
Version:    0.2.62
Release:    0
Group:      Multimedia/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    media-server.service
Source2:	media-server-user.service
Source3:    media-server-user.path
Source1001:     %{name}.manifest
Source1002:     libmedia-utils.manifest
Source1003:     libmedia-utils-devel.manifest
Requires(post): /usr/bin/vconftool
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(libsmack)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(cynara-client)
BuildRequires:  pkgconfig(cynara-session)

%description
Description: File manager service server

%package -n libmedia-utils
Summary:   Media server runtime library
Group:     Multimedia/Libraries
Requires:  media-server = %{version}-%{release}

%description -n libmedia-utils
Description : media server runtime library

%package -n libmedia-utils-devel
Summary:   Media server development library
Group:     Development/Multimedia
Requires:  libmedia-utils = %{version}-%{release}
Requires:  libtzplatform-config-devel

%description -n libmedia-utils-devel
Description: media server development library

%prep
%setup -q
cp %{SOURCE1001} %{SOURCE1002} %{SOURCE1003} .

%build
rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh
mkdir -p m4
%reconfigure --prefix=%{_prefix} --disable-static
%__make %{?jobs:-j%jobs}

%install
%make_install

mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}%{_unitdir}/media-server.service
mkdir -p %{buildroot}%{_unitdir_user}
install -m 644 %{SOURCE2} %{buildroot}%{_unitdir_user}/media-server-user.service
install -m 644 %{SOURCE3} %{buildroot}%{_unitdir_user}/media-server-user.path
ln -s ../media-server.service %{buildroot}%{_unitdir}/multi-user.target.wants/media-server.service
#ini file
mkdir -p %{buildroot}/usr/etc
cp -rf %{_builddir}/%{name}-%{version}/media_content_config.ini %{buildroot}/usr/etc/
cp -rf %{_builddir}/%{name}-%{version}/media-server-plugin %{buildroot}/usr/etc/media-server-plugin

%post
# setup dbupdate in user session
mkdir -p %{_unitdir_user}/default.target.wants/
ln -sf ../media-server-user.path  %{_unitdir_user}/default.target.wants/

%post -n libmedia-utils -p /sbin/ldconfig

%postun -n libmedia-utils -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/media-server
%{_bindir}/media-scanner
%{_bindir}/media-scanner-v2
%{_bindir}/mediadb-update
%exclude %attr(755,-,-) %{_sysconfdir}/rc.d/init.d/mediasvr
%exclude /etc/rc.d/rc3.d/S46mediasvr
%exclude /etc/rc.d/rc5.d/S46mediasvr
%{_unitdir}/media-server.service
%{_unitdir}/multi-user.target.wants/media-server.service
%{_unitdir_user}/media-server-user.service
%{_unitdir_user}/media-server-user.path
/usr/etc/media_content_config.ini
/usr/etc/media-server-plugin
%license LICENSE.APLv2.0

%files -n libmedia-utils
%manifest libmedia-utils.manifest
%license LICENSE.APLv2.0
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so.0
%{_libdir}/libmedia-utils.so.0.0.0

%files -n libmedia-utils-devel
%manifest libmedia-utils-devel.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so
%{_libdir}/pkgconfig/libmedia-utils.pc
%{_includedir}/media-utils/*.h


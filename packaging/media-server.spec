Name:       media-server
Summary:    A server for media content management
Version:    0.2.78
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
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(notification)
BuildRequires:  pkgconfig(iniparser)
BuildRequires:  pkgconfig(libsmack)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  pkgconfig(cynara-client)
BuildRequires:  pkgconfig(cynara-session)
BuildRequires:  gettext-tools

%description
Description: A server for media content management.

%package -n libmedia-utils
Summary:   The media server runtime library
Group:     Multimedia/Libraries
Requires:  media-server = %{version}-%{release}

%description -n libmedia-utils
Description : The media server runtime library.

%package -n libmedia-utils-devel
Summary:   The media server runtime library (development)
Group:     Multimedia/Development
Requires:  libmedia-utils = %{version}-%{release}
Requires:  libtzplatform-config-devel

%description -n libmedia-utils-devel
Description: The media server runtime library. (Development files included)

%prep
%setup -q
cp %{SOURCE1001} %{SOURCE1002} %{SOURCE1003} .
cp po/* .

%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE -DSYSCONFDIR=\\\"%{_sysconfdir}\\\""
rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh
mkdir -p m4
%reconfigure --prefix=%{_prefix} --disable-static
%__make %{?jobs:-j%jobs}

#install .po files
/usr/bin/msgfmt -o ar.mo ar.po
/usr/bin/msgfmt -o az.mo az.po
/usr/bin/msgfmt -o bg.mo bg.po
/usr/bin/msgfmt -o ca.mo ca.po
/usr/bin/msgfmt -o cs.mo cs.po
/usr/bin/msgfmt -o da.mo da.po
/usr/bin/msgfmt -o el_GR.mo el_GR.po
/usr/bin/msgfmt -o en.mo en.po
/usr/bin/msgfmt -o en_PH.mo en_PH.po
/usr/bin/msgfmt -o en_US.mo en_US.po
/usr/bin/msgfmt -o es_ES.mo es_ES.po
/usr/bin/msgfmt -o et.mo et.po
/usr/bin/msgfmt -o eu.mo eu.po
/usr/bin/msgfmt -o fi.mo fi.po
/usr/bin/msgfmt -o fr_CA.mo fr_CA.po
/usr/bin/msgfmt -o ga.mo ga.po
/usr/bin/msgfmt -o gl.mo gl.po
/usr/bin/msgfmt -o hi.mo hi.po
/usr/bin/msgfmt -o hr.mo hr.po
/usr/bin/msgfmt -o hu.mo hu.po
/usr/bin/msgfmt -o hy.mo hy.po
/usr/bin/msgfmt -o is.mo is.po
/usr/bin/msgfmt -o it_IT.mo it_IT.po
/usr/bin/msgfmt -o ja_JP.mo ja_JP.po
/usr/bin/msgfmt -o ka.mo ka.po
/usr/bin/msgfmt -o kk.mo kk.po
/usr/bin/msgfmt -o ko_KR.mo ko_KR.po
/usr/bin/msgfmt -o lt.mo lt.po
/usr/bin/msgfmt -o lv.mo lv.po
/usr/bin/msgfmt -o mk.mo mk.po
/usr/bin/msgfmt -o nb.mo nb.po
/usr/bin/msgfmt -o pl.mo pl.po
/usr/bin/msgfmt -o pt_BR.mo pt_BR.po
/usr/bin/msgfmt -o pt_PT.mo pt_PT.po
/usr/bin/msgfmt -o ro.mo ro.po
/usr/bin/msgfmt -o ru_RU.mo ru_RU.po
/usr/bin/msgfmt -o sk.mo sk.po
/usr/bin/msgfmt -o sl.mo sl.po
/usr/bin/msgfmt -o sr.mo sr.po
/usr/bin/msgfmt -o sv.mo sv.po
/usr/bin/msgfmt -o tr_TR.mo tr_TR.po
/usr/bin/msgfmt -o uk.mo uk.po
/usr/bin/msgfmt -o uz.mo uz.po
/usr/bin/msgfmt -o zh_CN.mo zh_CN.po
/usr/bin/msgfmt -o zh_HK.mo zh_HK.po
/usr/bin/msgfmt -o zh_TW.mo zh_TW.po

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}%{_datadir}/locale/ar/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/az/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/bg/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ca/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/cs/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/da/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/el_GR/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/en/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/en_PH/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/en_US/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/es_ES/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/et/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/eu/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/fi/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/fr_CA/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ga/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/gl/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/hi/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/hr/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/hu/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/hy/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/is/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/it_IT/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ja_JP/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ka/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/kk/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ko_KR/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/lt/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/lv/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/mk/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/nb/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/pl/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/pt_BR/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/pt_PT/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ro/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/ru_RU/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/sk/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/sl/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/sr/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/sv/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/tr_TR/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/uk/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/uz/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/zh_CN/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/zh_HK/LC_MESSAGES/
mkdir -p %{buildroot}%{_datadir}/locale/zh_TW/LC_MESSAGES/

cp ar.mo %{buildroot}%{_datadir}/locale/ar/LC_MESSAGES/media_svr.mo
cp az.mo %{buildroot}%{_datadir}/locale/az/LC_MESSAGES/media_svr.mo
cp bg.mo %{buildroot}%{_datadir}/locale/bg/LC_MESSAGES/media_svr.mo
cp ca.mo %{buildroot}%{_datadir}/locale/ca/LC_MESSAGES/media_svr.mo
cp cs.mo %{buildroot}%{_datadir}/locale/cs/LC_MESSAGES/media_svr.mo
cp da.mo %{buildroot}%{_datadir}/locale/da/LC_MESSAGES/media_svr.mo
cp el_GR.mo %{buildroot}%{_datadir}/locale/el_GR/LC_MESSAGES/media_svr.mo
cp en.mo %{buildroot}%{_datadir}/locale/en/LC_MESSAGES/media_svr.mo
cp en_PH.mo %{buildroot}%{_datadir}/locale/en_PH/LC_MESSAGES/media_svr.mo
cp en_US.mo %{buildroot}%{_datadir}/locale/en_US/LC_MESSAGES/media_svr.mo
cp es_ES.mo %{buildroot}%{_datadir}/locale/es_ES/LC_MESSAGES/media_svr.mo
cp et.mo %{buildroot}%{_datadir}/locale/et/LC_MESSAGES/media_svr.mo
cp eu.mo %{buildroot}%{_datadir}/locale/eu/LC_MESSAGES/media_svr.mo
cp fi.mo %{buildroot}%{_datadir}/locale/fi/LC_MESSAGES/media_svr.mo
cp fr_CA.mo %{buildroot}%{_datadir}/locale/fr_CA/LC_MESSAGES/media_svr.mo
cp ga.mo %{buildroot}%{_datadir}/locale/ga/LC_MESSAGES/media_svr.mo
cp gl.mo %{buildroot}%{_datadir}/locale/gl/LC_MESSAGES/media_svr.mo
cp hi.mo %{buildroot}%{_datadir}/locale/hi/LC_MESSAGES/media_svr.mo
cp hr.mo %{buildroot}%{_datadir}/locale/hr/LC_MESSAGES/media_svr.mo
cp hu.mo %{buildroot}%{_datadir}/locale/hu/LC_MESSAGES/media_svr.mo
cp hy.mo %{buildroot}%{_datadir}/locale/hy/LC_MESSAGES/media_svr.mo
cp is.mo %{buildroot}%{_datadir}/locale/is/LC_MESSAGES/media_svr.mo
cp it_IT.mo %{buildroot}%{_datadir}/locale/it_IT/LC_MESSAGES/media_svr.mo
cp ja_JP.mo %{buildroot}%{_datadir}/locale/ja_JP/LC_MESSAGES/media_svr.mo
cp ka.mo %{buildroot}%{_datadir}/locale/ka/LC_MESSAGES/media_svr.mo
cp kk.mo %{buildroot}%{_datadir}/locale/kk/LC_MESSAGES/media_svr.mo
cp ko_KR.mo %{buildroot}%{_datadir}/locale/ko_KR/LC_MESSAGES/media_svr.mo
cp lt.mo %{buildroot}%{_datadir}/locale/lt/LC_MESSAGES/media_svr.mo
cp lv.mo %{buildroot}%{_datadir}/locale/lv/LC_MESSAGES/media_svr.mo
cp mk.mo %{buildroot}%{_datadir}/locale/mk/LC_MESSAGES/media_svr.mo
cp nb.mo %{buildroot}%{_datadir}/locale/nb/LC_MESSAGES/media_svr.mo
cp pl.mo %{buildroot}%{_datadir}/locale/pl/LC_MESSAGES/media_svr.mo
cp pt_BR.mo %{buildroot}%{_datadir}/locale/pt_BR/LC_MESSAGES/media_svr.mo
cp pt_PT.mo %{buildroot}%{_datadir}/locale/pt_PT/LC_MESSAGES/media_svr.mo
cp ro.mo %{buildroot}%{_datadir}/locale/ro/LC_MESSAGES/media_svr.mo
cp ru_RU.mo %{buildroot}%{_datadir}/locale/ru_RU/LC_MESSAGES/media_svr.mo
cp sk.mo %{buildroot}%{_datadir}/locale/sk/LC_MESSAGES/media_svr.mo
cp sl.mo %{buildroot}%{_datadir}/locale/sl/LC_MESSAGES/media_svr.mo
cp sr.mo %{buildroot}%{_datadir}/locale/sr/LC_MESSAGES/media_svr.mo
cp sv.mo %{buildroot}%{_datadir}/locale/sv/LC_MESSAGES/media_svr.mo
cp tr_TR.mo %{buildroot}%{_datadir}/locale/tr_TR/LC_MESSAGES/media_svr.mo
cp uk.mo %{buildroot}%{_datadir}/locale/uk/LC_MESSAGES/media_svr.mo
cp uz.mo %{buildroot}%{_datadir}/locale/uz/LC_MESSAGES/media_svr.mo
cp zh_CN.mo %{buildroot}%{_datadir}/locale/zh_CN/LC_MESSAGES/media_svr.mo
cp zh_HK.mo %{buildroot}%{_datadir}/locale/zh_HK/LC_MESSAGES/media_svr.mo
cp zh_TW.mo %{buildroot}%{_datadir}/locale/zh_TW/LC_MESSAGES/media_svr.mo

%make_install

mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}%{_unitdir}/media-server.service
mkdir -p %{buildroot}%{_unitdir_user}
install -m 644 %{SOURCE2} %{buildroot}%{_unitdir_user}/media-server-user.service
install -m 644 %{SOURCE3} %{buildroot}%{_unitdir_user}/media-server-user.path
ln -s ../media-server.service %{buildroot}%{_unitdir}/multi-user.target.wants/media-server.service
#ini file
mkdir -p %{buildroot}/etc/multimedia
cp -rf %{_builddir}/%{name}-%{version}/media_content_config.ini %{buildroot}/etc/multimedia/media_content_config.ini
cp -rf %{_builddir}/%{name}-%{version}/media-server-plugin %{buildroot}/etc/multimedia/media-server-plugin

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
/etc/multimedia/media_content_config.ini
/etc/multimedia/media-server-plugin
%{_datadir}/locale/*/LC_MESSAGES/*
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


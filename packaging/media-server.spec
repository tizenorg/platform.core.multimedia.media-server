#sbs-git:slp/pkgs/m/media-server media-server 0.1.60 e216f565fe5687a6f387f2b3ee2097b926225517
Name:       media-server
Summary:    File manager service server.
Version: 0.1.66
Release:    1
Group:      Services
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz

Requires(post): /usr/bin/vconftool
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(drm-service)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(pmapi)

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

vconftool set -t int db/Apps/mediaserver/usbmode "0"
vconftool set -t string db/Apps/mediaserver/mmc_info ""


%files
%defattr(-,root,root,-)
%{_bindir}/media-server
%attr(755,-,-) %{_sysconfdir}/rc.d/init.d/mediasvr
/etc/rc.d/rc3.d/S48mediasvr
/etc/rc.d/rc5.d/S48mediasvr

%files -n libmedia-utils
%defattr(-,root,root,-)
%{_libdir}/libmedia-utils.so
%{_libdir}/libmedia-utils.so.0
%{_libdir}/libmedia-utils.so.0.0.0

%files -n libmedia-utils-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/libmedia-utils.pc
%{_includedir}/media-utils/*.h



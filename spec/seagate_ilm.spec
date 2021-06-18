Summary: A lock manager based on Seagate In-Drive-Mutex (IDM)
Name: seagate_ilm
Epoch: 1
Version: 1
Release: 1%{?dist}
License: LGPL-2.1-only
Group: System/Base
URL: http://github.com/Seagate/propeller
Source0: seagate_ilm.%{version}.tgz

BuildRequires: libuuid-devel
BuildRequires: libblkid-devel
BuildRequires: systemd-devel

%if 0%{?suse_version}
BuildRequires: libblkid1
BuildRequires: libudev-devel
BuildRequires: systemd
%endif

%if 0%{?rhel} || 0%{?centos}
BuildRequires: libblkid
%endif

%if 0%{?suse_version} || 0%{?centos} == 7
BuildRequires: python-pytest
%endif

%if 0%{?rhel} == 7
BuildRequires: pytest
%endif

%if 0%{?centos} == 8
BuildRequires: python3-pytest
%endif

%if 0%{?centos} != 8
BuildRequires: python-devel
BuildRequires: swig
%endif

%description
The Seagate IDM Lock Manager (ILM) manages lease for the host using the mutexes that exist in the drive.

%prep
%setup -q -n seagate_ilm.%{version}

%build
make %{?_smp_mflags} V=1

%install
make install DESTDIR=$RPM_BUILD_ROOT V=1
%if 0%{?suse_version}
ln -sf %{_sbindir}/service %{buildroot}%{_sbindir}/rcseagate_ilm
%endif


%clean
rm -rf $RPM_BUILD_ROOT

%pre
%service_add_pre seagate_ilm.service

%post
/sbin/ldconfig
%systemd_post seagate_ilm.service
%service_add_post seagate_ilm.service

%if 0%{?rhel} || 0%{?centos}
if [ "$1" = "1" ] ; then
  #enable and start seagate_ilm.service on completely new installation only, not on upgrades
  systemctl enable seagate_ilm.service
  systemctl start seagate_ilm.service
fi
%endif

%preun
%service_del_preun seagate_ilm.service
%systemd_preun seagate_ilm.service

%postun
/sbin/ldconfig
%service_del_postun seagate_ilm.service
%systemd_postun seagate_ilm.service

%files
%defattr(-,root,root,755)
%doc README.md

%{_sbindir}/seagate_ilm
%{_unitdir}/seagate_ilm.service
%{_sysconfdir}/logrotate.d/seagate_ilm
%if 0%{?suse_version}
%{_sbindir}/rcseagate_ilm
%endif

#Library and Development subpackages
%package devel
Summary: Development libraries and headers
Group: Development/Libraries/Other
License: LGPL-2.1-only
Requires: %{name} = %{epoch}:%{version}-%{release}

%description devel
This package contains files needed to develop applications that use the seagate_ilm libraries.

%post devel -p /sbin/ldconfig

%postun devel -p /sbin/ldconfig

%files devel
%defattr(444,root,root,-)
%{_libdir}/pkgconfig
%{_includedir}/ilm.h
%{_libdir}/libseagate_ilm.so
%{_libdir}/libseagate_ilm.so.0
%{_libdir}/libseagate_ilm.so.0.1

%changelog


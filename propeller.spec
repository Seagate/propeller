Summary: A lock manager based on Seagate In-Drive-Mutex (IDM)
Name: propeller
Epoch: 1
Version: 1
Release: 1%{?dist}
License: GPLv2 and LGPLv2
URL: http://github.com/Seagate/propeller
Source0: propeller.%{version}.tgz

BuildRequires: libuuid-devel
BuildRequires: libblkid
BuildRequires: libblkid-devel
BuildRequires: python-pytest
BuildRequires: python-devel
BuildRequires: libudev

%description
The Seagate IDM Lock Manager (ILM) manages lease for the host
using the mutexes that exist in the drive.

%prep
%setup -q -n propeller.%{version}

%build
make %{?_smp_mflags} V=1

%install
make install DESTDIR=$RPM_BUILD_ROOT V=1

%clean
rm -rf $RPM_BUILD_ROOT

%post
%systemd_post seagate_ilm
systemctl enable seagate_ilm
systemctl start seagate_ilm

%preun
%systemd_preun seagate_ilm

%postun
%systemd_postun seagate_ilm

%files
%defattr(-,root,root,755)
%doc README.md

%{_includedir}/ilm.h
%{_unitdir}/seagate_ilm.service
%{_libdir}/libseagate_ilm.so
%{_libdir}/libseagate_ilm.so.0
%{_libdir}/libseagate_ilm.so.0.1
%{_libdir}/pkgconfig
%{_sbindir}/seagate_ilm
%{_sysconfdir}/logrotate.d/seagate_ilm
   
%changelog


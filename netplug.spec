%define version 1.2.9
%define release 1
%define sysconfig %{_sysconfdir}/sysconfig/network-scripts

Summary: Daemon that responds to network cables being plugged in and out
Name: netplug
Version: %{version}
Release: %{release}
License: GPL
Group: System Environment/Base
URL: http://www.red-bean.com/~bos/
Packager: Bryan O'Sullivan <bos@serpentine.com>
Vendor: PathScale, Inc. <http://www.pathscale.com/>
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Requires: iproute >= 2.4.7

%description
Netplug is a daemon that manages network interfaces in response to
link-level events such as cables being plugged in and out.  When a
cable is plugged into an interface, the netplug daemon brings that
interface up.  When the cable is unplugged, the daemon brings that
interface back down.

This is extremely useful for systems such as laptops, which are
constantly being unplugged from one network and plugged into another,
and for moving systems in a machine room from one switch to another
without a need for manual intervention.

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make install prefix=$RPM_BUILD_ROOT \
	initdir=$RPM_BUILD_ROOT/%{_initrddir} \
	mandir=$RPM_BUILD_ROOT/%{_mandir}	

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config %{_sysconfdir}/netplug/netplugd.conf
%{_sysconfdir}/netplug.d
%{_sysconfdir}/netplug
%{_initrddir}/netplugd
/sbin/netplugd
%{_mandir}/man*/*

%doc COPYING ChangeLog NEWS README TODO

%post
/sbin/chkconfig --add netplugd

%preun
/sbin/chkconfig --del netplugd

%changelog
* Tue Aug 26 2003 Bryan O'Sullivan <bos@serpentine.com> - 
- Initial build.

Summary: Daemon that responds to network cables being plugged in and out
Name: netplug
Version: 1.0
Release: 1
License: GPL
Group: System Environment/Base
URL: http://www.serpentine.com/~bos/netplug
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description

Netplug is a daemon that manages a network interface in response to
link-level events such as cables being plugged in and out.  When a
cable is plugged into an interface, the netplug daemon brings that
interface up.  When the cable is unplugged, the daemon brings that
interface back down.

This is extremely useful for systems such as laptops, which are
constantly being unplugged from one network and plugged into another,
and for moving systems in a machine room from one switch to another
without manual intervention required.

%prep
%setup -q

%build

%install
rm -rf $RPM_BUILD_ROOT
make install prefix=%{prefix}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{prefix}/sbin/netplugd
%config %{prefix}/etc/netplug/netplug.conf
%{prefix}/etc/netplug.d
%{prefix}/etc/rc.d/init.d/netplugd

%doc


%changelog
* Tue Aug 26 2003 Bryan O'Sullivan <bos@serpentine.com> - 
- Initial build.



%define version 1.0
%define release 1
%define sysconfig /etc/sysconfig/network-scripts

Summary: Daemon that responds to network cables being plugged in and out
Name: netplug
Version: %{version}
Release: %{release}
License: GPL
Group: System Environment/Base
URL: http://www.serpentine.com/~bos/netplug
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
/sbin/netplugd
%config /etc/netplug/netplugd.conf
/etc/netplug.d
/etc/rc.d/init.d/netplugd
%docdir /usr/share/man/*
/usr/share/man/*/*

%doc COPYING README TODO

%post
/sbin/chkconfig --add netplugd

for cfg in %{sysconfig}/ifcfg-eth*; do
    if echo "$cfg" | grep -q pre-netplug; then
	continue
    fi
    if [ -f "$cfg.pre-netplug" ]; then
	continue
    fi
    sed -e 's/^ONBOOT=yes/ONBOOT=no/' "$cfg" > "$cfg.new.$$"
    if cmp -s "$cfg" "$cfg.new.$$"; then
	true
    else
	cp "$cfg" "$cfg.pre-netplug"
	cp "$cfg.new.$$" "$cfg"
	ifname=`echo "$cfg" | sed 's!^.*/ifcfg-\(.*\)$!\1!'`
	echo "Updated $ifname to be managed by netplug"
    fi
    rm "$cfg.new.$$"
done

%preun
/sbin/chkconfig --del netplugd

%postun
for precfg in %{sysconfig}/*.pre-netplug; do
    if [ ! -f "$precfg" ]; then
	continue
    fi
    cfg=`echo "$precfg" | sed -e 's!\.pre-netplug$!!'`
    sed -e 's/^ONBOOT=no/ONBOOT=yes/' "$cfg" > "$cfg.new.$$"
    if cmp -s "$cfg" "$cfg.new.$$"; then
	true
    else
	cp "$cfg.new.$$" "$cfg"
        ifname=`echo "$cfg" | sed -e 's!^.*/ifcfg-\(.*\)$!\1!'`
	echo "Restored $ifname to be brought up at boot time"
    fi
    rm "$cfg.new.$$" "$cfg.pre-netplug"
done

%changelog
* Tue Aug 26 2003 Bryan O'Sullivan <bos@serpentine.com> - 
- Initial build.

Name:           ltrace
BuildRequires:  binutils-devel
BuildRequires:  dejagnu
BuildRequires:  gcc-c++
BuildRequires:  libelf-devel
Url:            http://ltrace.org/
Summary:        Trace the Library and System Calls a Program Makes
License:        GPL-2.0+
Group:          Development/Tools/Debuggers
Version:        0.5.3
Release:        0
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source:         ltrace-%{version}.tar.bz2
Source2:        baselibs.conf

%description
Ltrace is a program that runs the specified command until it exits. It
intercepts and records the dynamic library calls that are called by the
executed process and the signals that are received by that process. It
can also intercept and print the system calls executed by the program.

The program to trace need not be recompiled for this, so you can use
ltrace on binaries for which you do not have access to the source.

This is still a work in progress, so, for example, the tracking to
child processes may fail or some things may not work as expected.


%prep
%setup -q

%build
export CFLAGS="%{optflags} -W -Wall"
./configure --prefix=/usr \
	    --sysconfdir=/etc \
	    --mandir=%{_mandir} \
%ifarch armv4l
	    --build=arm-tizen-linux
%else
	    --build=%{_target_cpu}-tizen-linux
%endif
make

%install
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}/usr/share/doc/ltrace

%files
%defattr(-,root,root)
%doc COPYING README 
%{_bindir}/ltrace
%{_mandir}/man1/ltrace.1.gz
%config /etc/ltrace.conf

%changelog

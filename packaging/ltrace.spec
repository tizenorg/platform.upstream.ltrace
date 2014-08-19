Name:           ltrace
Version:        0.7.2
Release:        0
BuildRequires:  binutils-devel
BuildRequires:  gcc-c++
BuildRequires:  libelf-devel
Url:            http://ltrace.org/
Summary:        Trace the Library and System Calls a Program Makes
License:        GPL-2.0+
Group:          Development/Tools
Source:         ltrace-%{version}.tar.bz2
Source2:        baselibs.conf
Source1001: 	ltrace.manifest

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
cp %{SOURCE1001} .

%build
export CFLAGS="%{optflags} -Wall -Wno-unused-local-typedefs"
/bin/sh ./autogen.sh
%configure --build=%{_target_cpu}-tizen-linux
make

%install
%make_install
rm -rf %{buildroot}/usr/share/doc/ltrace

%files
%manifest %{name}.manifest
%defattr(-,root,root)
%license COPYING
%{_bindir}/ltrace
%{_mandir}/man?/ltrace.?.gz
%{_mandir}/man?/ltrace.conf.?.gz
%config /usr/share/ltrace/syscalls.conf
%config /usr/share/ltrace/libc.so.conf
%config /usr/share/ltrace/libm.so.conf
%config /usr/share/ltrace/libacl.so.conf

%changelog

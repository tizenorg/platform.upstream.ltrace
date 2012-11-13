#
# spec file for package ltrace
#
# Copyright (c) 2012 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           ltrace
BuildRequires:  binutils-devel
BuildRequires:  dejagnu
BuildRequires:  gcc-c++
BuildRequires:  libelf-devel
Url:            http://ltrace.org/
# bug437293
%ifarch ppc64
Obsoletes:      ltrace-64bit
%endif
#
Summary:        Trace the Library and System Calls a Program Makes
License:        GPL-2.0+
Group:          Development/Tools/Debuggers
Version:        0.5.3
Release:        0
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
ExclusiveArch:  %ix86 s390 s390x ppc ppc64 armv4l x86_64 alpha ia64
Prefix:         /usr
Source:         ltrace-%{version}.tar.bz2
Source2:        baselibs.conf
Patch1:         ltrace.s390-more-arguments.patch
Patch2:         ltrace.demangle-lib.cstdlib.patch
Patch3:         ltrace.ppc.patch

%description
Ltrace is a program that runs the specified command until it exits. It
intercepts and records the dynamic library calls that are called by the
executed process and the signals that are received by that process. It
can also intercept and print the system calls executed by the program.

The program to trace need not be recompiled for this, so you can use
ltrace on binaries for which you do not have access to the source.

This is still a work in progress, so, for example, the tracking to
child processes may fail or some things may not work as expected.



Authors:
--------
    Juan Cespedes

%prep
%setup -q
%patch1 -p1
%patch2 -p1
%patch3 -p1

%build
export CFLAGS="%{optflags} -W -Wall"
./configure --prefix=/usr \
	    --sysconfdir=/etc \
	    --mandir=%{_mandir} \
%ifarch armv4l
	    --build=arm-suse-linux
%else
	    --build=%{_target_cpu}-suse-linux
%endif
make
%if 1
if make check
then
	echo 'no make check errors' > testresults.txt
else
	for file in `find testsuite -name "*.ltrace"`
	do
		echo
		echo $file
		echo
		cat $file
		echo
	done >> testresults.txt
fi
mv testresults.txt %{_target_cpu}-testresults.txt
ln testsuite/testrun.sum testsuite/%{_target_cpu}-testrun.sum
%else
echo no make check > %{_target_cpu}-testresults.txt
echo no make check > testsuite/%{_target_cpu}-testrun.sum
%endif

%install
make install DESTDIR=%{buildroot}
rm -rf %{buildroot}/usr/share/doc/ltrace

%files
%defattr(-,root,root)
%doc COPYING README ChangeLog %{_target_cpu}-testresults.txt testsuite/%{_target_cpu}-testrun.sum
%{_bindir}/ltrace
%{_mandir}/man1/ltrace.1.gz
%config /etc/ltrace.conf

%changelog

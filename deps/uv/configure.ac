# Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AC_PREREQ(2.57)
AC_INIT([libuv], [1.16.1], [https://github.com/libuv/libuv/issues])
AC_CONFIG_MACRO_DIR([m4])
m4_include([m4/libuv-extra-automake-flags.m4])
m4_include([m4/as_case.m4])
m4_include([m4/libuv-check-flags.m4])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects] UV_EXTRA_AUTOMAKE_FLAGS)
AC_CANONICAL_HOST
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_PROG_CC
AM_PROG_CC_C_O
AS_IF([AS_CASE([$host_os],[openedition*],  [false], [true])], [
  CC_CHECK_CFLAGS_APPEND([-pedantic])
])
CC_FLAG_VISIBILITY #[-fvisibility=hidden]
CC_CHECK_CFLAGS_APPEND([-g])
CC_CHECK_CFLAGS_APPEND([-std=gnu89])
CC_CHECK_CFLAGS_APPEND([-Wall])
CC_CHECK_CFLAGS_APPEND([-Wextra])
CC_CHECK_CFLAGS_APPEND([-Wno-unused-parameter])
CC_CHECK_CFLAGS_APPEND([-Wstrict-prototypes])
# AM_PROG_AR is not available in automake v0.11 but it's essential in v0.12.
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
# autoconf complains if AC_PROG_LIBTOOL precedes AM_PROG_AR.
AC_PROG_LIBTOOL
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
LT_INIT
# TODO(bnoordhuis) Check for -pthread vs. -pthreads
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([kstat], [kstat_lookup])
AC_CHECK_LIB([nsl], [gethostbyname])
AC_CHECK_LIB([perfstat], [perfstat_cpu])
AC_CHECK_LIB([pthread], [pthread_mutex_init])
AC_CHECK_LIB([rt], [clock_gettime])
AC_CHECK_LIB([sendfile], [sendfile])
AC_CHECK_LIB([socket], [socket])
AC_SYS_LARGEFILE
AM_CONDITIONAL([AIX],      [AS_CASE([$host_os],[aix*],          [true], [false])])
AM_CONDITIONAL([ANDROID],  [AS_CASE([$host_os],[linux-android*],[true], [false])])
AM_CONDITIONAL([CYGWIN],   [AS_CASE([$host_os],[cygwin*],       [true], [false])])
AM_CONDITIONAL([DARWIN],   [AS_CASE([$host_os],[darwin*],       [true], [false])])
AM_CONDITIONAL([DRAGONFLY],[AS_CASE([$host_os],[dragonfly*],    [true], [false])])
AM_CONDITIONAL([FREEBSD],  [AS_CASE([$host_os],[*freebsd*],     [true], [false])])
AM_CONDITIONAL([LINUX],    [AS_CASE([$host_os],[linux*],        [true], [false])])
AM_CONDITIONAL([MSYS],     [AS_CASE([$host_os],[msys*],         [true], [false])])
AM_CONDITIONAL([NETBSD],   [AS_CASE([$host_os],[netbsd*],       [true], [false])])
AM_CONDITIONAL([OPENBSD],  [AS_CASE([$host_os],[openbsd*],      [true], [false])])
AM_CONDITIONAL([OS390],    [AS_CASE([$host_os],[openedition*],  [true], [false])])
AM_CONDITIONAL([SUNOS],    [AS_CASE([$host_os],[solaris*],      [true], [false])])
AM_CONDITIONAL([WINNT],    [AS_CASE([$host_os],[mingw*],        [true], [false])])
AS_CASE([$host_os],[mingw*], [
    LIBS="$LIBS -lws2_32 -lpsapi -liphlpapi -lshell32 -luserenv -luser32"
])
AS_CASE([$host_os], [netbsd*], [AC_CHECK_LIB([kvm], [kvm_open])])
AC_CHECK_HEADERS([sys/ahafs_evProds.h])
AC_CHECK_PROG(PKG_CONFIG, pkg-config, yes)
AM_CONDITIONAL([HAVE_PKG_CONFIG], [test "x$PKG_CONFIG" != "x"])
AS_IF([test "x$PKG_CONFIG" != "x"], [
    AC_CONFIG_FILES([libuv.pc])
])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

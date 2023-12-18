#############################################################
#
# Copyright (C) the Massachusetts Institute of Technology.
# Copyright (C) Daniel Stenberg
#
# Permission to use, copy, modify, and distribute this
# software and its documentation for any purpose and without
# fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright
# notice and this permission notice appear in supporting
# documentation, and that the name of M.I.T. not be used in
# advertising or publicity pertaining to distribution of the
# software without specific, written prior permission.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is"
# without express or implied warranty.
#
# SPDX-License-Identifier: MIT
#
#############################################################
AC_PREREQ([2.60])

AC_INIT([c-ares], [1.24.0],
  [c-ares mailing list: http://lists.haxx.se/listinfo/c-ares])

CARES_VERSION_INFO="12:0:10"
dnl This flag accepts an argument of the form current[:revision[:age]]. So,
dnl passing -version-info 3:12:1 sets current to 3, revision to 12, and age to
dnl 1.
dnl
dnl If either revision or age are omitted, they default to 0. Also note that age
dnl must be less than or equal to the current interface number.
dnl
dnl Here are a set of rules to help you update your library version information:
dnl
dnl 1.Start with version information of 0:0:0 for each libtool library.
dnl
dnl 2.Update the version information only immediately before a public release of
dnl your software. More frequent updates are unnecessary, and only guarantee
dnl that the current interface number gets larger faster.
dnl
dnl 3.If the library source code has changed at all since the last update, then
dnl increment revision (c:r+1:a)
dnl
dnl 4.If any interfaces have been added, removed, or changed since the last
dnl update, increment current, and set revision to 0. (c+1:r=0:a)
dnl
dnl 5.If any interfaces have been added since the last public release, then
dnl increment age. (c:r:a+1)
dnl
dnl 6.If any interfaces have been removed since the last public release, then
dnl set age to 0. (c:r:a=0)
dnl
AC_SUBST([CARES_VERSION_INFO])

AC_CONFIG_SRCDIR([src/lib/ares_ipv6.h])
AC_CONFIG_HEADERS([src/lib/ares_config.h include/ares_build.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AX_REQUIRE_DEFINED([AX_CXX_COMPILE_STDCXX_14])

CARES_CHECK_OPTION_DEBUG
CARES_CHECK_OPTION_OPTIMIZE
CARES_CHECK_OPTION_WARNINGS
CARES_CHECK_OPTION_WERROR
CARES_CHECK_OPTION_SYMBOL_HIDING
CARES_CHECK_OPTION_EXPOSE_STATICS

XC_CHECK_PATH_SEPARATOR

dnl SED is mandatory for configure process and libtool.
dnl Set it now, allowing it to be changed later.
AC_PATH_PROG([SED], [sed], [not_found],
  [$PATH:/usr/bin:/usr/local/bin])
if test -z "$SED" || test "$SED" = "not_found"; then
  AC_MSG_ERROR([sed not found in PATH. Cannot continue without sed.])
fi
AC_SUBST([SED])

dnl GREP is mandatory for configure process and libtool.
dnl Set it now, allowing it to be changed later.
AC_PATH_PROG([GREP], [grep], [not_found],
  [$PATH:/usr/bin:/usr/local/bin])
if test -z "$GREP" || test "$GREP" = "not_found"; then
  AC_MSG_ERROR([grep not found in PATH. Cannot continue without grep.])
fi
AC_SUBST([GREP])

dnl EGREP is mandatory for configure process and libtool.
dnl Set it now, allowing it to be changed later.
if echo a | ($GREP -E '(a|b)') >/dev/null 2>&1; then
  AC_MSG_CHECKING([for egrep])
  EGREP="$GREP -E"
  AC_MSG_RESULT([$EGREP])
else
  AC_PATH_PROG([EGREP], [egrep], [not_found],
    [$PATH:/usr/bin:/usr/local/bin])
fi
if test -z "$EGREP" || test "$EGREP" = "not_found"; then
  AC_MSG_ERROR([egrep not found in PATH. Cannot continue without egrep.])
fi
AC_SUBST([EGREP])

dnl AR is mandatory for configure process and libtool.
dnl This is target dependent, so check it as a tool.
if test -z "$AR"; then
  dnl allow it to be overridden
  AC_PATH_TOOL([AR], [ar], [not_found],
    [$PATH:/usr/bin:/usr/local/bin])
  if test -z "$AR" || test "$AR" = "not_found"; then
    AC_MSG_ERROR([ar not found in PATH. Cannot continue without ar.])
  fi
fi
AC_SUBST([AR])

AX_CODE_COVERAGE


dnl
dnl Detect the canonical host and target build environment
dnl

AC_CANONICAL_HOST
dnl Get system canonical name
AC_DEFINE_UNQUOTED(OS, "${host}", [cpu-machine-OS])

XC_CHECK_PROG_CC
AX_CXX_COMPILE_STDCXX_14([noext],[optional])

XC_AUTOMAKE

dnl make sure we use system extensions like _GNU_SOURCE or _XOPEN_SOURCE
AC_USE_SYSTEM_EXTENSIONS

dnl This defines _ALL_SOURCE for AIX
CARES_CHECK_AIX_ALL_SOURCE

dnl Our configure and build reentrant settings
CARES_CONFIGURE_THREAD_SAFE
CARES_CONFIGURE_REENTRANT

dnl check for how to do large files
AC_SYS_LARGEFILE

case $host_os in
  solaris*)
    AC_DEFINE(ETC_INET, 1, [if a /etc/inet dir is being used])
    ;;
esac

XC_LIBTOOL


#
# Automake conditionals based on libtool related checks
#

AM_CONDITIONAL([CARES_LT_SHLIB_USE_VERSION_INFO],
  [test "x$xc_lt_shlib_use_version_info" = 'xyes'])
AM_CONDITIONAL([CARES_LT_SHLIB_USE_NO_UNDEFINED],
  [test "x$xc_lt_shlib_use_no_undefined" = 'xyes'])
AM_CONDITIONAL([CARES_LT_SHLIB_USE_MIMPURE_TEXT],
  [test "x$xc_lt_shlib_use_mimpure_text" = 'xyes'])

#
# Due to libtool and automake machinery limitations of not allowing
# specifying separate CPPFLAGS or CFLAGS when compiling objects for
# inclusion of these in shared or static libraries, we are forced to
# build using separate configure runs for shared and static libraries
# on systems where different CPPFLAGS or CFLAGS are mandatory in order
# to compile objects for each kind of library. Notice that relying on
# the '-DPIC' CFLAG that libtool provides is not valid given that the
# user might for example choose to build static libraries with PIC.
#

#
# Make our Makefile.am files use the staticlib CPPFLAG only when strictly
# targeting a static library and not building its shared counterpart.
#

AM_CONDITIONAL([USE_CPPFLAG_CARES_STATICLIB],
  [test "x$xc_lt_build_static_only" = 'xyes'])

#
# Make staticlib CPPFLAG variable and its definition visible in output
# files unconditionally, providing an empty definition unless strictly
# targeting a static library and not building its shared counterpart.
#

CPPFLAG_CARES_STATICLIB=
if test "x$xc_lt_build_static_only" = 'xyes'; then
  CPPFLAG_CARES_STATICLIB='-DCARES_STATICLIB'
fi
AC_SUBST([CPPFLAG_CARES_STATICLIB])

dnl **********************************************************************
dnl platform/compiler/architecture specific checks/flags
dnl **********************************************************************

CARES_CHECK_COMPILER
CARES_SET_COMPILER_BASIC_OPTS
CARES_SET_COMPILER_DEBUG_OPTS
CARES_SET_COMPILER_OPTIMIZE_OPTS
CARES_SET_COMPILER_WARNING_OPTS

if test "$want_warnings" = "yes"; then
  AX_APPEND_COMPILE_FLAGS([-Wall \
    -Wextra \
    -Wcast-align \
    -Wconversion \
    -Wdeclaration-after-statement \
    -Wdouble-promotion \
    -Wfloat-equal \
    -Wformat-security \
    -Winit-self \
    -Wjump-misses-init \
    -Wlogical-op \
    -Wmissing-braces \
    -Wmissing-declarations \
    -Wmissing-format-attribute \
    -Wmissing-include-dirs \
    -Wmissing-prototypes \
    -Wnested-externs \
    -Wno-coverage-mismatch \
    -Wold-style-definition \
    -Wpacked \
    -Wpointer-arith \
    -Wredundant-decls \
    -Wshadow \
    -Wsign-conversion \
    -Wstrict-overflow \
    -Wstrict-prototypes \
    -Wtrampolines \
    -Wundef \
    -Wunused \
    -Wvariadic-macros \
    -Wvla \
    -Wwrite-strings \
    -Werror=implicit-int \
    -Werror=implicit-function-declaration \
    -Werror=partial-availability \
  ], [CFLAGS], [-Werror])
fi

if test "$compiler_id" = "INTEL_UNIX_C"; then
  #
  if test "$compiler_num" -ge "1000"; then
    dnl icc 10.X or later
    CFLAGS="$CFLAGS -shared-intel"
  elif test "$compiler_num" -ge "900"; then
    dnl icc 9.X specific
    CFLAGS="$CFLAGS -i-dynamic"
  fi
  #
fi

CARES_CHECK_COMPILER_HALT_ON_ERROR
CARES_CHECK_COMPILER_ARRAY_SIZE_NEGATIVE
CARES_CHECK_COMPILER_PROTOTYPE_MISMATCH
CARES_CHECK_COMPILER_SYMBOL_HIDING

dnl **********************************************************************
dnl Compilation based checks should not be done before this point.
dnl **********************************************************************

dnl **********************************************************************
dnl Make sure that our checks for headers windows.h winsock.h winsock2.h
dnl and ws2tcpip.h take precedence over any other further checks which
dnl could be done later using AC_CHECK_HEADER or AC_CHECK_HEADERS for
dnl this specific header files. And do them before its results are used.
dnl **********************************************************************

CURL_CHECK_HEADER_WINDOWS
CURL_CHECK_NATIVE_WINDOWS
case X-"$ac_cv_native_windows" in
  X-yes)
    CURL_CHECK_HEADER_WINSOCK
    CURL_CHECK_HEADER_WINSOCK2
    CURL_CHECK_HEADER_WS2TCPIP
    CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0602"
    ;;
  *)
    ac_cv_header_winsock_h="no"
    ac_cv_header_winsock2_h="no"
    ac_cv_header_ws2tcpip_h="no"
    ;;
esac

dnl **********************************************************************
dnl Checks for libraries.
dnl **********************************************************************

CARES_CHECK_LIB_XNET

dnl gethostbyname without lib or in the nsl lib?
AC_CHECK_FUNC(gethostbyname,
              [HAVE_GETHOSTBYNAME="1"
              ],
              [ AC_CHECK_LIB(nsl, gethostbyname,
                             [HAVE_GETHOSTBYNAME="1"
                             LIBS="$LIBS -lnsl"
                             ])
              ])

if test "$HAVE_GETHOSTBYNAME" != "1"
then
  dnl gethostbyname in the socket lib?
  AC_CHECK_LIB(socket, gethostbyname,
               [HAVE_GETHOSTBYNAME="1"
               LIBS="$LIBS -lsocket"
               ])
fi

dnl At least one system has been identified to require BOTH nsl and socket
dnl libs at the same time to link properly.
if test "$HAVE_GETHOSTBYNAME" != "1"
then
  AC_MSG_CHECKING([for gethostbyname with both nsl and socket libs])
  my_ac_save_LIBS=$LIBS
  LIBS="-lnsl -lsocket $LIBS"
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
      gethostbyname();
    ]])
  ],[
    AC_MSG_RESULT([yes])
    HAVE_GETHOSTBYNAME="1"
  ],[
    AC_MSG_RESULT([no])
    LIBS=$my_ac_save_LIBS
  ])
fi

if test "$HAVE_GETHOSTBYNAME" != "1"
then
  dnl This is for winsock systems
  if test "$ac_cv_header_windows_h" = "yes"; then
    if test "$ac_cv_header_winsock_h" = "yes"; then
      case $host in
        *-*-mingw32ce*)
          winsock_LIB="-lwinsock"
          ;;
        *)
          winsock_LIB="-lwsock32"
          ;;
      esac
    fi
    if test "$ac_cv_header_winsock2_h" = "yes"; then
      winsock_LIB="-lws2_32"
    fi
    if test ! -z "$winsock_LIB"; then
      my_ac_save_LIBS=$LIBS
      LIBS="$winsock_LIB $LIBS"
      AC_MSG_CHECKING([for gethostbyname in $winsock_LIB])
      AC_LINK_IFELSE([
        AC_LANG_PROGRAM([[
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#else
#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#endif
#endif
#endif
        ]],[[
          gethostbyname("www.dummysite.com");
        ]])
      ],[
        AC_MSG_RESULT([yes])
        HAVE_GETHOSTBYNAME="1"
      ],[
        AC_MSG_RESULT([no])
        winsock_LIB=""
        LIBS=$my_ac_save_LIBS
      ])
    fi
  fi
fi

if test "$HAVE_GETHOSTBYNAME" != "1"
then
  dnl This is for Minix 3.1
  AC_MSG_CHECKING([for gethostbyname for Minix 3])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
/* Older Minix versions may need <net/gen/netdb.h> here instead */
#include <netdb.h>
    ]],[[
      gethostbyname("www.dummysite.com");
    ]])
  ],[
    AC_MSG_RESULT([yes])
    HAVE_GETHOSTBYNAME="1"
  ],[
    AC_MSG_RESULT([no])
  ])
fi

if test "$HAVE_GETHOSTBYNAME" != "1"
then
  dnl This is for eCos with a stubbed DNS implementation
  AC_MSG_CHECKING([for gethostbyname for eCos])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
#include <stdio.h>
#include <netdb.h>
    ]],[[
      gethostbyname("www.dummysite.com");
    ]])
  ],[
    AC_MSG_RESULT([yes])
    HAVE_GETHOSTBYNAME="1"
  ],[
    AC_MSG_RESULT([no])
  ])
fi

if test "$HAVE_GETHOSTBYNAME" != "1"
then
  dnl gethostbyname in the net lib - for BeOS
  AC_CHECK_LIB(net, gethostbyname,
               [HAVE_GETHOSTBYNAME="1"
               LIBS="$LIBS -lnet"
               ])
fi


if test "$HAVE_GETHOSTBYNAME" != "1"; then
  AC_MSG_ERROR([couldn't find libraries for gethostbyname()])
fi

dnl resolv lib for Apple (MacOS and iOS)
AS_IF([test "x$host_vendor" = "xapple"], [
  AC_SEARCH_LIBS([res_servicename], [resolv], [
    AC_DEFINE([CARES_USE_LIBRESOLV], [1], [Use resolver library to configure cares])
  ], [
    AC_MSG_ERROR([Unable to find libresolv which is required for iPhone targets])
  ])
])

dnl resolv lib for z/OS
AS_IF([test "x$host_vendor" = "xibm" -a "x$host_os" = "xopenedition" ], [
  AC_SEARCH_LIBS([res_init], [resolv], [
    AC_DEFINE([CARES_USE_LIBRESOLV], [1], [Use resolver library to configure cares])
  ], [
    AC_MSG_ERROR([Unable to find libresolv which is required for z/OS])
  ])
])

dnl resolve lib?
AC_CHECK_FUNC(strcasecmp, , [ AC_CHECK_LIB(resolve, strcasecmp) ])

if test "$ac_cv_lib_resolve_strcasecmp" = "$ac_cv_func_strcasecmp"; then
  AC_CHECK_LIB(resolve, strcasecmp,
              [LIBS="-lresolve $LIBS"],
               ,
               -lnsl)
fi
ac_cv_func_strcasecmp="no"

dnl Windows builds require linking to iphlpapi
if test "$ac_cv_header_winsock2_h" = "yes"; then
  LIBS="$LIBS -liphlpapi"
fi

CARES_CHECK_LIBS_CONNECT

dnl iOS 10?
AS_IF([test "x$host_vendor" = "xapple"], [
  AC_MSG_CHECKING([for iOS minimum version 10 or later])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#include <stdio.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
    ]], [[
#if TARGET_OS_IPHONE == 0 || __IPHONE_OS_VERSION_MIN_REQUIRED < 100000
#error Not iOS 10 or later
#endif
return 0;
   ]])
  ],[
    AC_MSG_RESULT([yes])
    ac_cv_ios_10="yes"
  ],[
    AC_MSG_RESULT([no])
  ])
])

dnl macOS 10.12?
AS_IF([test "x$host_vendor" = "xapple"], [
  AC_MSG_CHECKING([for macOS minimum version 10.12 or later])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#include <stdio.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
    ]], [[
#ifndef MAC_OS_X_VERSION_10_12
#  define MAC_OS_X_VERSION_10_12 101200
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#error Not macOS 10.12 or later
#endif
return 0;
   ]])
  ],[
    AC_MSG_RESULT([yes])
    ac_cv_macos_10_12="yes"
  ],[
    AC_MSG_RESULT([no])
  ])
])

dnl **********************************************************************
dnl In case that function clock_gettime with monotonic timer is available,
dnl check for additional required libraries.
dnl **********************************************************************
dnl Xcode 8 bug: iOS when targeting less than 10, or macOS when targeting less than 10.12 will
dnl say clock_gettime exists, it is a weak symbol that only exists in iOS 10 or macOS 10.12 and will
dnl cause a crash at runtime when running on older versions.  Skip finding CLOCK_MONOTONIC on older
dnl Apple OS's.
if test "x$host_vendor" != "xapple" || test "x$ac_cv_ios_10" = "xyes" || test "x$ac_cv_macos_10_12" = "xyes"; then
  CURL_CHECK_LIBS_CLOCK_GETTIME_MONOTONIC
fi

AC_MSG_CHECKING([whether to use libgcc])
AC_ARG_ENABLE(libgcc,
AS_HELP_STRING([--enable-libgcc],[use libgcc when linking]),
[ case "$enableval" in
  yes)
        LIBS="$LIBS -lgcc"
       AC_MSG_RESULT(yes)
       ;;
  *)   AC_MSG_RESULT(no)
       ;;
  esac ],
       AC_MSG_RESULT(no)
)


dnl Let's hope this split URL remains working:
dnl http://publibn.boulder.ibm.com/doc_link/en_US/a_doc_lib/aixprggd/ \
dnl genprogc/thread_quick_ref.htm


dnl **********************************************************************
dnl Back to "normal" configuring
dnl **********************************************************************

dnl Checks for header files.
AC_HEADER_STDC

AC_PROG_EGREP


CURL_CHECK_HEADER_MALLOC
CURL_CHECK_HEADER_MEMORY

dnl check for a few basic system headers we need
AC_CHECK_HEADERS(
       sys/types.h \
       sys/time.h \
       sys/select.h \
       sys/socket.h \
       sys/ioctl.h \
       sys/param.h \
       sys/uio.h \
       assert.h \
       iphlpapi.h \
       netioapi.h \
       netdb.h \
       netinet/in.h \
       netinet/tcp.h \
       net/if.h \
       ifaddrs.h \
       errno.h \
       socket.h \
       strings.h \
       stdbool.h \
       time.h \
       limits.h \
       arpa/nameser.h \
       arpa/nameser_compat.h \
       arpa/inet.h,
dnl to do if not found
[],
dnl to do if found
[],
dnl default includes
[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
dnl We do this default-include simply to make sure that the nameser_compat.h
dnl header *REALLY* can be include after the new nameser.h. It seems AIX 5.1
dnl (and others?) is not designed to allow this.
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

dnl *Sigh* these are needed in order for net/if.h to get properly detected.
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
]
)

dnl Test and set CARES_HAVE_ARPA_NAMESER_H / CARES_HAVE_ARPA_NAMESER_COMPAT_H
AC_CHECK_DECL([HAVE_ARPA_NAMESER_H],
[
CARES_DEFINE_UNQUOTED([CARES_HAVE_ARPA_NAMESER_H])
],
[]
)
AC_CHECK_DECL([HAVE_ARPA_NAMESER_COMPAT_H],
[
CARES_DEFINE_UNQUOTED([CARES_HAVE_ARPA_NAMESER_COMPAT_H])
],
[]
)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
m4_warn([obsolete],
[Update your code to rely only on HAVE_SYS_TIME_H,
then remove this warning and the obsolete code below it.
All current systems provide time.h; it need not be checked for.
Not all systems provide sys/time.h, but those that do, all allow
you to include it and time.h simultaneously.])dnl
AC_CHECK_HEADERS_ONCE([sys/time.h])
# Obsolete code to be removed.
if test $ac_cv_header_sys_time_h = yes; then
  AC_DEFINE([TIME_WITH_SYS_TIME],[1],[Define to 1 if you can safely include both <sys/time.h>
	     and <time.h>.  This macro is obsolete.])
fi
# End of obsolete code.

CURL_CHECK_STRUCT_TIMEVAL

AC_CHECK_TYPE(long long,
   [AC_DEFINE(HAVE_LONGLONG, 1,
     [Define to 1 if the compiler supports the 'long long' data type.])]
   longlong="yes"
)

if test "xyes" = "x$longlong"; then
  AC_MSG_CHECKING([if numberLL works])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
      long long val = 1000LL;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_LL, 1, [if your compiler supports LL])
  ],[
    AC_MSG_RESULT([no])
  ])
fi


# check for ssize_t
AC_CHECK_TYPE(ssize_t, [ CARES_TYPEOF_ARES_SSIZE_T=ssize_t ],
  [ CARES_TYPEOF_ARES_SSIZE_T=int ])

AC_DEFINE_UNQUOTED([CARES_TYPEOF_ARES_SSIZE_T], ${CARES_TYPEOF_ARES_SSIZE_T},
  [the signed version of size_t])


# check for bool type
AC_CHECK_TYPE([bool],[
  AC_DEFINE(HAVE_BOOL_T, 1,
    [Define to 1 if bool is an available type.])
], ,[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif
])

CARES_CONFIGURE_ARES_SOCKLEN_T

TYPE_IN_ADDR_T

TYPE_SOCKADDR_STORAGE

TYPE_SIG_ATOMIC_T

m4_warn([obsolete],
[your code may safely assume C89 semantics that RETSIGTYPE is void.
Remove this warning and the `AC_CACHE_CHECK' when you adjust the code.])dnl
AC_CACHE_CHECK([return type of signal handlers],[ac_cv_type_signal],[AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#include <sys/types.h>
#include <signal.h>
],
		 [return *(signal (0, 0)) (0) == 1;])],
		   [ac_cv_type_signal=int],
		   [ac_cv_type_signal=void])])
AC_DEFINE_UNQUOTED([RETSIGTYPE],[$ac_cv_type_signal],[Define as the return type of signal handlers
		    (`int' or `void').])


CURL_CHECK_FUNC_RECV
CURL_CHECK_FUNC_RECVFROM
CURL_CHECK_FUNC_SEND
CURL_CHECK_MSG_NOSIGNAL

CARES_CHECK_FUNC_CLOSESOCKET
CARES_CHECK_FUNC_CLOSESOCKET_CAMEL
CARES_CHECK_FUNC_CONNECT
CARES_CHECK_FUNC_FCNTL
CARES_CHECK_FUNC_FREEADDRINFO
CARES_CHECK_FUNC_GETADDRINFO
CARES_CHECK_FUNC_GETENV
CARES_CHECK_FUNC_GETHOSTBYADDR
CARES_CHECK_FUNC_GETHOSTBYNAME
CARES_CHECK_FUNC_GETHOSTNAME
CARES_CHECK_FUNC_GETRANDOM
CARES_CHECK_FUNC_GETSERVBYPORT_R
CARES_CHECK_FUNC_INET_NET_PTON
CARES_CHECK_FUNC_INET_NTOP
CARES_CHECK_FUNC_INET_PTON
CARES_CHECK_FUNC_IOCTL
CARES_CHECK_FUNC_IOCTLSOCKET
CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL
CARES_CHECK_FUNC_SETSOCKOPT
CARES_CHECK_FUNC_SOCKET
CARES_CHECK_FUNC_STRCASECMP
CARES_CHECK_FUNC_STRCMPI
CARES_CHECK_FUNC_STRDUP
CARES_CHECK_FUNC_STRICMP
CARES_CHECK_FUNC_STRNCASECMP
CARES_CHECK_FUNC_STRNCMPI
CARES_CHECK_FUNC_STRNICMP
CARES_CHECK_FUNC_WRITEV
CARES_CHECK_FUNC_ARC4RANDOM_BUF
CARES_CHECK_FUNC_STAT
CARES_CHECK_FUNC_GETIFADDRS

dnl check for AF_INET6
CARES_CHECK_CONSTANT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
  ], [PF_INET6],
     AC_DEFINE_UNQUOTED(HAVE_PF_INET6,1,[Define to 1 if you have PF_INET6.])
)

dnl check for PF_INET6
CARES_CHECK_CONSTANT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
  ], [AF_INET6],
     AC_DEFINE_UNQUOTED(HAVE_AF_INET6,1,[Define to 1 if you have AF_INET6.])
)


dnl check for the in6_addr structure
CARES_CHECK_STRUCT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
  ], [in6_addr],
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_IN6_ADDR,1,[Define to 1 if you have struct in6_addr.])
)

dnl check for the sockaddr_in6 structure
CARES_CHECK_STRUCT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
  ], [sockaddr_in6],
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_SOCKADDR_IN6,1,
       [Define to 1 if you have struct sockaddr_in6.]) ac_have_sockaddr_in6=yes
)

AC_CHECK_MEMBER(struct sockaddr_in6.sin6_scope_id,
    AC_DEFINE_UNQUOTED(HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID,1,
      [Define to 1 if your struct sockaddr_in6 has sin6_scope_id.])
   , ,
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
  ])

dnl check for the addrinfo structure
AC_CHECK_MEMBER(struct addrinfo.ai_flags,
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_ADDRINFO,1,
       [Define to 1 if you have struct addrinfo.]),,
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#endif
  ]
)


AC_CHECK_FUNCS([bitncmp \
  gettimeofday \
  if_indextoname \
  if_nametoindex \
  ConvertInterfaceIndexToLuid \
  ConvertInterfaceLuidToNameA
],[
],[
  func="$ac_func"
  AC_MSG_CHECKING([deeper for $func])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
      $func ();
    ]])
  ],[
    AC_MSG_RESULT([yes])
    eval "ac_cv_func_$func=yes"
    AC_DEFINE_UNQUOTED(XC_SH_TR_CPP([HAVE_$func]), [1],
      [Define to 1 if you have the $func function.])
  ],[
    AC_MSG_RESULT([but still no])
  ])
])

dnl Android. Some variants like arm64 may no longer have __system_property_get
dnl in libc, but they are defined in the headers.  Perform a link check.
AC_CHECK_FUNC([__system_property_get], [
    AC_DEFINE([HAVE___SYSTEM_PROPERTY_GET], [1], [Define if __system_property_get exists.])
])

dnl Check if the getnameinfo function is available
dnl and get the types of five of its arguments.
CURL_CHECK_FUNC_GETNAMEINFO


AC_C_BIGENDIAN(
    [AC_DEFINE(ARES_BIG_ENDIAN, 1,
      [define this if ares is built for a big endian system])],
    ,
    [AC_MSG_WARN([couldn't figure out endianness, assuming little endian!])]
)

dnl Check for user-specified random device
AC_ARG_WITH(random,
AS_HELP_STRING([--with-random=FILE],
               [read randomness from FILE (default=/dev/urandom)]),
    [ CARES_RANDOM_FILE="$withval" ],
    [ CARES_RANDOM_FILE="/dev/urandom" ]
)
if test -n "$CARES_RANDOM_FILE" && test X"$CARES_RANDOM_FILE" != Xno ; then
        AC_SUBST(CARES_RANDOM_FILE)
        AC_DEFINE_UNQUOTED(CARES_RANDOM_FILE, "$CARES_RANDOM_FILE",
        [a suitable file/device to read random data from])
fi

CARES_CHECK_OPTION_NONBLOCKING
CARES_CHECK_NONBLOCKING_SOCKET


dnl ------------ THREADING --------------
AC_ARG_ENABLE(cares-threads,
  AC_HELP_STRING([--disable-cares-threads], [Disable building of thread safety support]),
  [ CARES_THREADS=${enableval} ],
  [ CARES_THREADS=yes ])

dnl windows always supports threads, only check non-windows systems.
if test "${CARES_THREADS}" = "yes" -a "x${ac_cv_native_windows}" != "xyes" ; then
  AX_PTHREAD([ ], [
    AC_MSG_WARN([threads requested but not supported])
    CARES_THREADS=no
  ])

  if test "${CARES_THREADS}" = "yes" ; then
    AC_CHECK_HEADERS([pthread.h pthread_np.h])
    LIBS="$PTHREAD_LIBS $LIBS"
    CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
    CC="$PTHREAD_CC"
    CXX="$PTHREAD_CXX"
  fi
fi

if test "${CARES_THREADS}" = "yes" ; then
  AC_DEFINE([CARES_THREADS], [ 1 ], [Threading enabled])
fi

CARES_CONFIGURE_SYMBOL_HIDING

CARES_PRIVATE_LIBS="$LIBS"
AC_SUBST(CARES_PRIVATE_LIBS)

CARES_CFLAG_EXTRAS=""
if test X"$want_werror" = Xyes; then
  CARES_CFLAG_EXTRAS="-Werror"
fi
AC_SUBST(CARES_CFLAG_EXTRAS)

dnl squeeze whitespace out of some variables

squeeze CFLAGS
squeeze CPPFLAGS
squeeze DEFS
squeeze LDFLAGS
squeeze LIBS

squeeze CARES_PRIVATE_LIBS

XC_CHECK_BUILD_FLAGS

AC_MSG_CHECKING([whether to build tests])
AC_ARG_ENABLE(tests,
	AS_HELP_STRING([--enable-tests], [build test suite]),
	[ build_tests="$enableval" ],
	[ if test "x$HAVE_CXX14" = "x1" && test "x$cross_compiling" = "xno" ; then
	    build_tests="yes"
	  else
	    build_tests="no"
	  fi
	]
)

if test "x$build_tests" = "xyes" ; then
	if test "x$HAVE_CXX14" = "0" ; then
		AC_MSG_ERROR([*** Building tests requires a CXX14 compiler])
	fi
	if test "x$cross_compiling" = "xyes" ; then
		AC_MSG_ERROR([*** Tests not supported when cross compiling])
	fi
fi
AC_MSG_RESULT([$build_tests])


BUILD_SUBDIRS="include src docs"
if test "x$build_tests" = "xyes" ; then
  AC_CONFIG_SUBDIRS([test])
  BUILD_SUBDIRS="${BUILD_SUBDIRS} test"
fi

AC_SUBST(BUILD_SUBDIRS)

AC_CONFIG_FILES([Makefile           \
                 include/Makefile   \
                 src/Makefile       \
                 src/lib/Makefile   \
                 src/tools/Makefile \
                 docs/Makefile      \
                 libcares.pc ])

AC_OUTPUT
XC_AMEND_DISTCLEAN(['.'])

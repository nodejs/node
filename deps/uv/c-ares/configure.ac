AC_PREREQ(2.57)

dnl Version not hardcoded here. Fetched later from ares_version.h
AC_INIT([c-ares], [1.7.4],
  [c-ares mailing list: http://cool.haxx.se/mailman/listinfo/c-ares])

CARES_OVERRIDE_AUTOCONF

AC_CONFIG_SRCDIR([ares_ipv6.h])
AM_CONFIG_HEADER([ares_config.h ares_build.h])
AM_MAINTAINER_MODE

CARES_CHECK_OPTION_DEBUG
CARES_CHECK_OPTION_OPTIMIZE
CARES_CHECK_OPTION_WARNINGS
CARES_CHECK_OPTION_WERROR
CARES_CHECK_OPTION_CURLDEBUG
CARES_CHECK_OPTION_SYMBOL_HIDING

CARES_CHECK_PATH_SEPARATOR_REQUIRED

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
AC_PATH_TOOL([AR], [ar], [not_found],
  [$PATH:/usr/bin:/usr/local/bin])
if test -z "$AR" || test "$AR" = "not_found"; then
  AC_MSG_ERROR([ar not found in PATH. Cannot continue without ar.])
fi
AC_SUBST([AR])

dnl Remove non-configure distributed ares_build.h
if test -f ${srcdir}/ares_build.h; then
  rm -f ${srcdir}/ares_build.h
fi

dnl Fetch c-ares version from ares_version.h
VERSION=`$SED -ne 's/^#define ARES_VERSION_STR "\(.*\)"/\1/p' ${srcdir}/ares_version.h`
AM_INIT_AUTOMAKE([c-ares], [$VERSION])
AC_MSG_CHECKING([c-ares version])
AC_MSG_RESULT([$VERSION])

dnl
dnl Detect the canonical host and target build environment
dnl

AC_CANONICAL_HOST
dnl Get system canonical name
AC_DEFINE_UNQUOTED(OS, "${host}", [cpu-machine-OS])

CARES_CHECK_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

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

dnl support building of Windows DLLs
AC_LIBTOOL_WIN32_DLL

dnl force libtool to build static libraries with PIC on AMD64-Linux & FreeBSD
AC_MSG_CHECKING([if arch-OS host is AMD64-Linux/FreeBSD (to build static libraries with PIC)])
case $host in
  x86_64*linux*|amd64*freebsd*|ia64*freebsd*)
    AC_MSG_RESULT([yes])
    with_pic=yes
    ;;
  *)
    AC_MSG_RESULT([no])
    ;;
esac

AC_MSG_CHECKING([if compiler is icc (to build with PIC)])
case $CC in
  icc | */icc)
    AC_MSG_RESULT([yes])
    with_pic=yes
    ;;
  *)
    AC_MSG_RESULT([no])
    ;;
esac

dnl libtool setup
AC_PROG_LIBTOOL

AC_MSG_CHECKING([if we need CARES_BUILDING_LIBRARY])
case $host in
  *-*-mingw*)
    AC_DEFINE(CARES_BUILDING_LIBRARY, 1, [when building c-ares library])
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING([if we need CARES_STATICLIB])
    if test "X$enable_shared" = "Xno"
    then
      AC_DEFINE(CARES_STATICLIB, 1, [when not building a shared library])
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
    fi
    ;;
  *)
    AC_MSG_RESULT(no)
    ;;
esac

dnl **********************************************************************
dnl platform/compiler/architecture specific checks/flags
dnl **********************************************************************

CARES_CHECK_COMPILER
CARES_SET_COMPILER_BASIC_OPTS
CARES_SET_COMPILER_DEBUG_OPTS
CARES_SET_COMPILER_OPTIMIZE_OPTS
CARES_SET_COMPILER_WARNING_OPTS

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
CARES_CHECK_COMPILER_SYMBOL_HIDING

CARES_CHECK_NO_UNDEFINED
AM_CONDITIONAL(NO_UNDEFINED, test x$need_no_undefined = xyes)

CARES_CHECK_CURLDEBUG
AM_CONDITIONAL(CURLDEBUG, test x$want_curldebug = xyes)

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

dnl resolve lib?
AC_CHECK_FUNC(strcasecmp, , [ AC_CHECK_LIB(resolve, strcasecmp) ])

if test "$ac_cv_lib_resolve_strcasecmp" = "$ac_cv_func_strcasecmp"; then
  AC_CHECK_LIB(resolve, strcasecmp,
              [LIBS="-lresolve $LIBS"],
               ,
               -lnsl)
fi
ac_cv_func_strcasecmp="no"

CARES_CHECK_LIBS_CONNECT

dnl **********************************************************************
dnl In case that function clock_gettime with monotonic timer is available,
dnl check for additional required libraries.
dnl **********************************************************************
CURL_CHECK_LIBS_CLOCK_GETTIME_MONOTONIC

AC_MSG_CHECKING([whether to use libgcc])
AC_ARG_ENABLE(libgcc,
AC_HELP_STRING([--enable-libgcc],[use libgcc when linking]),
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
       netdb.h \
       netinet/in.h \
       netinet/tcp.h \
       net/if.h \
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

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
CURL_CHECK_STRUCT_TIMEVAL

AC_CHECK_SIZEOF(size_t)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
CARES_CONFIGURE_LONG
AC_CHECK_SIZEOF(time_t)

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
AC_CHECK_TYPE(ssize_t, ,
   AC_DEFINE(ssize_t, int, [the signed version of size_t]))

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

AC_TYPE_SIGNAL

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
CARES_CHECK_FUNC_GETHOSTBYADDR
CARES_CHECK_FUNC_GETHOSTBYNAME
CARES_CHECK_FUNC_GETHOSTNAME
CARES_CHECK_FUNC_GETSERVBYPORT_R
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
  if_indextoname
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
    AC_DEFINE_UNQUOTED(AS_TR_CPP([HAVE_$func]), [1],
      [Define to 1 if you have the $func function.])
  ],[
    AC_MSG_RESULT([but still no])
  ])
])


dnl Check for inet_net_pton
AC_CHECK_FUNCS(inet_net_pton)
dnl Again, some systems have it, but not IPv6
if test "$ac_cv_func_inet_net_pton" = "yes" ; then
AC_MSG_CHECKING(if inet_net_pton supports IPv6)
AC_TRY_RUN(
  [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
int main()
  {
    struct in6_addr addr6;
    if (inet_net_pton(AF_INET6, "::1", &addr6, sizeof(addr6)) < 1)
      exit(1);
    else
      exit(0);
  }
  ], [
       AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_INET_NET_PTON_IPV6,1,[Define to 1 if inet_net_pton supports IPv6.])
     ], AC_MSG_RESULT(no),AC_MSG_RESULT(no))
fi


AC_CHECK_SIZEOF(struct in6_addr, ,
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
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
]
)

AC_CHECK_SIZEOF(struct in_addr, ,
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
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
]
)


dnl Check if the getnameinfo function is available
dnl and get the types of five of its arguments.
CURL_CHECK_FUNC_GETNAMEINFO


AC_C_BIGENDIAN(
    [AC_DEFINE(ARES_BIG_ENDIAN, 1,
      [define this if ares is built for a big endian system])],
    ,
    [AC_MSG_WARN([couldn't figure out endianess, assuming little endian!])]
)

dnl Check for user-specified random device
AC_ARG_WITH(random,
AC_HELP_STRING([--with-random=FILE],
               [read randomness from FILE (default=/dev/urandom)]),
    [ RANDOM_FILE="$withval" ],
    [
        dnl Check for random device.  If we're cross compiling, we can't
        dnl check, and it's better to assume it doesn't exist than it is
        dnl to fail on AC_CHECK_FILE or later.
        if test "$cross_compiling" = "no"; then
          AC_CHECK_FILE("/dev/urandom", [ RANDOM_FILE="/dev/urandom"] )
        else
          AC_MSG_WARN([cannot check for /dev/urandom while cross compiling; assuming none])
        fi
        
    ]
)
if test -n "$RANDOM_FILE" && test X"$RANDOM_FILE" != Xno ; then
        AC_SUBST(RANDOM_FILE)
        AC_DEFINE_UNQUOTED(RANDOM_FILE, "$RANDOM_FILE",
        [a suitable file/device to read random data from])
fi

CARES_CHECK_OPTION_NONBLOCKING
CARES_CHECK_NONBLOCKING_SOCKET

CARES_CONFIGURE_SYMBOL_HIDING

CARES_PRIVATE_LIBS="$LIBS"
AC_SUBST(CARES_PRIVATE_LIBS)

CARES_CFLAG_EXTRAS=""
if test X"$want_werror" == Xyes; then
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

AC_CONFIG_FILES([Makefile libcares.pc])
AC_OUTPUT

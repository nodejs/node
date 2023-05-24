#***************************************************************************
#
# Copyright (C) 2008 - 2012 by Daniel Stenberg et al
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of M.I.T. not be used in advertising or
# publicity pertaining to distribution of the software without specific,
# written prior permission.  M.I.T. makes no representations about the
# suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
#
#***************************************************************************

# File version for 'aclocal' use. Keep it a single number.
# serial 46


dnl CARES_INCLUDES_ARPA_INET
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when arpa/inet.h is to be included.

AC_DEFUN([CARES_INCLUDES_ARPA_INET], [
cares_includes_arpa_inet="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h sys/socket.h netinet/in.h arpa/inet.h,
    [], [], [$cares_includes_arpa_inet])
])


dnl CARES_INCLUDES_FCNTL
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when fcntl.h is to be included.

AC_DEFUN([CARES_INCLUDES_FCNTL], [
cares_includes_fcntl="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h unistd.h fcntl.h,
    [], [], [$cares_includes_fcntl])
])


dnl CARES_INCLUDES_NETDB
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when netdb.h is to be included.

AC_DEFUN([CARES_INCLUDES_NETDB], [
cares_includes_netdb="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h netdb.h,
    [], [], [$cares_includes_netdb])
])


dnl CARES_INCLUDES_SOCKET
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when socket.h is to be included.

AC_DEFUN([CARES_INCLUDES_SOCKET], [
cares_includes_socket="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SOCKET_H
#  include <socket.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h socket.h,
    [], [], [$cares_includes_socket])
])


dnl CARES_INCLUDES_STDLIB
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when stdlib.h is to be included.

AC_DEFUN([CARES_INCLUDES_STDLIB], [
cares_includes_stdlib="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h stdlib.h,
    [], [], [$cares_includes_stdlib])
])


dnl CARES_INCLUDES_STRING
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when string(s).h is to be included.

AC_DEFUN([CARES_INCLUDES_STRING], [
cares_includes_string="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h string.h strings.h,
    [], [], [$cares_includes_string])
])


dnl CARES_INCLUDES_STROPTS
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when stropts.h is to be included.

AC_DEFUN([CARES_INCLUDES_STROPTS], [
cares_includes_stropts="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#ifdef HAVE_STROPTS_H
#  include <stropts.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h unistd.h sys/socket.h sys/ioctl.h stropts.h,
    [], [], [$cares_includes_stropts])
])


dnl CARES_INCLUDES_SYS_SOCKET
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when sys/socket.h is to be included.

AC_DEFUN([CARES_INCLUDES_SYS_SOCKET], [
cares_includes_sys_socket="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h sys/socket.h,
    [], [], [$cares_includes_sys_socket])
])


dnl CARES_INCLUDES_SYS_TYPES
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when sys/types.h is to be included.

AC_DEFUN([CARES_INCLUDES_SYS_TYPES], [
cares_includes_sys_types="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h,
    [], [], [$cares_includes_sys_types])
])


dnl CARES_INCLUDES_SYS_UIO
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when sys/uio.h is to be included.

AC_DEFUN([CARES_INCLUDES_SYS_UIO], [
cares_includes_sys_uio="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_UIO_H
#  include <sys/uio.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h sys/uio.h,
    [], [], [$cares_includes_sys_uio])
])


dnl CARES_INCLUDES_UNISTD
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when unistd.h is to be included.

AC_DEFUN([CARES_INCLUDES_UNISTD], [
cares_includes_unistd="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h unistd.h,
    [], [], [$cares_includes_unistd])
])


dnl CARES_INCLUDES_WINSOCK2
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when winsock(2).h is to be included.

AC_DEFUN([CARES_INCLUDES_WINSOCK2], [
cares_includes_winsock2="\
/* includes start */
#ifdef HAVE_WINDOWS_H
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifdef HAVE_WINSOCK2_H
#    include <winsock2.h>
#  else
#    ifdef HAVE_WINSOCK_H
#      include <winsock.h>
#    endif
#  endif
#endif
/* includes end */"
  CURL_CHECK_HEADER_WINDOWS
  CURL_CHECK_HEADER_WINSOCK
  CURL_CHECK_HEADER_WINSOCK2
])


dnl CARES_INCLUDES_WS2TCPIP
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when ws2tcpip.h is to be included.

AC_DEFUN([CARES_INCLUDES_WS2TCPIP], [
cares_includes_ws2tcpip="\
/* includes start */
#ifdef HAVE_WINDOWS_H
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifdef HAVE_WINSOCK2_H
#    include <winsock2.h>
#    ifdef HAVE_WS2TCPIP_H
#       include <ws2tcpip.h>
#    endif
#  endif
#endif
/* includes end */"
  CURL_CHECK_HEADER_WINDOWS
  CURL_CHECK_HEADER_WINSOCK2
  CURL_CHECK_HEADER_WS2TCPIP
])


dnl CARES_PREPROCESS_CALLCONV
dnl -------------------------------------------------
dnl Set up variable with a preprocessor block which
dnl defines function calling convention.

AC_DEFUN([CARES_PREPROCESS_CALLCONV], [
cares_preprocess_callconv="\
/* preprocess start */
#ifdef HAVE_WINDOWS_H
#  define FUNCALLCONV __stdcall
#else
#  define FUNCALLCONV
#endif
/* preprocess end */"
])


dnl CARES_CHECK_FUNC_CLOSESOCKET
dnl -------------------------------------------------
dnl Verify if closesocket is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_closesocket, then
dnl HAVE_CLOSESOCKET will be defined.

AC_DEFUN([CARES_CHECK_FUNC_CLOSESOCKET], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_SOCKET])dnl
  #
  tst_links_closesocket="unknown"
  tst_proto_closesocket="unknown"
  tst_compi_closesocket="unknown"
  tst_allow_closesocket="unknown"
  #
  AC_MSG_CHECKING([if closesocket can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_socket
    ]],[[
      if(0 != closesocket(0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_closesocket="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_closesocket="no"
  ])
  #
  if test "$tst_links_closesocket" = "yes"; then
    AC_MSG_CHECKING([if closesocket is prototyped])
    AC_EGREP_CPP([closesocket],[
      $cares_includes_winsock2
      $cares_includes_socket
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_closesocket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_closesocket="no"
    ])
  fi
  #
  if test "$tst_proto_closesocket" = "yes"; then
    AC_MSG_CHECKING([if closesocket is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_socket
      ]],[[
        if(0 != closesocket(0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_closesocket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_closesocket="no"
    ])
  fi
  #
  if test "$tst_compi_closesocket" = "yes"; then
    AC_MSG_CHECKING([if closesocket usage allowed])
    if test "x$cares_disallow_closesocket" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_closesocket="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_closesocket="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if closesocket might be used])
  if test "$tst_links_closesocket" = "yes" &&
     test "$tst_proto_closesocket" = "yes" &&
     test "$tst_compi_closesocket" = "yes" &&
     test "$tst_allow_closesocket" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_CLOSESOCKET, 1,
      [Define to 1 if you have the closesocket function.])
    ac_cv_func_closesocket="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_closesocket="no"
  fi
])


dnl CARES_CHECK_FUNC_CLOSESOCKET_CAMEL
dnl -------------------------------------------------
dnl Verify if CloseSocket is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_closesocket_camel,
dnl then HAVE_CLOSESOCKET_CAMEL will be defined.

AC_DEFUN([CARES_CHECK_FUNC_CLOSESOCKET_CAMEL], [
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  #
  tst_links_closesocket_camel="unknown"
  tst_proto_closesocket_camel="unknown"
  tst_compi_closesocket_camel="unknown"
  tst_allow_closesocket_camel="unknown"
  #
  AC_MSG_CHECKING([if CloseSocket can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_sys_socket
    ]],[[
      if(0 != CloseSocket(0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_closesocket_camel="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_closesocket_camel="no"
  ])
  #
  if test "$tst_links_closesocket_camel" = "yes"; then
    AC_MSG_CHECKING([if CloseSocket is prototyped])
    AC_EGREP_CPP([CloseSocket],[
      $cares_includes_sys_socket
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_closesocket_camel="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_closesocket_camel="no"
    ])
  fi
  #
  if test "$tst_proto_closesocket_camel" = "yes"; then
    AC_MSG_CHECKING([if CloseSocket is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_sys_socket
      ]],[[
        if(0 != CloseSocket(0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_closesocket_camel="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_closesocket_camel="no"
    ])
  fi
  #
  if test "$tst_compi_closesocket_camel" = "yes"; then
    AC_MSG_CHECKING([if CloseSocket usage allowed])
    if test "x$cares_disallow_closesocket_camel" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_closesocket_camel="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_closesocket_camel="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if CloseSocket might be used])
  if test "$tst_links_closesocket_camel" = "yes" &&
     test "$tst_proto_closesocket_camel" = "yes" &&
     test "$tst_compi_closesocket_camel" = "yes" &&
     test "$tst_allow_closesocket_camel" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_CLOSESOCKET_CAMEL, 1,
      [Define to 1 if you have the CloseSocket camel case function.])
    ac_cv_func_closesocket_camel="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_closesocket_camel="no"
  fi
])


dnl CARES_CHECK_FUNC_CONNECT
dnl -------------------------------------------------
dnl Verify if connect is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_connect, then
dnl HAVE_CONNECT will be defined.

AC_DEFUN([CARES_CHECK_FUNC_CONNECT], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  AC_REQUIRE([CARES_INCLUDES_SOCKET])dnl
  #
  tst_links_connect="unknown"
  tst_proto_connect="unknown"
  tst_compi_connect="unknown"
  tst_allow_connect="unknown"
  #
  AC_MSG_CHECKING([if connect can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_sys_socket
      $cares_includes_socket
    ]],[[
      if(0 != connect(0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_connect="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_connect="no"
  ])
  #
  if test "$tst_links_connect" = "yes"; then
    AC_MSG_CHECKING([if connect is prototyped])
    AC_EGREP_CPP([connect],[
      $cares_includes_winsock2
      $cares_includes_sys_socket
      $cares_includes_socket
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_connect="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_connect="no"
    ])
  fi
  #
  if test "$tst_proto_connect" = "yes"; then
    AC_MSG_CHECKING([if connect is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_sys_socket
        $cares_includes_socket
      ]],[[
        if(0 != connect(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_connect="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_connect="no"
    ])
  fi
  #
  if test "$tst_compi_connect" = "yes"; then
    AC_MSG_CHECKING([if connect usage allowed])
    if test "x$cares_disallow_connect" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_connect="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_connect="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if connect might be used])
  if test "$tst_links_connect" = "yes" &&
     test "$tst_proto_connect" = "yes" &&
     test "$tst_compi_connect" = "yes" &&
     test "$tst_allow_connect" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_CONNECT, 1,
      [Define to 1 if you have the connect function.])
    ac_cv_func_connect="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_connect="no"
  fi
])


dnl CARES_CHECK_FUNC_FCNTL
dnl -------------------------------------------------
dnl Verify if fcntl is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_fcntl, then
dnl HAVE_FCNTL will be defined.

AC_DEFUN([CARES_CHECK_FUNC_FCNTL], [
  AC_REQUIRE([CARES_INCLUDES_FCNTL])dnl
  #
  tst_links_fcntl="unknown"
  tst_proto_fcntl="unknown"
  tst_compi_fcntl="unknown"
  tst_allow_fcntl="unknown"
  #
  AC_MSG_CHECKING([if fcntl can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([fcntl])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_fcntl="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_fcntl="no"
  ])
  #
  if test "$tst_links_fcntl" = "yes"; then
    AC_MSG_CHECKING([if fcntl is prototyped])
    AC_EGREP_CPP([fcntl],[
      $cares_includes_fcntl
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_fcntl="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_fcntl="no"
    ])
  fi
  #
  if test "$tst_proto_fcntl" = "yes"; then
    AC_MSG_CHECKING([if fcntl is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_fcntl
      ]],[[
        if(0 != fcntl(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_fcntl="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_fcntl="no"
    ])
  fi
  #
  if test "$tst_compi_fcntl" = "yes"; then
    AC_MSG_CHECKING([if fcntl usage allowed])
    if test "x$cares_disallow_fcntl" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_fcntl="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_fcntl="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if fcntl might be used])
  if test "$tst_links_fcntl" = "yes" &&
     test "$tst_proto_fcntl" = "yes" &&
     test "$tst_compi_fcntl" = "yes" &&
     test "$tst_allow_fcntl" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_FCNTL, 1,
      [Define to 1 if you have the fcntl function.])
    ac_cv_func_fcntl="yes"
    CARES_CHECK_FUNC_FCNTL_O_NONBLOCK
  else
    AC_MSG_RESULT([no])
    ac_cv_func_fcntl="no"
  fi
])


dnl CARES_CHECK_FUNC_FCNTL_O_NONBLOCK
dnl -------------------------------------------------
dnl Verify if fcntl with status flag O_NONBLOCK is
dnl available, can be compiled, and seems to work. If
dnl all of these are true, then HAVE_FCNTL_O_NONBLOCK
dnl will be defined.

AC_DEFUN([CARES_CHECK_FUNC_FCNTL_O_NONBLOCK], [
  #
  tst_compi_fcntl_o_nonblock="unknown"
  tst_allow_fcntl_o_nonblock="unknown"
  #
  case $host_os in
    sunos4* | aix3* | beos*)
      dnl O_NONBLOCK does not work on these platforms
      cares_disallow_fcntl_o_nonblock="yes"
      ;;
  esac
  #
  if test "$ac_cv_func_fcntl" = "yes"; then
    AC_MSG_CHECKING([if fcntl O_NONBLOCK is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_fcntl
      ]],[[
        int flags = 0;
        if(0 != fcntl(0, F_SETFL, flags | O_NONBLOCK))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_fcntl_o_nonblock="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_fcntl_o_nonblock="no"
    ])
  fi
  #
  if test "$tst_compi_fcntl_o_nonblock" = "yes"; then
    AC_MSG_CHECKING([if fcntl O_NONBLOCK usage allowed])
    if test "x$cares_disallow_fcntl_o_nonblock" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_fcntl_o_nonblock="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_fcntl_o_nonblock="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if fcntl O_NONBLOCK might be used])
  if test "$tst_compi_fcntl_o_nonblock" = "yes" &&
     test "$tst_allow_fcntl_o_nonblock" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_FCNTL_O_NONBLOCK, 1,
      [Define to 1 if you have a working fcntl O_NONBLOCK function.])
    ac_cv_func_fcntl_o_nonblock="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_fcntl_o_nonblock="no"
  fi
])


dnl CARES_CHECK_FUNC_FREEADDRINFO
dnl -------------------------------------------------
dnl Verify if freeaddrinfo is available, prototyped,
dnl and can be compiled. If all of these are true,
dnl and usage has not been previously disallowed with
dnl shell variable cares_disallow_freeaddrinfo, then
dnl HAVE_FREEADDRINFO will be defined.

AC_DEFUN([CARES_CHECK_FUNC_FREEADDRINFO], [
  AC_REQUIRE([CARES_INCLUDES_WS2TCPIP])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  #
  tst_links_freeaddrinfo="unknown"
  tst_proto_freeaddrinfo="unknown"
  tst_compi_freeaddrinfo="unknown"
  tst_allow_freeaddrinfo="unknown"
  #
  AC_MSG_CHECKING([if freeaddrinfo can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_ws2tcpip
      $cares_includes_sys_socket
      $cares_includes_netdb
    ]],[[
      freeaddrinfo(0);
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_freeaddrinfo="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_freeaddrinfo="no"
  ])
  #
  if test "$tst_links_freeaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if freeaddrinfo is prototyped])
    AC_EGREP_CPP([freeaddrinfo],[
      $cares_includes_ws2tcpip
      $cares_includes_sys_socket
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_freeaddrinfo="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_freeaddrinfo="no"
    ])
  fi
  #
  if test "$tst_proto_freeaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if freeaddrinfo is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_ws2tcpip
        $cares_includes_sys_socket
        $cares_includes_netdb
      ]],[[
        freeaddrinfo(0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_freeaddrinfo="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_freeaddrinfo="no"
    ])
  fi
  #
  if test "$tst_compi_freeaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if freeaddrinfo usage allowed])
    if test "x$cares_disallow_freeaddrinfo" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_freeaddrinfo="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_freeaddrinfo="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if freeaddrinfo might be used])
  if test "$tst_links_freeaddrinfo" = "yes" &&
     test "$tst_proto_freeaddrinfo" = "yes" &&
     test "$tst_compi_freeaddrinfo" = "yes" &&
     test "$tst_allow_freeaddrinfo" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_FREEADDRINFO, 1,
      [Define to 1 if you have the freeaddrinfo function.])
    ac_cv_func_freeaddrinfo="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_freeaddrinfo="no"
  fi
])


dnl CARES_CHECK_FUNC_GETADDRINFO
dnl -------------------------------------------------
dnl Verify if getaddrinfo is available, prototyped, can
dnl be compiled and seems to work. If all of these are
dnl true, and usage has not been previously disallowed
dnl with shell variable cares_disallow_getaddrinfo, then
dnl HAVE_GETADDRINFO will be defined. Additionally when
dnl HAVE_GETADDRINFO gets defined this will also attempt
dnl to find out if getaddrinfo happens to be threadsafe,
dnl defining HAVE_GETADDRINFO_THREADSAFE when true.

AC_DEFUN([CARES_CHECK_FUNC_GETADDRINFO], [
  AC_REQUIRE([CARES_INCLUDES_WS2TCPIP])dnl
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  AC_REQUIRE([CURL_CHECK_NATIVE_WINDOWS])dnl
  #
  tst_links_getaddrinfo="unknown"
  tst_proto_getaddrinfo="unknown"
  tst_compi_getaddrinfo="unknown"
  tst_works_getaddrinfo="unknown"
  tst_allow_getaddrinfo="unknown"
  tst_tsafe_getaddrinfo="unknown"
  #
  AC_MSG_CHECKING([if getaddrinfo can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_ws2tcpip
      $cares_includes_sys_socket
      $cares_includes_netdb
    ]],[[
      if(0 != getaddrinfo(0, 0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_getaddrinfo="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_getaddrinfo="no"
  ])
  #
  if test "$tst_links_getaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if getaddrinfo is prototyped])
    AC_EGREP_CPP([getaddrinfo],[
      $cares_includes_ws2tcpip
      $cares_includes_sys_socket
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_getaddrinfo="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_getaddrinfo="no"
    ])
  fi
  #
  if test "$tst_proto_getaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if getaddrinfo is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_ws2tcpip
        $cares_includes_sys_socket
        $cares_includes_netdb
      ]],[[
        if(0 != getaddrinfo(0, 0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_getaddrinfo="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_getaddrinfo="no"
    ])
  fi
  #
  dnl only do runtime verification when not cross-compiling
  if test "x$cross_compiling" != "xyes" &&
    test "$tst_compi_getaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if getaddrinfo seems to work])
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_ws2tcpip
        $cares_includes_stdlib
        $cares_includes_string
        $cares_includes_sys_socket
        $cares_includes_netdb
      ]],[[
        struct addrinfo hints;
        struct addrinfo *ai = 0;
        int error;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        error = getaddrinfo("127.0.0.1", 0, &hints, &ai);
        if(error || !ai)
          exit(1); /* fail */
        else
          exit(0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_works_getaddrinfo="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_works_getaddrinfo="no"
    ])
  fi
  #
  if test "$tst_compi_getaddrinfo" = "yes" &&
    test "$tst_works_getaddrinfo" != "no"; then
    AC_MSG_CHECKING([if getaddrinfo usage allowed])
    if test "x$cares_disallow_getaddrinfo" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_getaddrinfo="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_getaddrinfo="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if getaddrinfo might be used])
  if test "$tst_links_getaddrinfo" = "yes" &&
     test "$tst_proto_getaddrinfo" = "yes" &&
     test "$tst_compi_getaddrinfo" = "yes" &&
     test "$tst_allow_getaddrinfo" = "yes" &&
     test "$tst_works_getaddrinfo" != "no"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETADDRINFO, 1,
      [Define to 1 if you have a working getaddrinfo function.])
    ac_cv_func_getaddrinfo="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_getaddrinfo="no"
    ac_cv_func_getaddrinfo_threadsafe="no"
  fi
  #
  if test "$ac_cv_func_getaddrinfo" = "yes"; then
    AC_MSG_CHECKING([if getaddrinfo is threadsafe])
    case $host_os in
      aix[[1234]].* | aix5.[[01]].*)
        dnl aix 5.1 and older
        tst_tsafe_getaddrinfo="no"
        ;;
      aix*)
        dnl aix 5.2 and newer
        tst_tsafe_getaddrinfo="yes"
        ;;
      darwin[[12345]].*)
        dnl darwin 5.0 and mac os x 10.1.X and older
        tst_tsafe_getaddrinfo="no"
        ;;
      darwin*)
        dnl darwin 6.0 and mac os x 10.2.X and newer
        tst_tsafe_getaddrinfo="yes"
        ;;
      freebsd[[1234]].* | freebsd5.[[1234]]*)
        dnl freebsd 5.4 and older
        tst_tsafe_getaddrinfo="no"
        ;;
      freebsd*)
        dnl freebsd 5.5 and newer
        tst_tsafe_getaddrinfo="yes"
        ;;
      hpux[[123456789]].* | hpux10.* | hpux11.0* | hpux11.10*)
        dnl hpux 11.10 and older
        tst_tsafe_getaddrinfo="no"
        ;;
      hpux*)
        dnl hpux 11.11 and newer
        tst_tsafe_getaddrinfo="yes"
        ;;
      netbsd[[123]].*)
        dnl netbsd 3.X and older
        tst_tsafe_getaddrinfo="no"
        ;;
      netbsd*)
        dnl netbsd 4.X and newer
        tst_tsafe_getaddrinfo="yes"
        ;;
      *bsd*)
        dnl All other bsd's
        tst_tsafe_getaddrinfo="no"
        ;;
      solaris2*)
        dnl solaris which have it
        tst_tsafe_getaddrinfo="yes"
        ;;
    esac
    if test "$tst_tsafe_getaddrinfo" = "unknown" &&
       test "$ac_cv_native_windows" = "yes"; then
      tst_tsafe_getaddrinfo="yes"
    fi
    if test "$tst_tsafe_getaddrinfo" = "unknown"; then
      CURL_CHECK_DEF_CC([h_errno], [
        $cares_includes_sys_socket
        $cares_includes_netdb
        ], [silent])
      if test "$curl_cv_have_def_h_errno" = "yes"; then
        tst_h_errno_macro="yes"
      else
        tst_h_errno_macro="no"
      fi
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_sys_socket
          $cares_includes_netdb
        ]],[[
          h_errno = 2;
          if(0 != h_errno)
            return 1;
        ]])
      ],[
        tst_h_errno_modifiable_lvalue="yes"
      ],[
        tst_h_errno_modifiable_lvalue="no"
      ])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
        ]],[[
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
          return 0;
#elif defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 700)
          return 0;
#else
          force compilation error
#endif
        ]])
      ],[
        tst_h_errno_sbs_issue_7="yes"
      ],[
        tst_h_errno_sbs_issue_7="no"
      ])
      if test "$tst_h_errno_macro" = "no" &&
         test "$tst_h_errno_modifiable_lvalue" = "no" &&
         test "$tst_h_errno_sbs_issue_7" = "no"; then
        tst_tsafe_getaddrinfo="no"
      else
        tst_tsafe_getaddrinfo="yes"
      fi
    fi
    AC_MSG_RESULT([$tst_tsafe_getaddrinfo])
    if test "$tst_tsafe_getaddrinfo" = "yes"; then
      AC_DEFINE_UNQUOTED(HAVE_GETADDRINFO_THREADSAFE, 1,
        [Define to 1 if the getaddrinfo function is threadsafe.])
      ac_cv_func_getaddrinfo_threadsafe="yes"
    else
      ac_cv_func_getaddrinfo_threadsafe="no"
    fi
  fi
])


dnl CARES_CHECK_FUNC_GETENV
dnl -------------------------------------------------
dnl Verify if getenv is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_getenv, then
dnl HAVE_GETENV will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETENV], [
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  #
  tst_links_getenv="unknown"
  tst_proto_getenv="unknown"
  tst_compi_getenv="unknown"
  tst_allow_getenv="unknown"
  #
  AC_MSG_CHECKING([if getenv can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getenv])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_getenv="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_getenv="no"
  ])
  #
  if test "$tst_links_getenv" = "yes"; then
    AC_MSG_CHECKING([if getenv is prototyped])
    AC_EGREP_CPP([getenv],[
      $cares_includes_stdlib
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_getenv="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_getenv="no"
    ])
  fi
  #
  if test "$tst_proto_getenv" = "yes"; then
    AC_MSG_CHECKING([if getenv is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stdlib
      ]],[[
        if(0 != getenv(0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_getenv="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_getenv="no"
    ])
  fi
  #
  if test "$tst_compi_getenv" = "yes"; then
    AC_MSG_CHECKING([if getenv usage allowed])
    if test "x$cares_disallow_getenv" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_getenv="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_getenv="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if getenv might be used])
  if test "$tst_links_getenv" = "yes" &&
     test "$tst_proto_getenv" = "yes" &&
     test "$tst_compi_getenv" = "yes" &&
     test "$tst_allow_getenv" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETENV, 1,
      [Define to 1 if you have the getenv function.])
    ac_cv_func_getenv="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_getenv="no"
  fi
])


dnl CARES_CHECK_FUNC_GETHOSTBYADDR
dnl -------------------------------------------------
dnl Verify if gethostbyaddr is available, prototyped,
dnl and can be compiled. If all of these are true,
dnl and usage has not been previously disallowed with
dnl shell variable cares_disallow_gethostbyaddr, then
dnl HAVE_GETHOSTBYADDR will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETHOSTBYADDR], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  #
  tst_links_gethostbyaddr="unknown"
  tst_proto_gethostbyaddr="unknown"
  tst_compi_gethostbyaddr="unknown"
  tst_allow_gethostbyaddr="unknown"
  #
  AC_MSG_CHECKING([if gethostbyaddr can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_netdb
    ]],[[
      if(0 != gethostbyaddr(0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_gethostbyaddr="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_gethostbyaddr="no"
  ])
  #
  if test "$tst_links_gethostbyaddr" = "yes"; then
    AC_MSG_CHECKING([if gethostbyaddr is prototyped])
    AC_EGREP_CPP([gethostbyaddr],[
      $cares_includes_winsock2
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_gethostbyaddr="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_gethostbyaddr="no"
    ])
  fi
  #
  if test "$tst_proto_gethostbyaddr" = "yes"; then
    AC_MSG_CHECKING([if gethostbyaddr is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_netdb
      ]],[[
        if(0 != gethostbyaddr(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_gethostbyaddr="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_gethostbyaddr="no"
    ])
  fi
  #
  if test "$tst_compi_gethostbyaddr" = "yes"; then
    AC_MSG_CHECKING([if gethostbyaddr usage allowed])
    if test "x$cares_disallow_gethostbyaddr" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_gethostbyaddr="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_gethostbyaddr="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if gethostbyaddr might be used])
  if test "$tst_links_gethostbyaddr" = "yes" &&
     test "$tst_proto_gethostbyaddr" = "yes" &&
     test "$tst_compi_gethostbyaddr" = "yes" &&
     test "$tst_allow_gethostbyaddr" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYADDR, 1,
      [Define to 1 if you have the gethostbyaddr function.])
    ac_cv_func_gethostbyaddr="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_gethostbyaddr="no"
  fi
])


dnl CARES_CHECK_FUNC_GETHOSTBYNAME
dnl -------------------------------------------------
dnl Verify if gethostbyname is available, prototyped,
dnl and can be compiled. If all of these are true,
dnl and usage has not been previously disallowed with
dnl shell variable cares_disallow_gethostbyname, then
dnl HAVE_GETHOSTBYNAME will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETHOSTBYNAME], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  #
  tst_links_gethostbyname="unknown"
  tst_proto_gethostbyname="unknown"
  tst_compi_gethostbyname="unknown"
  tst_allow_gethostbyname="unknown"
  #
  AC_MSG_CHECKING([if gethostbyname can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_netdb
    ]],[[
      if(0 != gethostbyname(0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_gethostbyname="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_gethostbyname="no"
  ])
  #
  if test "$tst_links_gethostbyname" = "yes"; then
    AC_MSG_CHECKING([if gethostbyname is prototyped])
    AC_EGREP_CPP([gethostbyname],[
      $cares_includes_winsock2
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_gethostbyname="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_gethostbyname="no"
    ])
  fi
  #
  if test "$tst_proto_gethostbyname" = "yes"; then
    AC_MSG_CHECKING([if gethostbyname is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_netdb
      ]],[[
        if(0 != gethostbyname(0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_gethostbyname="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_gethostbyname="no"
    ])
  fi
  #
  if test "$tst_compi_gethostbyname" = "yes"; then
    AC_MSG_CHECKING([if gethostbyname usage allowed])
    if test "x$cares_disallow_gethostbyname" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_gethostbyname="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_gethostbyname="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if gethostbyname might be used])
  if test "$tst_links_gethostbyname" = "yes" &&
     test "$tst_proto_gethostbyname" = "yes" &&
     test "$tst_compi_gethostbyname" = "yes" &&
     test "$tst_allow_gethostbyname" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETHOSTBYNAME, 1,
      [Define to 1 if you have the gethostbyname function.])
    ac_cv_func_gethostbyname="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_gethostbyname="no"
  fi
])


dnl CARES_CHECK_FUNC_GETHOSTNAME
dnl -------------------------------------------------
dnl Verify if gethostname is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_gethostname, then
dnl HAVE_GETHOSTNAME will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETHOSTNAME], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_UNISTD])dnl
  AC_REQUIRE([CARES_PREPROCESS_CALLCONV])dnl
  #
  tst_links_gethostname="unknown"
  tst_proto_gethostname="unknown"
  tst_compi_gethostname="unknown"
  tst_allow_gethostname="unknown"
  #
  AC_MSG_CHECKING([if gethostname can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_unistd
    ]],[[
      if(0 != gethostname(0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_gethostname="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_gethostname="no"
  ])
  #
  if test "$tst_links_gethostname" = "yes"; then
    AC_MSG_CHECKING([if gethostname is prototyped])
    AC_EGREP_CPP([gethostname],[
      $cares_includes_winsock2
      $cares_includes_unistd
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_gethostname="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_gethostname="no"
    ])
  fi
  #
  if test "$tst_proto_gethostname" = "yes"; then
    AC_MSG_CHECKING([if gethostname is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_unistd
      ]],[[
        if(0 != gethostname(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_gethostname="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_gethostname="no"
    ])
  fi
  #
  if test "$tst_compi_gethostname" = "yes"; then
    AC_MSG_CHECKING([for gethostname arg 2 data type])
    tst_gethostname_type_arg2="unknown"
    for tst_arg1 in 'char *' 'unsigned char *' 'void *'; do
      for tst_arg2 in 'int' 'unsigned int' 'size_t'; do
        if test "$tst_gethostname_type_arg2" = "unknown"; then
          AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
              $cares_includes_winsock2
              $cares_includes_unistd
              $cares_preprocess_callconv
              extern int FUNCALLCONV gethostname($tst_arg1, $tst_arg2);
            ]],[[
              if(0 != gethostname(0, 0))
                return 1;
            ]])
          ],[
            tst_gethostname_type_arg2="$tst_arg2"
          ])
        fi
      done
    done
    AC_MSG_RESULT([$tst_gethostname_type_arg2])
    if test "$tst_gethostname_type_arg2" != "unknown"; then
      AC_DEFINE_UNQUOTED(GETHOSTNAME_TYPE_ARG2, $tst_gethostname_type_arg2,
        [Define to the type of arg 2 for gethostname.])
    fi
  fi
  #
  if test "$tst_compi_gethostname" = "yes"; then
    AC_MSG_CHECKING([if gethostname usage allowed])
    if test "x$cares_disallow_gethostname" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_gethostname="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_gethostname="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if gethostname might be used])
  if test "$tst_links_gethostname" = "yes" &&
     test "$tst_proto_gethostname" = "yes" &&
     test "$tst_compi_gethostname" = "yes" &&
     test "$tst_allow_gethostname" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETHOSTNAME, 1,
      [Define to 1 if you have the gethostname function.])
    ac_cv_func_gethostname="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_gethostname="no"
  fi
])


dnl CARES_CHECK_FUNC_GETSERVBYPORT_R
dnl -------------------------------------------------
dnl Verify if getservbyport_r is available, prototyped,
dnl and can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_getservbyport_r, then
dnl HAVE_GETSERVBYPORT_R will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETSERVBYPORT_R], [
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  #
  tst_links_getservbyport_r="unknown"
  tst_proto_getservbyport_r="unknown"
  tst_compi_getservbyport_r="unknown"
  tst_allow_getservbyport_r="unknown"
  tst_nargs_getservbyport_r="unknown"
  #
  AC_MSG_CHECKING([if getservbyport_r can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getservbyport_r])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_getservbyport_r="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_getservbyport_r="no"
  ])
  #
  if test "$tst_links_getservbyport_r" = "yes"; then
    AC_MSG_CHECKING([if getservbyport_r is prototyped])
    AC_EGREP_CPP([getservbyport_r],[
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_getservbyport_r="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_getservbyport_r="no"
    ])
  fi
  #
  if test "$tst_proto_getservbyport_r" = "yes"; then
    if test "$tst_nargs_getservbyport_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyport_r takes 4 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyport_r(0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyport_r="yes"
        tst_nargs_getservbyport_r="4"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyport_r="no"
      ])
    fi
    if test "$tst_nargs_getservbyport_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyport_r takes 5 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyport_r(0, 0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyport_r="yes"
        tst_nargs_getservbyport_r="5"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyport_r="no"
      ])
    fi
    if test "$tst_nargs_getservbyport_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyport_r takes 6 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyport_r(0, 0, 0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyport_r="yes"
        tst_nargs_getservbyport_r="6"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyport_r="no"
      ])
    fi
    AC_MSG_CHECKING([if getservbyport_r is compilable])
    if test "$tst_compi_getservbyport_r" = "yes"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
  #
  if test "$tst_compi_getservbyport_r" = "yes"; then
    AC_MSG_CHECKING([if getservbyport_r usage allowed])
    if test "x$cares_disallow_getservbyport_r" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_getservbyport_r="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_getservbyport_r="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if getservbyport_r might be used])
  if test "$tst_links_getservbyport_r" = "yes" &&
     test "$tst_proto_getservbyport_r" = "yes" &&
     test "$tst_compi_getservbyport_r" = "yes" &&
     test "$tst_allow_getservbyport_r" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETSERVBYPORT_R, 1,
      [Define to 1 if you have the getservbyport_r function.])
    AC_DEFINE_UNQUOTED(GETSERVBYPORT_R_ARGS, $tst_nargs_getservbyport_r,
      [Specifies the number of arguments to getservbyport_r])
    if test "$tst_nargs_getservbyport_r" -eq "4"; then
      AC_DEFINE(GETSERVBYPORT_R_BUFSIZE, sizeof(struct servent_data),
        [Specifies the size of the buffer to pass to getservbyport_r])
    else
      AC_DEFINE(GETSERVBYPORT_R_BUFSIZE, 4096,
        [Specifies the size of the buffer to pass to getservbyport_r])
    fi
    ac_cv_func_getservbyport_r="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_getservbyport_r="no"
  fi
])


dnl CARES_CHECK_FUNC_GETSERVBYNAME_R
dnl -------------------------------------------------
dnl Verify if getservbyname_r is available, prototyped,
dnl and can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_getservbyname_r, then
dnl HAVE_GETSERVBYNAME_R will be defined.

AC_DEFUN([CARES_CHECK_FUNC_GETSERVBYNAME_R], [
  AC_REQUIRE([CARES_INCLUDES_NETDB])dnl
  #
  tst_links_getservbyname_r="unknown"
  tst_proto_getservbyname_r="unknown"
  tst_compi_getservbyname_r="unknown"
  tst_allow_getservbyname_r="unknown"
  tst_nargs_getservbyname_r="unknown"
  #
  AC_MSG_CHECKING([if getservbyname_r can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getservbyname_r])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_getservbyname_r="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_getservbyname_r="no"
  ])
  #
  if test "$tst_links_getservbyname_r" = "yes"; then
    AC_MSG_CHECKING([if getservbyname_r is prototyped])
    AC_EGREP_CPP([getservbyname_r],[
      $cares_includes_netdb
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_getservbyname_r="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_getservbyname_r="no"
    ])
  fi
  #
  if test "$tst_proto_getservbyname_r" = "yes"; then
    if test "$tst_nargs_getservbyname_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyname_r takes 4 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyname_r(0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyname_r="yes"
        tst_nargs_getservbyname_r="4"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyname_r="no"
      ])
    fi
    if test "$tst_nargs_getservbyname_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyname_r takes 5 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyname_r(0, 0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyname_r="yes"
        tst_nargs_getservbyname_r="5"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyname_r="no"
      ])
    fi
    if test "$tst_nargs_getservbyname_r" = "unknown"; then
      AC_MSG_CHECKING([if getservbyname_r takes 6 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_netdb
        ]],[[
          if(0 != getservbyname_r(0, 0, 0, 0, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        tst_compi_getservbyname_r="yes"
        tst_nargs_getservbyname_r="6"
      ],[
        AC_MSG_RESULT([no])
        tst_compi_getservbyname_r="no"
      ])
    fi
    AC_MSG_CHECKING([if getservbyname_r is compilable])
    if test "$tst_compi_getservbyname_r" = "yes"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
  #
  if test "$tst_compi_getservbyname_r" = "yes"; then
    AC_MSG_CHECKING([if getservbyname_r usage allowed])
    if test "x$cares_disallow_getservbyname_r" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_getservbyname_r="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_getservbyname_r="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if getservbyname_r might be used])
  if test "$tst_links_getservbyname_r" = "yes" &&
     test "$tst_proto_getservbyname_r" = "yes" &&
     test "$tst_compi_getservbyname_r" = "yes" &&
     test "$tst_allow_getservbyname_r" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GETSERVBYNAME_R, 1,
      [Define to 1 if you have the getservbyname_r function.])
    AC_DEFINE_UNQUOTED(GETSERVBYNAME_R_ARGS, $tst_nargs_getservbyname_r,
      [Specifies the number of arguments to getservbyname_r])
    if test "$tst_nargs_getservbyname_r" -eq "4"; then
      AC_DEFINE(GETSERVBYNAME_R_BUFSIZE, sizeof(struct servent_data),
        [Specifies the size of the buffer to pass to getservbyname_r])
    else
      AC_DEFINE(GETSERVBYNAME_R_BUFSIZE, 4096,
        [Specifies the size of the buffer to pass to getservbyname_r])
    fi
    ac_cv_func_getservbyname_r="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_getservbyname_r="no"
  fi
])


dnl CARES_CHECK_FUNC_INET_NET_PTON
dnl -------------------------------------------------
dnl Verify if inet_net_pton is available, prototyped, can
dnl be compiled and seems to work. If all of these are
dnl true, and usage has not been previously disallowed
dnl with shell variable cares_disallow_inet_net_pton, then
dnl HAVE_INET_NET_PTON will be defined.

AC_DEFUN([CARES_CHECK_FUNC_INET_NET_PTON], [
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  AC_REQUIRE([CARES_INCLUDES_ARPA_INET])dnl
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_inet_net_pton="unknown"
  tst_proto_inet_net_pton="unknown"
  tst_compi_inet_net_pton="unknown"
  tst_works_inet_net_pton="unknown"
  tst_allow_inet_net_pton="unknown"
  #
  AC_MSG_CHECKING([if inet_net_pton can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([inet_net_pton])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_inet_net_pton="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_inet_net_pton="no"
  ])
  #
  if test "$tst_links_inet_net_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_net_pton is prototyped])
    AC_EGREP_CPP([inet_net_pton],[
      $cares_includes_arpa_inet
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_inet_net_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_inet_net_pton="no"
    ])
  fi
  #
  if test "$tst_proto_inet_net_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_net_pton is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_arpa_inet
      ]],[[
        if(0 != inet_net_pton(0, 0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_inet_net_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_inet_net_pton="no"
    ])
  fi
  #
  dnl only do runtime verification when not cross-compiling
  if test "x$cross_compiling" != "xyes" &&
    test "$tst_compi_inet_net_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_net_pton seems to work])
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stdlib
        $cares_includes_arpa_inet
        $cares_includes_string
      ]],[[
        unsigned char ipv6a[16+1];
        unsigned char ipv4a[4+1];
        const char *ipv6net1 = "fe80::214:4fff:fe0b:76c8";
        const char *ipv6net2 = "::fffe:7f00:1";
        const char *ipv6net3 = "7f20:1::/64";
        const char *ipv6net4 = "7f20:1::/2147483649";
        const char *ipv4net1 = "192.168.100.1";
        const char *ipv4net2 = "192.168.100/32";
        const char *ipv4net3 = "192.168.100.1/2147483649";
        /* - */
        memset(ipv4a, 1, sizeof(ipv4a));
        if(32 != inet_net_pton(AF_INET, ipv4net1, ipv4a, 4))
          exit(1); /* fail */
        /* - */
        if( (ipv4a[0x00] != 0xc0) ||
            (ipv4a[0x01] != 0xa8) ||
            (ipv4a[0x02] != 0x64) ||
            (ipv4a[0x03] != 0x01) ||
            (ipv4a[0x04] != 0x01) )
          exit(1); /* fail */
        /* - */
        memset(ipv4a, 1, sizeof(ipv4a));
        if(32 != inet_net_pton(AF_INET, ipv4net2, ipv4a, 4))
          exit(1); /* fail */
        /* - */
        if( (ipv4a[0x00] != 0xc0) ||
            (ipv4a[0x01] != 0xa8) ||
            (ipv4a[0x02] != 0x64) ||
            (ipv4a[0x03] != 0x00) ||
            (ipv4a[0x04] != 0x01) )
          exit(1); /* fail */
        /* - */
        memset(ipv4a, 1, sizeof(ipv4a));
        if(-1 != inet_net_pton(AF_INET, ipv4net3, ipv4a, 4))
          exit(1); /* fail */
        /* - */
        memset(ipv6a, 1, sizeof(ipv6a));
        if(128 != inet_net_pton(AF_INET6, ipv6net1, ipv6a, 16))
          exit(1); /* fail */
        /* - */
        if( (ipv6a[0x00] != 0xfe) ||
            (ipv6a[0x01] != 0x80) ||
            (ipv6a[0x08] != 0x02) ||
            (ipv6a[0x09] != 0x14) ||
            (ipv6a[0x0a] != 0x4f) ||
            (ipv6a[0x0b] != 0xff) ||
            (ipv6a[0x0c] != 0xfe) ||
            (ipv6a[0x0d] != 0x0b) ||
            (ipv6a[0x0e] != 0x76) ||
            (ipv6a[0x0f] != 0xc8) ||
            (ipv6a[0x10] != 0x01) )
          exit(1); /* fail */
        /* - */
        if( (ipv6a[0x02] != 0x0) ||
            (ipv6a[0x03] != 0x0) ||
            (ipv6a[0x04] != 0x0) ||
            (ipv6a[0x05] != 0x0) ||
            (ipv6a[0x06] != 0x0) ||
            (ipv6a[0x07] != 0x0) )
          exit(1); /* fail */
        /* - */
        memset(ipv6a, 0, sizeof(ipv6a));
        ipv6a[0x10] = 0x01;
        if(128 != inet_net_pton(AF_INET6, ipv6net2, ipv6a, 16))
          exit(1); /* fail */
        /* - */
        if( (ipv6a[0x0a] != 0xff) ||
            (ipv6a[0x0b] != 0xfe) ||
            (ipv6a[0x0c] != 0x7f) ||
            (ipv6a[0x0f] != 0x01) ||
            (ipv6a[0x10] != 0x01) )
          exit(1); /* fail */
        /* - */
        if( (ipv6a[0x00] != 0x0) ||
            (ipv6a[0x01] != 0x0) ||
            (ipv6a[0x02] != 0x0) ||
            (ipv6a[0x03] != 0x0) ||
            (ipv6a[0x04] != 0x0) ||
            (ipv6a[0x05] != 0x0) ||
            (ipv6a[0x06] != 0x0) ||
            (ipv6a[0x07] != 0x0) ||
            (ipv6a[0x08] != 0x0) ||
            (ipv6a[0x09] != 0x0) ||
            (ipv6a[0x0d] != 0x0) ||
            (ipv6a[0x0e] != 0x0) )
          exit(1); /* fail */
        /* - */
        memset(ipv6a, 1, sizeof(ipv6a));
        if(64 != inet_net_pton(AF_INET6, ipv6net3, ipv6a, 16))
          exit(1); /* fail */
        if( (ipv6a[0x00] != 0x7f) ||
            (ipv6a[0x01] != 0x20) ||
            (ipv6a[0x03] != 0x01) ||
            (ipv6a[0x08] != 0x01) ||
            (ipv6a[0x09] != 0x01) ||
            (ipv6a[0x0a] != 0x01) ||
            (ipv6a[0x0b] != 0x01) ||
            (ipv6a[0x0c] != 0x01) ||
            (ipv6a[0x0d] != 0x01) ||
            (ipv6a[0x0e] != 0x01) ||
            (ipv6a[0x0f] != 0x01) ||
            (ipv6a[0x10] != 0x01) )
          exit(1); /* fail */
        if( (ipv6a[0x02] != 0x0) ||
            (ipv6a[0x04] != 0x0) ||
            (ipv6a[0x05] != 0x0) ||
            (ipv6a[0x06] != 0x0) ||
            (ipv6a[0x07] != 0x0) ||
            (ipv6a[0x07] != 0x0) )
          exit(1); /* fail */
        /* - */
        memset(ipv6a, 1, sizeof(ipv6a));
        if(-1 != inet_net_pton(AF_INET6, ipv6net4, ipv6a, 16))
          exit(1); /* fail */
        /* - */
        exit(0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_works_inet_net_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_works_inet_net_pton="no"
    ])
  fi
  #
  if test "$tst_compi_inet_net_pton" = "yes" &&
    test "$tst_works_inet_net_pton" != "no"; then
    AC_MSG_CHECKING([if inet_net_pton usage allowed])
    if test "x$cares_disallow_inet_net_pton" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_inet_net_pton="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_inet_net_pton="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if inet_net_pton might be used])
  if test "$tst_links_inet_net_pton" = "yes" &&
     test "$tst_proto_inet_net_pton" = "yes" &&
     test "$tst_compi_inet_net_pton" = "yes" &&
     test "$tst_allow_inet_net_pton" = "yes" &&
     test "$tst_works_inet_net_pton" != "no"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_INET_NET_PTON, 1,
      [Define to 1 if you have a IPv6 capable working inet_net_pton function.])
    ac_cv_func_inet_net_pton="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_inet_net_pton="no"
  fi
])


dnl CARES_CHECK_FUNC_INET_NTOP
dnl -------------------------------------------------
dnl Verify if inet_ntop is available, prototyped, can
dnl be compiled and seems to work. If all of these are
dnl true, and usage has not been previously disallowed
dnl with shell variable cares_disallow_inet_ntop, then
dnl HAVE_INET_NTOP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_INET_NTOP], [
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  AC_REQUIRE([CARES_INCLUDES_ARPA_INET])dnl
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_inet_ntop="unknown"
  tst_proto_inet_ntop="unknown"
  tst_compi_inet_ntop="unknown"
  tst_works_inet_ntop="unknown"
  tst_allow_inet_ntop="unknown"
  #
  AC_MSG_CHECKING([if inet_ntop can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([inet_ntop])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_inet_ntop="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_inet_ntop="no"
  ])
  #
  if test "$tst_links_inet_ntop" = "yes"; then
    AC_MSG_CHECKING([if inet_ntop is prototyped])
    AC_EGREP_CPP([inet_ntop],[
      $cares_includes_arpa_inet
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_inet_ntop="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_inet_ntop="no"
    ])
  fi
  #
  if test "$tst_proto_inet_ntop" = "yes"; then
    AC_MSG_CHECKING([if inet_ntop is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_arpa_inet
      ]],[[
        if(0 != inet_ntop(0, 0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_inet_ntop="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_inet_ntop="no"
    ])
  fi
  #
  dnl only do runtime verification when not cross-compiling
  if test "x$cross_compiling" != "xyes" &&
    test "$tst_compi_inet_ntop" = "yes"; then
    AC_MSG_CHECKING([if inet_ntop seems to work])
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stdlib
        $cares_includes_arpa_inet
        $cares_includes_string
      ]],[[
        char ipv6res[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
        char ipv4res[sizeof "255.255.255.255"];
        unsigned char ipv6a[26];
        unsigned char ipv4a[5];
        char *ipv6ptr = 0;
        char *ipv4ptr = 0;
        /* - */
        ipv4res[0] = '\0';
        ipv4a[0] = 0xc0;
        ipv4a[1] = 0xa8;
        ipv4a[2] = 0x64;
        ipv4a[3] = 0x01;
        ipv4a[4] = 0x01;
        /* - */
        ipv4ptr = inet_ntop(AF_INET, ipv4a, ipv4res, sizeof(ipv4res));
        if(!ipv4ptr)
          exit(1); /* fail */
        if(ipv4ptr != ipv4res)
          exit(1); /* fail */
        if(!ipv4ptr[0])
          exit(1); /* fail */
        if(memcmp(ipv4res, "192.168.100.1", 13) != 0)
          exit(1); /* fail */
        /* - */
        ipv6res[0] = '\0';
        memset(ipv6a, 0, sizeof(ipv6a));
        ipv6a[0] = 0xfe;
        ipv6a[1] = 0x80;
        ipv6a[8] = 0x02;
        ipv6a[9] = 0x14;
        ipv6a[10] = 0x4f;
        ipv6a[11] = 0xff;
        ipv6a[12] = 0xfe;
        ipv6a[13] = 0x0b;
        ipv6a[14] = 0x76;
        ipv6a[15] = 0xc8;
        ipv6a[25] = 0x01;
        /* - */
        ipv6ptr = inet_ntop(AF_INET6, ipv6a, ipv6res, sizeof(ipv6res));
        if(!ipv6ptr)
          exit(1); /* fail */
        if(ipv6ptr != ipv6res)
          exit(1); /* fail */
        if(!ipv6ptr[0])
          exit(1); /* fail */
        if(memcmp(ipv6res, "fe80::214:4fff:fe0b:76c8", 24) != 0)
          exit(1); /* fail */
        /* - */
        exit(0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_works_inet_ntop="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_works_inet_ntop="no"
    ])
  fi
  #
  if test "$tst_compi_inet_ntop" = "yes" &&
    test "$tst_works_inet_ntop" != "no"; then
    AC_MSG_CHECKING([if inet_ntop usage allowed])
    if test "x$cares_disallow_inet_ntop" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_inet_ntop="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_inet_ntop="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if inet_ntop might be used])
  if test "$tst_links_inet_ntop" = "yes" &&
     test "$tst_proto_inet_ntop" = "yes" &&
     test "$tst_compi_inet_ntop" = "yes" &&
     test "$tst_allow_inet_ntop" = "yes" &&
     test "$tst_works_inet_ntop" != "no"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_INET_NTOP, 1,
      [Define to 1 if you have a IPv6 capable working inet_ntop function.])
    ac_cv_func_inet_ntop="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_inet_ntop="no"
  fi
])


dnl CARES_CHECK_FUNC_INET_PTON
dnl -------------------------------------------------
dnl Verify if inet_pton is available, prototyped, can
dnl be compiled and seems to work. If all of these are
dnl true, and usage has not been previously disallowed
dnl with shell variable cares_disallow_inet_pton, then
dnl HAVE_INET_PTON will be defined.

AC_DEFUN([CARES_CHECK_FUNC_INET_PTON], [
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  AC_REQUIRE([CARES_INCLUDES_ARPA_INET])dnl
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_inet_pton="unknown"
  tst_proto_inet_pton="unknown"
  tst_compi_inet_pton="unknown"
  tst_works_inet_pton="unknown"
  tst_allow_inet_pton="unknown"
  #
  AC_MSG_CHECKING([if inet_pton can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([inet_pton])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_inet_pton="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_inet_pton="no"
  ])
  #
  if test "$tst_links_inet_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_pton is prototyped])
    AC_EGREP_CPP([inet_pton],[
      $cares_includes_arpa_inet
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_inet_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_inet_pton="no"
    ])
  fi
  #
  if test "$tst_proto_inet_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_pton is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_arpa_inet
      ]],[[
        if(0 != inet_pton(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_inet_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_inet_pton="no"
    ])
  fi
  #
  dnl only do runtime verification when not cross-compiling
  if test "x$cross_compiling" != "xyes" &&
    test "$tst_compi_inet_pton" = "yes"; then
    AC_MSG_CHECKING([if inet_pton seems to work])
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stdlib
        $cares_includes_arpa_inet
        $cares_includes_string
      ]],[[
        unsigned char ipv6a[16+1];
        unsigned char ipv4a[4+1];
        const char *ipv6src = "fe80::214:4fff:fe0b:76c8";
        const char *ipv4src = "192.168.100.1";
        /* - */
        memset(ipv4a, 1, sizeof(ipv4a));
        if(1 != inet_pton(AF_INET, ipv4src, ipv4a))
          exit(1); /* fail */
        /* - */
        if( (ipv4a[0] != 0xc0) ||
            (ipv4a[1] != 0xa8) ||
            (ipv4a[2] != 0x64) ||
            (ipv4a[3] != 0x01) ||
            (ipv4a[4] != 0x01) )
          exit(1); /* fail */
        /* - */
        memset(ipv6a, 1, sizeof(ipv6a));
        if(1 != inet_pton(AF_INET6, ipv6src, ipv6a))
          exit(1); /* fail */
        /* - */
        if( (ipv6a[0]  != 0xfe) ||
            (ipv6a[1]  != 0x80) ||
            (ipv6a[8]  != 0x02) ||
            (ipv6a[9]  != 0x14) ||
            (ipv6a[10] != 0x4f) ||
            (ipv6a[11] != 0xff) ||
            (ipv6a[12] != 0xfe) ||
            (ipv6a[13] != 0x0b) ||
            (ipv6a[14] != 0x76) ||
            (ipv6a[15] != 0xc8) ||
            (ipv6a[16] != 0x01) )
          exit(1); /* fail */
        /* - */
        if( (ipv6a[2]  != 0x0) ||
            (ipv6a[3]  != 0x0) ||
            (ipv6a[4]  != 0x0) ||
            (ipv6a[5]  != 0x0) ||
            (ipv6a[6]  != 0x0) ||
            (ipv6a[7]  != 0x0) )
          exit(1); /* fail */
        /* - */
        exit(0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_works_inet_pton="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_works_inet_pton="no"
    ])
  fi
  #
  if test "$tst_compi_inet_pton" = "yes" &&
    test "$tst_works_inet_pton" != "no"; then
    AC_MSG_CHECKING([if inet_pton usage allowed])
    if test "x$cares_disallow_inet_pton" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_inet_pton="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_inet_pton="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if inet_pton might be used])
  if test "$tst_links_inet_pton" = "yes" &&
     test "$tst_proto_inet_pton" = "yes" &&
     test "$tst_compi_inet_pton" = "yes" &&
     test "$tst_allow_inet_pton" = "yes" &&
     test "$tst_works_inet_pton" != "no"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_INET_PTON, 1,
      [Define to 1 if you have a IPv6 capable working inet_pton function.])
    ac_cv_func_inet_pton="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_inet_pton="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTL
dnl -------------------------------------------------
dnl Verify if ioctl is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_ioctl, then
dnl HAVE_IOCTL will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTL], [
  AC_REQUIRE([CARES_INCLUDES_STROPTS])dnl
  #
  tst_links_ioctl="unknown"
  tst_proto_ioctl="unknown"
  tst_compi_ioctl="unknown"
  tst_allow_ioctl="unknown"
  #
  AC_MSG_CHECKING([if ioctl can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([ioctl])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_ioctl="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_ioctl="no"
  ])
  #
  if test "$tst_links_ioctl" = "yes"; then
    AC_MSG_CHECKING([if ioctl is prototyped])
    AC_EGREP_CPP([ioctl],[
      $cares_includes_stropts
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_ioctl="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_ioctl="no"
    ])
  fi
  #
  if test "$tst_proto_ioctl" = "yes"; then
    AC_MSG_CHECKING([if ioctl is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stropts
      ]],[[
        if(0 != ioctl(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctl="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctl="no"
    ])
  fi
  #
  if test "$tst_compi_ioctl" = "yes"; then
    AC_MSG_CHECKING([if ioctl usage allowed])
    if test "x$cares_disallow_ioctl" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctl="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctl="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if ioctl might be used])
  if test "$tst_links_ioctl" = "yes" &&
     test "$tst_proto_ioctl" = "yes" &&
     test "$tst_compi_ioctl" = "yes" &&
     test "$tst_allow_ioctl" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTL, 1,
      [Define to 1 if you have the ioctl function.])
    ac_cv_func_ioctl="yes"
    CARES_CHECK_FUNC_IOCTL_FIONBIO
    CARES_CHECK_FUNC_IOCTL_SIOCGIFADDR
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctl="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTL_FIONBIO
dnl -------------------------------------------------
dnl Verify if ioctl with the FIONBIO command is
dnl available, can be compiled, and seems to work. If
dnl all of these are true, then HAVE_IOCTL_FIONBIO
dnl will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTL_FIONBIO], [
  #
  tst_compi_ioctl_fionbio="unknown"
  tst_allow_ioctl_fionbio="unknown"
  #
  if test "$ac_cv_func_ioctl" = "yes"; then
    AC_MSG_CHECKING([if ioctl FIONBIO is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stropts
      ]],[[
        int flags = 0;
        if(0 != ioctl(0, FIONBIO, &flags))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctl_fionbio="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctl_fionbio="no"
    ])
  fi
  #
  if test "$tst_compi_ioctl_fionbio" = "yes"; then
    AC_MSG_CHECKING([if ioctl FIONBIO usage allowed])
    if test "x$cares_disallow_ioctl_fionbio" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctl_fionbio="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctl_fionbio="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if ioctl FIONBIO might be used])
  if test "$tst_compi_ioctl_fionbio" = "yes" &&
     test "$tst_allow_ioctl_fionbio" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTL_FIONBIO, 1,
      [Define to 1 if you have a working ioctl FIONBIO function.])
    ac_cv_func_ioctl_fionbio="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctl_fionbio="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTL_SIOCGIFADDR
dnl -------------------------------------------------
dnl Verify if ioctl with the SIOCGIFADDR command is available,
dnl struct ifreq is defined, they can be compiled, and seem to
dnl work. If all of these are true, then HAVE_IOCTL_SIOCGIFADDR
dnl will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTL_SIOCGIFADDR], [
  #
  tst_compi_ioctl_siocgifaddr="unknown"
  tst_allow_ioctl_siocgifaddr="unknown"
  #
  if test "$ac_cv_func_ioctl" = "yes"; then
    AC_MSG_CHECKING([if ioctl SIOCGIFADDR is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stropts
        #include <net/if.h>
      ]],[[
        struct ifreq ifr;
        if(0 != ioctl(0, SIOCGIFADDR, &ifr))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctl_siocgifaddr="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctl_siocgifaddr="no"
    ])
  fi
  #
  if test "$tst_compi_ioctl_siocgifaddr" = "yes"; then
    AC_MSG_CHECKING([if ioctl SIOCGIFADDR usage allowed])
    if test "x$cares_disallow_ioctl_siocgifaddr" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctl_siocgifaddr="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctl_siocgifaddr="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if ioctl SIOCGIFADDR might be used])
  if test "$tst_compi_ioctl_siocgifaddr" = "yes" &&
     test "$tst_allow_ioctl_siocgifaddr" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTL_SIOCGIFADDR, 1,
      [Define to 1 if you have a working ioctl SIOCGIFADDR function.])
    ac_cv_func_ioctl_siocgifaddr="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctl_siocgifaddr="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTLSOCKET
dnl -------------------------------------------------
dnl Verify if ioctlsocket is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_ioctlsocket, then
dnl HAVE_IOCTLSOCKET will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTLSOCKET], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  #
  tst_links_ioctlsocket="unknown"
  tst_proto_ioctlsocket="unknown"
  tst_compi_ioctlsocket="unknown"
  tst_allow_ioctlsocket="unknown"
  #
  AC_MSG_CHECKING([if ioctlsocket can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
    ]],[[
      if(0 != ioctlsocket(0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_ioctlsocket="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_ioctlsocket="no"
  ])
  #
  if test "$tst_links_ioctlsocket" = "yes"; then
    AC_MSG_CHECKING([if ioctlsocket is prototyped])
    AC_EGREP_CPP([ioctlsocket],[
      $cares_includes_winsock2
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_ioctlsocket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_ioctlsocket="no"
    ])
  fi
  #
  if test "$tst_proto_ioctlsocket" = "yes"; then
    AC_MSG_CHECKING([if ioctlsocket is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
      ]],[[
        if(0 != ioctlsocket(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctlsocket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctlsocket="no"
    ])
  fi
  #
  if test "$tst_compi_ioctlsocket" = "yes"; then
    AC_MSG_CHECKING([if ioctlsocket usage allowed])
    if test "x$cares_disallow_ioctlsocket" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctlsocket="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctlsocket="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if ioctlsocket might be used])
  if test "$tst_links_ioctlsocket" = "yes" &&
     test "$tst_proto_ioctlsocket" = "yes" &&
     test "$tst_compi_ioctlsocket" = "yes" &&
     test "$tst_allow_ioctlsocket" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTLSOCKET, 1,
      [Define to 1 if you have the ioctlsocket function.])
    ac_cv_func_ioctlsocket="yes"
    CARES_CHECK_FUNC_IOCTLSOCKET_FIONBIO
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctlsocket="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTLSOCKET_FIONBIO
dnl -------------------------------------------------
dnl Verify if ioctlsocket with the FIONBIO command is
dnl available, can be compiled, and seems to work. If
dnl all of these are true, then HAVE_IOCTLSOCKET_FIONBIO
dnl will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTLSOCKET_FIONBIO], [
  #
  tst_compi_ioctlsocket_fionbio="unknown"
  tst_allow_ioctlsocket_fionbio="unknown"
  #
  if test "$ac_cv_func_ioctlsocket" = "yes"; then
    AC_MSG_CHECKING([if ioctlsocket FIONBIO is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
      ]],[[
        int flags = 0;
        if(0 != ioctlsocket(0, FIONBIO, &flags))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctlsocket_fionbio="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctlsocket_fionbio="no"
    ])
  fi
  #
  if test "$tst_compi_ioctlsocket_fionbio" = "yes"; then
    AC_MSG_CHECKING([if ioctlsocket FIONBIO usage allowed])
    if test "x$cares_disallow_ioctlsocket_fionbio" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctlsocket_fionbio="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctlsocket_fionbio="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if ioctlsocket FIONBIO might be used])
  if test "$tst_compi_ioctlsocket_fionbio" = "yes" &&
     test "$tst_allow_ioctlsocket_fionbio" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTLSOCKET_FIONBIO, 1,
      [Define to 1 if you have a working ioctlsocket FIONBIO function.])
    ac_cv_func_ioctlsocket_fionbio="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctlsocket_fionbio="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL
dnl -------------------------------------------------
dnl Verify if IoctlSocket is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_ioctlsocket_camel,
dnl then HAVE_IOCTLSOCKET_CAMEL will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL], [
  AC_REQUIRE([CARES_INCLUDES_STROPTS])dnl
  #
  tst_links_ioctlsocket_camel="unknown"
  tst_proto_ioctlsocket_camel="unknown"
  tst_compi_ioctlsocket_camel="unknown"
  tst_allow_ioctlsocket_camel="unknown"
  #
  AC_MSG_CHECKING([if IoctlSocket can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([IoctlSocket])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_ioctlsocket_camel="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_ioctlsocket_camel="no"
  ])
  #
  if test "$tst_links_ioctlsocket_camel" = "yes"; then
    AC_MSG_CHECKING([if IoctlSocket is prototyped])
    AC_EGREP_CPP([IoctlSocket],[
      $cares_includes_stropts
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_ioctlsocket_camel="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_ioctlsocket_camel="no"
    ])
  fi
  #
  if test "$tst_proto_ioctlsocket_camel" = "yes"; then
    AC_MSG_CHECKING([if IoctlSocket is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stropts
      ]],[[
        if(0 != IoctlSocket(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctlsocket_camel="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctlsocket_camel="no"
    ])
  fi
  #
  if test "$tst_compi_ioctlsocket_camel" = "yes"; then
    AC_MSG_CHECKING([if IoctlSocket usage allowed])
    if test "x$cares_disallow_ioctlsocket_camel" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctlsocket_camel="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctlsocket_camel="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if IoctlSocket might be used])
  if test "$tst_links_ioctlsocket_camel" = "yes" &&
     test "$tst_proto_ioctlsocket_camel" = "yes" &&
     test "$tst_compi_ioctlsocket_camel" = "yes" &&
     test "$tst_allow_ioctlsocket_camel" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTLSOCKET_CAMEL, 1,
      [Define to 1 if you have the IoctlSocket camel case function.])
    ac_cv_func_ioctlsocket_camel="yes"
    CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL_FIONBIO
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctlsocket_camel="no"
  fi
])


dnl CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL_FIONBIO
dnl -------------------------------------------------
dnl Verify if IoctlSocket with FIONBIO command is available,
dnl can be compiled, and seems to work. If all of these are
dnl true, then HAVE_IOCTLSOCKET_CAMEL_FIONBIO will be defined.

AC_DEFUN([CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL_FIONBIO], [
  #
  tst_compi_ioctlsocket_camel_fionbio="unknown"
  tst_allow_ioctlsocket_camel_fionbio="unknown"
  #
  if test "$ac_cv_func_ioctlsocket_camel" = "yes"; then
    AC_MSG_CHECKING([if IoctlSocket FIONBIO is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stropts
      ]],[[
        long flags = 0;
        if(0 != ioctlsocket(0, FIONBIO, &flags))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_ioctlsocket_camel_fionbio="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_ioctlsocket_camel_fionbio="no"
    ])
  fi
  #
  if test "$tst_compi_ioctlsocket_camel_fionbio" = "yes"; then
    AC_MSG_CHECKING([if IoctlSocket FIONBIO usage allowed])
    if test "x$cares_disallow_ioctlsocket_camel_fionbio" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_ioctlsocket_camel_fionbio="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_ioctlsocket_camel_fionbio="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if IoctlSocket FIONBIO might be used])
  if test "$tst_compi_ioctlsocket_camel_fionbio" = "yes" &&
     test "$tst_allow_ioctlsocket_camel_fionbio" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_IOCTLSOCKET_CAMEL_FIONBIO, 1,
      [Define to 1 if you have a working IoctlSocket camel case FIONBIO function.])
    ac_cv_func_ioctlsocket_camel_fionbio="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_ioctlsocket_camel_fionbio="no"
  fi
])


dnl CARES_CHECK_FUNC_SETSOCKOPT
dnl -------------------------------------------------
dnl Verify if setsockopt is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_setsockopt, then
dnl HAVE_SETSOCKOPT will be defined.

AC_DEFUN([CARES_CHECK_FUNC_SETSOCKOPT], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  #
  tst_links_setsockopt="unknown"
  tst_proto_setsockopt="unknown"
  tst_compi_setsockopt="unknown"
  tst_allow_setsockopt="unknown"
  #
  AC_MSG_CHECKING([if setsockopt can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_sys_socket
    ]],[[
      if(0 != setsockopt(0, 0, 0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_setsockopt="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_setsockopt="no"
  ])
  #
  if test "$tst_links_setsockopt" = "yes"; then
    AC_MSG_CHECKING([if setsockopt is prototyped])
    AC_EGREP_CPP([setsockopt],[
      $cares_includes_winsock2
      $cares_includes_sys_socket
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_setsockopt="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_setsockopt="no"
    ])
  fi
  #
  if test "$tst_proto_setsockopt" = "yes"; then
    AC_MSG_CHECKING([if setsockopt is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_sys_socket
      ]],[[
        if(0 != setsockopt(0, 0, 0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_setsockopt="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_setsockopt="no"
    ])
  fi
  #
  if test "$tst_compi_setsockopt" = "yes"; then
    AC_MSG_CHECKING([if setsockopt usage allowed])
    if test "x$cares_disallow_setsockopt" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_setsockopt="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_setsockopt="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if setsockopt might be used])
  if test "$tst_links_setsockopt" = "yes" &&
     test "$tst_proto_setsockopt" = "yes" &&
     test "$tst_compi_setsockopt" = "yes" &&
     test "$tst_allow_setsockopt" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_SETSOCKOPT, 1,
      [Define to 1 if you have the setsockopt function.])
    ac_cv_func_setsockopt="yes"
    CARES_CHECK_FUNC_SETSOCKOPT_SO_NONBLOCK
  else
    AC_MSG_RESULT([no])
    ac_cv_func_setsockopt="no"
  fi
])


dnl CARES_CHECK_FUNC_SETSOCKOPT_SO_NONBLOCK
dnl -------------------------------------------------
dnl Verify if setsockopt with the SO_NONBLOCK command is
dnl available, can be compiled, and seems to work. If
dnl all of these are true, then HAVE_SETSOCKOPT_SO_NONBLOCK
dnl will be defined.

AC_DEFUN([CARES_CHECK_FUNC_SETSOCKOPT_SO_NONBLOCK], [
  #
  tst_compi_setsockopt_so_nonblock="unknown"
  tst_allow_setsockopt_so_nonblock="unknown"
  #
  if test "$ac_cv_func_setsockopt" = "yes"; then
    AC_MSG_CHECKING([if setsockopt SO_NONBLOCK is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_sys_socket
      ]],[[
        if(0 != setsockopt(0, SOL_SOCKET, SO_NONBLOCK, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_setsockopt_so_nonblock="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_setsockopt_so_nonblock="no"
    ])
  fi
  #
  if test "$tst_compi_setsockopt_so_nonblock" = "yes"; then
    AC_MSG_CHECKING([if setsockopt SO_NONBLOCK usage allowed])
    if test "x$cares_disallow_setsockopt_so_nonblock" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_setsockopt_so_nonblock="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_setsockopt_so_nonblock="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if setsockopt SO_NONBLOCK might be used])
  if test "$tst_compi_setsockopt_so_nonblock" = "yes" &&
     test "$tst_allow_setsockopt_so_nonblock" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_SETSOCKOPT_SO_NONBLOCK, 1,
      [Define to 1 if you have a working setsockopt SO_NONBLOCK function.])
    ac_cv_func_setsockopt_so_nonblock="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_setsockopt_so_nonblock="no"
  fi
])


dnl CARES_CHECK_FUNC_SOCKET
dnl -------------------------------------------------
dnl Verify if socket is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_socket, then
dnl HAVE_SOCKET will be defined.

AC_DEFUN([CARES_CHECK_FUNC_SOCKET], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  AC_REQUIRE([CARES_INCLUDES_SOCKET])dnl
  #
  tst_links_socket="unknown"
  tst_proto_socket="unknown"
  tst_compi_socket="unknown"
  tst_allow_socket="unknown"
  #
  AC_MSG_CHECKING([if socket can be linked])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      $cares_includes_winsock2
      $cares_includes_sys_socket
      $cares_includes_socket
    ]],[[
      if(0 != socket(0, 0, 0))
        return 1;
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_socket="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_socket="no"
  ])
  #
  if test "$tst_links_socket" = "yes"; then
    AC_MSG_CHECKING([if socket is prototyped])
    AC_EGREP_CPP([socket],[
      $cares_includes_winsock2
      $cares_includes_sys_socket
      $cares_includes_socket
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_socket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_socket="no"
    ])
  fi
  #
  if test "$tst_proto_socket" = "yes"; then
    AC_MSG_CHECKING([if socket is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_winsock2
        $cares_includes_sys_socket
        $cares_includes_socket
      ]],[[
        if(0 != socket(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_socket="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_socket="no"
    ])
  fi
  #
  if test "$tst_compi_socket" = "yes"; then
    AC_MSG_CHECKING([if socket usage allowed])
    if test "x$cares_disallow_socket" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_socket="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_socket="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if socket might be used])
  if test "$tst_links_socket" = "yes" &&
     test "$tst_proto_socket" = "yes" &&
     test "$tst_compi_socket" = "yes" &&
     test "$tst_allow_socket" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_SOCKET, 1,
      [Define to 1 if you have the socket function.])
    ac_cv_func_socket="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_socket="no"
  fi
])


dnl CARES_CHECK_FUNC_STRCASECMP
dnl -------------------------------------------------
dnl Verify if strcasecmp is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strcasecmp, then
dnl HAVE_STRCASECMP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRCASECMP], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strcasecmp="unknown"
  tst_proto_strcasecmp="unknown"
  tst_compi_strcasecmp="unknown"
  tst_allow_strcasecmp="unknown"
  #
  AC_MSG_CHECKING([if strcasecmp can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strcasecmp])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strcasecmp="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strcasecmp="no"
  ])
  #
  if test "$tst_links_strcasecmp" = "yes"; then
    AC_MSG_CHECKING([if strcasecmp is prototyped])
    AC_EGREP_CPP([strcasecmp],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strcasecmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strcasecmp="no"
    ])
  fi
  #
  if test "$tst_proto_strcasecmp" = "yes"; then
    AC_MSG_CHECKING([if strcasecmp is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strcasecmp(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strcasecmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strcasecmp="no"
    ])
  fi
  #
  if test "$tst_compi_strcasecmp" = "yes"; then
    AC_MSG_CHECKING([if strcasecmp usage allowed])
    if test "x$cares_disallow_strcasecmp" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strcasecmp="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strcasecmp="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strcasecmp might be used])
  if test "$tst_links_strcasecmp" = "yes" &&
     test "$tst_proto_strcasecmp" = "yes" &&
     test "$tst_compi_strcasecmp" = "yes" &&
     test "$tst_allow_strcasecmp" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRCASECMP, 1,
      [Define to 1 if you have the strcasecmp function.])
    ac_cv_func_strcasecmp="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strcasecmp="no"
  fi
])


dnl CARES_CHECK_FUNC_STRCMPI
dnl -------------------------------------------------
dnl Verify if strcmpi is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strcmpi, then
dnl HAVE_STRCMPI will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRCMPI], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strcmpi="unknown"
  tst_proto_strcmpi="unknown"
  tst_compi_strcmpi="unknown"
  tst_allow_strcmpi="unknown"
  #
  AC_MSG_CHECKING([if strcmpi can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strcmpi])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strcmpi="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strcmpi="no"
  ])
  #
  if test "$tst_links_strcmpi" = "yes"; then
    AC_MSG_CHECKING([if strcmpi is prototyped])
    AC_EGREP_CPP([strcmpi],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strcmpi="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strcmpi="no"
    ])
  fi
  #
  if test "$tst_proto_strcmpi" = "yes"; then
    AC_MSG_CHECKING([if strcmpi is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strcmpi(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strcmpi="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strcmpi="no"
    ])
  fi
  #
  if test "$tst_compi_strcmpi" = "yes"; then
    AC_MSG_CHECKING([if strcmpi usage allowed])
    if test "x$cares_disallow_strcmpi" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strcmpi="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strcmpi="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strcmpi might be used])
  if test "$tst_links_strcmpi" = "yes" &&
     test "$tst_proto_strcmpi" = "yes" &&
     test "$tst_compi_strcmpi" = "yes" &&
     test "$tst_allow_strcmpi" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRCMPI, 1,
      [Define to 1 if you have the strcmpi function.])
    ac_cv_func_strcmpi="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strcmpi="no"
  fi
])


dnl CARES_CHECK_FUNC_STRDUP
dnl -------------------------------------------------
dnl Verify if strdup is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strdup, then
dnl HAVE_STRDUP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRDUP], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strdup="unknown"
  tst_proto_strdup="unknown"
  tst_compi_strdup="unknown"
  tst_allow_strdup="unknown"
  #
  AC_MSG_CHECKING([if strdup can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strdup])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strdup="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strdup="no"
  ])
  #
  if test "$tst_links_strdup" = "yes"; then
    AC_MSG_CHECKING([if strdup is prototyped])
    AC_EGREP_CPP([strdup],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strdup="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strdup="no"
    ])
  fi
  #
  if test "$tst_proto_strdup" = "yes"; then
    AC_MSG_CHECKING([if strdup is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strdup(0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strdup="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strdup="no"
    ])
  fi
  #
  if test "$tst_compi_strdup" = "yes"; then
    AC_MSG_CHECKING([if strdup usage allowed])
    if test "x$cares_disallow_strdup" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strdup="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strdup="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strdup might be used])
  if test "$tst_links_strdup" = "yes" &&
     test "$tst_proto_strdup" = "yes" &&
     test "$tst_compi_strdup" = "yes" &&
     test "$tst_allow_strdup" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRDUP, 1,
      [Define to 1 if you have the strdup function.])
    ac_cv_func_strdup="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strdup="no"
  fi
])


dnl CARES_CHECK_FUNC_STRICMP
dnl -------------------------------------------------
dnl Verify if stricmp is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_stricmp, then
dnl HAVE_STRICMP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRICMP], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_stricmp="unknown"
  tst_proto_stricmp="unknown"
  tst_compi_stricmp="unknown"
  tst_allow_stricmp="unknown"
  #
  AC_MSG_CHECKING([if stricmp can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([stricmp])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_stricmp="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_stricmp="no"
  ])
  #
  if test "$tst_links_stricmp" = "yes"; then
    AC_MSG_CHECKING([if stricmp is prototyped])
    AC_EGREP_CPP([stricmp],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_stricmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_stricmp="no"
    ])
  fi
  #
  if test "$tst_proto_stricmp" = "yes"; then
    AC_MSG_CHECKING([if stricmp is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != stricmp(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_stricmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_stricmp="no"
    ])
  fi
  #
  if test "$tst_compi_stricmp" = "yes"; then
    AC_MSG_CHECKING([if stricmp usage allowed])
    if test "x$cares_disallow_stricmp" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_stricmp="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_stricmp="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if stricmp might be used])
  if test "$tst_links_stricmp" = "yes" &&
     test "$tst_proto_stricmp" = "yes" &&
     test "$tst_compi_stricmp" = "yes" &&
     test "$tst_allow_stricmp" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRICMP, 1,
      [Define to 1 if you have the stricmp function.])
    ac_cv_func_stricmp="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_stricmp="no"
  fi
])


dnl CARES_CHECK_FUNC_STRNCASECMP
dnl -------------------------------------------------
dnl Verify if strncasecmp is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strncasecmp, then
dnl HAVE_STRNCASECMP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRNCASECMP], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strncasecmp="unknown"
  tst_proto_strncasecmp="unknown"
  tst_compi_strncasecmp="unknown"
  tst_allow_strncasecmp="unknown"
  #
  AC_MSG_CHECKING([if strncasecmp can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strncasecmp])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strncasecmp="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strncasecmp="no"
  ])
  #
  if test "$tst_links_strncasecmp" = "yes"; then
    AC_MSG_CHECKING([if strncasecmp is prototyped])
    AC_EGREP_CPP([strncasecmp],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strncasecmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strncasecmp="no"
    ])
  fi
  #
  if test "$tst_proto_strncasecmp" = "yes"; then
    AC_MSG_CHECKING([if strncasecmp is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strncasecmp(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strncasecmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strncasecmp="no"
    ])
  fi
  #
  if test "$tst_compi_strncasecmp" = "yes"; then
    AC_MSG_CHECKING([if strncasecmp usage allowed])
    if test "x$cares_disallow_strncasecmp" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strncasecmp="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strncasecmp="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strncasecmp might be used])
  if test "$tst_links_strncasecmp" = "yes" &&
     test "$tst_proto_strncasecmp" = "yes" &&
     test "$tst_compi_strncasecmp" = "yes" &&
     test "$tst_allow_strncasecmp" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRNCASECMP, 1,
      [Define to 1 if you have the strncasecmp function.])
    ac_cv_func_strncasecmp="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strncasecmp="no"
  fi
])


dnl CARES_CHECK_FUNC_STRNCMPI
dnl -------------------------------------------------
dnl Verify if strncmpi is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strncmpi, then
dnl HAVE_STRNCMPI will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRNCMPI], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strncmpi="unknown"
  tst_proto_strncmpi="unknown"
  tst_compi_strncmpi="unknown"
  tst_allow_strncmpi="unknown"
  #
  AC_MSG_CHECKING([if strncmpi can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strncmpi])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strncmpi="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strncmpi="no"
  ])
  #
  if test "$tst_links_strncmpi" = "yes"; then
    AC_MSG_CHECKING([if strncmpi is prototyped])
    AC_EGREP_CPP([strncmpi],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strncmpi="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strncmpi="no"
    ])
  fi
  #
  if test "$tst_proto_strncmpi" = "yes"; then
    AC_MSG_CHECKING([if strncmpi is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strncmpi(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strncmpi="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strncmpi="no"
    ])
  fi
  #
  if test "$tst_compi_strncmpi" = "yes"; then
    AC_MSG_CHECKING([if strncmpi usage allowed])
    if test "x$cares_disallow_strncmpi" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strncmpi="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strncmpi="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strncmpi might be used])
  if test "$tst_links_strncmpi" = "yes" &&
     test "$tst_proto_strncmpi" = "yes" &&
     test "$tst_compi_strncmpi" = "yes" &&
     test "$tst_allow_strncmpi" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRNCMPI, 1,
      [Define to 1 if you have the strncmpi function.])
    ac_cv_func_strncmpi="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strncmpi="no"
  fi
])


dnl CARES_CHECK_FUNC_STRNICMP
dnl -------------------------------------------------
dnl Verify if strnicmp is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_strnicmp, then
dnl HAVE_STRNICMP will be defined.

AC_DEFUN([CARES_CHECK_FUNC_STRNICMP], [
  AC_REQUIRE([CARES_INCLUDES_STRING])dnl
  #
  tst_links_strnicmp="unknown"
  tst_proto_strnicmp="unknown"
  tst_compi_strnicmp="unknown"
  tst_allow_strnicmp="unknown"
  #
  AC_MSG_CHECKING([if strnicmp can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strnicmp])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_strnicmp="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_strnicmp="no"
  ])
  #
  if test "$tst_links_strnicmp" = "yes"; then
    AC_MSG_CHECKING([if strnicmp is prototyped])
    AC_EGREP_CPP([strnicmp],[
      $cares_includes_string
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_strnicmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_strnicmp="no"
    ])
  fi
  #
  if test "$tst_proto_strnicmp" = "yes"; then
    AC_MSG_CHECKING([if strnicmp is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_string
      ]],[[
        if(0 != strnicmp(0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_strnicmp="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_strnicmp="no"
    ])
  fi
  #
  if test "$tst_compi_strnicmp" = "yes"; then
    AC_MSG_CHECKING([if strnicmp usage allowed])
    if test "x$cares_disallow_strnicmp" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_strnicmp="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_strnicmp="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if strnicmp might be used])
  if test "$tst_links_strnicmp" = "yes" &&
     test "$tst_proto_strnicmp" = "yes" &&
     test "$tst_compi_strnicmp" = "yes" &&
     test "$tst_allow_strnicmp" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STRNICMP, 1,
      [Define to 1 if you have the strnicmp function.])
    ac_cv_func_strnicmp="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_strnicmp="no"
  fi
])


dnl CARES_CHECK_FUNC_WRITEV
dnl -------------------------------------------------
dnl Verify if writev is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_writev, then
dnl HAVE_WRITEV will be defined.

AC_DEFUN([CARES_CHECK_FUNC_WRITEV], [
  AC_REQUIRE([CARES_INCLUDES_SYS_UIO])dnl
  #
  tst_links_writev="unknown"
  tst_proto_writev="unknown"
  tst_compi_writev="unknown"
  tst_allow_writev="unknown"
  #
  AC_MSG_CHECKING([if writev can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([writev])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_writev="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_writev="no"
  ])
  #
  if test "$tst_links_writev" = "yes"; then
    AC_MSG_CHECKING([if writev is prototyped])
    AC_EGREP_CPP([writev],[
      $cares_includes_sys_uio
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_writev="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_writev="no"
    ])
  fi
  #
  if test "$tst_proto_writev" = "yes"; then
    AC_MSG_CHECKING([if writev is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_sys_uio
      ]],[[
        if(0 != writev(0, 0, 0))
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_writev="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_writev="no"
    ])
  fi
  #
  if test "$tst_compi_writev" = "yes"; then
    AC_MSG_CHECKING([if writev usage allowed])
    if test "x$cares_disallow_writev" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_writev="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_writev="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if writev might be used])
  if test "$tst_links_writev" = "yes" &&
     test "$tst_proto_writev" = "yes" &&
     test "$tst_compi_writev" = "yes" &&
     test "$tst_allow_writev" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_WRITEV, 1,
      [Define to 1 if you have the writev function.])
    ac_cv_func_writev="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_writev="no"
  fi
])

dnl CARES_CHECK_FUNC_ARC4RANDOM_BUF
dnl -------------------------------------------------
dnl Verify if arc4random_buf is available, prototyped, and
dnl can be compiled. If all of these are true, and
dnl usage has not been previously disallowed with
dnl shell variable cares_disallow_arc4random_buf, then
dnl HAVE_ARC4RANDOM_BUF will be defined.

AC_DEFUN([CARES_CHECK_FUNC_ARC4RANDOM_BUF], [
  AC_REQUIRE([CARES_INCLUDES_STDLIB])dnl
  #
  tst_links_arc4random_buf="unknown"
  tst_proto_arc4random_buf="unknown"
  tst_compi_arc4random_buf="unknown"
  tst_allow_arc4random_buf="unknown"
  #
  AC_MSG_CHECKING([if arc4random_buf can be linked])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([arc4random_buf])
  ],[
    AC_MSG_RESULT([yes])
    tst_links_arc4random_buf="yes"
  ],[
    AC_MSG_RESULT([no])
    tst_links_arc4random_buf="no"
  ])
  #
  if test "$tst_links_arc4random_buf" = "yes"; then
    AC_MSG_CHECKING([if arc4random_buf is prototyped])
    AC_EGREP_CPP([arc4random_buf],[
      $cares_includes_stdlib
    ],[
      AC_MSG_RESULT([yes])
      tst_proto_arc4random_buf="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_proto_arc4random_buf="no"
    ])
  fi
  #
  if test "$tst_proto_arc4random_buf" = "yes"; then
    AC_MSG_CHECKING([if arc4random_buf is compilable])
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $cares_includes_stdlib
      ]],[[
          arc4random_buf(NULL, 0);
          return 1;
      ]])
    ],[
      AC_MSG_RESULT([yes])
      tst_compi_arc4random_buf="yes"
    ],[
      AC_MSG_RESULT([no])
      tst_compi_arc4random_buf="no"
    ])
  fi
  #
  if test "$tst_compi_arc4random_buf" = "yes"; then
    AC_MSG_CHECKING([if arc4random_buf usage allowed])
    if test "x$cares_disallow_arc4random_buf" != "xyes"; then
      AC_MSG_RESULT([yes])
      tst_allow_arc4random_buf="yes"
    else
      AC_MSG_RESULT([no])
      tst_allow_arc4random_buf="no"
    fi
  fi
  #
  AC_MSG_CHECKING([if arc4random_buf might be used])
  if test "$tst_links_arc4random_buf" = "yes" &&
     test "$tst_proto_arc4random_buf" = "yes" &&
     test "$tst_compi_arc4random_buf" = "yes" &&
     test "$tst_allow_arc4random_buf" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_ARC4RANDOM_BUF, 1,
      [Define to 1 if you have the arc4random_buf function.])
    ac_cv_func_arc4random_buf="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_arc4random_buf="no"
  fi
])


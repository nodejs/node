

dnl CURL_CHECK_DEF (SYMBOL, [INCLUDES], [SILENT])
dnl -------------------------------------------------
dnl Use the C preprocessor to find out if the given object-style symbol
dnl is defined and get its expansion. This macro will not use default
dnl includes even if no INCLUDES argument is given. This macro will run
dnl silently when invoked with three arguments. If the expansion would
dnl result in a set of double-quoted strings the returned expansion will
dnl actually be a single double-quoted string concatenating all them.

AC_DEFUN([CURL_CHECK_DEF], [
  AS_VAR_PUSHDEF([ac_HaveDef], [curl_cv_have_def_$1])dnl
  AS_VAR_PUSHDEF([ac_Def], [curl_cv_def_$1])dnl
  if test -z "$SED"; then
    AC_MSG_ERROR([SED not set. Cannot continue without SED being set.])
  fi
  if test -z "$GREP"; then
    AC_MSG_ERROR([GREP not set. Cannot continue without GREP being set.])
  fi
  ifelse($3,,[AC_MSG_CHECKING([for preprocessor definition of $1])])
  tmp_exp=""
  AC_PREPROC_IFELSE([
    AC_LANG_SOURCE(
ifelse($2,,,[$2])[[
#ifdef $1
CURL_DEF_TOKEN $1
#endif
    ]])
  ],[
    tmp_exp=`eval "$ac_cpp conftest.$ac_ext" 2>/dev/null | \
      "$GREP" CURL_DEF_TOKEN 2>/dev/null | \
      "$SED" 's/.*CURL_DEF_TOKEN[[ ]][[ ]]*//' 2>/dev/null | \
      "$SED" 's/[["]][[ ]]*[["]]//g' 2>/dev/null`
    if test -z "$tmp_exp" || test "$tmp_exp" = "$1"; then
      tmp_exp=""
    fi
  ])
  if test -z "$tmp_exp"; then
    AS_VAR_SET(ac_HaveDef, no)
    ifelse($3,,[AC_MSG_RESULT([no])])
  else
    AS_VAR_SET(ac_HaveDef, yes)
    AS_VAR_SET(ac_Def, $tmp_exp)
    ifelse($3,,[AC_MSG_RESULT([$tmp_exp])])
  fi
  AS_VAR_POPDEF([ac_Def])dnl
  AS_VAR_POPDEF([ac_HaveDef])dnl
])


dnl CURL_CHECK_DEF_CC (SYMBOL, [INCLUDES], [SILENT])
dnl -------------------------------------------------
dnl Use the C compiler to find out only if the given symbol is defined
dnl or not, this can not find out its expansion. This macro will not use
dnl default includes even if no INCLUDES argument is given. This macro
dnl will run silently when invoked with three arguments.

AC_DEFUN([CURL_CHECK_DEF_CC], [
  AS_VAR_PUSHDEF([ac_HaveDef], [curl_cv_have_def_$1])dnl
  ifelse($3,,[AC_MSG_CHECKING([for compiler definition of $1])])
  AC_COMPILE_IFELSE([
    AC_LANG_SOURCE(
ifelse($2,,,[$2])[[
int main (void)
{
#ifdef $1
  return 0;
#else
  force compilation error
#endif
}
    ]])
  ],[
    tst_symbol_defined="yes"
  ],[
    tst_symbol_defined="no"
  ])
  if test "$tst_symbol_defined" = "yes"; then
    AS_VAR_SET(ac_HaveDef, yes)
    ifelse($3,,[AC_MSG_RESULT([yes])])
  else
    AS_VAR_SET(ac_HaveDef, no)
    ifelse($3,,[AC_MSG_RESULT([no])])
  fi
  AS_VAR_POPDEF([ac_HaveDef])dnl
])


dnl CARES_CHECK_LIB_XNET
dnl -------------------------------------------------
dnl Verify if X/Open network library is required.

AC_DEFUN([CARES_CHECK_LIB_XNET], [
  AC_MSG_CHECKING([if X/Open network library is required])
  tst_lib_xnet_required="no"
  AC_COMPILE_IFELSE([
    AC_LANG_SOURCE([[
int main (void)
{
#if defined(__hpux) && defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600)
  return 0;
#elif defined(__hpux) && defined(_XOPEN_SOURCE_EXTENDED)
  return 0;
#else
  force compilation error
#endif
}
    ]])
  ],[
    tst_lib_xnet_required="yes"
    LIBS="$LIBS -lxnet"
  ])
  AC_MSG_RESULT([$tst_lib_xnet_required])
])


dnl CARES_CHECK_AIX_ALL_SOURCE
dnl -------------------------------------------------
dnl Provides a replacement of traditional AC_AIX with
dnl an uniform behaviour across all autoconf versions,
dnl and with our own placement rules.

AC_DEFUN([CARES_CHECK_AIX_ALL_SOURCE], [
  AH_VERBATIM([_ALL_SOURCE],
    [/* Define to 1 if OS is AIX. */
#ifndef _ALL_SOURCE
#  undef _ALL_SOURCE
#endif])
  AC_BEFORE([$0], [AC_SYS_LARGEFILE])dnl
  AC_BEFORE([$0], [CARES_CONFIGURE_REENTRANT])dnl
  AC_MSG_CHECKING([if OS is AIX (to define _ALL_SOURCE)])
  AC_EGREP_CPP([yes_this_is_aix],[
#ifdef _AIX
   yes_this_is_aix
#endif
  ],[
    AC_MSG_RESULT([yes])
    AC_DEFINE(_ALL_SOURCE)
  ],[
    AC_MSG_RESULT([no])
  ])
])


dnl CURL_CHECK_HEADER_WINDOWS
dnl -------------------------------------------------
dnl Check for compilable and valid windows.h header

AC_DEFUN([CURL_CHECK_HEADER_WINDOWS], [
  AC_CACHE_CHECK([for windows.h], [ac_cv_header_windows_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__)
        HAVE_WINDOWS_H shall not be defined.
#else
        int dummy=2*WINVER;
#endif
      ]])
    ],[
      ac_cv_header_windows_h="yes"
    ],[
      ac_cv_header_windows_h="no"
    ])
  ])
  case "$ac_cv_header_windows_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINDOWS_H, 1,
        [Define to 1 if you have the windows.h header file.])
      AC_DEFINE_UNQUOTED(WIN32_LEAN_AND_MEAN, 1,
        [Define to avoid automatic inclusion of winsock.h])
      ;;
  esac
])


dnl CURL_CHECK_NATIVE_WINDOWS
dnl -------------------------------------------------
dnl Check if building a native Windows target

AC_DEFUN([CURL_CHECK_NATIVE_WINDOWS], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([whether build target is a native Windows one], [ac_cv_native_windows], [
    if test "$ac_cv_header_windows_h" = "no"; then
      ac_cv_native_windows="no"
    else
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
        ]],[[
#if defined(__MINGW32__) || defined(__MINGW32CE__) || \
   (defined(_MSC_VER) && (defined(_WIN32) || defined(_WIN64)))
          int dummy=1;
#else
          Not a native Windows build target.
#endif
        ]])
      ],[
        ac_cv_native_windows="yes"
      ],[
        ac_cv_native_windows="no"
      ])
    fi
  ])
  AM_CONDITIONAL(DOING_NATIVE_WINDOWS, test "x$ac_cv_native_windows" = xyes)
])


dnl CURL_CHECK_HEADER_WINSOCK
dnl -------------------------------------------------
dnl Check for compilable and valid winsock.h header

AC_DEFUN([CURL_CHECK_HEADER_WINSOCK], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([for winsock.h], [ac_cv_header_winsock_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__)
        HAVE_WINSOCK_H shall not be defined.
#else
        int dummy=WSACleanup();
#endif
      ]])
    ],[
      ac_cv_header_winsock_h="yes"
    ],[
      ac_cv_header_winsock_h="no"
    ])
  ])
  case "$ac_cv_header_winsock_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINSOCK_H, 1,
        [Define to 1 if you have the winsock.h header file.])
      ;;
  esac
])


dnl CURL_CHECK_HEADER_WINSOCK2
dnl -------------------------------------------------
dnl Check for compilable and valid winsock2.h header

AC_DEFUN([CURL_CHECK_HEADER_WINSOCK2], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINDOWS])dnl
  AC_CACHE_CHECK([for winsock2.h], [ac_cv_header_winsock2_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__) || defined(__MINGW32CE__)
        HAVE_WINSOCK2_H shall not be defined.
#else
        int dummy=2*IPPROTO_ESP;
#endif
      ]])
    ],[
      ac_cv_header_winsock2_h="yes"
    ],[
      ac_cv_header_winsock2_h="no"
    ])
  ])
  case "$ac_cv_header_winsock2_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WINSOCK2_H, 1,
        [Define to 1 if you have the winsock2.h header file.])
      ;;
  esac
])


dnl CURL_CHECK_HEADER_WS2TCPIP
dnl -------------------------------------------------
dnl Check for compilable and valid ws2tcpip.h header

AC_DEFUN([CURL_CHECK_HEADER_WS2TCPIP], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK2])dnl
  AC_CACHE_CHECK([for ws2tcpip.h], [ac_cv_header_ws2tcpip_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
      ]],[[
#if defined(__CYGWIN__) || defined(__CEGCC__) || defined(__MINGW32CE__)
        HAVE_WS2TCPIP_H shall not be defined.
#else
        int dummy=2*IP_PKTINFO;
#endif
      ]])
    ],[
      ac_cv_header_ws2tcpip_h="yes"
    ],[
      ac_cv_header_ws2tcpip_h="no"
    ])
  ])
  case "$ac_cv_header_ws2tcpip_h" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_WS2TCPIP_H, 1,
        [Define to 1 if you have the ws2tcpip.h header file.])
      ;;
  esac
])


dnl CURL_CHECK_HEADER_MALLOC
dnl -------------------------------------------------
dnl Check for compilable and valid malloc.h header,
dnl and check if it is needed even with stdlib.h

AC_DEFUN([CURL_CHECK_HEADER_MALLOC], [
  AC_CACHE_CHECK([for malloc.h], [ac_cv_header_malloc_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include <malloc.h>
      ]],[[
        void *p = malloc(10);
        void *q = calloc(10,10);
        free(p);
        free(q);
      ]])
    ],[
      ac_cv_header_malloc_h="yes"
    ],[
      ac_cv_header_malloc_h="no"
    ])
  ])
  if test "$ac_cv_header_malloc_h" = "yes"; then
    AC_DEFINE_UNQUOTED(HAVE_MALLOC_H, 1,
      [Define to 1 if you have the malloc.h header file.])
    #
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include <stdlib.h>
      ]],[[
        void *p = malloc(10);
        void *q = calloc(10,10);
        free(p);
        free(q);
      ]])
    ],[
      curl_cv_need_header_malloc_h="no"
    ],[
      curl_cv_need_header_malloc_h="yes"
    ])
    #
    case "$curl_cv_need_header_malloc_h" in
      yes)
        AC_DEFINE_UNQUOTED(NEED_MALLOC_H, 1,
          [Define to 1 if you need the malloc.h header file even with stdlib.h])
        ;;
    esac
  fi
])


dnl CURL_CHECK_HEADER_MEMORY
dnl -------------------------------------------------
dnl Check for compilable and valid memory.h header,
dnl and check if it is needed even with stdlib.h for
dnl memory related functions.

AC_DEFUN([CURL_CHECK_HEADER_MEMORY], [
  AC_CACHE_CHECK([for memory.h], [ac_cv_header_memory_h], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include <memory.h>
      ]],[[
        void *p = malloc(10);
        void *q = calloc(10,10);
        free(p);
        free(q);
      ]])
    ],[
      ac_cv_header_memory_h="yes"
    ],[
      ac_cv_header_memory_h="no"
    ])
  ])
  if test "$ac_cv_header_memory_h" = "yes"; then
    AC_DEFINE_UNQUOTED(HAVE_MEMORY_H, 1,
      [Define to 1 if you have the memory.h header file.])
    #
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include <stdlib.h>
      ]],[[
        void *p = malloc(10);
        void *q = calloc(10,10);
        free(p);
        free(q);
      ]])
    ],[
      curl_cv_need_header_memory_h="no"
    ],[
      curl_cv_need_header_memory_h="yes"
    ])
    #
    case "$curl_cv_need_header_memory_h" in
      yes)
        AC_DEFINE_UNQUOTED(NEED_MEMORY_H, 1,
          [Define to 1 if you need the memory.h header file even with stdlib.h])
        ;;
    esac
  fi
])


dnl CURL_CHECK_FUNC_GETNAMEINFO
dnl -------------------------------------------------
dnl Test if the getnameinfo function is available,
dnl and check the types of five of its arguments.
dnl If the function succeeds HAVE_GETNAMEINFO will be
dnl defined, defining the types of the arguments in
dnl GETNAMEINFO_TYPE_ARG1, GETNAMEINFO_TYPE_ARG2,
dnl GETNAMEINFO_TYPE_ARG46 and GETNAMEINFO_TYPE_ARG7,
dnl and also defining the type qualifier of first
dnl argument in GETNAMEINFO_QUAL_ARG1.

AC_DEFUN([CURL_CHECK_FUNC_GETNAMEINFO], [
  AC_REQUIRE([CURL_CHECK_HEADER_WS2TCPIP])dnl
  AC_CHECK_HEADERS(sys/types.h sys/socket.h netdb.h)
  #
  AC_MSG_CHECKING([for getnameinfo])
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getnameinfo])
  ],[
    AC_MSG_RESULT([yes])
    curl_cv_getnameinfo="yes"
  ],[
    AC_MSG_RESULT([no])
    curl_cv_getnameinfo="no"
  ])
  #
  if test "$curl_cv_getnameinfo" != "yes"; then
    AC_MSG_CHECKING([deeper for getnameinfo])
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([[
      ]],[[
        getnameinfo();
      ]])
    ],[
      AC_MSG_RESULT([yes])
      curl_cv_getnameinfo="yes"
    ],[
      AC_MSG_RESULT([but still no])
      curl_cv_getnameinfo="no"
    ])
  fi
  #
  if test "$curl_cv_getnameinfo" != "yes"; then
    AC_MSG_CHECKING([deeper and deeper for getnameinfo])
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([[
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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#endif
      ]],[[
        getnameinfo(0, 0, 0, 0, 0, 0, 0);
      ]])
    ],[
      AC_MSG_RESULT([yes])
      curl_cv_getnameinfo="yes"
    ],[
      AC_MSG_RESULT([but still no])
      curl_cv_getnameinfo="no"
    ])
  fi
  #
  if test "$curl_cv_getnameinfo" = "yes"; then
    AC_CACHE_CHECK([types of arguments for getnameinfo],
      [curl_cv_func_getnameinfo_args], [
      curl_cv_func_getnameinfo_args="unknown"
      for gni_arg1 in 'struct sockaddr *' 'const struct sockaddr *' 'void *'; do
        for gni_arg2 in 'socklen_t' 'size_t' 'int'; do
          for gni_arg46 in 'size_t' 'int' 'socklen_t' 'unsigned int' 'DWORD'; do
            for gni_arg7 in 'int' 'unsigned int'; do
              if test "$curl_cv_func_getnameinfo_args" = "unknown"; then
                AC_COMPILE_IFELSE([
                  AC_LANG_PROGRAM([[
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#if (!defined(_WIN32_WINNT)) || (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#define GNICALLCONV WSAAPI
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#define GNICALLCONV
#endif
                    extern int GNICALLCONV getnameinfo($gni_arg1, $gni_arg2,
                                           char *, $gni_arg46,
                                           char *, $gni_arg46,
                                           $gni_arg7);
                  ]],[[
                    $gni_arg2 salen=0;
                    $gni_arg46 hostlen=0;
                    $gni_arg46 servlen=0;
                    $gni_arg7 flags=0;
                    int res = getnameinfo(0, salen, 0, hostlen, 0, servlen, flags);
                  ]])
                ],[
                  curl_cv_func_getnameinfo_args="$gni_arg1,$gni_arg2,$gni_arg46,$gni_arg7"
                ])
              fi
            done
          done
        done
      done
    ]) # AC-CACHE-CHECK
    if test "$curl_cv_func_getnameinfo_args" = "unknown"; then
      AC_MSG_WARN([Cannot find proper types to use for getnameinfo args])
      AC_MSG_WARN([HAVE_GETNAMEINFO will not be defined])
    else
      gni_prev_IFS=$IFS; IFS=','
      set dummy `echo "$curl_cv_func_getnameinfo_args" | sed 's/\*/\*/g'`
      IFS=$gni_prev_IFS
      shift
      #
      gni_qual_type_arg1=$[1]
      #
      AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG2, $[2],
        [Define to the type of arg 2 for getnameinfo.])
      AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG46, $[3],
        [Define to the type of args 4 and 6 for getnameinfo.])
      AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG7, $[4],
        [Define to the type of arg 7 for getnameinfo.])
      #
      prev_sh_opts=$-
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set -f
          ;;
      esac
      #
      case "$gni_qual_type_arg1" in
        const*)
          gni_qual_arg1=const
          gni_type_arg1=`echo $gni_qual_type_arg1 | sed 's/^const //'`
        ;;
        *)
          gni_qual_arg1=
          gni_type_arg1=$gni_qual_type_arg1
        ;;
      esac
      #
      AC_DEFINE_UNQUOTED(GETNAMEINFO_QUAL_ARG1, $gni_qual_arg1,
        [Define to the type qualifier of arg 1 for getnameinfo.])
      AC_DEFINE_UNQUOTED(GETNAMEINFO_TYPE_ARG1, $gni_type_arg1,
        [Define to the type of arg 1 for getnameinfo.])
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set +f
          ;;
      esac
      #
      AC_DEFINE_UNQUOTED(HAVE_GETNAMEINFO, 1,
        [Define to 1 if you have the getnameinfo function.])
      ac_cv_func_getnameinfo="yes"
    fi
  fi
])


dnl TYPE_SOCKADDR_STORAGE
dnl -------------------------------------------------
dnl Check for struct sockaddr_storage. Most IPv6-enabled
dnl hosts have it, but AIX 4.3 is one known exception.

AC_DEFUN([TYPE_SOCKADDR_STORAGE],
[
   AC_CHECK_TYPE([struct sockaddr_storage],
        AC_DEFINE(HAVE_STRUCT_SOCKADDR_STORAGE, 1,
                  [if struct sockaddr_storage is defined]), ,
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
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif
   ])
])


dnl CURL_CHECK_NI_WITHSCOPEID
dnl -------------------------------------------------
dnl Check for working NI_WITHSCOPEID in getnameinfo()

AC_DEFUN([CURL_CHECK_NI_WITHSCOPEID], [
  AC_REQUIRE([CURL_CHECK_FUNC_GETNAMEINFO])dnl
  AC_REQUIRE([TYPE_SOCKADDR_STORAGE])dnl
  AC_CHECK_HEADERS(stdio.h sys/types.h sys/socket.h \
                   netdb.h netinet/in.h arpa/inet.h)
  #
  AC_CACHE_CHECK([for working NI_WITHSCOPEID],
    [ac_cv_working_ni_withscopeid], [
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
      ]],[[
#if defined(NI_WITHSCOPEID) && defined(HAVE_GETNAMEINFO)
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE
        struct sockaddr_storage sa;
#else
        unsigned char sa[256];
#endif
        char hostbuf[NI_MAXHOST];
        int rc;
        GETNAMEINFO_TYPE_ARG2 salen = (GETNAMEINFO_TYPE_ARG2)sizeof(sa);
        GETNAMEINFO_TYPE_ARG46 hostlen = (GETNAMEINFO_TYPE_ARG46)sizeof(hostbuf);
        GETNAMEINFO_TYPE_ARG7 flags = NI_NUMERICHOST | NI_NUMERICSERV | NI_WITHSCOPEID;
        int fd = socket(AF_INET6, SOCK_STREAM, 0);
        if(fd < 0) {
          perror("socket()");
          return 1; /* Error creating socket */
        }
        rc = getsockname(fd, (GETNAMEINFO_TYPE_ARG1)&sa, &salen);
        if(rc) {
          perror("getsockname()");
          return 2; /* Error retrieving socket name */
        }
        rc = getnameinfo((GETNAMEINFO_TYPE_ARG1)&sa, salen, hostbuf, hostlen, NULL, 0, flags);
        if(rc) {
          printf("rc = %s\n", gai_strerror(rc));
          return 3; /* Error translating socket address */
        }
        return 0; /* Ok, NI_WITHSCOPEID works */
#else
        return 4; /* Error, NI_WITHSCOPEID not defined or no getnameinfo() */
#endif
      ]]) # AC-LANG-PROGRAM
    ],[
      # Exit code == 0. Program worked.
      ac_cv_working_ni_withscopeid="yes"
    ],[
      # Exit code != 0. Program failed.
      ac_cv_working_ni_withscopeid="no"
    ],[
      # Program is not run when cross-compiling. So we assume
      # NI_WITHSCOPEID will work if we are able to compile it.
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
        ]],[[
          unsigned int dummy= NI_NUMERICHOST | NI_NUMERICSERV | NI_WITHSCOPEID;
        ]])
      ],[
        ac_cv_working_ni_withscopeid="yes"
      ],[
        ac_cv_working_ni_withscopeid="no"
      ]) # AC-COMPILE-IFELSE
    ]) # AC-RUN-IFELSE
  ]) # AC-CACHE-CHECK
  case "$ac_cv_working_ni_withscopeid" in
    yes)
      AC_DEFINE(HAVE_NI_WITHSCOPEID, 1,
        [Define to 1 if NI_WITHSCOPEID exists and works.])
      ;;
  esac
])


dnl CURL_CHECK_FUNC_RECV
dnl -------------------------------------------------
dnl Test if the socket recv() function is available,
dnl and check its return type and the types of its
dnl arguments. If the function succeeds HAVE_RECV
dnl will be defined, defining the types of the arguments
dnl in RECV_TYPE_ARG1, RECV_TYPE_ARG2, RECV_TYPE_ARG3
dnl and RECV_TYPE_ARG4, defining the type of the function
dnl return value in RECV_TYPE_RETV.

AC_DEFUN([CURL_CHECK_FUNC_RECV], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK])dnl
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK2])dnl
  AC_CHECK_HEADERS(sys/types.h sys/socket.h)
  #
  AC_MSG_CHECKING([for recv])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
#undef inline
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
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
    ]],[[
      recv(0, 0, 0, 0);
    ]])
  ],[
    AC_MSG_RESULT([yes])
    curl_cv_recv="yes"
  ],[
    AC_MSG_RESULT([no])
    curl_cv_recv="no"
  ])
  #
  if test "$curl_cv_recv" = "yes"; then
    AC_CACHE_CHECK([types of args and return type for recv],
      [curl_cv_func_recv_args], [
      curl_cv_func_recv_args="unknown"
      for recv_retv in 'int' 'ssize_t'; do
        for recv_arg1 in 'int' 'ssize_t' 'SOCKET'; do
          for recv_arg2 in 'char *' 'void *'; do
            for recv_arg3 in 'size_t' 'int' 'socklen_t' 'unsigned int'; do
              for recv_arg4 in 'int' 'unsigned int'; do
                if test "$curl_cv_func_recv_args" = "unknown"; then
                  AC_COMPILE_IFELSE([
                    AC_LANG_PROGRAM([[
#undef inline
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
#define RECVCALLCONV PASCAL
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#define RECVCALLCONV
#endif
                      extern $recv_retv RECVCALLCONV
                      recv($recv_arg1, $recv_arg2, $recv_arg3, $recv_arg4);
                    ]],[[
                      $recv_arg1 s=0;
                      $recv_arg2 buf=0;
                      $recv_arg3 len=0;
                      $recv_arg4 flags=0;
                      $recv_retv res = recv(s, buf, len, flags);
                    ]])
                  ],[
                    curl_cv_func_recv_args="$recv_arg1,$recv_arg2,$recv_arg3,$recv_arg4,$recv_retv"
                  ])
                fi
              done
            done
          done
        done
      done
    ]) # AC-CACHE-CHECK
    if test "$curl_cv_func_recv_args" = "unknown"; then
      AC_MSG_ERROR([Cannot find proper types to use for recv args])
    else
      recv_prev_IFS=$IFS; IFS=','
      set dummy `echo "$curl_cv_func_recv_args" | sed 's/\*/\*/g'`
      IFS=$recv_prev_IFS
      shift
      #
      AC_DEFINE_UNQUOTED(RECV_TYPE_ARG1, $[1],
        [Define to the type of arg 1 for recv.])
      AC_DEFINE_UNQUOTED(RECV_TYPE_ARG2, $[2],
        [Define to the type of arg 2 for recv.])
      AC_DEFINE_UNQUOTED(RECV_TYPE_ARG3, $[3],
        [Define to the type of arg 3 for recv.])
      AC_DEFINE_UNQUOTED(RECV_TYPE_ARG4, $[4],
        [Define to the type of arg 4 for recv.])
      AC_DEFINE_UNQUOTED(RECV_TYPE_RETV, $[5],
        [Define to the function return type for recv.])
      #
      AC_DEFINE_UNQUOTED(HAVE_RECV, 1,
        [Define to 1 if you have the recv function.])
      ac_cv_func_recv="yes"
    fi
  else
    AC_MSG_ERROR([Unable to link function recv])
  fi
])


dnl CURL_CHECK_FUNC_SEND
dnl -------------------------------------------------
dnl Test if the socket send() function is available,
dnl and check its return type and the types of its
dnl arguments. If the function succeeds HAVE_SEND
dnl will be defined, defining the types of the arguments
dnl in SEND_TYPE_ARG1, SEND_TYPE_ARG2, SEND_TYPE_ARG3
dnl and SEND_TYPE_ARG4, defining the type of the function
dnl return value in SEND_TYPE_RETV, and also defining the
dnl type qualifier of second argument in SEND_QUAL_ARG2.

AC_DEFUN([CURL_CHECK_FUNC_SEND], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK])dnl
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK2])dnl
  AC_CHECK_HEADERS(sys/types.h sys/socket.h)
  #
  AC_MSG_CHECKING([for send])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
#undef inline
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
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
    ]],[[
      send(0, 0, 0, 0);
    ]])
  ],[
    AC_MSG_RESULT([yes])
    curl_cv_send="yes"
  ],[
    AC_MSG_RESULT([no])
    curl_cv_send="no"
  ])
  #
  if test "$curl_cv_send" = "yes"; then
    AC_CACHE_CHECK([types of args and return type for send],
      [curl_cv_func_send_args], [
      curl_cv_func_send_args="unknown"
      for send_retv in 'int' 'ssize_t'; do
        for send_arg1 in 'int' 'ssize_t' 'SOCKET'; do
          for send_arg2 in 'char *' 'void *' 'const char *' 'const void *'; do
            for send_arg3 in 'size_t' 'int' 'socklen_t' 'unsigned int'; do
              for send_arg4 in 'int' 'unsigned int'; do
                if test "$curl_cv_func_send_args" = "unknown"; then
                  AC_COMPILE_IFELSE([
                    AC_LANG_PROGRAM([[
#undef inline
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
#define SENDCALLCONV PASCAL
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#define SENDCALLCONV
#endif
                      extern $send_retv SENDCALLCONV
                      send($send_arg1, $send_arg2, $send_arg3, $send_arg4);
                    ]],[[
                      $send_arg1 s=0;
                      $send_arg3 len=0;
                      $send_arg4 flags=0;
                      $send_retv res = send(s, 0, len, flags);
                    ]])
                  ],[
                    curl_cv_func_send_args="$send_arg1,$send_arg2,$send_arg3,$send_arg4,$send_retv"
                  ])
                fi
              done
            done
          done
        done
      done
    ]) # AC-CACHE-CHECK
    if test "$curl_cv_func_send_args" = "unknown"; then
      AC_MSG_ERROR([Cannot find proper types to use for send args])
    else
      send_prev_IFS=$IFS; IFS=','
      set dummy `echo "$curl_cv_func_send_args" | sed 's/\*/\*/g'`
      IFS=$send_prev_IFS
      shift
      #
      send_qual_type_arg2=$[2]
      #
      AC_DEFINE_UNQUOTED(SEND_TYPE_ARG1, $[1],
        [Define to the type of arg 1 for send.])
      AC_DEFINE_UNQUOTED(SEND_TYPE_ARG3, $[3],
        [Define to the type of arg 3 for send.])
      AC_DEFINE_UNQUOTED(SEND_TYPE_ARG4, $[4],
        [Define to the type of arg 4 for send.])
      AC_DEFINE_UNQUOTED(SEND_TYPE_RETV, $[5],
        [Define to the function return type for send.])
      #
      prev_sh_opts=$-
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set -f
          ;;
      esac
      #
      case "$send_qual_type_arg2" in
        const*)
          send_qual_arg2=const
          send_type_arg2=`echo $send_qual_type_arg2 | sed 's/^const //'`
        ;;
        *)
          send_qual_arg2=
          send_type_arg2=$send_qual_type_arg2
        ;;
      esac
      #
      AC_DEFINE_UNQUOTED(SEND_QUAL_ARG2, $send_qual_arg2,
        [Define to the type qualifier of arg 2 for send.])
      AC_DEFINE_UNQUOTED(SEND_TYPE_ARG2, $send_type_arg2,
        [Define to the type of arg 2 for send.])
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set +f
          ;;
      esac
      #
      AC_DEFINE_UNQUOTED(HAVE_SEND, 1,
        [Define to 1 if you have the send function.])
      ac_cv_func_send="yes"
    fi
  else
    AC_MSG_ERROR([Unable to link function send])
  fi
])


dnl CURL_CHECK_FUNC_RECVFROM
dnl -------------------------------------------------
dnl Test if the socket recvfrom() function is available,
dnl and check its return type and the types of its
dnl arguments. If the function succeeds HAVE_RECVFROM
dnl will be defined, defining the types of the arguments
dnl in RECVFROM_TYPE_ARG1, RECVFROM_TYPE_ARG2, and so on
dnl to RECVFROM_TYPE_ARG6, defining also the type of the
dnl function return value in RECVFROM_TYPE_RETV.
dnl Notice that the types returned for pointer arguments
dnl will actually be the type pointed by the pointer.

AC_DEFUN([CURL_CHECK_FUNC_RECVFROM], [
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK])dnl
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK2])dnl
  AC_CHECK_HEADERS(sys/types.h sys/socket.h)
  #
  AC_MSG_CHECKING([for recvfrom])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
#undef inline
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
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
    ]],[[
      recvfrom(0, 0, 0, 0, 0, 0);
    ]])
  ],[
    AC_MSG_RESULT([yes])
    curl_cv_recvfrom="yes"
  ],[
    AC_MSG_RESULT([no])
    curl_cv_recvfrom="no"
  ])
  #
  if test "$curl_cv_recvfrom" = "yes"; then
    AC_CACHE_CHECK([types of args and return type for recvfrom],
      [curl_cv_func_recvfrom_args], [
      curl_cv_func_recvfrom_args="unknown"
      for recvfrom_retv in 'int' 'ssize_t'; do
        for recvfrom_arg1 in 'int' 'ssize_t' 'SOCKET'; do
          for recvfrom_arg2 in 'char *' 'void *'; do
            for recvfrom_arg3 in 'size_t' 'int' 'socklen_t' 'unsigned int'; do
              for recvfrom_arg4 in 'int' 'unsigned int'; do
                for recvfrom_arg5 in 'struct sockaddr *' 'void *' 'const struct sockaddr *'; do
                  for recvfrom_arg6 in 'socklen_t *' 'int *' 'unsigned int *' 'size_t *' 'void *'; do
                    if test "$curl_cv_func_recvfrom_args" = "unknown"; then
                      AC_COMPILE_IFELSE([
                        AC_LANG_PROGRAM([[
#undef inline
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
#define RECVFROMCALLCONV PASCAL
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#define RECVFROMCALLCONV
#endif
                          extern $recvfrom_retv RECVFROMCALLCONV
                          recvfrom($recvfrom_arg1, $recvfrom_arg2,
                                   $recvfrom_arg3, $recvfrom_arg4,
                                   $recvfrom_arg5, $recvfrom_arg6);
                        ]],[[
                          $recvfrom_arg1 s=0;
                          $recvfrom_arg2 buf=0;
                          $recvfrom_arg3 len=0;
                          $recvfrom_arg4 flags=0;
                          $recvfrom_arg5 addr=0;
                          $recvfrom_arg6 addrlen=0;
                          $recvfrom_retv res=0;
                          res = recvfrom(s, buf, len, flags, addr, addrlen);
                        ]])
                      ],[
                        curl_cv_func_recvfrom_args="$recvfrom_arg1,$recvfrom_arg2,$recvfrom_arg3,$recvfrom_arg4,$recvfrom_arg5,$recvfrom_arg6,$recvfrom_retv"
                      ])
                    fi
                  done
                done
              done
            done
          done
        done
      done
    ]) # AC-CACHE-CHECK
    # Nearly last minute change for this release starts here
    AC_DEFINE_UNQUOTED(HAVE_RECVFROM, 1,
      [Define to 1 if you have the recvfrom function.])
    ac_cv_func_recvfrom="yes"
    # Nearly last minute change for this release ends here
    if test "$curl_cv_func_recvfrom_args" = "unknown"; then
      AC_MSG_WARN([Cannot find proper types to use for recvfrom args])
    else
      recvfrom_prev_IFS=$IFS; IFS=','
      set dummy `echo "$curl_cv_func_recvfrom_args" | sed 's/\*/\*/g'`
      IFS=$recvfrom_prev_IFS
      shift
      #
      recvfrom_ptrt_arg2=$[2]
      recvfrom_qual_ptrt_arg5=$[5]
      recvfrom_ptrt_arg6=$[6]
      #
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG1, $[1],
        [Define to the type of arg 1 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG3, $[3],
        [Define to the type of arg 3 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG4, $[4],
        [Define to the type of arg 4 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_RETV, $[7],
        [Define to the function return type for recvfrom.])
      #
      prev_sh_opts=$-
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set -f
          ;;
      esac
      #
      case "$recvfrom_qual_ptrt_arg5" in
        const*)
          recvfrom_qual_arg5=const
          recvfrom_ptrt_arg5=`echo $recvfrom_qual_ptrt_arg5 | sed 's/^const //'`
        ;;
        *)
          recvfrom_qual_arg5=
          recvfrom_ptrt_arg5=$recvfrom_qual_ptrt_arg5
        ;;
      esac
      #
      recvfrom_type_arg2=`echo $recvfrom_ptrt_arg2 | sed 's/ \*//'`
      recvfrom_type_arg5=`echo $recvfrom_ptrt_arg5 | sed 's/ \*//'`
      recvfrom_type_arg6=`echo $recvfrom_ptrt_arg6 | sed 's/ \*//'`
      #
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG2, $recvfrom_type_arg2,
        [Define to the type pointed by arg 2 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_QUAL_ARG5, $recvfrom_qual_arg5,
        [Define to the type qualifier pointed by arg 5 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG5, $recvfrom_type_arg5,
        [Define to the type pointed by arg 5 for recvfrom.])
      AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG6, $recvfrom_type_arg6,
        [Define to the type pointed by arg 6 for recvfrom.])
      #
      if test "$recvfrom_type_arg2" = "void"; then
        AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG2_IS_VOID, 1,
          [Define to 1 if the type pointed by arg 2 for recvfrom is void.])
      fi
      if test "$recvfrom_type_arg5" = "void"; then
        AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG5_IS_VOID, 1,
          [Define to 1 if the type pointed by arg 5 for recvfrom is void.])
      fi
      if test "$recvfrom_type_arg6" = "void"; then
        AC_DEFINE_UNQUOTED(RECVFROM_TYPE_ARG6_IS_VOID, 1,
          [Define to 1 if the type pointed by arg 6 for recvfrom is void.])
      fi
      #
      case $prev_sh_opts in
        *f*)
          ;;
        *)
          set +f
          ;;
      esac
      #
      AC_DEFINE_UNQUOTED(HAVE_RECVFROM, 1,
        [Define to 1 if you have the recvfrom function.])
      ac_cv_func_recvfrom="yes"
    fi
  else
    AC_MSG_WARN([Unable to link function recvfrom])
    AC_MSG_WARN([Your system will be vulnerable to some forms of DNS cache poisoning])
  fi
])


dnl CURL_CHECK_MSG_NOSIGNAL
dnl -------------------------------------------------
dnl Check for MSG_NOSIGNAL

AC_DEFUN([CURL_CHECK_MSG_NOSIGNAL], [
  AC_CHECK_HEADERS(sys/types.h sys/socket.h)
  AC_CACHE_CHECK([for MSG_NOSIGNAL], [ac_cv_msg_nosignal], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
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
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
      ]],[[
        int flag=MSG_NOSIGNAL;
      ]])
    ],[
      ac_cv_msg_nosignal="yes"
    ],[
      ac_cv_msg_nosignal="no"
    ])
  ])
  case "$ac_cv_msg_nosignal" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_MSG_NOSIGNAL, 1,
        [Define to 1 if you have the MSG_NOSIGNAL flag.])
      ;;
  esac
])


dnl CURL_CHECK_STRUCT_TIMEVAL
dnl -------------------------------------------------
dnl Check for timeval struct

AC_DEFUN([CURL_CHECK_STRUCT_TIMEVAL], [
  AC_REQUIRE([AC_HEADER_TIME])dnl
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK])dnl
  AC_REQUIRE([CURL_CHECK_HEADER_WINSOCK2])dnl
  AC_CHECK_HEADERS(sys/types.h sys/time.h time.h sys/socket.h)
  AC_CACHE_CHECK([for struct timeval], [ac_cv_struct_timeval], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#undef inline
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
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
      ]],[[
        struct timeval ts;
        ts.tv_sec  = 0;
        ts.tv_usec = 0;
      ]])
    ],[
      ac_cv_struct_timeval="yes"
    ],[
      ac_cv_struct_timeval="no"
    ])
  ])
  case "$ac_cv_struct_timeval" in
    yes)
      AC_DEFINE_UNQUOTED(HAVE_STRUCT_TIMEVAL, 1,
        [Define to 1 if you have the timeval struct.])
      ;;
  esac
])


dnl TYPE_SIG_ATOMIC_T
dnl -------------------------------------------------
dnl Check if the sig_atomic_t type is available, and
dnl verify if it is already defined as volatile.

AC_DEFUN([TYPE_SIG_ATOMIC_T], [
  AC_CHECK_HEADERS(signal.h)
  AC_CHECK_TYPE([sig_atomic_t],[
    AC_DEFINE(HAVE_SIG_ATOMIC_T, 1,
      [Define to 1 if sig_atomic_t is an available typedef.])
  ], ,[
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
  ])
  case "$ac_cv_type_sig_atomic_t" in
    yes)
      #
      AC_MSG_CHECKING([if sig_atomic_t is already defined as volatile])
      AC_LINK_IFELSE([
        AC_LANG_PROGRAM([[
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
        ]],[[
          static volatile sig_atomic_t dummy = 0;
        ]])
      ],[
        AC_MSG_RESULT([no])
        ac_cv_sig_atomic_t_volatile="no"
      ],[
        AC_MSG_RESULT([yes])
        ac_cv_sig_atomic_t_volatile="yes"
      ])
      #
      if test "$ac_cv_sig_atomic_t_volatile" = "yes"; then
        AC_DEFINE(HAVE_SIG_ATOMIC_T_VOLATILE, 1,
          [Define to 1 if sig_atomic_t is already defined as volatile.])
      fi
      ;;
  esac
])


dnl TYPE_IN_ADDR_T
dnl -------------------------------------------------
dnl Check for in_addr_t: it is used to receive the return code of inet_addr()
dnl and a few other things.

AC_DEFUN([TYPE_IN_ADDR_T], [
  AC_CHECK_TYPE([in_addr_t], ,[
    dnl in_addr_t not available
    AC_CACHE_CHECK([for in_addr_t equivalent],
      [curl_cv_in_addr_t_equiv], [
      curl_cv_in_addr_t_equiv="unknown"
      for t in "unsigned long" int size_t unsigned long; do
        if test "$curl_cv_in_addr_t_equiv" = "unknown"; then
          AC_LINK_IFELSE([
            AC_LANG_PROGRAM([[
#undef inline
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
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif
            ]],[[
              $t data = inet_addr ("1.2.3.4");
            ]])
          ],[
            curl_cv_in_addr_t_equiv="$t"
          ])
        fi
      done
    ])
    case "$curl_cv_in_addr_t_equiv" in
      unknown)
        AC_MSG_ERROR([Cannot find a type to use in place of in_addr_t])
        ;;
      *)
        AC_DEFINE_UNQUOTED(in_addr_t, $curl_cv_in_addr_t_equiv,
          [Type to use in place of in_addr_t when system does not provide it.])
        ;;
    esac
  ],[
#undef inline
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
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif
  ])
])


dnl CURL_CHECK_FUNC_CLOCK_GETTIME_MONOTONIC
dnl -------------------------------------------------
dnl Check if monotonic clock_gettime is available.

AC_DEFUN([CURL_CHECK_FUNC_CLOCK_GETTIME_MONOTONIC], [
  AC_REQUIRE([AC_HEADER_TIME])dnl
  AC_CHECK_HEADERS(sys/types.h sys/time.h time.h)
  AC_MSG_CHECKING([for monotonic clock_gettime])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif
    ]],[[
      struct timespec ts;
      (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    ]])
  ],[
    AC_MSG_RESULT([yes])
    ac_cv_func_clock_gettime="yes"
  ],[
    AC_MSG_RESULT([no])
    ac_cv_func_clock_gettime="no"
  ])
  dnl Definition of HAVE_CLOCK_GETTIME_MONOTONIC is intentionally postponed
  dnl until library linking and run-time checks for clock_gettime succeed.
])


dnl CURL_CHECK_LIBS_CLOCK_GETTIME_MONOTONIC
dnl -------------------------------------------------
dnl If monotonic clock_gettime is available then,
dnl check and prepended to LIBS any needed libraries.

AC_DEFUN([CURL_CHECK_LIBS_CLOCK_GETTIME_MONOTONIC], [
  AC_REQUIRE([CURL_CHECK_FUNC_CLOCK_GETTIME_MONOTONIC])dnl
  #
  if test "$ac_cv_func_clock_gettime" = "yes"; then
    #
    AC_MSG_CHECKING([for clock_gettime in libraries])
    #
    curl_cv_save_LIBS="$LIBS"
    curl_cv_gclk_LIBS="unknown"
    #
    for x_xlibs in '' '-lrt' '-lposix4' ; do
      if test "$curl_cv_gclk_LIBS" = "unknown"; then
        if test -z "$x_xlibs"; then
          LIBS="$curl_cv_save_LIBS"
        else
          LIBS="$x_xlibs $curl_cv_save_LIBS"
        fi
        AC_LINK_IFELSE([
          AC_LANG_PROGRAM([[
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif
          ]],[[
            struct timespec ts;
            (void)clock_gettime(CLOCK_MONOTONIC, &ts);
          ]])
        ],[
          curl_cv_gclk_LIBS="$x_xlibs"
        ])
      fi
    done
    #
    LIBS="$curl_cv_save_LIBS"
    #
    case X-"$curl_cv_gclk_LIBS" in
      X-unknown)
        AC_MSG_RESULT([cannot find clock_gettime])
        AC_MSG_WARN([HAVE_CLOCK_GETTIME_MONOTONIC will not be defined])
        ac_cv_func_clock_gettime="no"
        ;;
      X-)
        AC_MSG_RESULT([no additional lib required])
        ac_cv_func_clock_gettime="yes"
        ;;
      *)
        if test -z "$curl_cv_save_LIBS"; then
          LIBS="$curl_cv_gclk_LIBS"
        else
          LIBS="$curl_cv_gclk_LIBS $curl_cv_save_LIBS"
        fi
        AC_MSG_RESULT([$curl_cv_gclk_LIBS])
        ac_cv_func_clock_gettime="yes"
        ;;
    esac
    #
    dnl only do runtime verification when not cross-compiling
    if test "x$cross_compiling" != "xyes" &&
      test "$ac_cv_func_clock_gettime" = "yes"; then
      AC_MSG_CHECKING([if monotonic clock_gettime works])
      AC_RUN_IFELSE([
        AC_LANG_PROGRAM([[
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#endif
        ]],[[
          struct timespec ts;
          if (0 == clock_gettime(CLOCK_MONOTONIC, &ts))
            exit(0);
          else
            exit(1);
        ]])
      ],[
        AC_MSG_RESULT([yes])
      ],[
        AC_MSG_RESULT([no])
        AC_MSG_WARN([HAVE_CLOCK_GETTIME_MONOTONIC will not be defined])
        ac_cv_func_clock_gettime="no"
        LIBS="$curl_cv_save_LIBS"
      ])
    fi
    #
    case "$ac_cv_func_clock_gettime" in
      yes)
        AC_DEFINE_UNQUOTED(HAVE_CLOCK_GETTIME_MONOTONIC, 1,
          [Define to 1 if you have the clock_gettime function and monotonic timer.])
        ;;
    esac
    #
  fi
  #
])


dnl CARES_CHECK_LIBS_CONNECT
dnl -------------------------------------------------
dnl Verify if network connect function is already available
dnl using current libraries or if another one is required.

AC_DEFUN([CARES_CHECK_LIBS_CONNECT], [
  AC_REQUIRE([CARES_INCLUDES_WINSOCK2])dnl
  AC_MSG_CHECKING([for connect in libraries])
  tst_connect_save_LIBS="$LIBS"
  tst_connect_need_LIBS="unknown"
  for tst_lib in '' '-lsocket' ; do
    if test "$tst_connect_need_LIBS" = "unknown"; then
      LIBS="$tst_lib $tst_connect_save_LIBS"
      AC_LINK_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_winsock2
          #ifndef HAVE_WINDOWS_H
            int connect(int, void*, int);
          #endif
        ]],[[
          if(0 != connect(0, 0, 0))
            return 1;
        ]])
      ],[
        tst_connect_need_LIBS="$tst_lib"
      ])
    fi
  done
  LIBS="$tst_connect_save_LIBS"
  #
  case X-"$tst_connect_need_LIBS" in
    X-unknown)
      AC_MSG_RESULT([cannot find connect])
      AC_MSG_ERROR([cannot find connect function in libraries.])
      ;;
    X-)
      AC_MSG_RESULT([yes])
      ;;
    *)
      AC_MSG_RESULT([$tst_connect_need_LIBS])
      LIBS="$tst_connect_need_LIBS $tst_connect_save_LIBS"
      ;;
  esac
])


dnl CARES_DEFINE_UNQUOTED (VARIABLE, [VALUE])
dnl -------------------------------------------------
dnl Like AC_DEFINE_UNQUOTED this macro will define a C preprocessor
dnl symbol that can be further used in custom template configuration
dnl files. This macro, unlike AC_DEFINE_UNQUOTED, does not use a third
dnl argument for the description. Symbol definitions done with this
dnl macro are intended to be exclusively used in handcrafted *.h.in
dnl template files. Contrary to what AC_DEFINE_UNQUOTED does, this one
dnl prevents autoheader generation and insertion of symbol template
dnl stub and definition into the first configuration header file. Do
dnl not use this macro as a replacement for AC_DEFINE_UNQUOTED, each
dnl one serves different functional needs.

AC_DEFUN([CARES_DEFINE_UNQUOTED], [
cat >>confdefs.h <<_EOF
[@%:@define] $1 ifelse($#, 2, [$2], 1)
_EOF
])

dnl CARES_CONFIGURE_ARES_SOCKLEN_T
dnl -------------------------------------------------
dnl Find out suitable ares_socklen_t data type definition and size, making
dnl appropriate definitions for template file ares_build.h.in
dnl to properly configure and use the library.
dnl
dnl The need for the ares_socklen_t definition arises mainly to properly
dnl interface HP-UX systems which on one hand have a typedef'ed socklen_t
dnl data type which is 32 or 64-Bit wide depending on the data model being
dnl used, and that on the other hand is only actually used when interfacing
dnl the X/Open sockets provided in the xnet library.

AC_DEFUN([CARES_CONFIGURE_ARES_SOCKLEN_T], [
  AC_REQUIRE([CARES_INCLUDES_WS2TCPIP])dnl
  AC_REQUIRE([CARES_INCLUDES_SYS_SOCKET])dnl
  AC_REQUIRE([CARES_PREPROCESS_CALLCONV])dnl
  #
  AC_MSG_CHECKING([for ares_socklen_t data type])
  cares_typeof_ares_socklen_t="unknown"
  for arg1 in int SOCKET; do
    for arg2 in 'struct sockaddr' void; do
      for t in socklen_t int size_t 'unsigned int' long 'unsigned long' void; do
        if test "$cares_typeof_ares_socklen_t" = "unknown"; then
          AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
              $cares_includes_ws2tcpip
              $cares_includes_sys_socket
              $cares_preprocess_callconv
              extern int FUNCALLCONV getpeername($arg1, $arg2 *, $t *);
            ]],[[
              $t *lenptr = 0;
              if(0 != getpeername(0, 0, lenptr))
                return 1;
            ]])
          ],[
            cares_typeof_ares_socklen_t="$t"
          ])
        fi
      done
    done
  done
  for t in socklen_t int; do
    if test "$cares_typeof_ares_socklen_t" = "void"; then
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $cares_includes_sys_socket
          typedef $t ares_socklen_t;
        ]],[[
          ares_socklen_t dummy;
        ]])
      ],[
        cares_typeof_ares_socklen_t="$t"
      ])
    fi
  done
  AC_MSG_RESULT([$cares_typeof_ares_socklen_t])
  if test "$cares_typeof_ares_socklen_t" = "void" ||
    test "$cares_typeof_ares_socklen_t" = "unknown"; then
    AC_MSG_ERROR([cannot find data type for ares_socklen_t.])
  fi
  #
  AC_MSG_CHECKING([size of ares_socklen_t])
  cares_sizeof_ares_socklen_t="unknown"
  cares_pull_headers_socklen_t="unknown"
  if test "$ac_cv_header_ws2tcpip_h" = "yes"; then
    tst_pull_header_checks='none ws2tcpip'
    tst_size_checks='4'
  else
    tst_pull_header_checks='none systypes syssocket'
    tst_size_checks='4 8 2'
  fi
  for tst_size in $tst_size_checks; do
    for tst_pull_headers in $tst_pull_header_checks; do
      if test "$cares_sizeof_ares_socklen_t" = "unknown"; then
        case $tst_pull_headers in
          ws2tcpip)
            tmp_includes="$cares_includes_ws2tcpip"
            ;;
          systypes)
            tmp_includes="$cares_includes_sys_types"
            ;;
          syssocket)
            tmp_includes="$cares_includes_sys_socket"
            ;;
          *)
            tmp_includes=""
            ;;
        esac
        AC_COMPILE_IFELSE([
          AC_LANG_PROGRAM([[
            $tmp_includes
            typedef $cares_typeof_ares_socklen_t ares_socklen_t;
            typedef char dummy_arr[sizeof(ares_socklen_t) == $tst_size ? 1 : -1];
          ]],[[
            ares_socklen_t dummy;
          ]])
        ],[
          cares_sizeof_ares_socklen_t="$tst_size"
          cares_pull_headers_socklen_t="$tst_pull_headers"
        ])
      fi
    done
  done
  AC_MSG_RESULT([$cares_sizeof_ares_socklen_t])
  if test "$cares_sizeof_ares_socklen_t" = "unknown"; then
    AC_MSG_ERROR([cannot find out size of ares_socklen_t.])
  fi
  #
  case $cares_pull_headers_socklen_t in
    ws2tcpip)
      CARES_DEFINE_UNQUOTED([CARES_PULL_WS2TCPIP_H])
      ;;
    systypes)
      CARES_DEFINE_UNQUOTED([CARES_PULL_SYS_TYPES_H])
      ;;
    syssocket)
      CARES_DEFINE_UNQUOTED([CARES_PULL_SYS_TYPES_H])
      CARES_DEFINE_UNQUOTED([CARES_PULL_SYS_SOCKET_H])
      ;;
  esac
  CARES_DEFINE_UNQUOTED([CARES_TYPEOF_ARES_SOCKLEN_T], [$cares_typeof_ares_socklen_t])
  CARES_DEFINE_UNQUOTED([CARES_SIZEOF_ARES_SOCKLEN_T], [$cares_sizeof_ares_socklen_t])
])


dnl This macro determines if the specified struct exists in the specified file
dnl Syntax:
dnl CARES_CHECK_STRUCT(headers, struct name, if found, [if not found])

AC_DEFUN([CARES_CHECK_STRUCT], [
  AC_MSG_CHECKING([for struct $2])
  AC_TRY_COMPILE([$1],
    [
      struct $2 struct_instance;
    ], ac_struct="yes", ac_found="no")
  if test "$ac_struct" = "yes" ; then
    AC_MSG_RESULT(yes)
    $3
  else
    AC_MSG_RESULT(no)
    $4
  fi
])

dnl This macro determines if the specified constant exists in the specified file
dnl Syntax:
dnl CARES_CHECK_CONSTANT(headers, constant name, if found, [if not found])

AC_DEFUN([CARES_CHECK_CONSTANT], [
  AC_MSG_CHECKING([for $2])
  AC_EGREP_CPP(VARIABLEWASDEFINED,
   [
      $1

      #ifdef $2
        VARIABLEWASDEFINED
      #else
        NJET
      #endif
    ], ac_constant="yes", ac_constant="no"
  )
  if test "$ac_constant" = "yes" ; then
    AC_MSG_RESULT(yes)
    $3
  else
    AC_MSG_RESULT(no)
    $4
  fi
])


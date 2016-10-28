#ifndef HEADER_CARES_CONFIG_WIN32_H
#define HEADER_CARES_CONFIG_WIN32_H

/* Copyright (C) 2004 - 2011 by Daniel Stenberg et al
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

/* ================================================================ */
/*   c-ares/config-win32.h - Hand crafted config file for Windows   */
/* ================================================================ */

/* ---------------------------------------------------------------- */
/*                          HEADER FILES                            */
/* ---------------------------------------------------------------- */

/* Define if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define if you have the <getopt.h> header file. */
#if defined(__MINGW32__) || defined(__POCC__)
#define HAVE_GETOPT_H 1
#endif

/* Define if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define if you have the <process.h> header file. */
#ifndef __SALFORDC__
#define HAVE_PROCESS_H 1
#endif

/* Define if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define if you have the <sys/time.h> header file */
/* #define HAVE_SYS_TIME_H 1 */

/* Define if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if you have the <unistd.h> header file. */
#if defined(__MINGW32__) || defined(__WATCOMC__) || defined(__LCC__) || \
    defined(__POCC__)
#define HAVE_UNISTD_H 1
#endif

/* Define if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define if you have the <winsock.h> header file. */
#define HAVE_WINSOCK_H 1

/* Define if you have the <winsock2.h> header file. */
#ifndef __SALFORDC__
#define HAVE_WINSOCK2_H 1
#endif

/* Define if you have the <ws2tcpip.h> header file. */
#ifndef __SALFORDC__
#define HAVE_WS2TCPIP_H 1
#endif

/* ---------------------------------------------------------------- */
/*                        OTHER HEADER INFO                         */
/* ---------------------------------------------------------------- */

/* Define if sig_atomic_t is an available typedef. */
#define HAVE_SIG_ATOMIC_T 1

/* Define if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>. */
/* #define TIME_WITH_SYS_TIME 1 */

/* ---------------------------------------------------------------- */
/*                             FUNCTIONS                            */
/* ---------------------------------------------------------------- */

/* Define if you have the closesocket function. */
#define HAVE_CLOSESOCKET 1

/* Define if you have the getenv function. */
#define HAVE_GETENV 1

/* Define if you have the gethostname function. */
#define HAVE_GETHOSTNAME 1

/* Define if you have the ioctlsocket function. */
#define HAVE_IOCTLSOCKET 1

/* Define if you have a working ioctlsocket FIONBIO function. */
#define HAVE_IOCTLSOCKET_FIONBIO 1

/* Define if you have the strcasecmp function. */
/* #define HAVE_STRCASECMP 1 */

/* Define if you have the strdup function. */
#define HAVE_STRDUP 1

/* Define if you have the stricmp function. */
#define HAVE_STRICMP 1

/* Define if you have the strncasecmp function. */
/* #define HAVE_STRNCASECMP 1 */

/* Define if you have the strnicmp function. */
#define HAVE_STRNICMP 1

/* Define if you have the recv function. */
#define HAVE_RECV 1

/* Define to the type of arg 1 for recv. */
#define RECV_TYPE_ARG1 SOCKET

/* Define to the type of arg 2 for recv. */
#define RECV_TYPE_ARG2 char *

/* Define to the type of arg 3 for recv. */
#define RECV_TYPE_ARG3 int

/* Define to the type of arg 4 for recv. */
#define RECV_TYPE_ARG4 int

/* Define to the function return type for recv. */
#define RECV_TYPE_RETV int

/* Define if you have the recvfrom function. */
#define HAVE_RECVFROM 1

/* Define to the type of arg 1 for recvfrom. */
#define RECVFROM_TYPE_ARG1 SOCKET

/* Define to the type pointed by arg 2 for recvfrom. */
#define RECVFROM_TYPE_ARG2 char

/* Define to the type of arg 3 for recvfrom. */
#define RECVFROM_TYPE_ARG3 int

/* Define to the type of arg 4 for recvfrom. */
#define RECVFROM_TYPE_ARG4 int

/* Define to the type pointed by arg 5 for recvfrom. */
#define RECVFROM_TYPE_ARG5 struct sockaddr

/* Define to the type pointed by arg 6 for recvfrom. */
#define RECVFROM_TYPE_ARG6 int

/* Define to the function return type for recvfrom. */
#define RECVFROM_TYPE_RETV int

/* Define if you have the send function. */
#define HAVE_SEND 1

/* Define to the type of arg 1 for send. */
#define SEND_TYPE_ARG1 SOCKET

/* Define to the type qualifier of arg 2 for send. */
#define SEND_QUAL_ARG2 const

/* Define to the type of arg 2 for send. */
#define SEND_TYPE_ARG2 char *

/* Define to the type of arg 3 for send. */
#define SEND_TYPE_ARG3 int

/* Define to the type of arg 4 for send. */
#define SEND_TYPE_ARG4 int

/* Define to the function return type for send. */
#define SEND_TYPE_RETV int

/* Specifics for the Watt-32 tcp/ip stack. */
#ifdef WATT32
  #define SOCKET              int
  #define NS_INADDRSZ         4
  #define HAVE_ARPA_NAMESER_H 1
  #define HAVE_ARPA_INET_H    1
  #define HAVE_NETDB_H        1
  #define HAVE_NETINET_IN_H   1
  #define HAVE_SYS_SOCKET_H   1
  #define HAVE_NETINET_TCP_H  1
  #define HAVE_AF_INET6       1
  #define HAVE_PF_INET6       1
  #define HAVE_STRUCT_IN6_ADDR     1
  #define HAVE_STRUCT_SOCKADDR_IN6 1
  #undef HAVE_WINSOCK_H
  #undef HAVE_WINSOCK2_H
  #undef HAVE_WS2TCPIP_H
#endif

/* ---------------------------------------------------------------- */
/*                       TYPEDEF REPLACEMENTS                       */
/* ---------------------------------------------------------------- */

/* Define if in_addr_t is not an available 'typedefed' type. */
#define in_addr_t unsigned long

/* Define to the return type of signal handlers (int or void). */
#define RETSIGTYPE void

#ifdef __cplusplus
/* Compiling headers in C++ mode means bool is available */
#define HAVE_BOOL_T
#endif

/* Define if ssize_t is not an available 'typedefed' type. */
#ifndef _SSIZE_T_DEFINED
#  if (defined(__WATCOMC__) && (__WATCOMC__ >= 1240)) || \
      defined(__POCC__) || \
      defined(__MINGW32__)
#  elif defined(_WIN64)
#    define _SSIZE_T_DEFINED
#    define ssize_t __int64
#  else
#    define _SSIZE_T_DEFINED
#    define ssize_t int
#  endif
#endif

/* ---------------------------------------------------------------- */
/*                            TYPE SIZES                            */
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/*                          STRUCT RELATED                          */
/* ---------------------------------------------------------------- */

/* Define if you have struct addrinfo. */
#define HAVE_STRUCT_ADDRINFO 1

/* Define if you have struct sockaddr_storage. */
#if !defined(__SALFORDC__) && !defined(__BORLANDC__)
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#endif

/* Define if you have struct timeval. */
#define HAVE_STRUCT_TIMEVAL 1

/* ---------------------------------------------------------------- */
/*                        COMPILER SPECIFIC                         */
/* ---------------------------------------------------------------- */

/* Define to avoid VS2005 complaining about portable C functions. */
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#  define _CRT_SECURE_NO_DEPRECATE 1
#  define _CRT_NONSTDC_NO_DEPRECATE 1
#endif

/* Officially, Microsoft's Windows SDK versions 6.X do not support Windows
   2000 as a supported build target. VS2008 default installations provide
   an embedded Windows SDK v6.0A along with the claim that Windows 2000 is
   a valid build target for VS2008. Popular belief is that binaries built
   with VS2008 using Windows SDK versions 6.X and Windows 2000 as a build
   target are functional. */
#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#  define VS2008_MIN_TARGET 0x0500
#endif

/* When no build target is specified VS2008 default build target is Windows
   Vista, which leaves out even Winsows XP. If no build target has been given
   for VS2008 we will target the minimum Officially supported build target,
   which happens to be Windows XP. */
#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#  define VS2008_DEF_TARGET  0x0501
#endif

/* VS2008 default target settings and minimum build target check. */
#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT VS2008_DEF_TARGET
#  endif
#  ifndef WINVER
#    define WINVER VS2008_DEF_TARGET
#  endif
#  if (_WIN32_WINNT < VS2008_MIN_TARGET) || (WINVER < VS2008_MIN_TARGET)
#    error VS2008 does not support Windows build targets prior to Windows 2000
#  endif
#endif

/* When no build target is specified Pelles C 5.00 and later default build
   target is Windows Vista. We override default target to be Windows 2000. */
#if defined(__POCC__) && (__POCC__ >= 500)
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0500
#  endif
#  ifndef WINVER
#    define WINVER 0x0500
#  endif
#endif

/* Availability of freeaddrinfo, getaddrinfo and getnameinfo functions is
   quite convoluted, compiler dependent and even build target dependent. */
#if defined(HAVE_WS2TCPIP_H)
#  if defined(__POCC__)
#    define HAVE_FREEADDRINFO 1
#    define HAVE_GETADDRINFO  1
#    define HAVE_GETNAMEINFO  1
#  elif defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0501)
#    define HAVE_FREEADDRINFO 1
#    define HAVE_GETADDRINFO  1
#    define HAVE_GETNAMEINFO  1
#  elif defined(_MSC_VER) && (_MSC_VER >= 1200)
#    define HAVE_FREEADDRINFO 1
#    define HAVE_GETADDRINFO  1
#    define HAVE_GETNAMEINFO  1
#  endif
#endif

#if defined(__POCC__)
#  ifndef _MSC_VER
#    error Microsoft extensions /Ze compiler option is required
#  endif
#  ifndef __POCC__OLDNAMES
#    error Compatibility names /Go compiler option is required
#  endif
#endif

/* ---------------------------------------------------------------- */
/*                         IPV6 COMPATIBILITY                       */
/* ---------------------------------------------------------------- */

/* Define if you have address family AF_INET6. */
#ifdef HAVE_WINSOCK2_H
#define HAVE_AF_INET6 1
#endif

/* Define if you have protocol family PF_INET6. */
#ifdef HAVE_WINSOCK2_H
#define HAVE_PF_INET6 1
#endif

/* Define if you have struct in6_addr. */
#ifdef HAVE_WS2TCPIP_H
#define HAVE_STRUCT_IN6_ADDR 1
#endif

/* Define if you have struct sockaddr_in6. */
#ifdef HAVE_WS2TCPIP_H
#define HAVE_STRUCT_SOCKADDR_IN6 1
#endif

/* Define if you have sockaddr_in6 with scopeid. */
#ifdef HAVE_WS2TCPIP_H
#define HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID 1
#endif

/* ---------------------------------------------------------------- */
/*                              Win CE                              */
/* ---------------------------------------------------------------- */

/* FIXME: A proper config-win32ce.h should be created to hold these */

/*
 *  System error codes for Windows CE
 */

#if defined(_WIN32_WCE) && !defined(HAVE_ERRNO_H)
#  define ENOENT    ERROR_FILE_NOT_FOUND
#  define ESRCH     ERROR_PATH_NOT_FOUND
#  define ENOMEM    ERROR_NOT_ENOUGH_MEMORY
#  define ENOSPC    ERROR_INVALID_PARAMETER
#endif

#endif /* HEADER_CARES_CONFIG_WIN32_H */

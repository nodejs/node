#ifndef HEADER_CARES_SETUP_H
#define HEADER_CARES_SETUP_H


/* Copyright (C) 2004 - 2012 by Daniel Stenberg et al
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

/*
 * Define WIN32 when build target is Win32 API
 */

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#define WIN32
#endif

/*
 * Include configuration script results or hand-crafted
 * configuration file for platforms which lack config tool.
 */

#ifdef HAVE_CONFIG_H
#include "ares_config.h"
#else

#ifdef WIN32
#include "config-win32.h"
#endif

#endif /* HAVE_CONFIG_H */

/* ================================================================ */
/* Definition of preprocessor macros/symbols which modify compiler  */
/* behaviour or generated code characteristics must be done here,   */
/* as appropriate, before any system header file is included. It is */
/* also possible to have them defined in the config file included   */
/* before this point. As a result of all this we frown inclusion of */
/* system header files in our config files, avoid this at any cost. */
/* ================================================================ */

/*
 * AIX 4.3 and newer needs _THREAD_SAFE defined to build
 * proper reentrant code. Others may also need it.
 */

#ifdef NEED_THREAD_SAFE
#  ifndef _THREAD_SAFE
#    define _THREAD_SAFE
#  endif
#endif

/*
 * Tru64 needs _REENTRANT set for a few function prototypes and
 * things to appear in the system header files. Unixware needs it
 * to build proper reentrant code. Others may also need it.
 */

#ifdef NEED_REENTRANT
#  ifndef _REENTRANT
#    define _REENTRANT
#  endif
#endif

/* ================================================================ */
/*  If you need to include a system header file for your platform,  */
/*  please, do it beyond the point further indicated in this file.  */
/* ================================================================ */

#if 1
# define SIZEOF_SHORT 2
#else  /* Disabled for the gyp-ified build. */
/*
 * c-ares external interface definitions are also used internally,
 * and might also include required system header files to define them.
 */

#include <ares_build.h>

/*
 * Compile time sanity checks must also be done when building the library.
 */

#include <ares_rules.h>
#endif

/* ================================================================= */
/* No system header file shall be included in this file before this  */
/* point. The only allowed ones are those included from ares_build.h */
/* ================================================================= */

/*
 * Include header files for windows builds before redefining anything.
 * Use this preproessor block only to include or exclude windows.h,
 * winsock2.h, ws2tcpip.h or winsock.h. Any other windows thing belongs
 * to any other further and independent block.  Under Cygwin things work
 * just as under linux (e.g. <sys/socket.h>) and the winsock headers should
 * never be included when __CYGWIN__ is defined.  configure script takes
 * care of this, not defining HAVE_WINDOWS_H, HAVE_WINSOCK_H, HAVE_WINSOCK2_H,
 * neither HAVE_WS2TCPIP_H when __CYGWIN__ is defined.
 */

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
#  else
#    ifdef HAVE_WINSOCK_H
#      include <winsock.h>
#    endif
#  endif
#endif

/*
 * Define USE_WINSOCK to 2 if we have and use WINSOCK2 API, else
 * define USE_WINSOCK to 1 if we have and use WINSOCK  API, else
 * undefine USE_WINSOCK.
 */

#undef USE_WINSOCK

#ifdef HAVE_WINSOCK2_H
#  define USE_WINSOCK 2
#else
#  ifdef HAVE_WINSOCK_H
#    define USE_WINSOCK 1
#  endif
#endif

/*
 * Work-arounds for systems without configure support
 */

#ifndef HAVE_CONFIG_H

#if !defined(HAVE_SYS_TIME_H) && !defined(_MSC_VER) && !defined(__WATCOMC__)
#define HAVE_SYS_TIME_H
#endif

#if !defined(HAVE_UNISTD_H) && !defined(_MSC_VER)
#define HAVE_UNISTD_H 1
#endif

#if !defined(HAVE_SYS_UIO_H) && !defined(WIN32) && !defined(MSDOS)
#define HAVE_SYS_UIO_H
#endif

#endif /* HAVE_CONFIG_H */

/*
 * Arg 2 type for gethostname in case it hasn't been defined in config file.
 */

#ifndef GETHOSTNAME_TYPE_ARG2
#  ifdef USE_WINSOCK
#    define GETHOSTNAME_TYPE_ARG2 int
#  else
#    define GETHOSTNAME_TYPE_ARG2 size_t
#  endif
#endif

#ifdef __POCC__
#  include <sys/types.h>
#  include <unistd.h>
#  define ESRCH 3
#endif

/*
 * Android does have the arpa/nameser.h header which is detected by configure
 * but it appears to be empty with recent NDK r7b / r7c, so we undefine here.
 */
#if (defined(ANDROID) || defined(__ANDROID__)) && defined(HAVE_ARPA_NAMESER_H)
#  undef HAVE_ARPA_NAMESER_H
#endif

/*
 * Recent autoconf versions define these symbols in ares_config.h. We don't
 * want them (since they collide with the libcurl ones when we build
 *  --enable-debug) so we undef them again here.
 */

#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef VERSION
#undef PACKAGE

/* IPv6 compatibility */
#if !defined(HAVE_AF_INET6)
#if defined(HAVE_PF_INET6)
#define AF_INET6 PF_INET6
#else
#define AF_INET6 AF_MAX+1
#endif
#endif

/*
 * Include macros and defines that should only be processed once.
 */

#ifndef __SETUP_ONCE_H
#include "setup_once.h"
#endif

#endif /* HEADER_CARES_SETUP_H */

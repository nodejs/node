/* MIT License
 *
 * Copyright (c) 2004 Daniel Stenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef __SETUP_ONCE_H
#define __SETUP_ONCE_H


/********************************************************************
 *                              NOTICE                              *
 *                             ========                             *
 *                                                                  *
 *  Content of header files lib/setup_once.h and ares/setup_once.h  *
 *  must be kept in sync. Modify the other one if you change this.  *
 *                                                                  *
 ********************************************************************/


/*
 * Inclusion of common header files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef NEED_MALLOC_H
#include <malloc.h>
#endif

#ifdef NEED_MEMORY_H
#include <memory.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
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

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#if defined(HAVE_STDBOOL_H) && defined(HAVE_BOOL_T)
#include <stdbool.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef __hpux
#  if !defined(_XOPEN_SOURCE_EXTENDED) || defined(_KERNEL)
#    ifdef _APP32_64BIT_OFF_T
#      define OLD_APP32_64BIT_OFF_T _APP32_64BIT_OFF_T
#      undef _APP32_64BIT_OFF_T
#    else
#      undef OLD_APP32_64BIT_OFF_T
#    endif
#  endif
#endif

#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef __hpux
#  if !defined(_XOPEN_SOURCE_EXTENDED) || defined(_KERNEL)
#    ifdef OLD_APP32_64BIT_OFF_T
#      define _APP32_64BIT_OFF_T OLD_APP32_64BIT_OFF_T
#      undef OLD_APP32_64BIT_OFF_T
#    endif
#  endif
#endif


/*
 * Definition of timeval struct for platforms that don't have it.
 */

#ifndef HAVE_STRUCT_TIMEVAL
struct timeval {
 long tv_sec;
 long tv_usec;
};
#endif


/*
 * If we have the MSG_NOSIGNAL define, make sure we use
 * it as the fourth argument of function send()
 */

#ifdef HAVE_MSG_NOSIGNAL
#define SEND_4TH_ARG MSG_NOSIGNAL
#else
#define SEND_4TH_ARG 0
#endif


#if defined(__minix)
/* Minix doesn't support recv on TCP sockets */
#define sread(x,y,z) (ares_ssize_t)read((RECV_TYPE_ARG1)(x), \
                                   (RECV_TYPE_ARG2)(y), \
                                   (RECV_TYPE_ARG3)(z))

#elif defined(HAVE_RECV)
/*
 * The definitions for the return type and arguments types
 * of functions recv() and send() belong and come from the
 * configuration file. Do not define them in any other place.
 *
 * HAVE_RECV is defined if you have a function named recv()
 * which is used to read incoming data from sockets. If your
 * function has another name then don't define HAVE_RECV.
 *
 * If HAVE_RECV is defined then RECV_TYPE_ARG1, RECV_TYPE_ARG2,
 * RECV_TYPE_ARG3, RECV_TYPE_ARG4 and RECV_TYPE_RETV must also
 * be defined.
 *
 * HAVE_SEND is defined if you have a function named send()
 * which is used to write outgoing data on a connected socket.
 * If yours has another name then don't define HAVE_SEND.
 *
 * If HAVE_SEND is defined then SEND_TYPE_ARG1, SEND_QUAL_ARG2,
 * SEND_TYPE_ARG2, SEND_TYPE_ARG3, SEND_TYPE_ARG4 and
 * SEND_TYPE_RETV must also be defined.
 */

#if !defined(RECV_TYPE_ARG1) || \
    !defined(RECV_TYPE_ARG2) || \
    !defined(RECV_TYPE_ARG3) || \
    !defined(RECV_TYPE_ARG4) || \
    !defined(RECV_TYPE_RETV)
  /* */
  Error Missing_definition_of_return_and_arguments_types_of_recv
  /* */
#else
#define sread(x,y,z) (ares_ssize_t)recv((RECV_TYPE_ARG1)(x), \
                                   (RECV_TYPE_ARG2)(y), \
                                   (RECV_TYPE_ARG3)(z), \
                                   (RECV_TYPE_ARG4)(0))
#endif
#else /* HAVE_RECV */
#ifndef sread
  /* */
  Error Missing_definition_of_macro_sread
  /* */
#endif
#endif /* HAVE_RECV */


#if defined(__minix)
/* Minix doesn't support send on TCP sockets */
#define swrite(x,y,z) (ares_ssize_t)write((SEND_TYPE_ARG1)(x), \
                                    (SEND_TYPE_ARG2)(y), \
                                    (SEND_TYPE_ARG3)(z))

#elif defined(HAVE_SEND)
#if !defined(SEND_TYPE_ARG1) || \
    !defined(SEND_QUAL_ARG2) || \
    !defined(SEND_TYPE_ARG2) || \
    !defined(SEND_TYPE_ARG3) || \
    !defined(SEND_TYPE_ARG4) || \
    !defined(SEND_TYPE_RETV)
  /* */
  Error Missing_definition_of_return_and_arguments_types_of_send
  /* */
#else
#define swrite(x,y,z) (ares_ssize_t)send((SEND_TYPE_ARG1)(x), \
                                    (SEND_TYPE_ARG2)(y), \
                                    (SEND_TYPE_ARG3)(z), \
                                    (SEND_TYPE_ARG4)(SEND_4TH_ARG))
#endif
#else /* HAVE_SEND */
#ifndef swrite
  /* */
  Error Missing_definition_of_macro_swrite
  /* */
#endif
#endif /* HAVE_SEND */


#if 0
#if defined(HAVE_RECVFROM)
/*
 * Currently recvfrom is only used on udp sockets.
 */
#if !defined(RECVFROM_TYPE_ARG1) || \
    !defined(RECVFROM_TYPE_ARG2) || \
    !defined(RECVFROM_TYPE_ARG3) || \
    !defined(RECVFROM_TYPE_ARG4) || \
    !defined(RECVFROM_TYPE_ARG5) || \
    !defined(RECVFROM_TYPE_ARG6) || \
    !defined(RECVFROM_TYPE_RETV)
  /* */
  Error Missing_definition_of_return_and_arguments_types_of_recvfrom
  /* */
#else
#define sreadfrom(s,b,bl,f,fl) (ares_ssize_t)recvfrom((RECVFROM_TYPE_ARG1)  (s),  \
                                                 (RECVFROM_TYPE_ARG2 *)(b),  \
                                                 (RECVFROM_TYPE_ARG3)  (bl), \
                                                 (RECVFROM_TYPE_ARG4)  (0),  \
                                                 (RECVFROM_TYPE_ARG5 *)(f),  \
                                                 (RECVFROM_TYPE_ARG6 *)(fl))
#endif
#else /* HAVE_RECVFROM */
#ifndef sreadfrom
  /* */
  Error Missing_definition_of_macro_sreadfrom
  /* */
#endif
#endif /* HAVE_RECVFROM */


#ifdef RECVFROM_TYPE_ARG6_IS_VOID
#  define RECVFROM_ARG6_T int
#else
#  define RECVFROM_ARG6_T RECVFROM_TYPE_ARG6
#endif
#endif /* if 0 */


/*
 * Function-like macro definition used to close a socket.
 */

#if defined(HAVE_CLOSESOCKET)
#  define sclose(x)  closesocket((x))
#elif defined(HAVE_CLOSESOCKET_CAMEL)
#  define sclose(x)  CloseSocket((x))
#elif defined(HAVE_CLOSE_S)
#  define sclose(x)  close_s((x))
#else
#  define sclose(x)  close((x))
#endif


/*
 * Uppercase macro versions of ANSI/ISO is*() functions/macros which
 * avoid negative number inputs with argument byte codes > 127.
 */

#define ISSPACE(x)  (isspace((int)  ((unsigned char)x)))
#define ISDIGIT(x)  (isdigit((int)  ((unsigned char)x)))
#define ISALNUM(x)  (isalnum((int)  ((unsigned char)x)))
#define ISXDIGIT(x) (isxdigit((int) ((unsigned char)x)))
#define ISGRAPH(x)  (isgraph((int)  ((unsigned char)x)))
#define ISALPHA(x)  (isalpha((int)  ((unsigned char)x)))
#define ISPRINT(x)  (isprint((int)  ((unsigned char)x)))
#define ISUPPER(x)  (isupper((int)  ((unsigned char)x)))
#define ISLOWER(x)  (islower((int)  ((unsigned char)x)))
#define ISASCII(x)  (isascii((int)  ((unsigned char)x)))

#define ISBLANK(x)  (int)((((unsigned char)x) == ' ') || \
                          (((unsigned char)x) == '\t'))

#define TOLOWER(x)  (tolower((int)  ((unsigned char)x)))


/*
 * 'bool' stuff compatible with HP-UX headers.
 */

#if defined(__hpux) && !defined(HAVE_BOOL_T)
   typedef int bool;
#  define false 0
#  define true 1
#  define HAVE_BOOL_T
#endif


/*
 * 'bool' exists on platforms with <stdbool.h>, i.e. C99 platforms.
 * On non-C99 platforms there's no bool, so define an enum for that.
 * On C99 platforms 'false' and 'true' also exist. Enum uses a
 * global namespace though, so use bool_false and bool_true.
 */

#ifndef HAVE_BOOL_T
  typedef enum {
      bool_false = 0,
      bool_true  = 1
  } bool;

/*
 * Use a define to let 'true' and 'false' use those enums.  There
 * are currently no use of true and false in libcurl proper, but
 * there are some in the examples. This will cater for any later
 * code happening to use true and false.
 */
#  define false bool_false
#  define true  bool_true
#  define HAVE_BOOL_T
#endif


/*
 * Redefine TRUE and FALSE too, to catch current use. With this
 * change, 'bool found = 1' will give a warning on MIPSPro, but
 * 'bool found = TRUE' will not. Change tested on IRIX/MIPSPro,
 * AIX 5.1/Xlc, Tru64 5.1/cc, w/make test too.
 */

#ifndef TRUE
#define TRUE true
#endif
#ifndef FALSE
#define FALSE false
#endif


/*
 * Macro WHILE_FALSE may be used to build single-iteration do-while loops,
 * avoiding compiler warnings. Mostly intended for other macro definitions.
 */

#define WHILE_FALSE  while(0)

#if defined(_MSC_VER) && !defined(__POCC__)
#  undef WHILE_FALSE
#  if (_MSC_VER < 1500)
#    define WHILE_FALSE  while(1, 0)
#  else
#    define WHILE_FALSE \
__pragma(warning(push)) \
__pragma(warning(disable:4127)) \
while(0) \
__pragma(warning(pop))
#  endif
#endif


/*
 * Typedef to 'int' if sig_atomic_t is not an available 'typedefed' type.
 */

#ifndef HAVE_SIG_ATOMIC_T
typedef int sig_atomic_t;
#define HAVE_SIG_ATOMIC_T
#endif


/*
 * Convenience SIG_ATOMIC_T definition
 */

#ifdef HAVE_SIG_ATOMIC_T_VOLATILE
#define SIG_ATOMIC_T static sig_atomic_t
#else
#define SIG_ATOMIC_T static volatile sig_atomic_t
#endif


/*
 * Default return type for signal handlers.
 */

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif


/*
 * Macro used to include code only in debug builds.
 */

#ifdef DEBUGBUILD
#define DEBUGF(x) x
#else
#define DEBUGF(x) do { } WHILE_FALSE
#endif


/*
 * Macro used to include assertion code only in debug builds.
 */

#if defined(DEBUGBUILD) && defined(HAVE_ASSERT_H)
#define DEBUGASSERT(x) assert(x)
#else
#define DEBUGASSERT(x) do { } WHILE_FALSE
#endif


/*
 * Macro SOCKERRNO / SET_SOCKERRNO() returns / sets the *socket-related* errno
 * (or equivalent) on this platform to hide platform details to code using it.
 */

#ifdef USE_WINSOCK
#define SOCKERRNO         ((int)WSAGetLastError())
#define SET_SOCKERRNO(x)  (WSASetLastError((int)(x)))
#else
#define SOCKERRNO         (errno)
#define SET_SOCKERRNO(x)  (errno = (x))
#endif


/*
 * Macro ERRNO / SET_ERRNO() returns / sets the NOT *socket-related* errno
 * (or equivalent) on this platform to hide platform details to code using it.
 */

#if defined(WIN32) && !defined(WATT32)
#define ERRNO         ((int)GetLastError())
#define SET_ERRNO(x)  (SetLastError((DWORD)(x)))
#else
#define ERRNO         (errno)
#define SET_ERRNO(x)  (errno = (x))
#endif


/*
 * Portable error number symbolic names defined to Winsock error codes.
 */

#ifdef USE_WINSOCK
#undef  EBADF            /* override definition in errno.h */
#define EBADF            WSAEBADF
#undef  EINTR            /* override definition in errno.h */
#define EINTR            WSAEINTR
#undef  EINVAL           /* override definition in errno.h */
#define EINVAL           WSAEINVAL
#undef  EWOULDBLOCK      /* override definition in errno.h */
#define EWOULDBLOCK      WSAEWOULDBLOCK
#undef  EINPROGRESS      /* override definition in errno.h */
#define EINPROGRESS      WSAEINPROGRESS
#undef  EALREADY         /* override definition in errno.h */
#define EALREADY         WSAEALREADY
#undef  ENOTSOCK         /* override definition in errno.h */
#define ENOTSOCK         WSAENOTSOCK
#undef  EDESTADDRREQ     /* override definition in errno.h */
#define EDESTADDRREQ     WSAEDESTADDRREQ
#undef  EMSGSIZE         /* override definition in errno.h */
#define EMSGSIZE         WSAEMSGSIZE
#undef  EPROTOTYPE       /* override definition in errno.h */
#define EPROTOTYPE       WSAEPROTOTYPE
#undef  ENOPROTOOPT      /* override definition in errno.h */
#define ENOPROTOOPT      WSAENOPROTOOPT
#undef  EPROTONOSUPPORT  /* override definition in errno.h */
#define EPROTONOSUPPORT  WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT  WSAESOCKTNOSUPPORT
#undef  EOPNOTSUPP       /* override definition in errno.h */
#define EOPNOTSUPP       WSAEOPNOTSUPP
#define EPFNOSUPPORT     WSAEPFNOSUPPORT
#undef  EAFNOSUPPORT     /* override definition in errno.h */
#define EAFNOSUPPORT     WSAEAFNOSUPPORT
#undef  EADDRINUSE       /* override definition in errno.h */
#define EADDRINUSE       WSAEADDRINUSE
#undef  EADDRNOTAVAIL    /* override definition in errno.h */
#define EADDRNOTAVAIL    WSAEADDRNOTAVAIL
#undef  ENETDOWN         /* override definition in errno.h */
#define ENETDOWN         WSAENETDOWN
#undef  ENETUNREACH      /* override definition in errno.h */
#define ENETUNREACH      WSAENETUNREACH
#undef  ENETRESET        /* override definition in errno.h */
#define ENETRESET        WSAENETRESET
#undef  ECONNABORTED     /* override definition in errno.h */
#define ECONNABORTED     WSAECONNABORTED
#undef  ECONNRESET       /* override definition in errno.h */
#define ECONNRESET       WSAECONNRESET
#undef  ENOBUFS          /* override definition in errno.h */
#define ENOBUFS          WSAENOBUFS
#undef  EISCONN          /* override definition in errno.h */
#define EISCONN          WSAEISCONN
#undef  ENOTCONN         /* override definition in errno.h */
#define ENOTCONN         WSAENOTCONN
#define ESHUTDOWN        WSAESHUTDOWN
#define ETOOMANYREFS     WSAETOOMANYREFS
#undef  ETIMEDOUT        /* override definition in errno.h */
#define ETIMEDOUT        WSAETIMEDOUT
#undef  ECONNREFUSED     /* override definition in errno.h */
#define ECONNREFUSED     WSAECONNREFUSED
#undef  ELOOP            /* override definition in errno.h */
#define ELOOP            WSAELOOP
#ifndef ENAMETOOLONG     /* possible previous definition in errno.h */
#define ENAMETOOLONG     WSAENAMETOOLONG
#endif
#define EHOSTDOWN        WSAEHOSTDOWN
#undef  EHOSTUNREACH     /* override definition in errno.h */
#define EHOSTUNREACH     WSAEHOSTUNREACH
#ifndef ENOTEMPTY        /* possible previous definition in errno.h */
#define ENOTEMPTY        WSAENOTEMPTY
#endif
#define EPROCLIM         WSAEPROCLIM
#define EUSERS           WSAEUSERS
#define EDQUOT           WSAEDQUOT
#define ESTALE           WSAESTALE
#define EREMOTE          WSAEREMOTE
#endif


/*
 *  Actually use __32_getpwuid() on 64-bit VMS builds for getpwuid()
 */

#if defined(__VMS) && \
    defined(__INITIAL_POINTER_SIZE) && (__INITIAL_POINTER_SIZE == 64)
#define getpwuid __32_getpwuid
#endif


/*
 * Macro argv_item_t hides platform details to code using it.
 */

#ifdef __VMS
#define argv_item_t  __char_ptr32
#else
#define argv_item_t  char *
#endif


/*
 * We use this ZERO_NULL to avoid picky compiler warnings,
 * when assigning a NULL pointer to a function pointer var.
 */

#define ZERO_NULL 0


#endif /* __SETUP_ONCE_H */

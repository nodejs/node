/*
 * libev event processing core, watcher management
 *
 * Copyright (c) 2007-2019 Marc Alexander Lehmann <libev@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

/* this big block deduces configuration from config.h */
#ifndef EV_STANDALONE
# ifdef EV_CONFIG_H
#  include EV_CONFIG_H
# else
#  include "config.h"
# endif

# if HAVE_FLOOR
#  ifndef EV_USE_FLOOR
#   define EV_USE_FLOOR 1
#  endif
# endif

# if HAVE_CLOCK_SYSCALL
#  ifndef EV_USE_CLOCK_SYSCALL
#   define EV_USE_CLOCK_SYSCALL 1
#   ifndef EV_USE_REALTIME
#    define EV_USE_REALTIME  0
#   endif
#   ifndef EV_USE_MONOTONIC
#    define EV_USE_MONOTONIC 1
#   endif
#  endif
# elif !defined EV_USE_CLOCK_SYSCALL
#  define EV_USE_CLOCK_SYSCALL 0
# endif

# if HAVE_CLOCK_GETTIME
#  ifndef EV_USE_MONOTONIC
#   define EV_USE_MONOTONIC 1
#  endif
#  ifndef EV_USE_REALTIME
#   define EV_USE_REALTIME  0
#  endif
# else
#  ifndef EV_USE_MONOTONIC
#   define EV_USE_MONOTONIC 0
#  endif
#  ifndef EV_USE_REALTIME
#   define EV_USE_REALTIME  0
#  endif
# endif

# if HAVE_NANOSLEEP
#  ifndef EV_USE_NANOSLEEP
#    define EV_USE_NANOSLEEP EV_FEATURE_OS
#  endif
# else
#   undef EV_USE_NANOSLEEP
#   define EV_USE_NANOSLEEP 0
# endif

# if HAVE_SELECT && HAVE_SYS_SELECT_H
#  ifndef EV_USE_SELECT
#   define EV_USE_SELECT EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_SELECT
#  define EV_USE_SELECT 0
# endif

# if HAVE_POLL && HAVE_POLL_H
#  ifndef EV_USE_POLL
#   define EV_USE_POLL EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_POLL
#  define EV_USE_POLL 0
# endif
   
# if HAVE_EPOLL_CTL && HAVE_SYS_EPOLL_H
#  ifndef EV_USE_EPOLL
#   define EV_USE_EPOLL EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_EPOLL
#  define EV_USE_EPOLL 0
# endif
   
# if HAVE_LINUX_AIO_ABI_H
#  ifndef EV_USE_LINUXAIO
#   define EV_USE_LINUXAIO 0 /* was: EV_FEATURE_BACKENDS, always off by default */
#  endif
# else
#  undef EV_USE_LINUXAIO
#  define EV_USE_LINUXAIO 0
# endif
   
# if HAVE_LINUX_FS_H && HAVE_SYS_TIMERFD_H && HAVE_KERNEL_RWF_T
#  ifndef EV_USE_IOURING
#   define EV_USE_IOURING EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_IOURING
#  define EV_USE_IOURING 0
# endif
 
# if HAVE_KQUEUE && HAVE_SYS_EVENT_H
#  ifndef EV_USE_KQUEUE
#   define EV_USE_KQUEUE EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_KQUEUE
#  define EV_USE_KQUEUE 0
# endif
   
# if HAVE_PORT_H && HAVE_PORT_CREATE
#  ifndef EV_USE_PORT
#   define EV_USE_PORT EV_FEATURE_BACKENDS
#  endif
# else
#  undef EV_USE_PORT
#  define EV_USE_PORT 0
# endif

# if HAVE_INOTIFY_INIT && HAVE_SYS_INOTIFY_H
#  ifndef EV_USE_INOTIFY
#   define EV_USE_INOTIFY EV_FEATURE_OS
#  endif
# else
#  undef EV_USE_INOTIFY
#  define EV_USE_INOTIFY 0
# endif

# if HAVE_SIGNALFD && HAVE_SYS_SIGNALFD_H
#  ifndef EV_USE_SIGNALFD
#   define EV_USE_SIGNALFD EV_FEATURE_OS
#  endif
# else
#  undef EV_USE_SIGNALFD
#  define EV_USE_SIGNALFD 0
# endif

# if HAVE_EVENTFD
#  ifndef EV_USE_EVENTFD
#   define EV_USE_EVENTFD EV_FEATURE_OS
#  endif
# else
#  undef EV_USE_EVENTFD
#  define EV_USE_EVENTFD 0
# endif

# if HAVE_SYS_TIMERFD_H
#  ifndef EV_USE_TIMERFD
#   define EV_USE_TIMERFD EV_FEATURE_OS
#  endif
# else
#  undef EV_USE_TIMERFD
#  define EV_USE_TIMERFD 0
# endif

#endif

/* OS X, in its infinite idiocy, actually HARDCODES
 * a limit of 1024 into their select. Where people have brains,
 * OS X engineers apparently have a vacuum. Or maybe they were
 * ordered to have a vacuum, or they do anything for money.
 * This might help. Or not.
 * Note that this must be defined early, as other include files
 * will rely on this define as well.
 */
#define _DARWIN_UNLIMITED_SELECT 1

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stddef.h>

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>

#include <signal.h>

#ifdef EV_H
# include EV_H
#else
# include "ev.h"
#endif

#if EV_NO_THREADS
# undef EV_NO_SMP
# define EV_NO_SMP 1
# undef ECB_NO_THREADS
# define ECB_NO_THREADS 1
#endif
#if EV_NO_SMP
# undef EV_NO_SMP
# define ECB_NO_SMP 1
#endif

#ifndef _WIN32
# include <sys/time.h>
# include <sys/wait.h>
# include <unistd.h>
#else
# include <io.h>
# define WIN32_LEAN_AND_MEAN
# include <winsock2.h>
# include <windows.h>
# ifndef EV_SELECT_IS_WINSOCKET
#  define EV_SELECT_IS_WINSOCKET 1
# endif
# undef EV_AVOID_STDIO
#endif

/* this block tries to deduce configuration from header-defined symbols and defaults */

/* try to deduce the maximum number of signals on this platform */
#if defined EV_NSIG
/* use what's provided */
#elif defined NSIG
# define EV_NSIG (NSIG)
#elif defined _NSIG
# define EV_NSIG (_NSIG)
#elif defined SIGMAX
# define EV_NSIG (SIGMAX+1)
#elif defined SIG_MAX
# define EV_NSIG (SIG_MAX+1)
#elif defined _SIG_MAX
# define EV_NSIG (_SIG_MAX+1)
#elif defined MAXSIG
# define EV_NSIG (MAXSIG+1)
#elif defined MAX_SIG
# define EV_NSIG (MAX_SIG+1)
#elif defined SIGARRAYSIZE
# define EV_NSIG (SIGARRAYSIZE) /* Assume ary[SIGARRAYSIZE] */
#elif defined _sys_nsig
# define EV_NSIG (_sys_nsig) /* Solaris 2.5 */
#else
# define EV_NSIG (8 * sizeof (sigset_t) + 1)
#endif

#ifndef EV_USE_FLOOR
# define EV_USE_FLOOR 0
#endif

#ifndef EV_USE_CLOCK_SYSCALL
# if __linux && __GLIBC__ == 2 && __GLIBC_MINOR__ < 17
#  define EV_USE_CLOCK_SYSCALL EV_FEATURE_OS
# else
#  define EV_USE_CLOCK_SYSCALL 0
# endif
#endif

#if !(_POSIX_TIMERS > 0)
# ifndef EV_USE_MONOTONIC
#  define EV_USE_MONOTONIC 0
# endif
# ifndef EV_USE_REALTIME
#  define EV_USE_REALTIME 0
# endif
#endif

#ifndef EV_USE_MONOTONIC
# if defined _POSIX_MONOTONIC_CLOCK && _POSIX_MONOTONIC_CLOCK >= 0
#  define EV_USE_MONOTONIC EV_FEATURE_OS
# else
#  define EV_USE_MONOTONIC 0
# endif
#endif

#ifndef EV_USE_REALTIME
# define EV_USE_REALTIME !EV_USE_CLOCK_SYSCALL
#endif

#ifndef EV_USE_NANOSLEEP
# if _POSIX_C_SOURCE >= 199309L
#  define EV_USE_NANOSLEEP EV_FEATURE_OS
# else
#  define EV_USE_NANOSLEEP 0
# endif
#endif

#ifndef EV_USE_SELECT
# define EV_USE_SELECT EV_FEATURE_BACKENDS
#endif

#ifndef EV_USE_POLL
# ifdef _WIN32
#  define EV_USE_POLL 0
# else
#  define EV_USE_POLL EV_FEATURE_BACKENDS
# endif
#endif

#ifndef EV_USE_EPOLL
# if __linux && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
#  define EV_USE_EPOLL EV_FEATURE_BACKENDS
# else
#  define EV_USE_EPOLL 0
# endif
#endif

#ifndef EV_USE_KQUEUE
# define EV_USE_KQUEUE 0
#endif

#ifndef EV_USE_PORT
# define EV_USE_PORT 0
#endif

#ifndef EV_USE_LINUXAIO
# if __linux /* libev currently assumes linux/aio_abi.h is always available on linux */
#  define EV_USE_LINUXAIO 0 /* was: 1, always off by default */
# else
#  define EV_USE_LINUXAIO 0
# endif
#endif

#ifndef EV_USE_IOURING
# if __linux /* later checks might disable again */
#  define EV_USE_IOURING 1
# else
#  define EV_USE_IOURING 0
# endif
#endif

#ifndef EV_USE_INOTIFY
# if __linux && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
#  define EV_USE_INOTIFY EV_FEATURE_OS
# else
#  define EV_USE_INOTIFY 0
# endif
#endif

#ifndef EV_PID_HASHSIZE
# define EV_PID_HASHSIZE EV_FEATURE_DATA ? 16 : 1
#endif

#ifndef EV_INOTIFY_HASHSIZE
# define EV_INOTIFY_HASHSIZE EV_FEATURE_DATA ? 16 : 1
#endif

#ifndef EV_USE_EVENTFD
# if __linux && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 7))
#  define EV_USE_EVENTFD EV_FEATURE_OS
# else
#  define EV_USE_EVENTFD 0
# endif
#endif

#ifndef EV_USE_SIGNALFD
# if __linux && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 7))
#  define EV_USE_SIGNALFD EV_FEATURE_OS
# else
#  define EV_USE_SIGNALFD 0
# endif
#endif

#ifndef EV_USE_TIMERFD
# if __linux && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8))
#  define EV_USE_TIMERFD EV_FEATURE_OS
# else
#  define EV_USE_TIMERFD 0
# endif
#endif

#if 0 /* debugging */
# define EV_VERIFY 3
# define EV_USE_4HEAP 1
# define EV_HEAP_CACHE_AT 1
#endif

#ifndef EV_VERIFY
# define EV_VERIFY (EV_FEATURE_API ? 1 : 0)
#endif

#ifndef EV_USE_4HEAP
# define EV_USE_4HEAP EV_FEATURE_DATA
#endif

#ifndef EV_HEAP_CACHE_AT
# define EV_HEAP_CACHE_AT EV_FEATURE_DATA
#endif

#ifdef __ANDROID__
/* supposedly, android doesn't typedef fd_mask */
# undef EV_USE_SELECT
# define EV_USE_SELECT 0
/* supposedly, we need to include syscall.h, not sys/syscall.h, so just disable */
# undef EV_USE_CLOCK_SYSCALL
# define EV_USE_CLOCK_SYSCALL 0
#endif

/* aix's poll.h seems to cause lots of trouble */
#ifdef _AIX
/* AIX has a completely broken poll.h header */
# undef EV_USE_POLL
# define EV_USE_POLL 0
#endif

/* on linux, we can use a (slow) syscall to avoid a dependency on pthread, */
/* which makes programs even slower. might work on other unices, too. */
#if EV_USE_CLOCK_SYSCALL
# include <sys/syscall.h>
# ifdef SYS_clock_gettime
#  define clock_gettime(id, ts) syscall (SYS_clock_gettime, (id), (ts))
#  undef EV_USE_MONOTONIC
#  define EV_USE_MONOTONIC 1
#  define EV_NEED_SYSCALL 1
# else
#  undef EV_USE_CLOCK_SYSCALL
#  define EV_USE_CLOCK_SYSCALL 0
# endif
#endif

/* this block fixes any misconfiguration where we know we run into trouble otherwise */

#ifndef CLOCK_MONOTONIC
# undef EV_USE_MONOTONIC
# define EV_USE_MONOTONIC 0
#endif

#ifndef CLOCK_REALTIME
# undef EV_USE_REALTIME
# define EV_USE_REALTIME 0
#endif

#if !EV_STAT_ENABLE
# undef EV_USE_INOTIFY
# define EV_USE_INOTIFY 0
#endif

#if __linux && EV_USE_IOURING
# include <linux/version.h>
# if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
#  undef EV_USE_IOURING
#  define EV_USE_IOURING 0
# endif
#endif

#if !EV_USE_NANOSLEEP
/* hp-ux has it in sys/time.h, which we unconditionally include above */
# if !defined _WIN32 && !defined __hpux
#  include <sys/select.h>
# endif
#endif

#if EV_USE_LINUXAIO
# include <sys/syscall.h>
# if SYS_io_getevents && EV_USE_EPOLL /* linuxaio backend requires epoll backend */
#  define EV_NEED_SYSCALL 1
# else
#  undef EV_USE_LINUXAIO
#  define EV_USE_LINUXAIO 0
# endif
#endif

#if EV_USE_IOURING
# include <sys/syscall.h>
# if !SYS_io_uring_setup && __linux && !__alpha
#  define SYS_io_uring_setup     425
#  define SYS_io_uring_enter     426
#  define SYS_io_uring_wregister 427
# endif
# if SYS_io_uring_setup && EV_USE_EPOLL /* iouring backend requires epoll backend */
#  define EV_NEED_SYSCALL 1
# else
#  undef EV_USE_IOURING
#  define EV_USE_IOURING 0
# endif
#endif

#if EV_USE_INOTIFY
# include <sys/statfs.h>
# include <sys/inotify.h>
/* some very old inotify.h headers don't have IN_DONT_FOLLOW */
# ifndef IN_DONT_FOLLOW
#  undef EV_USE_INOTIFY
#  define EV_USE_INOTIFY 0
# endif
#endif

#if EV_USE_EVENTFD
/* our minimum requirement is glibc 2.7 which has the stub, but not the full header */
# include <stdint.h>
# ifndef EFD_NONBLOCK
#  define EFD_NONBLOCK O_NONBLOCK
# endif
# ifndef EFD_CLOEXEC
#  ifdef O_CLOEXEC
#   define EFD_CLOEXEC O_CLOEXEC
#  else
#   define EFD_CLOEXEC 02000000
#  endif
# endif
EV_CPP(extern "C") int (eventfd) (unsigned int initval, int flags);
#endif

#if EV_USE_SIGNALFD
/* our minimum requirement is glibc 2.7 which has the stub, but not the full header */
# include <stdint.h>
# ifndef SFD_NONBLOCK
#  define SFD_NONBLOCK O_NONBLOCK
# endif
# ifndef SFD_CLOEXEC
#  ifdef O_CLOEXEC
#   define SFD_CLOEXEC O_CLOEXEC
#  else
#   define SFD_CLOEXEC 02000000
#  endif
# endif
EV_CPP (extern "C") int (signalfd) (int fd, const sigset_t *mask, int flags);

struct signalfd_siginfo
{
  uint32_t ssi_signo;
  char pad[128 - sizeof (uint32_t)];
};
#endif

/* for timerfd, libev core requires TFD_TIMER_CANCEL_ON_SET &c */
#if EV_USE_TIMERFD
# include <sys/timerfd.h>
/* timerfd is only used for periodics */
# if !(defined (TFD_TIMER_CANCEL_ON_SET) && defined (TFD_CLOEXEC) && defined (TFD_NONBLOCK)) || !EV_PERIODIC_ENABLE
#  undef EV_USE_TIMERFD
#  define EV_USE_TIMERFD 0
# endif
#endif

/*****************************************************************************/

#if EV_VERIFY >= 3
# define EV_FREQUENT_CHECK ev_verify (EV_A)
#else
# define EV_FREQUENT_CHECK do { } while (0)
#endif

/*
 * This is used to work around floating point rounding problems.
 * This value is good at least till the year 4000.
 */
#define MIN_INTERVAL  0.0001220703125 /* 1/2**13, good till 4000 */
/*#define MIN_INTERVAL  0.00000095367431640625 /* 1/2**20, good till 2200 */

#define MIN_TIMEJUMP   1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME  59.743 /* never wait longer than this time (to detect time jumps) */
#define MAX_BLOCKTIME2 1500001.07 /* same, but when timerfd is used to detect jumps, also safe delay to not overflow */

/* find a portable timestamp that is "always" in the future but fits into time_t.
 * this is quite hard, and we are mostly guessing - we handle 32 bit signed/unsigned time_t,
 * and sizes larger than 32 bit, and maybe the unlikely floating point time_t */
#define EV_TSTAMP_HUGE \
  (sizeof (time_t) >= 8     ? 10000000000000.  \
   : 0 < (time_t)4294967295 ?     4294967295.  \
   :                              2147483647.) \

#ifndef EV_TS_CONST
# define EV_TS_CONST(nv) nv
# define EV_TS_TO_MSEC(a) a * 1e3 + 0.9999
# define EV_TS_FROM_USEC(us) us * 1e-6
# define EV_TV_SET(tv,t) do { tv.tv_sec = (long)t; tv.tv_usec = (long)((t - tv.tv_sec) * 1e6); } while (0)
# define EV_TS_SET(ts,t) do { ts.tv_sec = (long)t; ts.tv_nsec = (long)((t - ts.tv_sec) * 1e9); } while (0)
# define EV_TV_GET(tv) ((tv).tv_sec + (tv).tv_usec * 1e-6)
# define EV_TS_GET(ts) ((ts).tv_sec + (ts).tv_nsec * 1e-9)
#endif

/* the following is ecb.h embedded into libev - use update_ev_c to update from an external copy */
/* ECB.H BEGIN */
/*
 * libecb - http://software.schmorp.de/pkg/libecb
 *
 * Copyright (©) 2009-2015,2018-2020 Marc Alexander Lehmann <libecb@schmorp.de>
 * Copyright (©) 2011 Emanuele Giaquinta
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

#ifndef ECB_H
#define ECB_H

/* 16 bits major, 16 bits minor */
#define ECB_VERSION 0x00010008

#include <string.h> /* for memcpy */

#if defined (_WIN32) && !defined (__MINGW32__)
  typedef   signed char   int8_t;
  typedef unsigned char  uint8_t;
  typedef   signed char   int_fast8_t;
  typedef unsigned char  uint_fast8_t;
  typedef   signed short  int16_t;
  typedef unsigned short uint16_t;
  typedef   signed int    int_fast16_t;
  typedef unsigned int   uint_fast16_t;
  typedef   signed int    int32_t;
  typedef unsigned int   uint32_t;
  typedef   signed int    int_fast32_t;
  typedef unsigned int   uint_fast32_t;
  #if __GNUC__
    typedef   signed long long int64_t;
    typedef unsigned long long uint64_t;
  #else /* _MSC_VER || __BORLANDC__ */
    typedef   signed __int64   int64_t;
    typedef unsigned __int64   uint64_t;
  #endif
  typedef  int64_t  int_fast64_t;
  typedef uint64_t uint_fast64_t;
  #ifdef _WIN64
    #define ECB_PTRSIZE 8
    typedef uint64_t uintptr_t;
    typedef  int64_t  intptr_t;
  #else
    #define ECB_PTRSIZE 4
    typedef uint32_t uintptr_t;
    typedef  int32_t  intptr_t;
  #endif
#else
  #include <inttypes.h>
  #if (defined INTPTR_MAX ? INTPTR_MAX : ULONG_MAX) > 0xffffffffU
    #define ECB_PTRSIZE 8
  #else
    #define ECB_PTRSIZE 4
  #endif
#endif

#define ECB_GCC_AMD64 (__amd64 || __amd64__ || __x86_64 || __x86_64__)
#define ECB_MSVC_AMD64 (_M_AMD64 || _M_X64)

#ifndef ECB_OPTIMIZE_SIZE
  #if __OPTIMIZE_SIZE__
    #define ECB_OPTIMIZE_SIZE 1
  #else
    #define ECB_OPTIMIZE_SIZE 0
  #endif
#endif

/* work around x32 idiocy by defining proper macros */
#if ECB_GCC_AMD64 || ECB_MSVC_AMD64
  #if _ILP32
    #define ECB_AMD64_X32 1
  #else
    #define ECB_AMD64 1
  #endif
#endif

/* many compilers define _GNUC_ to some versions but then only implement
 * what their idiot authors think are the "more important" extensions,
 * causing enormous grief in return for some better fake benchmark numbers.
 * or so.
 * we try to detect these and simply assume they are not gcc - if they have
 * an issue with that they should have done it right in the first place.
 */
#if !defined __GNUC_MINOR__ || defined __INTEL_COMPILER || defined __SUNPRO_C || defined __SUNPRO_CC || defined __llvm__ || defined __clang__
  #define ECB_GCC_VERSION(major,minor) 0
#else
  #define ECB_GCC_VERSION(major,minor) (__GNUC__ > (major) || (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#endif

#define ECB_CLANG_VERSION(major,minor) (__clang_major__ > (major) || (__clang_major__ == (major) && __clang_minor__ >= (minor)))

#if __clang__ && defined __has_builtin
  #define ECB_CLANG_BUILTIN(x) __has_builtin (x)
#else
  #define ECB_CLANG_BUILTIN(x) 0
#endif

#if __clang__ && defined __has_extension
  #define ECB_CLANG_EXTENSION(x) __has_extension (x)
#else
  #define ECB_CLANG_EXTENSION(x) 0
#endif

#define ECB_CPP   (__cplusplus+0)
#define ECB_CPP11 (__cplusplus >= 201103L)
#define ECB_CPP14 (__cplusplus >= 201402L)
#define ECB_CPP17 (__cplusplus >= 201703L)

#if ECB_CPP
  #define ECB_C            0
  #define ECB_STDC_VERSION 0
#else
  #define ECB_C            1
  #define ECB_STDC_VERSION __STDC_VERSION__
#endif

#define ECB_C99   (ECB_STDC_VERSION >= 199901L)
#define ECB_C11   (ECB_STDC_VERSION >= 201112L)
#define ECB_C17   (ECB_STDC_VERSION >= 201710L)

#if ECB_CPP
  #define ECB_EXTERN_C extern "C"
  #define ECB_EXTERN_C_BEG ECB_EXTERN_C {
  #define ECB_EXTERN_C_END }
#else
  #define ECB_EXTERN_C extern
  #define ECB_EXTERN_C_BEG
  #define ECB_EXTERN_C_END
#endif

/*****************************************************************************/

/* ECB_NO_THREADS - ecb is not used by multiple threads, ever */
/* ECB_NO_SMP     - ecb might be used in multiple threads, but only on a single cpu */

#if ECB_NO_THREADS
  #define ECB_NO_SMP 1
#endif

#if ECB_NO_SMP
  #define ECB_MEMORY_FENCE do { } while (0)
#endif

/* http://www-01.ibm.com/support/knowledgecenter/SSGH3R_13.1.0/com.ibm.xlcpp131.aix.doc/compiler_ref/compiler_builtins.html */
#if __xlC__ && ECB_CPP
  #include <builtins.h>
#endif

#if 1400 <= _MSC_VER
  #include <intrin.h> /* fence functions _ReadBarrier, also bit search functions _BitScanReverse */
#endif

#ifndef ECB_MEMORY_FENCE
  #if ECB_GCC_VERSION(2,5) || defined __INTEL_COMPILER || (__llvm__ && __GNUC__) || __SUNPRO_C >= 0x5110 || __SUNPRO_CC >= 0x5110
    #define ECB_MEMORY_FENCE_RELAXED __asm__ __volatile__ ("" : : : "memory")
    #if __i386 || __i386__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("lock; orb $0, -1(%%esp)" : : : "memory")
      #define ECB_MEMORY_FENCE_ACQUIRE __asm__ __volatile__ (""                        : : : "memory")
      #define ECB_MEMORY_FENCE_RELEASE __asm__ __volatile__ (""                        : : : "memory")
    #elif ECB_GCC_AMD64
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("mfence"   : : : "memory")
      #define ECB_MEMORY_FENCE_ACQUIRE __asm__ __volatile__ (""         : : : "memory")
      #define ECB_MEMORY_FENCE_RELEASE __asm__ __volatile__ (""         : : : "memory")
    #elif __powerpc__ || __ppc__ || __powerpc64__ || __ppc64__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("sync"     : : : "memory")
    #elif defined __ARM_ARCH_2__ \
      || defined __ARM_ARCH_3__  || defined __ARM_ARCH_3M__  \
      || defined __ARM_ARCH_4__  || defined __ARM_ARCH_4T__  \
      || defined __ARM_ARCH_5__  || defined __ARM_ARCH_5E__  \
      || defined __ARM_ARCH_5T__ || defined __ARM_ARCH_5TE__ \
      || defined __ARM_ARCH_5TEJ__
      /* should not need any, unless running old code on newer cpu - arm doesn't support that */
    #elif defined __ARM_ARCH_6__  || defined __ARM_ARCH_6J__  \
       || defined __ARM_ARCH_6K__ || defined __ARM_ARCH_6ZK__ \
       || defined __ARM_ARCH_6T2__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("mcr p15,0,%0,c7,c10,5" : : "r" (0) : "memory")
    #elif defined __ARM_ARCH_7__  || defined __ARM_ARCH_7A__  \
       || defined __ARM_ARCH_7R__ || defined __ARM_ARCH_7M__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("dmb"      : : : "memory")
    #elif __aarch64__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("dmb ish"  : : : "memory")
    #elif (__sparc || __sparc__) && !(__sparc_v8__ || defined __sparcv8)
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("membar #LoadStore | #LoadLoad | #StoreStore | #StoreLoad" : : : "memory")
      #define ECB_MEMORY_FENCE_ACQUIRE __asm__ __volatile__ ("membar #LoadStore | #LoadLoad"                            : : : "memory")
      #define ECB_MEMORY_FENCE_RELEASE __asm__ __volatile__ ("membar #LoadStore             | #StoreStore")
    #elif defined __s390__ || defined __s390x__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("bcr 15,0" : : : "memory")
    #elif defined __mips__
      /* GNU/Linux emulates sync on mips1 architectures, so we force its use */
      /* anybody else who still uses mips1 is supposed to send in their version, with detection code. */
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ (".set mips2; sync; .set mips0" : : : "memory")
    #elif defined __alpha__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("mb"       : : : "memory")
    #elif defined __hppa__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ (""         : : : "memory")
      #define ECB_MEMORY_FENCE_RELEASE __asm__ __volatile__ ("")
    #elif defined __ia64__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("mf"       : : : "memory")
    #elif defined __m68k__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ (""         : : : "memory")
    #elif defined __m88k__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ ("tb1 0,%%r0,128" : : : "memory")
    #elif defined __sh__
      #define ECB_MEMORY_FENCE         __asm__ __volatile__ (""         : : : "memory")
    #endif
  #endif
#endif

#ifndef ECB_MEMORY_FENCE
  #if ECB_GCC_VERSION(4,7)
    /* see comment below (stdatomic.h) about the C11 memory model. */
    #define ECB_MEMORY_FENCE         __atomic_thread_fence (__ATOMIC_SEQ_CST)
    #define ECB_MEMORY_FENCE_ACQUIRE __atomic_thread_fence (__ATOMIC_ACQUIRE)
    #define ECB_MEMORY_FENCE_RELEASE __atomic_thread_fence (__ATOMIC_RELEASE)
    #define ECB_MEMORY_FENCE_RELAXED __atomic_thread_fence (__ATOMIC_RELAXED)

  #elif ECB_CLANG_EXTENSION(c_atomic)
    /* see comment below (stdatomic.h) about the C11 memory model. */
    #define ECB_MEMORY_FENCE         __c11_atomic_thread_fence (__ATOMIC_SEQ_CST)
    #define ECB_MEMORY_FENCE_ACQUIRE __c11_atomic_thread_fence (__ATOMIC_ACQUIRE)
    #define ECB_MEMORY_FENCE_RELEASE __c11_atomic_thread_fence (__ATOMIC_RELEASE)
    #define ECB_MEMORY_FENCE_RELAXED __c11_atomic_thread_fence (__ATOMIC_RELAXED)

  #elif ECB_GCC_VERSION(4,4) || defined __INTEL_COMPILER || defined __clang__
    #define ECB_MEMORY_FENCE         __sync_synchronize ()
  #elif _MSC_VER >= 1500 /* VC++ 2008 */
    /* apparently, microsoft broke all the memory barrier stuff in Visual Studio 2008... */
    #pragma intrinsic(_ReadBarrier,_WriteBarrier,_ReadWriteBarrier)
    #define ECB_MEMORY_FENCE         _ReadWriteBarrier (); MemoryBarrier()
    #define ECB_MEMORY_FENCE_ACQUIRE _ReadWriteBarrier (); MemoryBarrier() /* according to msdn, _ReadBarrier is not a load fence */
    #define ECB_MEMORY_FENCE_RELEASE _WriteBarrier (); MemoryBarrier()
  #elif _MSC_VER >= 1400 /* VC++ 2005 */
    #pragma intrinsic(_ReadBarrier,_WriteBarrier,_ReadWriteBarrier)
    #define ECB_MEMORY_FENCE         _ReadWriteBarrier ()
    #define ECB_MEMORY_FENCE_ACQUIRE _ReadWriteBarrier () /* according to msdn, _ReadBarrier is not a load fence */
    #define ECB_MEMORY_FENCE_RELEASE _WriteBarrier ()
  #elif defined _WIN32
    #include <WinNT.h>
    #define ECB_MEMORY_FENCE         MemoryBarrier () /* actually just xchg on x86... scary */
  #elif __SUNPRO_C >= 0x5110 || __SUNPRO_CC >= 0x5110
    #include <mbarrier.h>
    #define ECB_MEMORY_FENCE         __machine_rw_barrier  ()
    #define ECB_MEMORY_FENCE_ACQUIRE __machine_acq_barrier ()
    #define ECB_MEMORY_FENCE_RELEASE __machine_rel_barrier ()
    #define ECB_MEMORY_FENCE_RELAXED __compiler_barrier ()
  #elif __xlC__
    #define ECB_MEMORY_FENCE         __sync ()
  #endif
#endif

#ifndef ECB_MEMORY_FENCE
  #if ECB_C11 && !defined __STDC_NO_ATOMICS__
    /* we assume that these memory fences work on all variables/all memory accesses, */
    /* not just C11 atomics and atomic accesses */
    #include <stdatomic.h>
    #define ECB_MEMORY_FENCE         atomic_thread_fence (memory_order_seq_cst)
    #define ECB_MEMORY_FENCE_ACQUIRE atomic_thread_fence (memory_order_acquire)
    #define ECB_MEMORY_FENCE_RELEASE atomic_thread_fence (memory_order_release)
  #endif
#endif

#ifndef ECB_MEMORY_FENCE
  #if !ECB_AVOID_PTHREADS
    /*
     * if you get undefined symbol references to pthread_mutex_lock,
     * or failure to find pthread.h, then you should implement
     * the ECB_MEMORY_FENCE operations for your cpu/compiler
     * OR provide pthread.h and link against the posix thread library
     * of your system.
     */
    #include <pthread.h>
    #define ECB_NEEDS_PTHREADS 1
    #define ECB_MEMORY_FENCE_NEEDS_PTHREADS 1

    static pthread_mutex_t ecb_mf_lock = PTHREAD_MUTEX_INITIALIZER;
    #define ECB_MEMORY_FENCE do { pthread_mutex_lock (&ecb_mf_lock); pthread_mutex_unlock (&ecb_mf_lock); } while (0)
  #endif
#endif

#if !defined ECB_MEMORY_FENCE_ACQUIRE && defined ECB_MEMORY_FENCE
  #define ECB_MEMORY_FENCE_ACQUIRE ECB_MEMORY_FENCE
#endif

#if !defined ECB_MEMORY_FENCE_RELEASE && defined ECB_MEMORY_FENCE
  #define ECB_MEMORY_FENCE_RELEASE ECB_MEMORY_FENCE
#endif

#if !defined ECB_MEMORY_FENCE_RELAXED && defined ECB_MEMORY_FENCE
  #define ECB_MEMORY_FENCE_RELAXED ECB_MEMORY_FENCE /* very heavy-handed */
#endif

/*****************************************************************************/

#if ECB_CPP
  #define ecb_inline static inline
#elif ECB_GCC_VERSION(2,5)
  #define ecb_inline static __inline__
#elif ECB_C99
  #define ecb_inline static inline
#else
  #define ecb_inline static
#endif

#if ECB_GCC_VERSION(3,3)
  #define ecb_restrict __restrict__
#elif ECB_C99
  #define ecb_restrict restrict
#else
  #define ecb_restrict
#endif

typedef int ecb_bool;

#define ECB_CONCAT_(a, b) a ## b
#define ECB_CONCAT(a, b) ECB_CONCAT_(a, b)
#define ECB_STRINGIFY_(a) # a
#define ECB_STRINGIFY(a) ECB_STRINGIFY_(a)
#define ECB_STRINGIFY_EXPR(expr) ((expr), ECB_STRINGIFY_ (expr))

#define ecb_function_ ecb_inline

#if ECB_GCC_VERSION(3,1) || ECB_CLANG_VERSION(2,8)
  #define ecb_attribute(attrlist)        __attribute__ (attrlist)
#else
  #define ecb_attribute(attrlist)
#endif

#if ECB_GCC_VERSION(3,1) || ECB_CLANG_BUILTIN(__builtin_constant_p)
  #define ecb_is_constant(expr)          __builtin_constant_p (expr)
#else
  /* possible C11 impl for integral types
  typedef struct ecb_is_constant_struct ecb_is_constant_struct;
  #define ecb_is_constant(expr)          _Generic ((1 ? (struct ecb_is_constant_struct *)0 : (void *)((expr) - (expr)), ecb_is_constant_struct *: 0, default: 1)) */

  #define ecb_is_constant(expr)          0
#endif

#if ECB_GCC_VERSION(3,1) || ECB_CLANG_BUILTIN(__builtin_expect)
  #define ecb_expect(expr,value)         __builtin_expect ((expr),(value))
#else
  #define ecb_expect(expr,value)         (expr)
#endif

#if ECB_GCC_VERSION(3,1) || ECB_CLANG_BUILTIN(__builtin_prefetch)
  #define ecb_prefetch(addr,rw,locality) __builtin_prefetch (addr, rw, locality)
#else
  #define ecb_prefetch(addr,rw,locality)
#endif

/* no emulation for ecb_decltype */
#if ECB_CPP11
  // older implementations might have problems with decltype(x)::type, work around it
  template<class T> struct ecb_decltype_t { typedef T type; };
  #define ecb_decltype(x) ecb_decltype_t<decltype (x)>::type
#elif ECB_GCC_VERSION(3,0) || ECB_CLANG_VERSION(2,8)
  #define ecb_decltype(x) __typeof__ (x)
#endif

#if _MSC_VER >= 1300
  #define ecb_deprecated __declspec (deprecated)
#else
  #define ecb_deprecated ecb_attribute ((__deprecated__))
#endif

#if _MSC_VER >= 1500
  #define ecb_deprecated_message(msg) __declspec (deprecated (msg))
#elif ECB_GCC_VERSION(4,5)
  #define ecb_deprecated_message(msg) ecb_attribute ((__deprecated__ (msg))
#else
  #define ecb_deprecated_message(msg) ecb_deprecated
#endif

#if _MSC_VER >= 1400
  #define ecb_noinline __declspec (noinline)
#else
  #define ecb_noinline ecb_attribute ((__noinline__))
#endif

#define ecb_unused     ecb_attribute ((__unused__))
#define ecb_const      ecb_attribute ((__const__))
#define ecb_pure       ecb_attribute ((__pure__))

#if ECB_C11 || __IBMC_NORETURN
  /* http://www-01.ibm.com/support/knowledgecenter/SSGH3R_13.1.0/com.ibm.xlcpp131.aix.doc/language_ref/noreturn.html */
  #define ecb_noreturn   _Noreturn
#elif ECB_CPP11
  #define ecb_noreturn   [[noreturn]]
#elif _MSC_VER >= 1200
  /* http://msdn.microsoft.com/en-us/library/k6ktzx3s.aspx */
  #define ecb_noreturn   __declspec (noreturn)
#else
  #define ecb_noreturn   ecb_attribute ((__noreturn__))
#endif

#if ECB_GCC_VERSION(4,3)
  #define ecb_artificial ecb_attribute ((__artificial__))
  #define ecb_hot        ecb_attribute ((__hot__))
  #define ecb_cold       ecb_attribute ((__cold__))
#else
  #define ecb_artificial
  #define ecb_hot
  #define ecb_cold
#endif

/* put around conditional expressions if you are very sure that the  */
/* expression is mostly true or mostly false. note that these return */
/* booleans, not the expression.                                     */
#define ecb_expect_false(expr) ecb_expect (!!(expr), 0)
#define ecb_expect_true(expr)  ecb_expect (!!(expr), 1)
/* for compatibility to the rest of the world */
#define ecb_likely(expr)   ecb_expect_true  (expr)
#define ecb_unlikely(expr) ecb_expect_false (expr)

/* count trailing zero bits and count # of one bits */
#if ECB_GCC_VERSION(3,4) \
    || (ECB_CLANG_BUILTIN(__builtin_clz) && ECB_CLANG_BUILTIN(__builtin_clzll) \
        && ECB_CLANG_BUILTIN(__builtin_ctz) && ECB_CLANG_BUILTIN(__builtin_ctzll) \
        && ECB_CLANG_BUILTIN(__builtin_popcount))
  /* we assume int == 32 bit, long == 32 or 64 bit and long long == 64 bit */
  #define ecb_ld32(x)      (__builtin_clz      (x) ^ 31)
  #define ecb_ld64(x)      (__builtin_clzll    (x) ^ 63)
  #define ecb_ctz32(x)      __builtin_ctz      (x)
  #define ecb_ctz64(x)      __builtin_ctzll    (x)
  #define ecb_popcount32(x) __builtin_popcount (x)
  /* no popcountll */
#else
  ecb_function_ ecb_const int ecb_ctz32 (uint32_t x);
  ecb_function_ ecb_const int
  ecb_ctz32 (uint32_t x)
  {
#if 1400 <= _MSC_VER && (_M_IX86 || _M_X64 || _M_IA64 || _M_ARM)
    unsigned long r;
    _BitScanForward (&r, x);
    return (int)r;
#else
    int r = 0;

    x &= ~x + 1; /* this isolates the lowest bit */

#if ECB_branchless_on_i386
    r += !!(x & 0xaaaaaaaa) << 0;
    r += !!(x & 0xcccccccc) << 1;
    r += !!(x & 0xf0f0f0f0) << 2;
    r += !!(x & 0xff00ff00) << 3;
    r += !!(x & 0xffff0000) << 4;
#else
    if (x & 0xaaaaaaaa) r +=  1;
    if (x & 0xcccccccc) r +=  2;
    if (x & 0xf0f0f0f0) r +=  4;
    if (x & 0xff00ff00) r +=  8;
    if (x & 0xffff0000) r += 16;
#endif

    return r;
#endif
  }

  ecb_function_ ecb_const int ecb_ctz64 (uint64_t x);
  ecb_function_ ecb_const int
  ecb_ctz64 (uint64_t x)
  {
#if 1400 <= _MSC_VER && (_M_X64 || _M_IA64 || _M_ARM)
    unsigned long r;
    _BitScanForward64 (&r, x);
    return (int)r;
#else
    int shift = x & 0xffffffff ? 0 : 32;
    return ecb_ctz32 (x >> shift) + shift;
#endif
  }

  ecb_function_ ecb_const int ecb_popcount32 (uint32_t x);
  ecb_function_ ecb_const int
  ecb_popcount32 (uint32_t x)
  {
    x -=  (x >> 1) & 0x55555555;
    x  = ((x >> 2) & 0x33333333) + (x & 0x33333333);
    x  = ((x >> 4) + x) & 0x0f0f0f0f;
    x *= 0x01010101;

    return x >> 24;
  }

  ecb_function_ ecb_const int ecb_ld32 (uint32_t x);
  ecb_function_ ecb_const int ecb_ld32 (uint32_t x)
  {
#if 1400 <= _MSC_VER && (_M_IX86 || _M_X64 || _M_IA64 || _M_ARM)
    unsigned long r;
    _BitScanReverse (&r, x);
    return (int)r;
#else
    int r = 0;

    if (x >> 16) { x >>= 16; r += 16; }
    if (x >>  8) { x >>=  8; r +=  8; }
    if (x >>  4) { x >>=  4; r +=  4; }
    if (x >>  2) { x >>=  2; r +=  2; }
    if (x >>  1) {           r +=  1; }

    return r;
#endif
  }

  ecb_function_ ecb_const int ecb_ld64 (uint64_t x);
  ecb_function_ ecb_const int ecb_ld64 (uint64_t x)
  {
#if 1400 <= _MSC_VER && (_M_X64 || _M_IA64 || _M_ARM)
    unsigned long r;
    _BitScanReverse64 (&r, x);
    return (int)r;
#else
    int r = 0;

    if (x >> 32) { x >>= 32; r += 32; }

    return r + ecb_ld32 (x);
#endif
  }
#endif

ecb_function_ ecb_const ecb_bool ecb_is_pot32 (uint32_t x);
ecb_function_ ecb_const ecb_bool ecb_is_pot32 (uint32_t x) { return !(x & (x - 1)); }
ecb_function_ ecb_const ecb_bool ecb_is_pot64 (uint64_t x);
ecb_function_ ecb_const ecb_bool ecb_is_pot64 (uint64_t x) { return !(x & (x - 1)); }

ecb_function_ ecb_const uint8_t  ecb_bitrev8  (uint8_t  x);
ecb_function_ ecb_const uint8_t  ecb_bitrev8  (uint8_t  x)
{
  return (  (x * 0x0802U & 0x22110U)
          | (x * 0x8020U & 0x88440U)) * 0x10101U >> 16;
}

ecb_function_ ecb_const uint16_t ecb_bitrev16 (uint16_t x);
ecb_function_ ecb_const uint16_t ecb_bitrev16 (uint16_t x)
{
  x = ((x >>  1) &     0x5555) | ((x &     0x5555) <<  1);
  x = ((x >>  2) &     0x3333) | ((x &     0x3333) <<  2);
  x = ((x >>  4) &     0x0f0f) | ((x &     0x0f0f) <<  4);
  x = ( x >>  8              ) | ( x               <<  8);

  return x;
}

ecb_function_ ecb_const uint32_t ecb_bitrev32 (uint32_t x);
ecb_function_ ecb_const uint32_t ecb_bitrev32 (uint32_t x)
{
  x = ((x >>  1) & 0x55555555) | ((x & 0x55555555) <<  1);
  x = ((x >>  2) & 0x33333333) | ((x & 0x33333333) <<  2);
  x = ((x >>  4) & 0x0f0f0f0f) | ((x & 0x0f0f0f0f) <<  4);
  x = ((x >>  8) & 0x00ff00ff) | ((x & 0x00ff00ff) <<  8);
  x = ( x >> 16              ) | ( x               << 16);

  return x;
}

/* popcount64 is only available on 64 bit cpus as gcc builtin */
/* so for this version we are lazy */
ecb_function_ ecb_const int ecb_popcount64 (uint64_t x);
ecb_function_ ecb_const int
ecb_popcount64 (uint64_t x)
{
  return ecb_popcount32 (x) + ecb_popcount32 (x >> 32);
}

ecb_inline ecb_const uint8_t  ecb_rotl8  (uint8_t  x, unsigned int count);
ecb_inline ecb_const uint8_t  ecb_rotr8  (uint8_t  x, unsigned int count);
ecb_inline ecb_const uint16_t ecb_rotl16 (uint16_t x, unsigned int count);
ecb_inline ecb_const uint16_t ecb_rotr16 (uint16_t x, unsigned int count);
ecb_inline ecb_const uint32_t ecb_rotl32 (uint32_t x, unsigned int count);
ecb_inline ecb_const uint32_t ecb_rotr32 (uint32_t x, unsigned int count);
ecb_inline ecb_const uint64_t ecb_rotl64 (uint64_t x, unsigned int count);
ecb_inline ecb_const uint64_t ecb_rotr64 (uint64_t x, unsigned int count);

ecb_inline ecb_const uint8_t  ecb_rotl8  (uint8_t  x, unsigned int count) { return (x >> ( 8 - count)) | (x << count); }
ecb_inline ecb_const uint8_t  ecb_rotr8  (uint8_t  x, unsigned int count) { return (x << ( 8 - count)) | (x >> count); }
ecb_inline ecb_const uint16_t ecb_rotl16 (uint16_t x, unsigned int count) { return (x >> (16 - count)) | (x << count); }
ecb_inline ecb_const uint16_t ecb_rotr16 (uint16_t x, unsigned int count) { return (x << (16 - count)) | (x >> count); }
ecb_inline ecb_const uint32_t ecb_rotl32 (uint32_t x, unsigned int count) { return (x >> (32 - count)) | (x << count); }
ecb_inline ecb_const uint32_t ecb_rotr32 (uint32_t x, unsigned int count) { return (x << (32 - count)) | (x >> count); }
ecb_inline ecb_const uint64_t ecb_rotl64 (uint64_t x, unsigned int count) { return (x >> (64 - count)) | (x << count); }
ecb_inline ecb_const uint64_t ecb_rotr64 (uint64_t x, unsigned int count) { return (x << (64 - count)) | (x >> count); }

#if ECB_CPP

inline uint8_t  ecb_ctz (uint8_t  v) { return ecb_ctz32 (v); }
inline uint16_t ecb_ctz (uint16_t v) { return ecb_ctz32 (v); }
inline uint32_t ecb_ctz (uint32_t v) { return ecb_ctz32 (v); }
inline uint64_t ecb_ctz (uint64_t v) { return ecb_ctz64 (v); }

inline bool ecb_is_pot (uint8_t  v) { return ecb_is_pot32 (v); }
inline bool ecb_is_pot (uint16_t v) { return ecb_is_pot32 (v); }
inline bool ecb_is_pot (uint32_t v) { return ecb_is_pot32 (v); }
inline bool ecb_is_pot (uint64_t v) { return ecb_is_pot64 (v); }

inline int ecb_ld (uint8_t  v) { return ecb_ld32 (v); }
inline int ecb_ld (uint16_t v) { return ecb_ld32 (v); }
inline int ecb_ld (uint32_t v) { return ecb_ld32 (v); }
inline int ecb_ld (uint64_t v) { return ecb_ld64 (v); }

inline int ecb_popcount (uint8_t  v) { return ecb_popcount32 (v); }
inline int ecb_popcount (uint16_t v) { return ecb_popcount32 (v); }
inline int ecb_popcount (uint32_t v) { return ecb_popcount32 (v); }
inline int ecb_popcount (uint64_t v) { return ecb_popcount64 (v); }

inline uint8_t  ecb_bitrev (uint8_t  v) { return ecb_bitrev8  (v); }
inline uint16_t ecb_bitrev (uint16_t v) { return ecb_bitrev16 (v); }
inline uint32_t ecb_bitrev (uint32_t v) { return ecb_bitrev32 (v); }

inline uint8_t  ecb_rotl (uint8_t  v, unsigned int count) { return ecb_rotl8  (v, count); }
inline uint16_t ecb_rotl (uint16_t v, unsigned int count) { return ecb_rotl16 (v, count); }
inline uint32_t ecb_rotl (uint32_t v, unsigned int count) { return ecb_rotl32 (v, count); }
inline uint64_t ecb_rotl (uint64_t v, unsigned int count) { return ecb_rotl64 (v, count); }

inline uint8_t  ecb_rotr (uint8_t  v, unsigned int count) { return ecb_rotr8  (v, count); }
inline uint16_t ecb_rotr (uint16_t v, unsigned int count) { return ecb_rotr16 (v, count); }
inline uint32_t ecb_rotr (uint32_t v, unsigned int count) { return ecb_rotr32 (v, count); }
inline uint64_t ecb_rotr (uint64_t v, unsigned int count) { return ecb_rotr64 (v, count); }

#endif

#if ECB_GCC_VERSION(4,3) || (ECB_CLANG_BUILTIN(__builtin_bswap32) && ECB_CLANG_BUILTIN(__builtin_bswap64))
  #if ECB_GCC_VERSION(4,8) || ECB_CLANG_BUILTIN(__builtin_bswap16)
  #define ecb_bswap16(x)  __builtin_bswap16 (x)
  #else
  #define ecb_bswap16(x) (__builtin_bswap32 (x) >> 16)
  #endif
  #define ecb_bswap32(x)  __builtin_bswap32 (x)
  #define ecb_bswap64(x)  __builtin_bswap64 (x)
#elif _MSC_VER
  #include <stdlib.h>
  #define ecb_bswap16(x) ((uint16_t)_byteswap_ushort ((uint16_t)(x)))
  #define ecb_bswap32(x) ((uint32_t)_byteswap_ulong  ((uint32_t)(x)))
  #define ecb_bswap64(x) ((uint64_t)_byteswap_uint64 ((uint64_t)(x)))
#else
  ecb_function_ ecb_const uint16_t ecb_bswap16 (uint16_t x);
  ecb_function_ ecb_const uint16_t
  ecb_bswap16 (uint16_t x)
  {
    return ecb_rotl16 (x, 8);
  }

  ecb_function_ ecb_const uint32_t ecb_bswap32 (uint32_t x);
  ecb_function_ ecb_const uint32_t
  ecb_bswap32 (uint32_t x)
  {
    return (((uint32_t)ecb_bswap16 (x)) << 16) | ecb_bswap16 (x >> 16);
  }

  ecb_function_ ecb_const uint64_t ecb_bswap64 (uint64_t x);
  ecb_function_ ecb_const uint64_t
  ecb_bswap64 (uint64_t x)
  {
    return (((uint64_t)ecb_bswap32 (x)) << 32) | ecb_bswap32 (x >> 32);
  }
#endif

#if ECB_GCC_VERSION(4,5) || ECB_CLANG_BUILTIN(__builtin_unreachable)
  #define ecb_unreachable() __builtin_unreachable ()
#else
  /* this seems to work fine, but gcc always emits a warning for it :/ */
  ecb_inline ecb_noreturn void ecb_unreachable (void);
  ecb_inline ecb_noreturn void ecb_unreachable (void) { }
#endif

/* try to tell the compiler that some condition is definitely true */
#define ecb_assume(cond) if (!(cond)) ecb_unreachable (); else 0

ecb_inline ecb_const uint32_t ecb_byteorder_helper (void);
ecb_inline ecb_const uint32_t
ecb_byteorder_helper (void)
{
  /* the union code still generates code under pressure in gcc, */
  /* but less than using pointers, and always seems to */
  /* successfully return a constant. */
  /* the reason why we have this horrible preprocessor mess */
  /* is to avoid it in all cases, at least on common architectures */
  /* or when using a recent enough gcc version (>= 4.6) */
#if (defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) \
    || ((__i386 || __i386__ || _M_IX86 || ECB_GCC_AMD64 || ECB_MSVC_AMD64) && !__VOS__)
  #define ECB_LITTLE_ENDIAN 1
  return 0x44332211;
#elif (defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) \
      || ((__AARCH64EB__ || __MIPSEB__ || __ARMEB__) && !__VOS__)
  #define ECB_BIG_ENDIAN 1
  return 0x11223344;
#else
  union
  {
    uint8_t c[4];
    uint32_t u;
  } u = { 0x11, 0x22, 0x33, 0x44 };
  return u.u;
#endif
}

ecb_inline ecb_const ecb_bool ecb_big_endian    (void);
ecb_inline ecb_const ecb_bool ecb_big_endian    (void) { return ecb_byteorder_helper () == 0x11223344; }
ecb_inline ecb_const ecb_bool ecb_little_endian (void);
ecb_inline ecb_const ecb_bool ecb_little_endian (void) { return ecb_byteorder_helper () == 0x44332211; }

/*****************************************************************************/
/* unaligned load/store */

ecb_inline uint_fast16_t ecb_be_u16_to_host (uint_fast16_t v) { return ecb_little_endian () ? ecb_bswap16 (v) : v; }
ecb_inline uint_fast32_t ecb_be_u32_to_host (uint_fast32_t v) { return ecb_little_endian () ? ecb_bswap32 (v) : v; }
ecb_inline uint_fast64_t ecb_be_u64_to_host (uint_fast64_t v) { return ecb_little_endian () ? ecb_bswap64 (v) : v; }

ecb_inline uint_fast16_t ecb_le_u16_to_host (uint_fast16_t v) { return ecb_big_endian    () ? ecb_bswap16 (v) : v; }
ecb_inline uint_fast32_t ecb_le_u32_to_host (uint_fast32_t v) { return ecb_big_endian    () ? ecb_bswap32 (v) : v; }
ecb_inline uint_fast64_t ecb_le_u64_to_host (uint_fast64_t v) { return ecb_big_endian    () ? ecb_bswap64 (v) : v; }

ecb_inline uint_fast16_t ecb_peek_u16_u (const void *ptr) { uint16_t v; memcpy (&v, ptr, sizeof (v)); return v; }
ecb_inline uint_fast32_t ecb_peek_u32_u (const void *ptr) { uint32_t v; memcpy (&v, ptr, sizeof (v)); return v; }
ecb_inline uint_fast64_t ecb_peek_u64_u (const void *ptr) { uint64_t v; memcpy (&v, ptr, sizeof (v)); return v; }

ecb_inline uint_fast16_t ecb_peek_be_u16_u (const void *ptr) { return ecb_be_u16_to_host (ecb_peek_u16_u (ptr)); }
ecb_inline uint_fast32_t ecb_peek_be_u32_u (const void *ptr) { return ecb_be_u32_to_host (ecb_peek_u32_u (ptr)); }
ecb_inline uint_fast64_t ecb_peek_be_u64_u (const void *ptr) { return ecb_be_u64_to_host (ecb_peek_u64_u (ptr)); }

ecb_inline uint_fast16_t ecb_peek_le_u16_u (const void *ptr) { return ecb_le_u16_to_host (ecb_peek_u16_u (ptr)); }
ecb_inline uint_fast32_t ecb_peek_le_u32_u (const void *ptr) { return ecb_le_u32_to_host (ecb_peek_u32_u (ptr)); }
ecb_inline uint_fast64_t ecb_peek_le_u64_u (const void *ptr) { return ecb_le_u64_to_host (ecb_peek_u64_u (ptr)); }

ecb_inline uint_fast16_t ecb_host_to_be_u16 (uint_fast16_t v) { return ecb_little_endian () ? ecb_bswap16 (v) : v; }
ecb_inline uint_fast32_t ecb_host_to_be_u32 (uint_fast32_t v) { return ecb_little_endian () ? ecb_bswap32 (v) : v; }
ecb_inline uint_fast64_t ecb_host_to_be_u64 (uint_fast64_t v) { return ecb_little_endian () ? ecb_bswap64 (v) : v; }

ecb_inline uint_fast16_t ecb_host_to_le_u16 (uint_fast16_t v) { return ecb_big_endian    () ? ecb_bswap16 (v) : v; }
ecb_inline uint_fast32_t ecb_host_to_le_u32 (uint_fast32_t v) { return ecb_big_endian    () ? ecb_bswap32 (v) : v; }
ecb_inline uint_fast64_t ecb_host_to_le_u64 (uint_fast64_t v) { return ecb_big_endian    () ? ecb_bswap64 (v) : v; }

ecb_inline void ecb_poke_u16_u (void *ptr, uint16_t v) { memcpy (ptr, &v, sizeof (v)); }
ecb_inline void ecb_poke_u32_u (void *ptr, uint32_t v) { memcpy (ptr, &v, sizeof (v)); }
ecb_inline void ecb_poke_u64_u (void *ptr, uint64_t v) { memcpy (ptr, &v, sizeof (v)); }

ecb_inline void ecb_poke_be_u16_u (void *ptr, uint_fast16_t v) { ecb_poke_u16_u (ptr, ecb_host_to_be_u16 (v)); }
ecb_inline void ecb_poke_be_u32_u (void *ptr, uint_fast32_t v) { ecb_poke_u32_u (ptr, ecb_host_to_be_u32 (v)); }
ecb_inline void ecb_poke_be_u64_u (void *ptr, uint_fast64_t v) { ecb_poke_u64_u (ptr, ecb_host_to_be_u64 (v)); }
                                                                                                
ecb_inline void ecb_poke_le_u16_u (void *ptr, uint_fast16_t v) { ecb_poke_u16_u (ptr, ecb_host_to_le_u16 (v)); }
ecb_inline void ecb_poke_le_u32_u (void *ptr, uint_fast32_t v) { ecb_poke_u32_u (ptr, ecb_host_to_le_u32 (v)); }
ecb_inline void ecb_poke_le_u64_u (void *ptr, uint_fast64_t v) { ecb_poke_u64_u (ptr, ecb_host_to_le_u64 (v)); }

#if ECB_CPP

inline uint8_t  ecb_bswap (uint8_t  v) { return v; }
inline uint16_t ecb_bswap (uint16_t v) { return ecb_bswap16 (v); }
inline uint32_t ecb_bswap (uint32_t v) { return ecb_bswap32 (v); }
inline uint64_t ecb_bswap (uint64_t v) { return ecb_bswap64 (v); }

template<typename T> inline T ecb_be_to_host (T v) { return ecb_little_endian () ? ecb_bswap (v) : v; }
template<typename T> inline T ecb_le_to_host (T v) { return ecb_big_endian    () ? ecb_bswap (v) : v; }
template<typename T> inline T ecb_peek       (const void *ptr) { return *(const T *)ptr; }
template<typename T> inline T ecb_peek_be    (const void *ptr) { return ecb_be_to_host (ecb_peek  <T> (ptr)); }
template<typename T> inline T ecb_peek_le    (const void *ptr) { return ecb_le_to_host (ecb_peek  <T> (ptr)); }
template<typename T> inline T ecb_peek_u     (const void *ptr) { T v; memcpy (&v, ptr, sizeof (v)); return v; }
template<typename T> inline T ecb_peek_be_u  (const void *ptr) { return ecb_be_to_host (ecb_peek_u<T> (ptr)); }
template<typename T> inline T ecb_peek_le_u  (const void *ptr) { return ecb_le_to_host (ecb_peek_u<T> (ptr)); }

template<typename T> inline T ecb_host_to_be (T v) { return ecb_little_endian () ? ecb_bswap (v) : v; }
template<typename T> inline T ecb_host_to_le (T v) { return ecb_big_endian    () ? ecb_bswap (v) : v; }
template<typename T> inline void ecb_poke      (void *ptr, T v) { *(T *)ptr = v; }
template<typename T> inline void ecb_poke_be   (void *ptr, T v) { return ecb_poke  <T> (ptr, ecb_host_to_be (v)); }
template<typename T> inline void ecb_poke_le   (void *ptr, T v) { return ecb_poke  <T> (ptr, ecb_host_to_le (v)); }
template<typename T> inline void ecb_poke_u    (void *ptr, T v) { memcpy (ptr, &v, sizeof (v)); }
template<typename T> inline void ecb_poke_be_u (void *ptr, T v) { return ecb_poke_u<T> (ptr, ecb_host_to_be (v)); }
template<typename T> inline void ecb_poke_le_u (void *ptr, T v) { return ecb_poke_u<T> (ptr, ecb_host_to_le (v)); }

#endif

/*****************************************************************************/

#if ECB_GCC_VERSION(3,0) || ECB_C99
  #define ecb_mod(m,n) ((m) % (n) + ((m) % (n) < 0 ? (n) : 0))
#else
  #define ecb_mod(m,n) ((m) < 0 ? ((n) - 1 - ((-1 - (m)) % (n))) : ((m) % (n)))
#endif

#if ECB_CPP
  template<typename T>
  static inline T ecb_div_rd (T val, T div)
  {
    return val < 0 ? - ((-val + div - 1) / div) : (val          ) / div;
  }
  template<typename T>
  static inline T ecb_div_ru (T val, T div)
  {
    return val < 0 ? - ((-val          ) / div) : (val + div - 1) / div;
  }
#else
  #define ecb_div_rd(val,div) ((val) < 0 ? - ((-(val) + (div) - 1) / (div)) : ((val)            ) / (div))
  #define ecb_div_ru(val,div) ((val) < 0 ? - ((-(val)            ) / (div)) : ((val) + (div) - 1) / (div))
#endif

#if ecb_cplusplus_does_not_suck
  /* does not work for local types (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2657.htm) */
  template<typename T, int N>
  static inline int ecb_array_length (const T (&arr)[N])
  {
    return N;
  }
#else
  #define ecb_array_length(name) (sizeof (name) / sizeof (name [0]))
#endif

/*****************************************************************************/

ecb_function_ ecb_const uint32_t ecb_binary16_to_binary32 (uint32_t x);
ecb_function_ ecb_const uint32_t
ecb_binary16_to_binary32 (uint32_t x)
{
  unsigned int s = (x & 0x8000) << (31 - 15);
  int          e = (x >> 10) & 0x001f;
  unsigned int m =  x        & 0x03ff;

  if (ecb_expect_false (e == 31))
    /* infinity or NaN */
    e = 255 - (127 - 15);
  else if (ecb_expect_false (!e))
    {
      if (ecb_expect_true (!m))
        /* zero, handled by code below by forcing e to 0 */
        e = 0 - (127 - 15);
      else
        {
          /* subnormal, renormalise */
          unsigned int s = 10 - ecb_ld32 (m);

          m = (m << s) & 0x3ff; /* mask implicit bit */
          e -= s - 1;
        }
    }

  /* e and m now are normalised, or zero, (or inf or nan) */
  e += 127 - 15;

  return s | (e << 23) | (m << (23 - 10));
}

ecb_function_ ecb_const uint16_t ecb_binary32_to_binary16 (uint32_t x);
ecb_function_ ecb_const uint16_t
ecb_binary32_to_binary16 (uint32_t x)
{
  unsigned int s =  (x >> 16) & 0x00008000; /* sign bit, the easy part */
  unsigned int e = ((x >> 23) & 0x000000ff) - (127 - 15); /* the desired exponent */
  unsigned int m =   x        & 0x007fffff;

  x &= 0x7fffffff;

  /* if it's within range of binary16 normals, use fast path */
  if (ecb_expect_true (0x38800000 <= x && x <= 0x477fefff))
    {
      /* mantissa round-to-even */
      m += 0x00000fff + ((m >> (23 - 10)) & 1);

      /* handle overflow */
      if (ecb_expect_false (m >= 0x00800000))
        {
          m >>= 1;
          e +=  1;
        }

      return s | (e << 10) | (m >> (23 - 10));
    }

  /* handle large numbers and infinity */
  if (ecb_expect_true (0x477fefff < x && x <= 0x7f800000))
    return s | 0x7c00;

  /* handle zero, subnormals and small numbers */
  if (ecb_expect_true (x < 0x38800000))
    {
      /* zero */
      if (ecb_expect_true (!x))
        return s;

      /* handle subnormals */

      /* too small, will be zero */
      if (e < (14 - 24)) /* might not be sharp, but is good enough */
        return s;

      m |= 0x00800000; /* make implicit bit explicit */

      /* very tricky - we need to round to the nearest e (+10) bit value */
      {
        unsigned int bits = 14 - e;
        unsigned int half = (1 << (bits - 1)) - 1;
        unsigned int even = (m >> bits) & 1;

        /* if this overflows, we will end up with a normalised number */
        m = (m + half + even) >> bits;
      }

      return s | m;
    }

  /* handle NaNs, preserve leftmost nan bits, but make sure we don't turn them into infinities */
  m >>= 13;

  return s | 0x7c00 | m | !m;
}

/*******************************************************************************/
/* floating point stuff, can be disabled by defining ECB_NO_LIBM */

/* basically, everything uses "ieee pure-endian" floating point numbers */
/* the only noteworthy exception is ancient armle, which uses order 43218765 */
#if 0 \
    || __i386 || __i386__ \
    || ECB_GCC_AMD64 \
    || __powerpc__ || __ppc__ || __powerpc64__ || __ppc64__ \
    || defined __s390__ || defined __s390x__ \
    || defined __mips__ \
    || defined __alpha__ \
    || defined __hppa__ \
    || defined __ia64__ \
    || defined __m68k__ \
    || defined __m88k__ \
    || defined __sh__ \
    || defined _M_IX86 || defined ECB_MSVC_AMD64 || defined _M_IA64 \
    || (defined __arm__ && (defined __ARM_EABI__ || defined __EABI__ || defined __VFP_FP__ || defined _WIN32_WCE || defined __ANDROID__)) \
    || defined __aarch64__
  #define ECB_STDFP 1
#else
  #define ECB_STDFP 0
#endif

#ifndef ECB_NO_LIBM

  #include <math.h> /* for frexp*, ldexp*, INFINITY, NAN */

  /* only the oldest of old doesn't have this one. solaris. */
  #ifdef INFINITY
    #define ECB_INFINITY INFINITY
  #else
    #define ECB_INFINITY HUGE_VAL
  #endif

  #ifdef NAN
    #define ECB_NAN NAN
  #else
    #define ECB_NAN ECB_INFINITY
  #endif

  #if ECB_C99 || _XOPEN_VERSION >= 600 || _POSIX_VERSION >= 200112L
    #define ecb_ldexpf(x,e) ldexpf ((x), (e))
    #define ecb_frexpf(x,e) frexpf ((x), (e))
  #else
    #define ecb_ldexpf(x,e) (float) ldexp ((double) (x), (e))
    #define ecb_frexpf(x,e) (float) frexp ((double) (x), (e))
  #endif

  /* convert a float to ieee single/binary32 */
  ecb_function_ ecb_const uint32_t ecb_float_to_binary32 (float x);
  ecb_function_ ecb_const uint32_t
  ecb_float_to_binary32 (float x)
  {
    uint32_t r;

    #if ECB_STDFP
      memcpy (&r, &x, 4);
    #else
      /* slow emulation, works for anything but -0 */
      uint32_t m;
      int e;

      if (x == 0e0f                    ) return 0x00000000U;
      if (x > +3.40282346638528860e+38f) return 0x7f800000U;
      if (x < -3.40282346638528860e+38f) return 0xff800000U;
      if (x != x                       ) return 0x7fbfffffU;

      m = ecb_frexpf (x, &e) * 0x1000000U;

      r = m & 0x80000000U;

      if (r)
        m = -m;

      if (e <= -126)
        {
          m &= 0xffffffU;
          m >>= (-125 - e);
          e = -126;
        }

      r |= (e + 126) << 23;
      r |= m & 0x7fffffU;
    #endif

    return r;
  }

  /* converts an ieee single/binary32 to a float */
  ecb_function_ ecb_const float ecb_binary32_to_float (uint32_t x);
  ecb_function_ ecb_const float
  ecb_binary32_to_float (uint32_t x)
  {
    float r;

    #if ECB_STDFP
      memcpy (&r, &x, 4);
    #else
      /* emulation, only works for normals and subnormals and +0 */
      int neg = x >> 31;
      int e = (x >> 23) & 0xffU;

      x &= 0x7fffffU;

      if (e)
        x |= 0x800000U;
      else
        e = 1;

      /* we distrust ldexpf a bit and do the 2**-24 scaling by an extra multiply */
      r = ecb_ldexpf (x * (0.5f / 0x800000U), e - 126);

      r = neg ? -r : r;
    #endif

    return r;
  }

  /* convert a double to ieee double/binary64 */
  ecb_function_ ecb_const uint64_t ecb_double_to_binary64 (double x);
  ecb_function_ ecb_const uint64_t
  ecb_double_to_binary64 (double x)
  {
    uint64_t r;

    #if ECB_STDFP
      memcpy (&r, &x, 8);
    #else
      /* slow emulation, works for anything but -0 */
      uint64_t m;
      int e;

      if (x == 0e0                     ) return 0x0000000000000000U;
      if (x > +1.79769313486231470e+308) return 0x7ff0000000000000U;
      if (x < -1.79769313486231470e+308) return 0xfff0000000000000U;
      if (x != x                       ) return 0X7ff7ffffffffffffU;

      m = frexp (x, &e) * 0x20000000000000U;

      r = m & 0x8000000000000000;;

      if (r)
        m = -m;

      if (e <= -1022)
        {
          m &= 0x1fffffffffffffU;
          m >>= (-1021 - e);
          e = -1022;
        }

      r |= ((uint64_t)(e + 1022)) << 52;
      r |= m & 0xfffffffffffffU;
    #endif

    return r;
  }

  /* converts an ieee double/binary64 to a double */
  ecb_function_ ecb_const double ecb_binary64_to_double (uint64_t x);
  ecb_function_ ecb_const double
  ecb_binary64_to_double (uint64_t x)
  {
    double r;

    #if ECB_STDFP
      memcpy (&r, &x, 8);
    #else
      /* emulation, only works for normals and subnormals and +0 */
      int neg = x >> 63;
      int e = (x >> 52) & 0x7ffU;

      x &= 0xfffffffffffffU;

      if (e)
        x |= 0x10000000000000U;
      else
        e = 1;

      /* we distrust ldexp a bit and do the 2**-53 scaling by an extra multiply */
      r = ldexp (x * (0.5 / 0x10000000000000U), e - 1022);

      r = neg ? -r : r;
    #endif

    return r;
  }

  /* convert a float to ieee half/binary16 */
  ecb_function_ ecb_const uint16_t ecb_float_to_binary16 (float x);
  ecb_function_ ecb_const uint16_t
  ecb_float_to_binary16 (float x)
  {
    return ecb_binary32_to_binary16 (ecb_float_to_binary32 (x));
  }

  /* convert an ieee half/binary16 to float */
  ecb_function_ ecb_const float ecb_binary16_to_float (uint16_t x);
  ecb_function_ ecb_const float
  ecb_binary16_to_float (uint16_t x)
  {
    return ecb_binary32_to_float (ecb_binary16_to_binary32 (x));
  }

#endif

#endif

/* ECB.H END */

#if ECB_MEMORY_FENCE_NEEDS_PTHREADS
/* if your architecture doesn't need memory fences, e.g. because it is
 * single-cpu/core, or if you use libev in a project that doesn't use libev
 * from multiple threads, then you can define ECB_NO_THREADS when compiling
 * libev, in which cases the memory fences become nops.
 * alternatively, you can remove this #error and link against libpthread,
 * which will then provide the memory fences.
 */
# error "memory fences not defined for your architecture, please report"
#endif

#ifndef ECB_MEMORY_FENCE
# define ECB_MEMORY_FENCE do { } while (0)
# define ECB_MEMORY_FENCE_ACQUIRE ECB_MEMORY_FENCE
# define ECB_MEMORY_FENCE_RELEASE ECB_MEMORY_FENCE
#endif

#define inline_size        ecb_inline

#if EV_FEATURE_CODE
# define inline_speed      ecb_inline
#else
# define inline_speed      ecb_noinline static
#endif

/*****************************************************************************/
/* raw syscall wrappers */

#if EV_NEED_SYSCALL

#include <sys/syscall.h>

/*
 * define some syscall wrappers for common architectures
 * this is mostly for nice looks during debugging, not performance.
 * our syscalls return < 0, not == -1, on error. which is good
 * enough for linux aio.
 * TODO: arm is also common nowadays, maybe even mips and x86
 * TODO: after implementing this, it suddenly looks like overkill, but its hard to remove...
 */
#if __GNUC__ && __linux && ECB_AMD64 && !EV_FEATURE_CODE
  /* the costly errno access probably kills this for size optimisation */

  #define ev_syscall(nr,narg,arg1,arg2,arg3,arg4,arg5,arg6)            \
    ({                                                                 \
        long res;                                                      \
        register unsigned long r6 __asm__ ("r9" );                     \
        register unsigned long r5 __asm__ ("r8" );                     \
        register unsigned long r4 __asm__ ("r10");                     \
        register unsigned long r3 __asm__ ("rdx");                     \
        register unsigned long r2 __asm__ ("rsi");                     \
        register unsigned long r1 __asm__ ("rdi");                     \
        if (narg >= 6) r6 = (unsigned long)(arg6);                     \
        if (narg >= 5) r5 = (unsigned long)(arg5);                     \
        if (narg >= 4) r4 = (unsigned long)(arg4);                     \
        if (narg >= 3) r3 = (unsigned long)(arg3);                     \
        if (narg >= 2) r2 = (unsigned long)(arg2);                     \
        if (narg >= 1) r1 = (unsigned long)(arg1);                     \
        __asm__ __volatile__ (                                         \
          "syscall\n\t"                                                \
          : "=a" (res)                                                 \
          : "0" (nr), "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r5) \
          : "cc", "r11", "cx", "memory");                              \
        errno = -res;                                                  \
        res;                                                           \
    })

#endif

#ifdef ev_syscall
  #define ev_syscall0(nr)                               ev_syscall (nr, 0,    0,    0,    0,    0,    0,   0)
  #define ev_syscall1(nr,arg1)                          ev_syscall (nr, 1, arg1,    0,    0,    0,    0,   0)
  #define ev_syscall2(nr,arg1,arg2)                     ev_syscall (nr, 2, arg1, arg2,    0,    0,    0,   0)
  #define ev_syscall3(nr,arg1,arg2,arg3)                ev_syscall (nr, 3, arg1, arg2, arg3,    0,    0,   0)
  #define ev_syscall4(nr,arg1,arg2,arg3,arg4)           ev_syscall (nr, 3, arg1, arg2, arg3, arg4,    0,   0)
  #define ev_syscall5(nr,arg1,arg2,arg3,arg4,arg5)      ev_syscall (nr, 5, arg1, arg2, arg3, arg4, arg5,   0)
  #define ev_syscall6(nr,arg1,arg2,arg3,arg4,arg5,arg6) ev_syscall (nr, 6, arg1, arg2, arg3, arg4, arg5,arg6)
#else
  #define ev_syscall0(nr)                               syscall (nr)
  #define ev_syscall1(nr,arg1)                          syscall (nr, arg1)
  #define ev_syscall2(nr,arg1,arg2)                     syscall (nr, arg1, arg2)
  #define ev_syscall3(nr,arg1,arg2,arg3)                syscall (nr, arg1, arg2, arg3)
  #define ev_syscall4(nr,arg1,arg2,arg3,arg4)           syscall (nr, arg1, arg2, arg3, arg4)
  #define ev_syscall5(nr,arg1,arg2,arg3,arg4,arg5)      syscall (nr, arg1, arg2, arg3, arg4, arg5)
  #define ev_syscall6(nr,arg1,arg2,arg3,arg4,arg5,arg6) syscall (nr, arg1, arg2, arg3, arg4, arg5,arg6)
#endif

#endif

/*****************************************************************************/

#define NUMPRI (EV_MAXPRI - EV_MINPRI + 1)

#if EV_MINPRI == EV_MAXPRI
# define ABSPRI(w) (((W)w), 0)
#else
# define ABSPRI(w) (((W)w)->priority - EV_MINPRI)
#endif

#define EMPTY /* required for microsofts broken pseudo-c compiler */

typedef ev_watcher *W;
typedef ev_watcher_list *WL;
typedef ev_watcher_time *WT;

#define ev_active(w) ((W)(w))->active
#define ev_at(w) ((WT)(w))->at

#if EV_USE_REALTIME
/* sig_atomic_t is used to avoid per-thread variables or locking but still */
/* giving it a reasonably high chance of working on typical architectures */
static EV_ATOMIC_T have_realtime; /* did clock_gettime (CLOCK_REALTIME) work? */
#endif

#if EV_USE_MONOTONIC
static EV_ATOMIC_T have_monotonic; /* did clock_gettime (CLOCK_MONOTONIC) work? */
#endif

#ifndef EV_FD_TO_WIN32_HANDLE
# define EV_FD_TO_WIN32_HANDLE(fd) _get_osfhandle (fd)
#endif
#ifndef EV_WIN32_HANDLE_TO_FD
# define EV_WIN32_HANDLE_TO_FD(handle) _open_osfhandle (handle, 0)
#endif
#ifndef EV_WIN32_CLOSE_FD
# define EV_WIN32_CLOSE_FD(fd) close (fd)
#endif

#ifdef _WIN32
# include "ev_win32.c"
#endif

/*****************************************************************************/

#if EV_USE_LINUXAIO
# include <linux/aio_abi.h> /* probably only needed for aio_context_t */
#endif

/* define a suitable floor function (only used by periodics atm) */

#if EV_USE_FLOOR
# include <math.h>
# define ev_floor(v) floor (v)
#else

#include <float.h>

/* a floor() replacement function, should be independent of ev_tstamp type */
ecb_noinline
static ev_tstamp
ev_floor (ev_tstamp v)
{
  /* the choice of shift factor is not terribly important */
#if FLT_RADIX != 2 /* assume FLT_RADIX == 10 */
  const ev_tstamp shift = sizeof (unsigned long) >= 8 ? 10000000000000000000. : 1000000000.;
#else
  const ev_tstamp shift = sizeof (unsigned long) >= 8 ? 18446744073709551616. : 4294967296.;
#endif

  /* special treatment for negative arguments */
  if (ecb_expect_false (v < 0.))
    {
      ev_tstamp f = -ev_floor (-v);

      return f - (f == v ? 0 : 1);
    }

  /* argument too large for an unsigned long? then reduce it */
  if (ecb_expect_false (v >= shift))
    {
      ev_tstamp f;

      if (v == v - 1.)
        return v; /* very large numbers are assumed to be integer */

      f = shift * ev_floor (v * (1. / shift));
      return f + ev_floor (v - f);
    }

  /* fits into an unsigned long */
  return (unsigned long)v;
}

#endif

/*****************************************************************************/

#ifdef __linux
# include <sys/utsname.h>
#endif

ecb_noinline ecb_cold
static unsigned int
ev_linux_version (void)
{
#ifdef __linux
  unsigned int v = 0;
  struct utsname buf;
  int i;
  char *p = buf.release;

  if (uname (&buf))
    return 0;

  for (i = 3+1; --i; )
    {
      unsigned int c = 0;

      for (;;)
        {
          if (*p >= '0' && *p <= '9')
            c = c * 10 + *p++ - '0';
          else
            {
              p += *p == '.';
              break;
            }
        }

      v = (v << 8) | c;
    }

  return v;
#else
  return 0;
#endif
}

/*****************************************************************************/

#if EV_AVOID_STDIO
ecb_noinline ecb_cold
static void
ev_printerr (const char *msg)
{
  write (STDERR_FILENO, msg, strlen (msg));
}
#endif

static void (*syserr_cb)(const char *msg) EV_NOEXCEPT;

ecb_cold
void
ev_set_syserr_cb (void (*cb)(const char *msg) EV_NOEXCEPT) EV_NOEXCEPT
{
  syserr_cb = cb;
}

ecb_noinline ecb_cold
static void
ev_syserr (const char *msg)
{
  if (!msg)
    msg = "(libev) system error";

  if (syserr_cb)
    syserr_cb (msg);
  else
    {
#if EV_AVOID_STDIO
      ev_printerr (msg);
      ev_printerr (": ");
      ev_printerr (strerror (errno));
      ev_printerr ("\n");
#else
      perror (msg);
#endif
      abort ();
    }
}

static void *
ev_realloc_emul (void *ptr, long size) EV_NOEXCEPT
{
  /* some systems, notably openbsd and darwin, fail to properly
   * implement realloc (x, 0) (as required by both ansi c-89 and
   * the single unix specification, so work around them here.
   * recently, also (at least) fedora and debian started breaking it,
   * despite documenting it otherwise.
   */

  if (size)
    return realloc (ptr, size);

  free (ptr);
  return 0;
}

static void *(*alloc)(void *ptr, long size) EV_NOEXCEPT = ev_realloc_emul;

ecb_cold
void
ev_set_allocator (void *(*cb)(void *ptr, long size) EV_NOEXCEPT) EV_NOEXCEPT
{
  alloc = cb;
}

inline_speed void *
ev_realloc (void *ptr, long size)
{
  ptr = alloc (ptr, size);

  if (!ptr && size)
    {
#if EV_AVOID_STDIO
      ev_printerr ("(libev) memory allocation failed, aborting.\n");
#else
      fprintf (stderr, "(libev) cannot allocate %ld bytes, aborting.", size);
#endif
      abort ();
    }

  return ptr;
}

#define ev_malloc(size) ev_realloc (0, (size))
#define ev_free(ptr)    ev_realloc ((ptr), 0)

/*****************************************************************************/

/* set in reify when reification needed */
#define EV_ANFD_REIFY 1

/* file descriptor info structure */
typedef struct
{
  WL head;
  unsigned char events; /* the events watched for */
  unsigned char reify;  /* flag set when this ANFD needs reification (EV_ANFD_REIFY, EV__IOFDSET) */
  unsigned char emask;  /* some backends store the actual kernel mask in here */
  unsigned char eflags; /* flags field for use by backends */
#if EV_USE_EPOLL
  unsigned int egen;    /* generation counter to counter epoll bugs */
#endif
#if EV_SELECT_IS_WINSOCKET || EV_USE_IOCP
  SOCKET handle;
#endif
#if EV_USE_IOCP
  OVERLAPPED or, ow;
#endif
} ANFD;

/* stores the pending event set for a given watcher */
typedef struct
{
  W w;
  int events; /* the pending event set for the given watcher */
} ANPENDING;

#if EV_USE_INOTIFY
/* hash table entry per inotify-id */
typedef struct
{
  WL head;
} ANFS;
#endif

/* Heap Entry */
#if EV_HEAP_CACHE_AT
  /* a heap element */
  typedef struct {
    ev_tstamp at;
    WT w;
  } ANHE;

  #define ANHE_w(he)        (he).w     /* access watcher, read-write */
  #define ANHE_at(he)       (he).at    /* access cached at, read-only */
  #define ANHE_at_cache(he) (he).at = (he).w->at /* update at from watcher */
#else
  /* a heap element */
  typedef WT ANHE;

  #define ANHE_w(he)        (he)
  #define ANHE_at(he)       (he)->at
  #define ANHE_at_cache(he)
#endif

#if EV_MULTIPLICITY

  struct ev_loop
  {
    ev_tstamp ev_rt_now;
    #define ev_rt_now ((loop)->ev_rt_now)
    #define VAR(name,decl) decl;
      #include "ev_vars.h"
    #undef VAR
  };
  #include "ev_wrap.h"

  static struct ev_loop default_loop_struct;
  EV_API_DECL struct ev_loop *ev_default_loop_ptr = 0; /* needs to be initialised to make it a definition despite extern */

#else

  EV_API_DECL ev_tstamp ev_rt_now = EV_TS_CONST (0.); /* needs to be initialised to make it a definition despite extern */
  #define VAR(name,decl) static decl;
    #include "ev_vars.h"
  #undef VAR

  static int ev_default_loop_ptr;

#endif

#if EV_FEATURE_API
# define EV_RELEASE_CB if (ecb_expect_false (release_cb)) release_cb (EV_A)
# define EV_ACQUIRE_CB if (ecb_expect_false (acquire_cb)) acquire_cb (EV_A)
# define EV_INVOKE_PENDING invoke_cb (EV_A)
#else
# define EV_RELEASE_CB (void)0
# define EV_ACQUIRE_CB (void)0
# define EV_INVOKE_PENDING ev_invoke_pending (EV_A)
#endif

#define EVBREAK_RECURSE 0x80

/*****************************************************************************/

#ifndef EV_HAVE_EV_TIME
ev_tstamp
ev_time (void) EV_NOEXCEPT
{
#if EV_USE_REALTIME
  if (ecb_expect_true (have_realtime))
    {
      struct timespec ts;
      clock_gettime (CLOCK_REALTIME, &ts);
      return EV_TS_GET (ts);
    }
#endif

  {
    struct timeval tv;
    gettimeofday (&tv, 0);
    return EV_TV_GET (tv);
  }
}
#endif

inline_size ev_tstamp
get_clock (void)
{
#if EV_USE_MONOTONIC
  if (ecb_expect_true (have_monotonic))
    {
      struct timespec ts;
      clock_gettime (CLOCK_MONOTONIC, &ts);
      return EV_TS_GET (ts);
    }
#endif

  return ev_time ();
}

#if EV_MULTIPLICITY
ev_tstamp
ev_now (EV_P) EV_NOEXCEPT
{
  return ev_rt_now;
}
#endif

void
ev_sleep (ev_tstamp delay) EV_NOEXCEPT
{
  if (delay > EV_TS_CONST (0.))
    {
#if EV_USE_NANOSLEEP
      struct timespec ts;

      EV_TS_SET (ts, delay);
      nanosleep (&ts, 0);
#elif defined _WIN32
      /* maybe this should round up, as ms is very low resolution */
      /* compared to select (µs) or nanosleep (ns) */
      Sleep ((unsigned long)(EV_TS_TO_MSEC (delay)));
#else
      struct timeval tv;

      /* here we rely on sys/time.h + sys/types.h + unistd.h providing select */
      /* something not guaranteed by newer posix versions, but guaranteed */
      /* by older ones */
      EV_TV_SET (tv, delay);
      select (0, 0, 0, 0, &tv);
#endif
    }
}

/*****************************************************************************/

#define MALLOC_ROUND 4096 /* prefer to allocate in chunks of this size, must be 2**n and >> 4 longs */

/* find a suitable new size for the given array, */
/* hopefully by rounding to a nice-to-malloc size */
inline_size int
array_nextsize (int elem, int cur, int cnt)
{
  int ncur = cur + 1;

  do
    ncur <<= 1;
  while (cnt > ncur);

  /* if size is large, round to MALLOC_ROUND - 4 * longs to accommodate malloc overhead */
  if (elem * ncur > MALLOC_ROUND - sizeof (void *) * 4)
    {
      ncur *= elem;
      ncur = (ncur + elem + (MALLOC_ROUND - 1) + sizeof (void *) * 4) & ~(MALLOC_ROUND - 1);
      ncur = ncur - sizeof (void *) * 4;
      ncur /= elem;
    }

  return ncur;
}

ecb_noinline ecb_cold
static void *
array_realloc (int elem, void *base, int *cur, int cnt)
{
  *cur = array_nextsize (elem, *cur, cnt);
  return ev_realloc (base, elem * *cur);
}

#define array_needsize_noinit(base,offset,count)

#define array_needsize_zerofill(base,offset,count)	\
  memset ((void *)(base + offset), 0, sizeof (*(base)) * (count))

#define array_needsize(type,base,cur,cnt,init)			\
  if (ecb_expect_false ((cnt) > (cur)))				\
    {								\
      ecb_unused int ocur_ = (cur);				\
      (base) = (type *)array_realloc				\
         (sizeof (type), (base), &(cur), (cnt));		\
      init ((base), ocur_, ((cur) - ocur_));			\
    }

#if 0
#define array_slim(type,stem)					\
  if (stem ## max < array_roundsize (stem ## cnt >> 2))		\
    {								\
      stem ## max = array_roundsize (stem ## cnt >> 1);		\
      base = (type *)ev_realloc (base, sizeof (type) * (stem ## max));\
      fprintf (stderr, "slimmed down " # stem " to %d\n", stem ## max);/*D*/\
    }
#endif

#define array_free(stem, idx) \
  ev_free (stem ## s idx); stem ## cnt idx = stem ## max idx = 0; stem ## s idx = 0

/*****************************************************************************/

/* dummy callback for pending events */
ecb_noinline
static void
pendingcb (EV_P_ ev_prepare *w, int revents)
{
}

ecb_noinline
void
ev_feed_event (EV_P_ void *w, int revents) EV_NOEXCEPT
{
  W w_ = (W)w;
  int pri = ABSPRI (w_);

  if (ecb_expect_false (w_->pending))
    pendings [pri][w_->pending - 1].events |= revents;
  else
    {
      w_->pending = ++pendingcnt [pri];
      array_needsize (ANPENDING, pendings [pri], pendingmax [pri], w_->pending, array_needsize_noinit);
      pendings [pri][w_->pending - 1].w      = w_;
      pendings [pri][w_->pending - 1].events = revents;
    }

  pendingpri = NUMPRI - 1;
}

inline_speed void
feed_reverse (EV_P_ W w)
{
  array_needsize (W, rfeeds, rfeedmax, rfeedcnt + 1, array_needsize_noinit);
  rfeeds [rfeedcnt++] = w;
}

inline_size void
feed_reverse_done (EV_P_ int revents)
{
  do
    ev_feed_event (EV_A_ rfeeds [--rfeedcnt], revents);
  while (rfeedcnt);
}

inline_speed void
queue_events (EV_P_ W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    ev_feed_event (EV_A_ events [i], type);
}

/*****************************************************************************/

inline_speed void
fd_event_nocheck (EV_P_ int fd, int revents)
{
  ANFD *anfd = anfds + fd;
  ev_io *w;

  for (w = (ev_io *)anfd->head; w; w = (ev_io *)((WL)w)->next)
    {
      int ev = w->events & revents;

      if (ev)
        ev_feed_event (EV_A_ (W)w, ev);
    }
}

/* do not submit kernel events for fds that have reify set */
/* because that means they changed while we were polling for new events */
inline_speed void
fd_event (EV_P_ int fd, int revents)
{
  ANFD *anfd = anfds + fd;

  if (ecb_expect_true (!anfd->reify))
    fd_event_nocheck (EV_A_ fd, revents);
}

void
ev_feed_fd_event (EV_P_ int fd, int revents) EV_NOEXCEPT
{
  if (fd >= 0 && fd < anfdmax)
    fd_event_nocheck (EV_A_ fd, revents);
}

/* make sure the external fd watch events are in-sync */
/* with the kernel/libev internal state */
inline_size void
fd_reify (EV_P)
{
  int i;

  /* most backends do not modify the fdchanges list in backend_modfiy.
   * except io_uring, which has fixed-size buffers which might force us
   * to handle events in backend_modify, causing fdchanges to be amended,
   * which could result in an endless loop.
   * to avoid this, we do not dynamically handle fds that were added
   * during fd_reify. that means that for those backends, fdchangecnt
   * might be non-zero during poll, which must cause them to not block.
   * to not put too much of a burden on other backends, this detail
   * needs to be handled in the backend.
   */
  int changecnt = fdchangecnt;

#if EV_SELECT_IS_WINSOCKET || EV_USE_IOCP
  for (i = 0; i < changecnt; ++i)
    {
      int fd = fdchanges [i];
      ANFD *anfd = anfds + fd;

      if (anfd->reify & EV__IOFDSET && anfd->head)
        {
          SOCKET handle = EV_FD_TO_WIN32_HANDLE (fd);

          if (handle != anfd->handle)
            {
              unsigned long arg;

              assert (("libev: only socket fds supported in this configuration", ioctlsocket (handle, FIONREAD, &arg) == 0));

              /* handle changed, but fd didn't - we need to do it in two steps */
              backend_modify (EV_A_ fd, anfd->events, 0);
              anfd->events = 0;
              anfd->handle = handle;
            }
        }
    }
#endif

  for (i = 0; i < changecnt; ++i)
    {
      int fd = fdchanges [i];
      ANFD *anfd = anfds + fd;
      ev_io *w;

      unsigned char o_events = anfd->events;
      unsigned char o_reify  = anfd->reify;

      anfd->reify = 0;

      /*if (ecb_expect_true (o_reify & EV_ANFD_REIFY)) probably a deoptimisation */
        {
          anfd->events = 0;

          for (w = (ev_io *)anfd->head; w; w = (ev_io *)((WL)w)->next)
            anfd->events |= (unsigned char)w->events;

          if (o_events != anfd->events)
            o_reify = EV__IOFDSET; /* actually |= */
        }

      if (o_reify & EV__IOFDSET)
        backend_modify (EV_A_ fd, o_events, anfd->events);
    }

  /* normally, fdchangecnt hasn't changed. if it has, then new fds have been added.
   * this is a rare case (see beginning comment in this function), so we copy them to the
   * front and hope the backend handles this case.
   */
  if (ecb_expect_false (fdchangecnt != changecnt))
    memmove (fdchanges, fdchanges + changecnt, (fdchangecnt - changecnt) * sizeof (*fdchanges));

  fdchangecnt -= changecnt;
}

/* something about the given fd changed */
inline_size
void
fd_change (EV_P_ int fd, int flags)
{
  unsigned char reify = anfds [fd].reify;
  anfds [fd].reify = reify | flags;

  if (ecb_expect_true (!reify))
    {
      ++fdchangecnt;
      array_needsize (int, fdchanges, fdchangemax, fdchangecnt, array_needsize_noinit);
      fdchanges [fdchangecnt - 1] = fd;
    }
}

/* the given fd is invalid/unusable, so make sure it doesn't hurt us anymore */
inline_speed ecb_cold void
fd_kill (EV_P_ int fd)
{
  ev_io *w;

  while ((w = (ev_io *)anfds [fd].head))
    {
      ev_io_stop (EV_A_ w);
      ev_feed_event (EV_A_ (W)w, EV_ERROR | EV_READ | EV_WRITE);
    }
}

/* check whether the given fd is actually valid, for error recovery */
inline_size ecb_cold int
fd_valid (int fd)
{
#ifdef _WIN32
  return EV_FD_TO_WIN32_HANDLE (fd) != -1;
#else
  return fcntl (fd, F_GETFD) != -1;
#endif
}

/* called on EBADF to verify fds */
ecb_noinline ecb_cold
static void
fd_ebadf (EV_P)
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      if (!fd_valid (fd) && errno == EBADF)
        fd_kill (EV_A_ fd);
}

/* called on ENOMEM in select/poll to kill some fds and retry */
ecb_noinline ecb_cold
static void
fd_enomem (EV_P)
{
  int fd;

  for (fd = anfdmax; fd--; )
    if (anfds [fd].events)
      {
        fd_kill (EV_A_ fd);
        break;
      }
}

/* usually called after fork if backend needs to re-arm all fds from scratch */
ecb_noinline
static void
fd_rearm_all (EV_P)
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      {
        anfds [fd].events = 0;
        anfds [fd].emask  = 0;
        fd_change (EV_A_ fd, EV__IOFDSET | EV_ANFD_REIFY);
      }
}

/* used to prepare libev internal fd's */
/* this is not fork-safe */
inline_speed void
fd_intern (int fd)
{
#ifdef _WIN32
  unsigned long arg = 1;
  ioctlsocket (EV_FD_TO_WIN32_HANDLE (fd), FIONBIO, &arg);
#else
  fcntl (fd, F_SETFD, FD_CLOEXEC);
  fcntl (fd, F_SETFL, O_NONBLOCK);
#endif
}

/*****************************************************************************/

/*
 * the heap functions want a real array index. array index 0 is guaranteed to not
 * be in-use at any time. the first heap entry is at array [HEAP0]. DHEAP gives
 * the branching factor of the d-tree.
 */

/*
 * at the moment we allow libev the luxury of two heaps,
 * a small-code-size 2-heap one and a ~1.5kb larger 4-heap
 * which is more cache-efficient.
 * the difference is about 5% with 50000+ watchers.
 */
#if EV_USE_4HEAP

#define DHEAP 4
#define HEAP0 (DHEAP - 1) /* index of first element in heap */
#define HPARENT(k) ((((k) - HEAP0 - 1) / DHEAP) + HEAP0)
#define UPHEAP_DONE(p,k) ((p) == (k))

/* away from the root */
inline_speed void
downheap (ANHE *heap, int N, int k)
{
  ANHE he = heap [k];
  ANHE *E = heap + N + HEAP0;

  for (;;)
    {
      ev_tstamp minat;
      ANHE *minpos;
      ANHE *pos = heap + DHEAP * (k - HEAP0) + HEAP0 + 1;

      /* find minimum child */
      if (ecb_expect_true (pos + DHEAP - 1 < E))
        {
          /* fast path */                               (minpos = pos + 0), (minat = ANHE_at (*minpos));
          if (               minat > ANHE_at (pos [1])) (minpos = pos + 1), (minat = ANHE_at (*minpos));
          if (               minat > ANHE_at (pos [2])) (minpos = pos + 2), (minat = ANHE_at (*minpos));
          if (               minat > ANHE_at (pos [3])) (minpos = pos + 3), (minat = ANHE_at (*minpos));
        }
      else if (pos < E)
        {
          /* slow path */                               (minpos = pos + 0), (minat = ANHE_at (*minpos));
          if (pos + 1 < E && minat > ANHE_at (pos [1])) (minpos = pos + 1), (minat = ANHE_at (*minpos));
          if (pos + 2 < E && minat > ANHE_at (pos [2])) (minpos = pos + 2), (minat = ANHE_at (*minpos));
          if (pos + 3 < E && minat > ANHE_at (pos [3])) (minpos = pos + 3), (minat = ANHE_at (*minpos));
        }
      else
        break;

      if (ANHE_at (he) <= minat)
        break;

      heap [k] = *minpos;
      ev_active (ANHE_w (*minpos)) = k;

      k = minpos - heap;
    }

  heap [k] = he;
  ev_active (ANHE_w (he)) = k;
}

#else /* not 4HEAP */

#define HEAP0 1
#define HPARENT(k) ((k) >> 1)
#define UPHEAP_DONE(p,k) (!(p))

/* away from the root */
inline_speed void
downheap (ANHE *heap, int N, int k)
{
  ANHE he = heap [k];

  for (;;)
    {
      int c = k << 1;

      if (c >= N + HEAP0)
        break;

      c += c + 1 < N + HEAP0 && ANHE_at (heap [c]) > ANHE_at (heap [c + 1])
           ? 1 : 0;

      if (ANHE_at (he) <= ANHE_at (heap [c]))
        break;

      heap [k] = heap [c];
      ev_active (ANHE_w (heap [k])) = k;
      
      k = c;
    }

  heap [k] = he;
  ev_active (ANHE_w (he)) = k;
}
#endif

/* towards the root */
inline_speed void
upheap (ANHE *heap, int k)
{
  ANHE he = heap [k];

  for (;;)
    {
      int p = HPARENT (k);

      if (UPHEAP_DONE (p, k) || ANHE_at (heap [p]) <= ANHE_at (he))
        break;

      heap [k] = heap [p];
      ev_active (ANHE_w (heap [k])) = k;
      k = p;
    }

  heap [k] = he;
  ev_active (ANHE_w (he)) = k;
}

/* move an element suitably so it is in a correct place */
inline_size void
adjustheap (ANHE *heap, int N, int k)
{
  if (k > HEAP0 && ANHE_at (heap [k]) <= ANHE_at (heap [HPARENT (k)]))
    upheap (heap, k);
  else
    downheap (heap, N, k);
}

/* rebuild the heap: this function is used only once and executed rarely */
inline_size void
reheap (ANHE *heap, int N)
{
  int i;

  /* we don't use floyds algorithm, upheap is simpler and is more cache-efficient */
  /* also, this is easy to implement and correct for both 2-heaps and 4-heaps */
  for (i = 0; i < N; ++i)
    upheap (heap, i + HEAP0);
}

/*****************************************************************************/

/* associate signal watchers to a signal */
typedef struct
{
  EV_ATOMIC_T pending;
#if EV_MULTIPLICITY
  EV_P;
#endif
  WL head;
} ANSIG;

static ANSIG signals [EV_NSIG - 1];

/*****************************************************************************/

#if EV_SIGNAL_ENABLE || EV_ASYNC_ENABLE

ecb_noinline ecb_cold
static void
evpipe_init (EV_P)
{
  if (!ev_is_active (&pipe_w))
    {
      int fds [2];

# if EV_USE_EVENTFD
      fds [0] = -1;
      fds [1] = eventfd (0, EFD_NONBLOCK | EFD_CLOEXEC);
      if (fds [1] < 0 && errno == EINVAL)
        fds [1] = eventfd (0, 0);

      if (fds [1] < 0)
# endif
        {
          while (pipe (fds))
            ev_syserr ("(libev) error creating signal/async pipe");

          fd_intern (fds [0]);
        }

      evpipe [0] = fds [0];

      if (evpipe [1] < 0)
        evpipe [1] = fds [1]; /* first call, set write fd */
      else
        {
          /* on subsequent calls, do not change evpipe [1] */
          /* so that evpipe_write can always rely on its value. */
          /* this branch does not do anything sensible on windows, */
          /* so must not be executed on windows */

          dup2 (fds [1], evpipe [1]);
          close (fds [1]);
        }

      fd_intern (evpipe [1]);

      ev_io_set (&pipe_w, evpipe [0] < 0 ? evpipe [1] : evpipe [0], EV_READ);
      ev_io_start (EV_A_ &pipe_w);
      ev_unref (EV_A); /* watcher should not keep loop alive */
    }
}

inline_speed void
evpipe_write (EV_P_ EV_ATOMIC_T *flag)
{
  ECB_MEMORY_FENCE; /* push out the write before this function was called, acquire flag */

  if (ecb_expect_true (*flag))
    return;

  *flag = 1;
  ECB_MEMORY_FENCE_RELEASE; /* make sure flag is visible before the wakeup */

  pipe_write_skipped = 1;

  ECB_MEMORY_FENCE; /* make sure pipe_write_skipped is visible before we check pipe_write_wanted */

  if (pipe_write_wanted)
    {
      int old_errno;

      pipe_write_skipped = 0;
      ECB_MEMORY_FENCE_RELEASE;

      old_errno = errno; /* save errno because write will clobber it */

#if EV_USE_EVENTFD
      if (evpipe [0] < 0)
        {
          uint64_t counter = 1;
          write (evpipe [1], &counter, sizeof (uint64_t));
        }
      else
#endif
        {
#ifdef _WIN32
          WSABUF buf;
          DWORD sent;
          buf.buf = (char *)&buf;
          buf.len = 1;
          WSASend (EV_FD_TO_WIN32_HANDLE (evpipe [1]), &buf, 1, &sent, 0, 0, 0);
#else
          write (evpipe [1], &(evpipe [1]), 1);
#endif
        }

      errno = old_errno;
    }
}

/* called whenever the libev signal pipe */
/* got some events (signal, async) */
static void
pipecb (EV_P_ ev_io *iow, int revents)
{
  int i;

  if (revents & EV_READ)
    {
#if EV_USE_EVENTFD
      if (evpipe [0] < 0)
        {
          uint64_t counter;
          read (evpipe [1], &counter, sizeof (uint64_t));
        }
      else
#endif
        {
          char dummy[4];
#ifdef _WIN32
          WSABUF buf;
          DWORD recvd;
          DWORD flags = 0;
          buf.buf = dummy;
          buf.len = sizeof (dummy);
          WSARecv (EV_FD_TO_WIN32_HANDLE (evpipe [0]), &buf, 1, &recvd, &flags, 0, 0);
#else
          read (evpipe [0], &dummy, sizeof (dummy));
#endif
        }
    }

  pipe_write_skipped = 0;

  ECB_MEMORY_FENCE; /* push out skipped, acquire flags */

#if EV_SIGNAL_ENABLE
  if (sig_pending)
    {
      sig_pending = 0;

      ECB_MEMORY_FENCE;

      for (i = EV_NSIG - 1; i--; )
        if (ecb_expect_false (signals [i].pending))
          ev_feed_signal_event (EV_A_ i + 1);
    }
#endif

#if EV_ASYNC_ENABLE
  if (async_pending)
    {
      async_pending = 0;

      ECB_MEMORY_FENCE;

      for (i = asynccnt; i--; )
        if (asyncs [i]->sent)
          {
            asyncs [i]->sent = 0;
            ECB_MEMORY_FENCE_RELEASE;
            ev_feed_event (EV_A_ asyncs [i], EV_ASYNC);
          }
    }
#endif
}

/*****************************************************************************/

void
ev_feed_signal (int signum) EV_NOEXCEPT
{
#if EV_MULTIPLICITY
  EV_P;
  ECB_MEMORY_FENCE_ACQUIRE;
  EV_A = signals [signum - 1].loop;

  if (!EV_A)
    return;
#endif

  signals [signum - 1].pending = 1;
  evpipe_write (EV_A_ &sig_pending);
}

static void
ev_sighandler (int signum)
{
#ifdef _WIN32
  signal (signum, ev_sighandler);
#endif

  ev_feed_signal (signum);
}

ecb_noinline
void
ev_feed_signal_event (EV_P_ int signum) EV_NOEXCEPT
{
  WL w;

  if (ecb_expect_false (signum <= 0 || signum >= EV_NSIG))
    return;

  --signum;

#if EV_MULTIPLICITY
  /* it is permissible to try to feed a signal to the wrong loop */
  /* or, likely more useful, feeding a signal nobody is waiting for */

  if (ecb_expect_false (signals [signum].loop != EV_A))
    return;
#endif

  signals [signum].pending = 0;
  ECB_MEMORY_FENCE_RELEASE;

  for (w = signals [signum].head; w; w = w->next)
    ev_feed_event (EV_A_ (W)w, EV_SIGNAL);
}

#if EV_USE_SIGNALFD
static void
sigfdcb (EV_P_ ev_io *iow, int revents)
{
  struct signalfd_siginfo si[2], *sip; /* these structs are big */

  for (;;)
    {
      ssize_t res = read (sigfd, si, sizeof (si));

      /* not ISO-C, as res might be -1, but works with SuS */
      for (sip = si; (char *)sip < (char *)si + res; ++sip)
        ev_feed_signal_event (EV_A_ sip->ssi_signo);

      if (res < (ssize_t)sizeof (si))
        break;
    }
}
#endif

#endif

/*****************************************************************************/

#if EV_CHILD_ENABLE
static WL childs [EV_PID_HASHSIZE];

static ev_signal childev;

#ifndef WIFCONTINUED
# define WIFCONTINUED(status) 0
#endif

/* handle a single child status event */
inline_speed void
child_reap (EV_P_ int chain, int pid, int status)
{
  ev_child *w;
  int traced = WIFSTOPPED (status) || WIFCONTINUED (status);

  for (w = (ev_child *)childs [chain & ((EV_PID_HASHSIZE) - 1)]; w; w = (ev_child *)((WL)w)->next)
    {
      if ((w->pid == pid || !w->pid)
          && (!traced || (w->flags & 1)))
        {
          ev_set_priority (w, EV_MAXPRI); /* need to do it *now*, this *must* be the same prio as the signal watcher itself */
          w->rpid    = pid;
          w->rstatus = status;
          ev_feed_event (EV_A_ (W)w, EV_CHILD);
        }
    }
}

#ifndef WCONTINUED
# define WCONTINUED 0
#endif

/* called on sigchld etc., calls waitpid */
static void
childcb (EV_P_ ev_signal *sw, int revents)
{
  int pid, status;

  /* some systems define WCONTINUED but then fail to support it (linux 2.4) */
  if (0 >= (pid = waitpid (-1, &status, WNOHANG | WUNTRACED | WCONTINUED)))
    if (!WCONTINUED
        || errno != EINVAL
        || 0 >= (pid = waitpid (-1, &status, WNOHANG | WUNTRACED)))
      return;

  /* make sure we are called again until all children have been reaped */
  /* we need to do it this way so that the callback gets called before we continue */
  ev_feed_event (EV_A_ (W)sw, EV_SIGNAL);

  child_reap (EV_A_ pid, pid, status);
  if ((EV_PID_HASHSIZE) > 1)
    child_reap (EV_A_ 0, pid, status); /* this might trigger a watcher twice, but feed_event catches that */
}

#endif

/*****************************************************************************/

#if EV_USE_TIMERFD

static void periodics_reschedule (EV_P);

static void
timerfdcb (EV_P_ ev_io *iow, int revents)
{
  struct itimerspec its = { 0 };

  its.it_value.tv_sec = ev_rt_now + (int)MAX_BLOCKTIME2;
  timerfd_settime (timerfd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &its, 0);

  ev_rt_now = ev_time ();
  /* periodics_reschedule only needs ev_rt_now */
  /* but maybe in the future we want the full treatment. */
  /*
  now_floor = EV_TS_CONST (0.);
  time_update (EV_A_ EV_TSTAMP_HUGE);
  */
#if EV_PERIODIC_ENABLE
  periodics_reschedule (EV_A);
#endif
}

ecb_noinline ecb_cold
static void
evtimerfd_init (EV_P)
{
  if (!ev_is_active (&timerfd_w))
    {
      timerfd = timerfd_create (CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);

      if (timerfd >= 0)
        {
          fd_intern (timerfd); /* just to be sure */

          ev_io_init (&timerfd_w, timerfdcb, timerfd, EV_READ);
          ev_set_priority (&timerfd_w, EV_MINPRI);
          ev_io_start (EV_A_ &timerfd_w);
          ev_unref (EV_A); /* watcher should not keep loop alive */

          /* (re-) arm timer */
          timerfdcb (EV_A_ 0, 0);
        }
    }
}

#endif

/*****************************************************************************/

#if EV_USE_IOCP
# include "ev_iocp.c"
#endif
#if EV_USE_PORT
# include "ev_port.c"
#endif
#if EV_USE_KQUEUE
# include "ev_kqueue.c"
#endif
#if EV_USE_EPOLL
# include "ev_epoll.c"
#endif
#if EV_USE_LINUXAIO
# include "ev_linuxaio.c"
#endif
#if EV_USE_IOURING
# include "ev_iouring.c"
#endif
#if EV_USE_POLL
# include "ev_poll.c"
#endif
#if EV_USE_SELECT
# include "ev_select.c"
#endif

ecb_cold int
ev_version_major (void) EV_NOEXCEPT
{
  return EV_VERSION_MAJOR;
}

ecb_cold int
ev_version_minor (void) EV_NOEXCEPT
{
  return EV_VERSION_MINOR;
}

/* return true if we are running with elevated privileges and should ignore env variables */
inline_size ecb_cold int
enable_secure (void)
{
#ifdef _WIN32
  return 0;
#else
  return getuid () != geteuid ()
      || getgid () != getegid ();
#endif
}

ecb_cold
unsigned int
ev_supported_backends (void) EV_NOEXCEPT
{
  unsigned int flags = 0;

  if (EV_USE_PORT                                      ) flags |= EVBACKEND_PORT;
  if (EV_USE_KQUEUE                                    ) flags |= EVBACKEND_KQUEUE;
  if (EV_USE_EPOLL                                     ) flags |= EVBACKEND_EPOLL;
  if (EV_USE_LINUXAIO                                  ) flags |= EVBACKEND_LINUXAIO;
  if (EV_USE_IOURING && ev_linux_version () >= 0x050601) flags |= EVBACKEND_IOURING; /* 5.6.1+ */
  if (EV_USE_POLL                                      ) flags |= EVBACKEND_POLL;
  if (EV_USE_SELECT                                    ) flags |= EVBACKEND_SELECT;

  return flags;
}

ecb_cold
unsigned int
ev_recommended_backends (void) EV_NOEXCEPT
{
  unsigned int flags = ev_supported_backends ();

#ifndef __NetBSD__
  /* kqueue is borked on everything but netbsd apparently */
  /* it usually doesn't work correctly on anything but sockets and pipes */
  flags &= ~EVBACKEND_KQUEUE;
#endif
#ifdef __APPLE__
  /* only select works correctly on that "unix-certified" platform */
  flags &= ~EVBACKEND_KQUEUE; /* horribly broken, even for sockets */
  flags &= ~EVBACKEND_POLL;   /* poll is based on kqueue from 10.5 onwards */
#endif
#ifdef __FreeBSD__
  flags &= ~EVBACKEND_POLL;   /* poll return value is unusable (http://forums.freebsd.org/archive/index.php/t-10270.html) */
#endif

  /* TODO: linuxaio is very experimental */
#if !EV_RECOMMEND_LINUXAIO
  flags &= ~EVBACKEND_LINUXAIO;
#endif
  /* TODO: linuxaio is super experimental */
#if !EV_RECOMMEND_IOURING
  flags &= ~EVBACKEND_IOURING;
#endif

  return flags;
}

ecb_cold
unsigned int
ev_embeddable_backends (void) EV_NOEXCEPT
{
  int flags = EVBACKEND_EPOLL | EVBACKEND_KQUEUE | EVBACKEND_PORT | EVBACKEND_IOURING;

  /* epoll embeddability broken on all linux versions up to at least 2.6.23 */
  if (ev_linux_version () < 0x020620) /* disable it on linux < 2.6.32 */
    flags &= ~EVBACKEND_EPOLL;

  /* EVBACKEND_LINUXAIO is theoretically embeddable, but suffers from a performance overhead */

  return flags;
}

unsigned int
ev_backend (EV_P) EV_NOEXCEPT
{
  return backend;
}

#if EV_FEATURE_API
unsigned int
ev_iteration (EV_P) EV_NOEXCEPT
{
  return loop_count;
}

unsigned int
ev_depth (EV_P) EV_NOEXCEPT
{
  return loop_depth;
}

void
ev_set_io_collect_interval (EV_P_ ev_tstamp interval) EV_NOEXCEPT
{
  io_blocktime = interval;
}

void
ev_set_timeout_collect_interval (EV_P_ ev_tstamp interval) EV_NOEXCEPT
{
  timeout_blocktime = interval;
}

void
ev_set_userdata (EV_P_ void *data) EV_NOEXCEPT
{
  userdata = data;
}

void *
ev_userdata (EV_P) EV_NOEXCEPT
{
  return userdata;
}

void
ev_set_invoke_pending_cb (EV_P_ ev_loop_callback invoke_pending_cb) EV_NOEXCEPT
{
  invoke_cb = invoke_pending_cb;
}

void
ev_set_loop_release_cb (EV_P_ void (*release)(EV_P) EV_NOEXCEPT, void (*acquire)(EV_P) EV_NOEXCEPT) EV_NOEXCEPT
{
  release_cb = release;
  acquire_cb = acquire;
}
#endif

/* initialise a loop structure, must be zero-initialised */
ecb_noinline ecb_cold
static void
loop_init (EV_P_ unsigned int flags) EV_NOEXCEPT
{
  if (!backend)
    {
      origflags = flags;

#if EV_USE_REALTIME
      if (!have_realtime)
        {
          struct timespec ts;

          if (!clock_gettime (CLOCK_REALTIME, &ts))
            have_realtime = 1;
        }
#endif

#if EV_USE_MONOTONIC
      if (!have_monotonic)
        {
          struct timespec ts;

          if (!clock_gettime (CLOCK_MONOTONIC, &ts))
            have_monotonic = 1;
        }
#endif

      /* pid check not overridable via env */
#ifndef _WIN32
      if (flags & EVFLAG_FORKCHECK)
        curpid = getpid ();
#endif

      if (!(flags & EVFLAG_NOENV)
          && !enable_secure ()
          && getenv ("LIBEV_FLAGS"))
        flags = atoi (getenv ("LIBEV_FLAGS"));

      ev_rt_now          = ev_time ();
      mn_now             = get_clock ();
      now_floor          = mn_now;
      rtmn_diff          = ev_rt_now - mn_now;
#if EV_FEATURE_API
      invoke_cb          = ev_invoke_pending;
#endif

      io_blocktime       = 0.;
      timeout_blocktime  = 0.;
      backend            = 0;
      backend_fd         = -1;
      sig_pending        = 0;
#if EV_ASYNC_ENABLE
      async_pending      = 0;
#endif
      pipe_write_skipped = 0;
      pipe_write_wanted  = 0;
      evpipe [0]         = -1;
      evpipe [1]         = -1;
#if EV_USE_INOTIFY
      fs_fd              = flags & EVFLAG_NOINOTIFY ? -1 : -2;
#endif
#if EV_USE_SIGNALFD
      sigfd              = flags & EVFLAG_SIGNALFD  ? -2 : -1;
#endif
#if EV_USE_TIMERFD
      timerfd            = flags & EVFLAG_NOTIMERFD ? -1 : -2;
#endif

      if (!(flags & EVBACKEND_MASK))
        flags |= ev_recommended_backends ();

#if EV_USE_IOCP
      if (!backend && (flags & EVBACKEND_IOCP    )) backend = iocp_init      (EV_A_ flags);
#endif
#if EV_USE_PORT
      if (!backend && (flags & EVBACKEND_PORT    )) backend = port_init      (EV_A_ flags);
#endif
#if EV_USE_KQUEUE
      if (!backend && (flags & EVBACKEND_KQUEUE  )) backend = kqueue_init    (EV_A_ flags);
#endif
#if EV_USE_IOURING
      if (!backend && (flags & EVBACKEND_IOURING )) backend = iouring_init   (EV_A_ flags);
#endif
#if EV_USE_LINUXAIO
      if (!backend && (flags & EVBACKEND_LINUXAIO)) backend = linuxaio_init  (EV_A_ flags);
#endif
#if EV_USE_EPOLL
      if (!backend && (flags & EVBACKEND_EPOLL   )) backend = epoll_init     (EV_A_ flags);
#endif
#if EV_USE_POLL
      if (!backend && (flags & EVBACKEND_POLL    )) backend = poll_init      (EV_A_ flags);
#endif
#if EV_USE_SELECT
      if (!backend && (flags & EVBACKEND_SELECT  )) backend = select_init    (EV_A_ flags);
#endif

      ev_prepare_init (&pending_w, pendingcb);

#if EV_SIGNAL_ENABLE || EV_ASYNC_ENABLE
      ev_init (&pipe_w, pipecb);
      ev_set_priority (&pipe_w, EV_MAXPRI);
#endif
    }
}

/* free up a loop structure */
ecb_cold
void
ev_loop_destroy (EV_P)
{
  int i;

#if EV_MULTIPLICITY
  /* mimic free (0) */
  if (!EV_A)
    return;
#endif

#if EV_CLEANUP_ENABLE
  /* queue cleanup watchers (and execute them) */
  if (ecb_expect_false (cleanupcnt))
    {
      queue_events (EV_A_ (W *)cleanups, cleanupcnt, EV_CLEANUP);
      EV_INVOKE_PENDING;
    }
#endif

#if EV_CHILD_ENABLE
  if (ev_is_default_loop (EV_A) && ev_is_active (&childev))
    {
      ev_ref (EV_A); /* child watcher */
      ev_signal_stop (EV_A_ &childev);
    }
#endif

  if (ev_is_active (&pipe_w))
    {
      /*ev_ref (EV_A);*/
      /*ev_io_stop (EV_A_ &pipe_w);*/

      if (evpipe [0] >= 0) EV_WIN32_CLOSE_FD (evpipe [0]);
      if (evpipe [1] >= 0) EV_WIN32_CLOSE_FD (evpipe [1]);
    }

#if EV_USE_SIGNALFD
  if (ev_is_active (&sigfd_w))
    close (sigfd);
#endif

#if EV_USE_TIMERFD
  if (ev_is_active (&timerfd_w))
    close (timerfd);
#endif

#if EV_USE_INOTIFY
  if (fs_fd >= 0)
    close (fs_fd);
#endif

  if (backend_fd >= 0)
    close (backend_fd);

#if EV_USE_IOCP
  if (backend == EVBACKEND_IOCP    ) iocp_destroy     (EV_A);
#endif
#if EV_USE_PORT
  if (backend == EVBACKEND_PORT    ) port_destroy     (EV_A);
#endif
#if EV_USE_KQUEUE
  if (backend == EVBACKEND_KQUEUE  ) kqueue_destroy   (EV_A);
#endif
#if EV_USE_IOURING
  if (backend == EVBACKEND_IOURING ) iouring_destroy  (EV_A);
#endif
#if EV_USE_LINUXAIO
  if (backend == EVBACKEND_LINUXAIO) linuxaio_destroy (EV_A);
#endif
#if EV_USE_EPOLL
  if (backend == EVBACKEND_EPOLL   ) epoll_destroy    (EV_A);
#endif
#if EV_USE_POLL
  if (backend == EVBACKEND_POLL    ) poll_destroy     (EV_A);
#endif
#if EV_USE_SELECT
  if (backend == EVBACKEND_SELECT  ) select_destroy   (EV_A);
#endif

  for (i = NUMPRI; i--; )
    {
      array_free (pending, [i]);
#if EV_IDLE_ENABLE
      array_free (idle, [i]);
#endif
    }

  ev_free (anfds); anfds = 0; anfdmax = 0;

  /* have to use the microsoft-never-gets-it-right macro */
  array_free (rfeed, EMPTY);
  array_free (fdchange, EMPTY);
  array_free (timer, EMPTY);
#if EV_PERIODIC_ENABLE
  array_free (periodic, EMPTY);
#endif
#if EV_FORK_ENABLE
  array_free (fork, EMPTY);
#endif
#if EV_CLEANUP_ENABLE
  array_free (cleanup, EMPTY);
#endif
  array_free (prepare, EMPTY);
  array_free (check, EMPTY);
#if EV_ASYNC_ENABLE
  array_free (async, EMPTY);
#endif

  backend = 0;

#if EV_MULTIPLICITY
  if (ev_is_default_loop (EV_A))
#endif
    ev_default_loop_ptr = 0;
#if EV_MULTIPLICITY
  else
    ev_free (EV_A);
#endif
}

#if EV_USE_INOTIFY
inline_size void infy_fork (EV_P);
#endif

inline_size void
loop_fork (EV_P)
{
#if EV_USE_PORT
  if (backend == EVBACKEND_PORT    ) port_fork     (EV_A);
#endif
#if EV_USE_KQUEUE
  if (backend == EVBACKEND_KQUEUE  ) kqueue_fork   (EV_A);
#endif
#if EV_USE_IOURING
  if (backend == EVBACKEND_IOURING ) iouring_fork  (EV_A);
#endif
#if EV_USE_LINUXAIO
  if (backend == EVBACKEND_LINUXAIO) linuxaio_fork (EV_A);
#endif
#if EV_USE_EPOLL
  if (backend == EVBACKEND_EPOLL   ) epoll_fork    (EV_A);
#endif
#if EV_USE_INOTIFY
  infy_fork (EV_A);
#endif

  if (postfork != 2)
    {
      #if EV_USE_SIGNALFD
        /* surprisingly, nothing needs to be done for signalfd, accoridng to docs, it does the right thing on fork */
      #endif
      
      #if EV_USE_TIMERFD
        if (ev_is_active (&timerfd_w))
          {
            ev_ref (EV_A);
            ev_io_stop (EV_A_ &timerfd_w);

            close (timerfd);
            timerfd = -2;
      
            evtimerfd_init (EV_A);
            /* reschedule periodics, in case we missed something */
            ev_feed_event (EV_A_ &timerfd_w, EV_CUSTOM);
          }
      #endif
      
      #if EV_SIGNAL_ENABLE || EV_ASYNC_ENABLE
        if (ev_is_active (&pipe_w))
          {
            /* pipe_write_wanted must be false now, so modifying fd vars should be safe */
      
            ev_ref (EV_A);
            ev_io_stop (EV_A_ &pipe_w);
      
            if (evpipe [0] >= 0)
              EV_WIN32_CLOSE_FD (evpipe [0]);
      
            evpipe_init (EV_A);
            /* iterate over everything, in case we missed something before */
            ev_feed_event (EV_A_ &pipe_w, EV_CUSTOM);
          }
      #endif
    }

  postfork = 0;
}

#if EV_MULTIPLICITY

ecb_cold
struct ev_loop *
ev_loop_new (unsigned int flags) EV_NOEXCEPT
{
  EV_P = (struct ev_loop *)ev_malloc (sizeof (struct ev_loop));

  memset (EV_A, 0, sizeof (struct ev_loop));
  loop_init (EV_A_ flags);

  if (ev_backend (EV_A))
    return EV_A;

  ev_free (EV_A);
  return 0;
}

#endif /* multiplicity */

#if EV_VERIFY
ecb_noinline ecb_cold
static void
verify_watcher (EV_P_ W w)
{
  assert (("libev: watcher has invalid priority", ABSPRI (w) >= 0 && ABSPRI (w) < NUMPRI));

  if (w->pending)
    assert (("libev: pending watcher not on pending queue", pendings [ABSPRI (w)][w->pending - 1].w == w));
}

ecb_noinline ecb_cold
static void
verify_heap (EV_P_ ANHE *heap, int N)
{
  int i;

  for (i = HEAP0; i < N + HEAP0; ++i)
    {
      assert (("libev: active index mismatch in heap", ev_active (ANHE_w (heap [i])) == i));
      assert (("libev: heap condition violated", i == HEAP0 || ANHE_at (heap [HPARENT (i)]) <= ANHE_at (heap [i])));
      assert (("libev: heap at cache mismatch", ANHE_at (heap [i]) == ev_at (ANHE_w (heap [i]))));

      verify_watcher (EV_A_ (W)ANHE_w (heap [i]));
    }
}

ecb_noinline ecb_cold
static void
array_verify (EV_P_ W *ws, int cnt)
{
  while (cnt--)
    {
      assert (("libev: active index mismatch", ev_active (ws [cnt]) == cnt + 1));
      verify_watcher (EV_A_ ws [cnt]);
    }
}
#endif

#if EV_FEATURE_API
void ecb_cold
ev_verify (EV_P) EV_NOEXCEPT
{
#if EV_VERIFY
  int i;
  WL w, w2;

  assert (activecnt >= -1);

  assert (fdchangemax >= fdchangecnt);
  for (i = 0; i < fdchangecnt; ++i)
    assert (("libev: negative fd in fdchanges", fdchanges [i] >= 0));

  assert (anfdmax >= 0);
  for (i = 0; i < anfdmax; ++i)
    {
      int j = 0;

      for (w = w2 = anfds [i].head; w; w = w->next)
        {
          verify_watcher (EV_A_ (W)w);

          if (j++ & 1)
            {
              assert (("libev: io watcher list contains a loop", w != w2));
              w2 = w2->next;
            }

          assert (("libev: inactive fd watcher on anfd list", ev_active (w) == 1));
          assert (("libev: fd mismatch between watcher and anfd", ((ev_io *)w)->fd == i));
        }
    }

  assert (timermax >= timercnt);
  verify_heap (EV_A_ timers, timercnt);

#if EV_PERIODIC_ENABLE
  assert (periodicmax >= periodiccnt);
  verify_heap (EV_A_ periodics, periodiccnt);
#endif

  for (i = NUMPRI; i--; )
    {
      assert (pendingmax [i] >= pendingcnt [i]);
#if EV_IDLE_ENABLE
      assert (idleall >= 0);
      assert (idlemax [i] >= idlecnt [i]);
      array_verify (EV_A_ (W *)idles [i], idlecnt [i]);
#endif
    }

#if EV_FORK_ENABLE
  assert (forkmax >= forkcnt);
  array_verify (EV_A_ (W *)forks, forkcnt);
#endif

#if EV_CLEANUP_ENABLE
  assert (cleanupmax >= cleanupcnt);
  array_verify (EV_A_ (W *)cleanups, cleanupcnt);
#endif

#if EV_ASYNC_ENABLE
  assert (asyncmax >= asynccnt);
  array_verify (EV_A_ (W *)asyncs, asynccnt);
#endif

#if EV_PREPARE_ENABLE
  assert (preparemax >= preparecnt);
  array_verify (EV_A_ (W *)prepares, preparecnt);
#endif

#if EV_CHECK_ENABLE
  assert (checkmax >= checkcnt);
  array_verify (EV_A_ (W *)checks, checkcnt);
#endif

# if 0
#if EV_CHILD_ENABLE
  for (w = (ev_child *)childs [chain & ((EV_PID_HASHSIZE) - 1)]; w; w = (ev_child *)((WL)w)->next)
  for (signum = EV_NSIG; signum--; ) if (signals [signum].pending)
#endif
# endif
#endif
}
#endif

#if EV_MULTIPLICITY
ecb_cold
struct ev_loop *
#else
int
#endif
ev_default_loop (unsigned int flags) EV_NOEXCEPT
{
  if (!ev_default_loop_ptr)
    {
#if EV_MULTIPLICITY
      EV_P = ev_default_loop_ptr = &default_loop_struct;
#else
      ev_default_loop_ptr = 1;
#endif

      loop_init (EV_A_ flags);

      if (ev_backend (EV_A))
        {
#if EV_CHILD_ENABLE
          ev_signal_init (&childev, childcb, SIGCHLD);
          ev_set_priority (&childev, EV_MAXPRI);
          ev_signal_start (EV_A_ &childev);
          ev_unref (EV_A); /* child watcher should not keep loop alive */
#endif
        }
      else
        ev_default_loop_ptr = 0;
    }

  return ev_default_loop_ptr;
}

void
ev_loop_fork (EV_P) EV_NOEXCEPT
{
  postfork = 1;
}

/*****************************************************************************/

void
ev_invoke (EV_P_ void *w, int revents)
{
  EV_CB_INVOKE ((W)w, revents);
}

unsigned int
ev_pending_count (EV_P) EV_NOEXCEPT
{
  int pri;
  unsigned int count = 0;

  for (pri = NUMPRI; pri--; )
    count += pendingcnt [pri];

  return count;
}

ecb_noinline
void
ev_invoke_pending (EV_P)
{
  pendingpri = NUMPRI;

  do
    {
      --pendingpri;

      /* pendingpri possibly gets modified in the inner loop */
      while (pendingcnt [pendingpri])
        {
          ANPENDING *p = pendings [pendingpri] + --pendingcnt [pendingpri];

          p->w->pending = 0;
          EV_CB_INVOKE (p->w, p->events);
          EV_FREQUENT_CHECK;
        }
    }
  while (pendingpri);
}

#if EV_IDLE_ENABLE
/* make idle watchers pending. this handles the "call-idle */
/* only when higher priorities are idle" logic */
inline_size void
idle_reify (EV_P)
{
  if (ecb_expect_false (idleall))
    {
      int pri;

      for (pri = NUMPRI; pri--; )
        {
          if (pendingcnt [pri])
            break;

          if (idlecnt [pri])
            {
              queue_events (EV_A_ (W *)idles [pri], idlecnt [pri], EV_IDLE);
              break;
            }
        }
    }
}
#endif

/* make timers pending */
inline_size void
timers_reify (EV_P)
{
  EV_FREQUENT_CHECK;

  if (timercnt && ANHE_at (timers [HEAP0]) < mn_now)
    {
      do
        {
          ev_timer *w = (ev_timer *)ANHE_w (timers [HEAP0]);

          /*assert (("libev: inactive timer on timer heap detected", ev_is_active (w)));*/

          /* first reschedule or stop timer */
          if (w->repeat)
            {
              ev_at (w) += w->repeat;
              if (ev_at (w) < mn_now)
                ev_at (w) = mn_now;

              assert (("libev: negative ev_timer repeat value found while processing timers", w->repeat > EV_TS_CONST (0.)));

              ANHE_at_cache (timers [HEAP0]);
              downheap (timers, timercnt, HEAP0);
            }
          else
            ev_timer_stop (EV_A_ w); /* nonrepeating: stop timer */

          EV_FREQUENT_CHECK;
          feed_reverse (EV_A_ (W)w);
        }
      while (timercnt && ANHE_at (timers [HEAP0]) < mn_now);

      feed_reverse_done (EV_A_ EV_TIMER);
    }
}

#if EV_PERIODIC_ENABLE

ecb_noinline
static void
periodic_recalc (EV_P_ ev_periodic *w)
{
  ev_tstamp interval = w->interval > MIN_INTERVAL ? w->interval : MIN_INTERVAL;
  ev_tstamp at = w->offset + interval * ev_floor ((ev_rt_now - w->offset) / interval);

  /* the above almost always errs on the low side */
  while (at <= ev_rt_now)
    {
      ev_tstamp nat = at + w->interval;

      /* when resolution fails us, we use ev_rt_now */
      if (ecb_expect_false (nat == at))
        {
          at = ev_rt_now;
          break;
        }

      at = nat;
    }

  ev_at (w) = at;
}

/* make periodics pending */
inline_size void
periodics_reify (EV_P)
{
  EV_FREQUENT_CHECK;

  while (periodiccnt && ANHE_at (periodics [HEAP0]) < ev_rt_now)
    {
      do
        {
          ev_periodic *w = (ev_periodic *)ANHE_w (periodics [HEAP0]);

          /*assert (("libev: inactive timer on periodic heap detected", ev_is_active (w)));*/

          /* first reschedule or stop timer */
          if (w->reschedule_cb)
            {
              ev_at (w) = w->reschedule_cb (w, ev_rt_now);

              assert (("libev: ev_periodic reschedule callback returned time in the past", ev_at (w) >= ev_rt_now));

              ANHE_at_cache (periodics [HEAP0]);
              downheap (periodics, periodiccnt, HEAP0);
            }
          else if (w->interval)
            {
              periodic_recalc (EV_A_ w);
              ANHE_at_cache (periodics [HEAP0]);
              downheap (periodics, periodiccnt, HEAP0);
            }
          else
            ev_periodic_stop (EV_A_ w); /* nonrepeating: stop timer */

          EV_FREQUENT_CHECK;
          feed_reverse (EV_A_ (W)w);
        }
      while (periodiccnt && ANHE_at (periodics [HEAP0]) < ev_rt_now);

      feed_reverse_done (EV_A_ EV_PERIODIC);
    }
}

/* simply recalculate all periodics */
/* TODO: maybe ensure that at least one event happens when jumping forward? */
ecb_noinline ecb_cold
static void
periodics_reschedule (EV_P)
{
  int i;

  /* adjust periodics after time jump */
  for (i = HEAP0; i < periodiccnt + HEAP0; ++i)
    {
      ev_periodic *w = (ev_periodic *)ANHE_w (periodics [i]);

      if (w->reschedule_cb)
        ev_at (w) = w->reschedule_cb (w, ev_rt_now);
      else if (w->interval)
        periodic_recalc (EV_A_ w);

      ANHE_at_cache (periodics [i]);
    }

  reheap (periodics, periodiccnt);
}
#endif

/* adjust all timers by a given offset */
ecb_noinline ecb_cold
static void
timers_reschedule (EV_P_ ev_tstamp adjust)
{
  int i;

  for (i = 0; i < timercnt; ++i)
    {
      ANHE *he = timers + i + HEAP0;
      ANHE_w (*he)->at += adjust;
      ANHE_at_cache (*he);
    }
}

/* fetch new monotonic and realtime times from the kernel */
/* also detect if there was a timejump, and act accordingly */
inline_speed void
time_update (EV_P_ ev_tstamp max_block)
{
#if EV_USE_MONOTONIC
  if (ecb_expect_true (have_monotonic))
    {
      int i;
      ev_tstamp odiff = rtmn_diff;

      mn_now = get_clock ();

      /* only fetch the realtime clock every 0.5*MIN_TIMEJUMP seconds */
      /* interpolate in the meantime */
      if (ecb_expect_true (mn_now - now_floor < EV_TS_CONST (MIN_TIMEJUMP * .5)))
        {
          ev_rt_now = rtmn_diff + mn_now;
          return;
        }

      now_floor = mn_now;
      ev_rt_now = ev_time ();

      /* loop a few times, before making important decisions.
       * on the choice of "4": one iteration isn't enough,
       * in case we get preempted during the calls to
       * ev_time and get_clock. a second call is almost guaranteed
       * to succeed in that case, though. and looping a few more times
       * doesn't hurt either as we only do this on time-jumps or
       * in the unlikely event of having been preempted here.
       */
      for (i = 4; --i; )
        {
          ev_tstamp diff;
          rtmn_diff = ev_rt_now - mn_now;

          diff = odiff - rtmn_diff;

          if (ecb_expect_true ((diff < EV_TS_CONST (0.) ? -diff : diff) < EV_TS_CONST (MIN_TIMEJUMP)))
            return; /* all is well */

          ev_rt_now = ev_time ();
          mn_now    = get_clock ();
          now_floor = mn_now;
        }

      /* no timer adjustment, as the monotonic clock doesn't jump */
      /* timers_reschedule (EV_A_ rtmn_diff - odiff) */
# if EV_PERIODIC_ENABLE
      periodics_reschedule (EV_A);
# endif
    }
  else
#endif
    {
      ev_rt_now = ev_time ();

      if (ecb_expect_false (mn_now > ev_rt_now || ev_rt_now > mn_now + max_block + EV_TS_CONST (MIN_TIMEJUMP)))
        {
          /* adjust timers. this is easy, as the offset is the same for all of them */
          timers_reschedule (EV_A_ ev_rt_now - mn_now);
#if EV_PERIODIC_ENABLE
          periodics_reschedule (EV_A);
#endif
        }

      mn_now = ev_rt_now;
    }
}

int
ev_run (EV_P_ int flags)
{
#if EV_FEATURE_API
  ++loop_depth;
#endif

  assert (("libev: ev_loop recursion during release detected", loop_done != EVBREAK_RECURSE));

  loop_done = EVBREAK_CANCEL;

  EV_INVOKE_PENDING; /* in case we recurse, ensure ordering stays nice and clean */

  do
    {
#if EV_VERIFY >= 2
      ev_verify (EV_A);
#endif

#ifndef _WIN32
      if (ecb_expect_false (curpid)) /* penalise the forking check even more */
        if (ecb_expect_false (getpid () != curpid))
          {
            curpid = getpid ();
            postfork = 1;
          }
#endif

#if EV_FORK_ENABLE
      /* we might have forked, so queue fork handlers */
      if (ecb_expect_false (postfork))
        if (forkcnt)
          {
            queue_events (EV_A_ (W *)forks, forkcnt, EV_FORK);
            EV_INVOKE_PENDING;
          }
#endif

#if EV_PREPARE_ENABLE
      /* queue prepare watchers (and execute them) */
      if (ecb_expect_false (preparecnt))
        {
          queue_events (EV_A_ (W *)prepares, preparecnt, EV_PREPARE);
          EV_INVOKE_PENDING;
        }
#endif

      if (ecb_expect_false (loop_done))
        break;

      /* we might have forked, so reify kernel state if necessary */
      if (ecb_expect_false (postfork))
        loop_fork (EV_A);

      /* update fd-related kernel structures */
      fd_reify (EV_A);

      /* calculate blocking time */
      {
        ev_tstamp waittime  = 0.;
        ev_tstamp sleeptime = 0.;

        /* remember old timestamp for io_blocktime calculation */
        ev_tstamp prev_mn_now = mn_now;

        /* update time to cancel out callback processing overhead */
        time_update (EV_A_ EV_TS_CONST (EV_TSTAMP_HUGE));

        /* from now on, we want a pipe-wake-up */
        pipe_write_wanted = 1;

        ECB_MEMORY_FENCE; /* make sure pipe_write_wanted is visible before we check for potential skips */

        if (ecb_expect_true (!(flags & EVRUN_NOWAIT || idleall || !activecnt || pipe_write_skipped)))
          {
            waittime = EV_TS_CONST (MAX_BLOCKTIME);

#if EV_USE_TIMERFD
            /* sleep a lot longer when we can reliably detect timejumps */
            if (ecb_expect_true (timerfd >= 0))
              waittime = EV_TS_CONST (MAX_BLOCKTIME2);
#endif
#if !EV_PERIODIC_ENABLE
            /* without periodics but with monotonic clock there is no need */
            /* for any time jump detection, so sleep longer */
            if (ecb_expect_true (have_monotonic))
              waittime = EV_TS_CONST (MAX_BLOCKTIME2);
#endif

            if (timercnt)
              {
                ev_tstamp to = ANHE_at (timers [HEAP0]) - mn_now;
                if (waittime > to) waittime = to;
              }

#if EV_PERIODIC_ENABLE
            if (periodiccnt)
              {
                ev_tstamp to = ANHE_at (periodics [HEAP0]) - ev_rt_now;
                if (waittime > to) waittime = to;
              }
#endif

            /* don't let timeouts decrease the waittime below timeout_blocktime */
            if (ecb_expect_false (waittime < timeout_blocktime))
              waittime = timeout_blocktime;

            /* now there are two more special cases left, either we have
             * already-expired timers, so we should not sleep, or we have timers
             * that expire very soon, in which case we need to wait for a minimum
             * amount of time for some event loop backends.
             */
            if (ecb_expect_false (waittime < backend_mintime))
              waittime = waittime <= EV_TS_CONST (0.)
                 ? EV_TS_CONST (0.)
                 : backend_mintime;

            /* extra check because io_blocktime is commonly 0 */
            if (ecb_expect_false (io_blocktime))
              {
                sleeptime = io_blocktime - (mn_now - prev_mn_now);

                if (sleeptime > waittime - backend_mintime)
                  sleeptime = waittime - backend_mintime;

                if (ecb_expect_true (sleeptime > EV_TS_CONST (0.)))
                  {
                    ev_sleep (sleeptime);
                    waittime -= sleeptime;
                  }
              }
          }

#if EV_FEATURE_API
        ++loop_count;
#endif
        assert ((loop_done = EVBREAK_RECURSE, 1)); /* assert for side effect */
        backend_poll (EV_A_ waittime);
        assert ((loop_done = EVBREAK_CANCEL, 1)); /* assert for side effect */

        pipe_write_wanted = 0; /* just an optimisation, no fence needed */

        ECB_MEMORY_FENCE_ACQUIRE;
        if (pipe_write_skipped)
          {
            assert (("libev: pipe_w not active, but pipe not written", ev_is_active (&pipe_w)));
            ev_feed_event (EV_A_ &pipe_w, EV_CUSTOM);
          }

        /* update ev_rt_now, do magic */
        time_update (EV_A_ waittime + sleeptime);
      }

      /* queue pending timers and reschedule them */
      timers_reify (EV_A); /* relative timers called last */
#if EV_PERIODIC_ENABLE
      periodics_reify (EV_A); /* absolute timers called first */
#endif

#if EV_IDLE_ENABLE
      /* queue idle watchers unless other events are pending */
      idle_reify (EV_A);
#endif

#if EV_CHECK_ENABLE
      /* queue check watchers, to be executed first */
      if (ecb_expect_false (checkcnt))
        queue_events (EV_A_ (W *)checks, checkcnt, EV_CHECK);
#endif

      EV_INVOKE_PENDING;
    }
  while (ecb_expect_true (
    activecnt
    && !loop_done
    && !(flags & (EVRUN_ONCE | EVRUN_NOWAIT))
  ));

  if (loop_done == EVBREAK_ONE)
    loop_done = EVBREAK_CANCEL;

#if EV_FEATURE_API
  --loop_depth;
#endif

  return activecnt;
}

void
ev_break (EV_P_ int how) EV_NOEXCEPT
{
  loop_done = how;
}

void
ev_ref (EV_P) EV_NOEXCEPT
{
  ++activecnt;
}

void
ev_unref (EV_P) EV_NOEXCEPT
{
  --activecnt;
}

void
ev_now_update (EV_P) EV_NOEXCEPT
{
  time_update (EV_A_ EV_TSTAMP_HUGE);
}

void
ev_suspend (EV_P) EV_NOEXCEPT
{
  ev_now_update (EV_A);
}

void
ev_resume (EV_P) EV_NOEXCEPT
{
  ev_tstamp mn_prev = mn_now;

  ev_now_update (EV_A);
  timers_reschedule (EV_A_ mn_now - mn_prev);
#if EV_PERIODIC_ENABLE
  /* TODO: really do this? */
  periodics_reschedule (EV_A);
#endif
}

/*****************************************************************************/
/* singly-linked list management, used when the expected list length is short */

inline_size void
wlist_add (WL *head, WL elem)
{
  elem->next = *head;
  *head = elem;
}

inline_size void
wlist_del (WL *head, WL elem)
{
  while (*head)
    {
      if (ecb_expect_true (*head == elem))
        {
          *head = elem->next;
          break;
        }

      head = &(*head)->next;
    }
}

/* internal, faster, version of ev_clear_pending */
inline_speed void
clear_pending (EV_P_ W w)
{
  if (w->pending)
    {
      pendings [ABSPRI (w)][w->pending - 1].w = (W)&pending_w;
      w->pending = 0;
    }
}

int
ev_clear_pending (EV_P_ void *w) EV_NOEXCEPT
{
  W w_ = (W)w;
  int pending = w_->pending;

  if (ecb_expect_true (pending))
    {
      ANPENDING *p = pendings [ABSPRI (w_)] + pending - 1;
      p->w = (W)&pending_w;
      w_->pending = 0;
      return p->events;
    }
  else
    return 0;
}

inline_size void
pri_adjust (EV_P_ W w)
{
  int pri = ev_priority (w);
  pri = pri < EV_MINPRI ? EV_MINPRI : pri;
  pri = pri > EV_MAXPRI ? EV_MAXPRI : pri;
  ev_set_priority (w, pri);
}

inline_speed void
ev_start (EV_P_ W w, int active)
{
  pri_adjust (EV_A_ w);
  w->active = active;
  ev_ref (EV_A);
}

inline_size void
ev_stop (EV_P_ W w)
{
  ev_unref (EV_A);
  w->active = 0;
}

/*****************************************************************************/

ecb_noinline
void
ev_io_start (EV_P_ ev_io *w) EV_NOEXCEPT
{
  int fd = w->fd;

  if (ecb_expect_false (ev_is_active (w)))
    return;

  assert (("libev: ev_io_start called with negative fd", fd >= 0));
  assert (("libev: ev_io_start called with illegal event mask", !(w->events & ~(EV__IOFDSET | EV_READ | EV_WRITE))));

#if EV_VERIFY >= 2
  assert (("libev: ev_io_start called on watcher with invalid fd", fd_valid (fd)));
#endif
  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, 1);
  array_needsize (ANFD, anfds, anfdmax, fd + 1, array_needsize_zerofill);
  wlist_add (&anfds[fd].head, (WL)w);

  /* common bug, apparently */
  assert (("libev: ev_io_start called with corrupted watcher", ((WL)w)->next != (WL)w));

  fd_change (EV_A_ fd, w->events & EV__IOFDSET | EV_ANFD_REIFY);
  w->events &= ~EV__IOFDSET;

  EV_FREQUENT_CHECK;
}

ecb_noinline
void
ev_io_stop (EV_P_ ev_io *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  assert (("libev: ev_io_stop called with illegal fd (must stay constant after start!)", w->fd >= 0 && w->fd < anfdmax));

#if EV_VERIFY >= 2
  assert (("libev: ev_io_stop called on watcher with invalid fd", fd_valid (w->fd)));
#endif
  EV_FREQUENT_CHECK;

  wlist_del (&anfds[w->fd].head, (WL)w);
  ev_stop (EV_A_ (W)w);

  fd_change (EV_A_ w->fd, EV_ANFD_REIFY);

  EV_FREQUENT_CHECK;
}

ecb_noinline
void
ev_timer_start (EV_P_ ev_timer *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  ev_at (w) += mn_now;

  assert (("libev: ev_timer_start called with negative timer repeat value", w->repeat >= 0.));

  EV_FREQUENT_CHECK;

  ++timercnt;
  ev_start (EV_A_ (W)w, timercnt + HEAP0 - 1);
  array_needsize (ANHE, timers, timermax, ev_active (w) + 1, array_needsize_noinit);
  ANHE_w (timers [ev_active (w)]) = (WT)w;
  ANHE_at_cache (timers [ev_active (w)]);
  upheap (timers, ev_active (w));

  EV_FREQUENT_CHECK;

  /*assert (("libev: internal timer heap corruption", timers [ev_active (w)] == (WT)w));*/
}

ecb_noinline
void
ev_timer_stop (EV_P_ ev_timer *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    assert (("libev: internal timer heap corruption", ANHE_w (timers [active]) == (WT)w));

    --timercnt;

    if (ecb_expect_true (active < timercnt + HEAP0))
      {
        timers [active] = timers [timercnt + HEAP0];
        adjustheap (timers, timercnt, active);
      }
  }

  ev_at (w) -= mn_now;

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}

ecb_noinline
void
ev_timer_again (EV_P_ ev_timer *w) EV_NOEXCEPT
{
  EV_FREQUENT_CHECK;

  clear_pending (EV_A_ (W)w);

  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          ev_at (w) = mn_now + w->repeat;
          ANHE_at_cache (timers [ev_active (w)]);
          adjustheap (timers, timercnt, ev_active (w));
        }
      else
        ev_timer_stop (EV_A_ w);
    }
  else if (w->repeat)
    {
      ev_at (w) = w->repeat;
      ev_timer_start (EV_A_ w);
    }

  EV_FREQUENT_CHECK;
}

ev_tstamp
ev_timer_remaining (EV_P_ ev_timer *w) EV_NOEXCEPT
{
  return ev_at (w) - (ev_is_active (w) ? mn_now : EV_TS_CONST (0.));
}

#if EV_PERIODIC_ENABLE
ecb_noinline
void
ev_periodic_start (EV_P_ ev_periodic *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

#if EV_USE_TIMERFD
  if (timerfd == -2)
    evtimerfd_init (EV_A);
#endif

  if (w->reschedule_cb)
    ev_at (w) = w->reschedule_cb (w, ev_rt_now);
  else if (w->interval)
    {
      assert (("libev: ev_periodic_start called with negative interval value", w->interval >= 0.));
      periodic_recalc (EV_A_ w);
    }
  else
    ev_at (w) = w->offset;

  EV_FREQUENT_CHECK;

  ++periodiccnt;
  ev_start (EV_A_ (W)w, periodiccnt + HEAP0 - 1);
  array_needsize (ANHE, periodics, periodicmax, ev_active (w) + 1, array_needsize_noinit);
  ANHE_w (periodics [ev_active (w)]) = (WT)w;
  ANHE_at_cache (periodics [ev_active (w)]);
  upheap (periodics, ev_active (w));

  EV_FREQUENT_CHECK;

  /*assert (("libev: internal periodic heap corruption", ANHE_w (periodics [ev_active (w)]) == (WT)w));*/
}

ecb_noinline
void
ev_periodic_stop (EV_P_ ev_periodic *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    assert (("libev: internal periodic heap corruption", ANHE_w (periodics [active]) == (WT)w));

    --periodiccnt;

    if (ecb_expect_true (active < periodiccnt + HEAP0))
      {
        periodics [active] = periodics [periodiccnt + HEAP0];
        adjustheap (periodics, periodiccnt, active);
      }
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}

ecb_noinline
void
ev_periodic_again (EV_P_ ev_periodic *w) EV_NOEXCEPT
{
  /* TODO: use adjustheap and recalculation */
  ev_periodic_stop (EV_A_ w);
  ev_periodic_start (EV_A_ w);
}
#endif

#ifndef SA_RESTART
# define SA_RESTART 0
#endif

#if EV_SIGNAL_ENABLE

ecb_noinline
void
ev_signal_start (EV_P_ ev_signal *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  assert (("libev: ev_signal_start called with illegal signal number", w->signum > 0 && w->signum < EV_NSIG));

#if EV_MULTIPLICITY
  assert (("libev: a signal must not be attached to two different loops",
           !signals [w->signum - 1].loop || signals [w->signum - 1].loop == loop));

  signals [w->signum - 1].loop = EV_A;
  ECB_MEMORY_FENCE_RELEASE;
#endif

  EV_FREQUENT_CHECK;

#if EV_USE_SIGNALFD
  if (sigfd == -2)
    {
      sigfd = signalfd (-1, &sigfd_set, SFD_NONBLOCK | SFD_CLOEXEC);
      if (sigfd < 0 && errno == EINVAL)
        sigfd = signalfd (-1, &sigfd_set, 0); /* retry without flags */

      if (sigfd >= 0)
        {
          fd_intern (sigfd); /* doing it twice will not hurt */

          sigemptyset (&sigfd_set);

          ev_io_init (&sigfd_w, sigfdcb, sigfd, EV_READ);
          ev_set_priority (&sigfd_w, EV_MAXPRI);
          ev_io_start (EV_A_ &sigfd_w);
          ev_unref (EV_A); /* signalfd watcher should not keep loop alive */
        }
    }

  if (sigfd >= 0)
    {
      /* TODO: check .head */
      sigaddset (&sigfd_set, w->signum);
      sigprocmask (SIG_BLOCK, &sigfd_set, 0);

      signalfd (sigfd, &sigfd_set, 0);
    }
#endif

  ev_start (EV_A_ (W)w, 1);
  wlist_add (&signals [w->signum - 1].head, (WL)w);

  if (!((WL)w)->next)
# if EV_USE_SIGNALFD
    if (sigfd < 0) /*TODO*/
# endif
      {
# ifdef _WIN32
        evpipe_init (EV_A);

        signal (w->signum, ev_sighandler);
# else
        struct sigaction sa;

        evpipe_init (EV_A);

        sa.sa_handler = ev_sighandler;
        sigfillset (&sa.sa_mask);
        sa.sa_flags = SA_RESTART; /* if restarting works we save one iteration */
        sigaction (w->signum, &sa, 0);

        if (origflags & EVFLAG_NOSIGMASK)
          {
            sigemptyset (&sa.sa_mask);
            sigaddset (&sa.sa_mask, w->signum);
            sigprocmask (SIG_UNBLOCK, &sa.sa_mask, 0);
          }
#endif
      }

  EV_FREQUENT_CHECK;
}

ecb_noinline
void
ev_signal_stop (EV_P_ ev_signal *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  wlist_del (&signals [w->signum - 1].head, (WL)w);
  ev_stop (EV_A_ (W)w);

  if (!signals [w->signum - 1].head)
    {
#if EV_MULTIPLICITY
      signals [w->signum - 1].loop = 0; /* unattach from signal */
#endif
#if EV_USE_SIGNALFD
      if (sigfd >= 0)
        {
          sigset_t ss;

          sigemptyset (&ss);
          sigaddset (&ss, w->signum);
          sigdelset (&sigfd_set, w->signum);

          signalfd (sigfd, &sigfd_set, 0);
          sigprocmask (SIG_UNBLOCK, &ss, 0);
        }
      else
#endif
        signal (w->signum, SIG_DFL);
    }

  EV_FREQUENT_CHECK;
}

#endif

#if EV_CHILD_ENABLE

void
ev_child_start (EV_P_ ev_child *w) EV_NOEXCEPT
{
#if EV_MULTIPLICITY
  assert (("libev: child watchers are only supported in the default loop", loop == ev_default_loop_ptr));
#endif
  if (ecb_expect_false (ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, 1);
  wlist_add (&childs [w->pid & ((EV_PID_HASHSIZE) - 1)], (WL)w);

  EV_FREQUENT_CHECK;
}

void
ev_child_stop (EV_P_ ev_child *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  wlist_del (&childs [w->pid & ((EV_PID_HASHSIZE) - 1)], (WL)w);
  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}

#endif

#if EV_STAT_ENABLE

# ifdef _WIN32
#  undef lstat
#  define lstat(a,b) _stati64 (a,b)
# endif

#define DEF_STAT_INTERVAL  5.0074891
#define NFS_STAT_INTERVAL 30.1074891 /* for filesystems potentially failing inotify */
#define MIN_STAT_INTERVAL  0.1074891

ecb_noinline static void stat_timer_cb (EV_P_ ev_timer *w_, int revents);

#if EV_USE_INOTIFY

/* the * 2 is to allow for alignment padding, which for some reason is >> 8 */
# define EV_INOTIFY_BUFSIZE (sizeof (struct inotify_event) * 2 + NAME_MAX)

ecb_noinline
static void
infy_add (EV_P_ ev_stat *w)
{
  w->wd = inotify_add_watch (fs_fd, w->path,
                             IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF | IN_MODIFY
                             | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
                             | IN_DONT_FOLLOW | IN_MASK_ADD);

  if (w->wd >= 0)
    {
      struct statfs sfs;

      /* now local changes will be tracked by inotify, but remote changes won't */
      /* unless the filesystem is known to be local, we therefore still poll */
      /* also do poll on <2.6.25, but with normal frequency */

      if (!fs_2625)
        w->timer.repeat = w->interval ? w->interval : DEF_STAT_INTERVAL;
      else if (!statfs (w->path, &sfs)
               && (sfs.f_type == 0x1373 /* devfs */
                   || sfs.f_type == 0x4006 /* fat */
                   || sfs.f_type == 0x4d44 /* msdos */
                   || sfs.f_type == 0xEF53 /* ext2/3 */
                   || sfs.f_type == 0x72b6 /* jffs2 */
                   || sfs.f_type == 0x858458f6 /* ramfs */
                   || sfs.f_type == 0x5346544e /* ntfs */
                   || sfs.f_type == 0x3153464a /* jfs */
                   || sfs.f_type == 0x9123683e /* btrfs */
                   || sfs.f_type == 0x52654973 /* reiser3 */
                   || sfs.f_type == 0x01021994 /* tmpfs */
                   || sfs.f_type == 0x58465342 /* xfs */))
        w->timer.repeat = 0.; /* filesystem is local, kernel new enough */
      else
        w->timer.repeat = w->interval ? w->interval : NFS_STAT_INTERVAL; /* remote, use reduced frequency */
    }
  else
    {
      /* can't use inotify, continue to stat */
      w->timer.repeat = w->interval ? w->interval : DEF_STAT_INTERVAL;

      /* if path is not there, monitor some parent directory for speedup hints */
      /* note that exceeding the hardcoded path limit is not a correctness issue, */
      /* but an efficiency issue only */
      if ((errno == ENOENT || errno == EACCES) && strlen (w->path) < 4096)
        {
          char path [4096];
          strcpy (path, w->path);

          do
            {
              int mask = IN_MASK_ADD | IN_DELETE_SELF | IN_MOVE_SELF
                       | (errno == EACCES ? IN_ATTRIB : IN_CREATE | IN_MOVED_TO);

              char *pend = strrchr (path, '/');

              if (!pend || pend == path)
                break;

              *pend = 0;
              w->wd = inotify_add_watch (fs_fd, path, mask);
            }
          while (w->wd < 0 && (errno == ENOENT || errno == EACCES));
        }
    }

  if (w->wd >= 0)
    wlist_add (&fs_hash [w->wd & ((EV_INOTIFY_HASHSIZE) - 1)].head, (WL)w);

  /* now re-arm timer, if required */
  if (ev_is_active (&w->timer)) ev_ref (EV_A);
  ev_timer_again (EV_A_ &w->timer);
  if (ev_is_active (&w->timer)) ev_unref (EV_A);
}

ecb_noinline
static void
infy_del (EV_P_ ev_stat *w)
{
  int slot;
  int wd = w->wd;

  if (wd < 0)
    return;

  w->wd = -2;
  slot = wd & ((EV_INOTIFY_HASHSIZE) - 1);
  wlist_del (&fs_hash [slot].head, (WL)w);

  /* remove this watcher, if others are watching it, they will rearm */
  inotify_rm_watch (fs_fd, wd);
}

ecb_noinline
static void
infy_wd (EV_P_ int slot, int wd, struct inotify_event *ev)
{
  if (slot < 0)
    /* overflow, need to check for all hash slots */
    for (slot = 0; slot < (EV_INOTIFY_HASHSIZE); ++slot)
      infy_wd (EV_A_ slot, wd, ev);
  else
    {
      WL w_;

      for (w_ = fs_hash [slot & ((EV_INOTIFY_HASHSIZE) - 1)].head; w_; )
        {
          ev_stat *w = (ev_stat *)w_;
          w_ = w_->next; /* lets us remove this watcher and all before it */

          if (w->wd == wd || wd == -1)
            {
              if (ev->mask & (IN_IGNORED | IN_UNMOUNT | IN_DELETE_SELF))
                {
                  wlist_del (&fs_hash [slot & ((EV_INOTIFY_HASHSIZE) - 1)].head, (WL)w);
                  w->wd = -1;
                  infy_add (EV_A_ w); /* re-add, no matter what */
                }

              stat_timer_cb (EV_A_ &w->timer, 0);
            }
        }
    }
}

static void
infy_cb (EV_P_ ev_io *w, int revents)
{
  char buf [EV_INOTIFY_BUFSIZE];
  int ofs;
  int len = read (fs_fd, buf, sizeof (buf));

  for (ofs = 0; ofs < len; )
    {
      struct inotify_event *ev = (struct inotify_event *)(buf + ofs);
      infy_wd (EV_A_ ev->wd, ev->wd, ev);
      ofs += sizeof (struct inotify_event) + ev->len;
    }
}

inline_size ecb_cold
void
ev_check_2625 (EV_P)
{
  /* kernels < 2.6.25 are borked
   * http://www.ussg.indiana.edu/hypermail/linux/kernel/0711.3/1208.html
   */
  if (ev_linux_version () < 0x020619)
    return;

  fs_2625 = 1;
}

inline_size int
infy_newfd (void)
{
#if defined IN_CLOEXEC && defined IN_NONBLOCK
  int fd = inotify_init1 (IN_CLOEXEC | IN_NONBLOCK);
  if (fd >= 0)
    return fd;
#endif
  return inotify_init ();
}

inline_size void
infy_init (EV_P)
{
  if (fs_fd != -2)
    return;

  fs_fd = -1;

  ev_check_2625 (EV_A);

  fs_fd = infy_newfd ();

  if (fs_fd >= 0)
    {
      fd_intern (fs_fd);
      ev_io_init (&fs_w, infy_cb, fs_fd, EV_READ);
      ev_set_priority (&fs_w, EV_MAXPRI);
      ev_io_start (EV_A_ &fs_w);
      ev_unref (EV_A);
    }
}

inline_size void
infy_fork (EV_P)
{
  int slot;

  if (fs_fd < 0)
    return;

  ev_ref (EV_A);
  ev_io_stop (EV_A_ &fs_w);
  close (fs_fd);
  fs_fd = infy_newfd ();

  if (fs_fd >= 0)
    {
      fd_intern (fs_fd);
      ev_io_set (&fs_w, fs_fd, EV_READ);
      ev_io_start (EV_A_ &fs_w);
      ev_unref (EV_A);
    }

  for (slot = 0; slot < (EV_INOTIFY_HASHSIZE); ++slot)
    {
      WL w_ = fs_hash [slot].head;
      fs_hash [slot].head = 0;

      while (w_)
        {
          ev_stat *w = (ev_stat *)w_;
          w_ = w_->next; /* lets us add this watcher */

          w->wd = -1;

          if (fs_fd >= 0)
            infy_add (EV_A_ w); /* re-add, no matter what */
          else
            {
              w->timer.repeat = w->interval ? w->interval : DEF_STAT_INTERVAL;
              if (ev_is_active (&w->timer)) ev_ref (EV_A);
              ev_timer_again (EV_A_ &w->timer);
              if (ev_is_active (&w->timer)) ev_unref (EV_A);
            }
        }
    }
}

#endif

#ifdef _WIN32
# define EV_LSTAT(p,b) _stati64 (p, b)
#else
# define EV_LSTAT(p,b) lstat (p, b)
#endif

void
ev_stat_stat (EV_P_ ev_stat *w) EV_NOEXCEPT
{
  if (lstat (w->path, &w->attr) < 0)
    w->attr.st_nlink = 0;
  else if (!w->attr.st_nlink)
    w->attr.st_nlink = 1;
}

ecb_noinline
static void
stat_timer_cb (EV_P_ ev_timer *w_, int revents)
{
  ev_stat *w = (ev_stat *)(((char *)w_) - offsetof (ev_stat, timer));

  ev_statdata prev = w->attr;
  ev_stat_stat (EV_A_ w);

  /* memcmp doesn't work on netbsd, they.... do stuff to their struct stat */
  if (
    prev.st_dev      != w->attr.st_dev
    || prev.st_ino   != w->attr.st_ino
    || prev.st_mode  != w->attr.st_mode
    || prev.st_nlink != w->attr.st_nlink
    || prev.st_uid   != w->attr.st_uid
    || prev.st_gid   != w->attr.st_gid
    || prev.st_rdev  != w->attr.st_rdev
    || prev.st_size  != w->attr.st_size
    || prev.st_atime != w->attr.st_atime
    || prev.st_mtime != w->attr.st_mtime
    || prev.st_ctime != w->attr.st_ctime
  ) {
      /* we only update w->prev on actual differences */
      /* in case we test more often than invoke the callback, */
      /* to ensure that prev is always different to attr */
      w->prev = prev;

      #if EV_USE_INOTIFY
        if (fs_fd >= 0)
          {
            infy_del (EV_A_ w);
            infy_add (EV_A_ w);
            ev_stat_stat (EV_A_ w); /* avoid race... */
          }
      #endif

      ev_feed_event (EV_A_ w, EV_STAT);
    }
}

void
ev_stat_start (EV_P_ ev_stat *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  ev_stat_stat (EV_A_ w);

  if (w->interval < MIN_STAT_INTERVAL && w->interval)
    w->interval = MIN_STAT_INTERVAL;

  ev_timer_init (&w->timer, stat_timer_cb, 0., w->interval ? w->interval : DEF_STAT_INTERVAL);
  ev_set_priority (&w->timer, ev_priority (w));

#if EV_USE_INOTIFY
  infy_init (EV_A);

  if (fs_fd >= 0)
    infy_add (EV_A_ w);
  else
#endif
    {
      ev_timer_again (EV_A_ &w->timer);
      ev_unref (EV_A);
    }

  ev_start (EV_A_ (W)w, 1);

  EV_FREQUENT_CHECK;
}

void
ev_stat_stop (EV_P_ ev_stat *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

#if EV_USE_INOTIFY
  infy_del (EV_A_ w);
#endif

  if (ev_is_active (&w->timer))
    {
      ev_ref (EV_A);
      ev_timer_stop (EV_A_ &w->timer);
    }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_IDLE_ENABLE
void
ev_idle_start (EV_P_ ev_idle *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  pri_adjust (EV_A_ (W)w);

  EV_FREQUENT_CHECK;

  {
    int active = ++idlecnt [ABSPRI (w)];

    ++idleall;
    ev_start (EV_A_ (W)w, active);

    array_needsize (ev_idle *, idles [ABSPRI (w)], idlemax [ABSPRI (w)], active, array_needsize_noinit);
    idles [ABSPRI (w)][active - 1] = w;
  }

  EV_FREQUENT_CHECK;
}

void
ev_idle_stop (EV_P_ ev_idle *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    idles [ABSPRI (w)][active - 1] = idles [ABSPRI (w)][--idlecnt [ABSPRI (w)]];
    ev_active (idles [ABSPRI (w)][active - 1]) = active;

    ev_stop (EV_A_ (W)w);
    --idleall;
  }

  EV_FREQUENT_CHECK;
}
#endif

#if EV_PREPARE_ENABLE
void
ev_prepare_start (EV_P_ ev_prepare *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, ++preparecnt);
  array_needsize (ev_prepare *, prepares, preparemax, preparecnt, array_needsize_noinit);
  prepares [preparecnt - 1] = w;

  EV_FREQUENT_CHECK;
}

void
ev_prepare_stop (EV_P_ ev_prepare *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    prepares [active - 1] = prepares [--preparecnt];
    ev_active (prepares [active - 1]) = active;
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_CHECK_ENABLE
void
ev_check_start (EV_P_ ev_check *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, ++checkcnt);
  array_needsize (ev_check *, checks, checkmax, checkcnt, array_needsize_noinit);
  checks [checkcnt - 1] = w;

  EV_FREQUENT_CHECK;
}

void
ev_check_stop (EV_P_ ev_check *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    checks [active - 1] = checks [--checkcnt];
    ev_active (checks [active - 1]) = active;
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_EMBED_ENABLE
ecb_noinline
void
ev_embed_sweep (EV_P_ ev_embed *w) EV_NOEXCEPT
{
  ev_run (w->other, EVRUN_NOWAIT);
}

static void
embed_io_cb (EV_P_ ev_io *io, int revents)
{
  ev_embed *w = (ev_embed *)(((char *)io) - offsetof (ev_embed, io));

  if (ev_cb (w))
    ev_feed_event (EV_A_ (W)w, EV_EMBED);
  else
    ev_run (w->other, EVRUN_NOWAIT);
}

static void
embed_prepare_cb (EV_P_ ev_prepare *prepare, int revents)
{
  ev_embed *w = (ev_embed *)(((char *)prepare) - offsetof (ev_embed, prepare));

  {
    EV_P = w->other;

    while (fdchangecnt)
      {
        fd_reify (EV_A);
        ev_run (EV_A_ EVRUN_NOWAIT);
      }
  }
}

#if EV_FORK_ENABLE
static void
embed_fork_cb (EV_P_ ev_fork *fork_w, int revents)
{
  ev_embed *w = (ev_embed *)(((char *)fork_w) - offsetof (ev_embed, fork));

  ev_embed_stop (EV_A_ w);

  {
    EV_P = w->other;

    ev_loop_fork (EV_A);
    ev_run (EV_A_ EVRUN_NOWAIT);
  }

  ev_embed_start (EV_A_ w);
}
#endif

#if 0
static void
embed_idle_cb (EV_P_ ev_idle *idle, int revents)
{
  ev_idle_stop (EV_A_ idle);
}
#endif

void
ev_embed_start (EV_P_ ev_embed *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  {
    EV_P = w->other;
    assert (("libev: loop to be embedded is not embeddable", backend & ev_embeddable_backends ()));
    ev_io_init (&w->io, embed_io_cb, backend_fd, EV_READ);
  }

  EV_FREQUENT_CHECK;

  ev_set_priority (&w->io, ev_priority (w));
  ev_io_start (EV_A_ &w->io);

  ev_prepare_init (&w->prepare, embed_prepare_cb);
  ev_set_priority (&w->prepare, EV_MINPRI);
  ev_prepare_start (EV_A_ &w->prepare);

#if EV_FORK_ENABLE
  ev_fork_init (&w->fork, embed_fork_cb);
  ev_fork_start (EV_A_ &w->fork);
#endif

  /*ev_idle_init (&w->idle, e,bed_idle_cb);*/

  ev_start (EV_A_ (W)w, 1);

  EV_FREQUENT_CHECK;
}

void
ev_embed_stop (EV_P_ ev_embed *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_io_stop      (EV_A_ &w->io);
  ev_prepare_stop (EV_A_ &w->prepare);
#if EV_FORK_ENABLE
  ev_fork_stop    (EV_A_ &w->fork);
#endif

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_FORK_ENABLE
void
ev_fork_start (EV_P_ ev_fork *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, ++forkcnt);
  array_needsize (ev_fork *, forks, forkmax, forkcnt, array_needsize_noinit);
  forks [forkcnt - 1] = w;

  EV_FREQUENT_CHECK;
}

void
ev_fork_stop (EV_P_ ev_fork *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    forks [active - 1] = forks [--forkcnt];
    ev_active (forks [active - 1]) = active;
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_CLEANUP_ENABLE
void
ev_cleanup_start (EV_P_ ev_cleanup *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, ++cleanupcnt);
  array_needsize (ev_cleanup *, cleanups, cleanupmax, cleanupcnt, array_needsize_noinit);
  cleanups [cleanupcnt - 1] = w;

  /* cleanup watchers should never keep a refcount on the loop */
  ev_unref (EV_A);
  EV_FREQUENT_CHECK;
}

void
ev_cleanup_stop (EV_P_ ev_cleanup *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;
  ev_ref (EV_A);

  {
    int active = ev_active (w);

    cleanups [active - 1] = cleanups [--cleanupcnt];
    ev_active (cleanups [active - 1]) = active;
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}
#endif

#if EV_ASYNC_ENABLE
void
ev_async_start (EV_P_ ev_async *w) EV_NOEXCEPT
{
  if (ecb_expect_false (ev_is_active (w)))
    return;

  w->sent = 0;

  evpipe_init (EV_A);

  EV_FREQUENT_CHECK;

  ev_start (EV_A_ (W)w, ++asynccnt);
  array_needsize (ev_async *, asyncs, asyncmax, asynccnt, array_needsize_noinit);
  asyncs [asynccnt - 1] = w;

  EV_FREQUENT_CHECK;
}

void
ev_async_stop (EV_P_ ev_async *w) EV_NOEXCEPT
{
  clear_pending (EV_A_ (W)w);
  if (ecb_expect_false (!ev_is_active (w)))
    return;

  EV_FREQUENT_CHECK;

  {
    int active = ev_active (w);

    asyncs [active - 1] = asyncs [--asynccnt];
    ev_active (asyncs [active - 1]) = active;
  }

  ev_stop (EV_A_ (W)w);

  EV_FREQUENT_CHECK;
}

void
ev_async_send (EV_P_ ev_async *w) EV_NOEXCEPT
{
  w->sent = 1;
  evpipe_write (EV_A_ &async_pending);
}
#endif

/*****************************************************************************/

struct ev_once
{
  ev_io io;
  ev_timer to;
  void (*cb)(int revents, void *arg);
  void *arg;
};

static void
once_cb (EV_P_ struct ev_once *once, int revents)
{
  void (*cb)(int revents, void *arg) = once->cb;
  void *arg = once->arg;

  ev_io_stop    (EV_A_ &once->io);
  ev_timer_stop (EV_A_ &once->to);
  ev_free (once);

  cb (revents, arg);
}

static void
once_cb_io (EV_P_ ev_io *w, int revents)
{
  struct ev_once *once = (struct ev_once *)(((char *)w) - offsetof (struct ev_once, io));

  once_cb (EV_A_ once, revents | ev_clear_pending (EV_A_ &once->to));
}

static void
once_cb_to (EV_P_ ev_timer *w, int revents)
{
  struct ev_once *once = (struct ev_once *)(((char *)w) - offsetof (struct ev_once, to));

  once_cb (EV_A_ once, revents | ev_clear_pending (EV_A_ &once->io));
}

void
ev_once (EV_P_ int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg) EV_NOEXCEPT
{
  struct ev_once *once = (struct ev_once *)ev_malloc (sizeof (struct ev_once));

  once->cb  = cb;
  once->arg = arg;

  ev_init (&once->io, once_cb_io);
  if (fd >= 0)
    {
      ev_io_set (&once->io, fd, events);
      ev_io_start (EV_A_ &once->io);
    }

  ev_init (&once->to, once_cb_to);
  if (timeout >= 0.)
    {
      ev_timer_set (&once->to, timeout, 0.);
      ev_timer_start (EV_A_ &once->to);
    }
}

/*****************************************************************************/

#if EV_WALK_ENABLE
ecb_cold
void
ev_walk (EV_P_ int types, void (*cb)(EV_P_ int type, void *w)) EV_NOEXCEPT
{
  int i, j;
  ev_watcher_list *wl, *wn;

  if (types & (EV_IO | EV_EMBED))
    for (i = 0; i < anfdmax; ++i)
      for (wl = anfds [i].head; wl; )
        {
          wn = wl->next;

#if EV_EMBED_ENABLE
          if (ev_cb ((ev_io *)wl) == embed_io_cb)
            {
              if (types & EV_EMBED)
                cb (EV_A_ EV_EMBED, ((char *)wl) - offsetof (struct ev_embed, io));
            }
          else
#endif
#if EV_USE_INOTIFY
          if (ev_cb ((ev_io *)wl) == infy_cb)
            ;
          else
#endif
          if ((ev_io *)wl != &pipe_w)
            if (types & EV_IO)
              cb (EV_A_ EV_IO, wl);

          wl = wn;
        }

  if (types & (EV_TIMER | EV_STAT))
    for (i = timercnt + HEAP0; i-- > HEAP0; )
#if EV_STAT_ENABLE
      /*TODO: timer is not always active*/
      if (ev_cb ((ev_timer *)ANHE_w (timers [i])) == stat_timer_cb)
        {
          if (types & EV_STAT)
            cb (EV_A_ EV_STAT, ((char *)ANHE_w (timers [i])) - offsetof (struct ev_stat, timer));
        }
      else
#endif
      if (types & EV_TIMER)
        cb (EV_A_ EV_TIMER, ANHE_w (timers [i]));

#if EV_PERIODIC_ENABLE
  if (types & EV_PERIODIC)
    for (i = periodiccnt + HEAP0; i-- > HEAP0; )
      cb (EV_A_ EV_PERIODIC, ANHE_w (periodics [i]));
#endif

#if EV_IDLE_ENABLE
  if (types & EV_IDLE)
    for (j = NUMPRI; j--; )
      for (i = idlecnt [j]; i--; )
        cb (EV_A_ EV_IDLE, idles [j][i]);
#endif

#if EV_FORK_ENABLE
  if (types & EV_FORK)
    for (i = forkcnt; i--; )
      if (ev_cb (forks [i]) != embed_fork_cb)
        cb (EV_A_ EV_FORK, forks [i]);
#endif

#if EV_ASYNC_ENABLE
  if (types & EV_ASYNC)
    for (i = asynccnt; i--; )
      cb (EV_A_ EV_ASYNC, asyncs [i]);
#endif

#if EV_PREPARE_ENABLE
  if (types & EV_PREPARE)
    for (i = preparecnt; i--; )
# if EV_EMBED_ENABLE
      if (ev_cb (prepares [i]) != embed_prepare_cb)
# endif
        cb (EV_A_ EV_PREPARE, prepares [i]);
#endif

#if EV_CHECK_ENABLE
  if (types & EV_CHECK)
    for (i = checkcnt; i--; )
      cb (EV_A_ EV_CHECK, checks [i]);
#endif

#if EV_SIGNAL_ENABLE
  if (types & EV_SIGNAL)
    for (i = 0; i < EV_NSIG - 1; ++i)
      for (wl = signals [i].head; wl; )
        {
          wn = wl->next;
          cb (EV_A_ EV_SIGNAL, wl);
          wl = wn;
        }
#endif

#if EV_CHILD_ENABLE
  if (types & EV_CHILD)
    for (i = (EV_PID_HASHSIZE); i--; )
      for (wl = childs [i]; wl; )
        {
          wn = wl->next;
          cb (EV_A_ EV_CHILD, wl);
          wl = wn;
        }
#endif
/* EV_STAT     0x00001000 /* stat data changed */
/* EV_EMBED    0x00010000 /* embedded event loop needs sweep */
}
#endif

#if EV_MULTIPLICITY
  #include "ev_wrap.h"
#endif


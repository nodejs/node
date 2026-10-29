/*
 * Copyright 1995-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_E_OS_H
#define OSSL_E_OS_H

#include <limits.h>
#include <openssl/opensslconf.h>

#include <openssl/e_os2.h>
#include <openssl/crypto.h>
#include "internal/numbers.h" /* Ensure the definition of SIZE_MAX */

/*
 * <openssl/e_os2.h> contains what we can justify to make visible to the
 * outside; this file e_os.h is not part of the exported interface.
 */

#if defined(OPENSSL_SYS_VXWORKS) || defined(OPENSSL_SYS_UEFI)
#define NO_CHMOD
#define NO_SYSLOG
#endif

#define get_last_sys_error() errno
#define clear_sys_error() errno = 0
#define set_sys_error(e) errno = (e)

/********************************************************************
 The Microsoft section
 ********************************************************************/
#if defined(OPENSSL_SYS_WIN32) && !defined(WIN32)
#define WIN32
#endif
#if defined(OPENSSL_SYS_WINDOWS) && !defined(WINDOWS)
#define WINDOWS
#endif
#if defined(OPENSSL_SYS_MSDOS) && !defined(MSDOS)
#define MSDOS
#endif

#ifdef WIN32
#undef get_last_sys_error
#undef clear_sys_error
#undef set_sys_error
#define get_last_sys_error() GetLastError()
#define clear_sys_error() SetLastError(0)
#define set_sys_error(e) SetLastError(e)
#if !defined(WINNT)
#define WIN_CONSOLE_BUG
#endif
#else
#endif

#if (defined(WINDOWS) || defined(MSDOS))

#ifdef __DJGPP__
#include <unistd.h>
#include <sys/stat.h>
#define _setmode setmode
#define _O_TEXT O_TEXT
#define _O_BINARY O_BINARY
#undef DEVRANDOM_EGD /*  Neither MS-DOS nor FreeDOS provide 'egd' sockets.  */
#undef DEVRANDOM
#define DEVRANDOM "/dev/urandom\x24"
#endif /* __DJGPP__ */

#ifndef S_IFDIR
#define S_IFDIR _S_IFDIR
#endif

#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif

#if !defined(WINNT) && !defined(__DJGPP__)
#define NO_SYSLOG
#endif

#ifdef WINDOWS
#if !defined(_WIN32_WINNT)
/*
 * The _WIN32_WINNT is described here:
 * https://learn.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-170
 * In a nutshell the macro defines minimal required Windows version where
 * the resulting application is guaranteed to run on. If left undefined here,
 * then the definition is provided by the Windows SDK found on host where
 * application is being built.
 *
 * OpenSSL defaults to version 0x501, which matches Windows XP, meaning the
 * compiled library will use APIs available on Windows XP and later.  User may
 * override the version specified here at build time using command as
 * follows:
 *     perl ./Configure "-D_WIN32_WINNT=0x...." ...
 *
 * The list of recognized constants (as found in the link above) is as follows:
 * 	0x0400 // Windows NT 4.0
 *	0x0500 // Windows 2000
 *	0x0501 // Windows XP
 *	0x0502 // Windows Server 2003
 *	0x0600 // Windows Vista, Windows Server 2008, Windows Vista
 *	0x0601 // Windows 7
 *	0x0602 // Windows 8
 *	0x0603 // Windows 8.1
 *	0x0A00 // Windows 10
 */
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#if defined(_MSC_VER) && !defined(_DLL) && defined(stdin)
#if _MSC_VER >= 1300 && _MSC_VER < 1600
#undef stdin
#undef stdout
#undef stderr
FILE *__iob_func(void);
#define stdin (&__iob_func()[0])
#define stdout (&__iob_func()[1])
#define stderr (&__iob_func()[2])
#endif
#endif
#endif

#include <io.h>
#include <fcntl.h>

#define EXIT(n) exit(n)
#define LIST_SEPARATOR_CHAR ';'
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
#define DEFAULT_HOME "C:"

/* Avoid Visual Studio 13 GetVersion deprecated problems */
#if defined(_MSC_VER) && _MSC_VER >= 1800
#define check_winnt() (1)
#define check_win_minplat(x) (1)
#else
#define check_winnt() (GetVersion() < 0x80000000)
#define check_win_minplat(x) (LOBYTE(LOWORD(GetVersion())) >= (x))
#endif

/*
 * MSVC versions earlier than Visual Studio 2015 (_MSC_VER < 1900) do not
 * declare or define C99 snprintf or vsnprintf.  Definitions are supplied
 * by crypto/msvc2013_snprintf.c, which is built only on the matching
 * Configure target variants.
 */
#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdarg.h>
int msvc_translate_printf_format(const char *format, const char **out,
    char **tmp);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list args);
#endif

#else /* The non-microsoft world */

#if defined(OPENSSL_SYS_VXWORKS)
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifdef OPENSSL_SYS_VMS
#define VMS 1
/*
 * some programs don't include stdlib, so exit() and others give implicit
 * function warnings
 */
#include <stdlib.h>
#if defined(__DECC)
#include <unistd.h>
#else
#include <unixlib.h>
#endif
#define LIST_SEPARATOR_CHAR ','
/* We don't have any well-defined random devices on VMS, yet... */
#undef DEVRANDOM
/*-
   We need to do this since VMS has the following coding on status codes:

   Bits 0-2: status type: 0 = warning, 1 = success, 2 = error, 3 = info ...
             The important thing to know is that odd numbers are considered
             good, while even ones are considered errors.
   Bits 3-15: actual status number
   Bits 16-27: facility number.  0 is considered "unknown"
   Bits 28-31: control bits.  If bit 28 is set, the shell won't try to
               output the message (which, for random codes, just looks ugly)

   So, what we do here is to change 0 to 1 to get the default success status,
   and everything else is shifted up to fit into the status number field, and
   the status is tagged as an error, which is what is wanted here.

   Finally, we add the VMS C facility code 0x35a000, because there are some
   programs, such as Perl, that will reinterpret the code back to something
   POSIX.  'man perlvms' explains it further.

   NOTE: the perlvms manual wants to turn all codes 2 to 255 into success
   codes (status type = 1).  I couldn't disagree more.  Fortunately, the
   status type doesn't seem to bother Perl.
   -- Richard Levitte
*/
#define EXIT(n) exit((n) ? (((n) << 3) | 2 | 0x10000000 | 0x35a000) : 1)

#define DEFAULT_HOME "SYS$LOGIN:"

#else
/* !defined VMS */
#include <unistd.h>
#include <sys/types.h>
#ifdef OPENSSL_SYS_WIN32_CYGWIN
#include <io.h>
#include <fcntl.h>
#endif

#define LIST_SEPARATOR_CHAR ':'
#define EXIT(n) exit(n)
#endif

#endif

/***********************************************/

#if defined(OPENSSL_SYS_WINDOWS)
#if defined(_MSC_VER) && (_MSC_VER >= 1310)
#define open _open
#define fdopen _fdopen
#define close _close
#ifndef strdup
#define strdup _strdup
#endif
#define unlink _unlink
#define fileno _fileno
#define isatty _isatty
#endif
#else
#include <strings.h>
#endif

/* vxworks */
#if defined(OPENSSL_SYS_VXWORKS)
#include <ioLib.h>
#include <tickLib.h>
#include <sysLib.h>
#include <vxWorks.h>
#include <sockLib.h>
#include <taskLib.h>

typedef int TTY_STRUCT;
#define sleep(a) taskDelay((a) * sysClkRateGet())

/*
 * NOTE: these are implemented by helpers in database app! if the database is
 * not linked, we need to implement them elsewhere
 */
struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const char *addr, int length, int type);
struct servent *getservbyname(const char *name, const char *proto);

#endif
/* end vxworks */

/* ----------------------------- HP NonStop -------------------------------- */
/* Required to support platform variant without getpid() and pid_t. */
#if defined(__TANDEM) && defined(_GUARDIAN_TARGET)
#include <strings.h>
#include <netdb.h>
#define getservbyname(name, proto) getservbyname((char *)name, proto)
#define gethostbyname(name) gethostbyname((char *)name)
#define ioctlsocket(a, b, c) ioctl(a, b, c)
#ifdef NO_GETPID
inline int nssgetpid(void);
#ifndef NSSGETPID_MACRO
#define NSSGETPID_MACRO
#include <cextdecs.h(PROCESSHANDLE_GETMINE_)>
#include <cextdecs.h(PROCESSHANDLE_DECOMPOSE_)>
inline int nssgetpid(void)
{
    short phandle[10] = { 0 };
    union pseudo_pid {
        struct {
            short cpu;
            short pin;
        } cpu_pin;
        int ppid;
    } ppid = { 0 };
    PROCESSHANDLE_GETMINE_(phandle);
    PROCESSHANDLE_DECOMPOSE_(phandle, &ppid.cpu_pin.cpu, &ppid.cpu_pin.pin);
    return ppid.ppid;
}
#define getpid(a) nssgetpid(a)
#endif /* NSSGETPID_MACRO */
#endif /* NO_GETPID */
/*#  define setsockopt(a,b,c,d,f) setsockopt(a,b,c,(char*)d,f)*/
/*#  define getsockopt(a,b,c,d,f) getsockopt(a,b,c,(char*)d,f)*/
/*#  define connect(a,b,c) connect(a,(struct sockaddr *)b,c)*/
/*#  define bind(a,b,c) bind(a,(struct sockaddr *)b,c)*/
/*#  define sendto(a,b,c,d,e,f) sendto(a,(char*)b,c,d,(struct sockaddr *)e,f)*/
#if defined(OPENSSL_THREADS) && !defined(_PUT_MODEL_)
/*
 * HPNS SPT threads
 */
#define SPT_THREAD_SIGNAL 1
#define SPT_THREAD_AWARE 1
#include <spthread.h>
#undef close
#define close spt_close
/*
#   define get_last_socket_error()	errno
#   define clear_socket_error()	errno=0
#   define ioctlsocket(a,b,c)	ioctl(a,b,c)
#   define closesocket(s)		close(s)
#   define readsocket(s,b,n)	read((s),(char*)(b),(n))
#   define writesocket(s,b,n)	write((s),(char*)(b),(n)
*/
#define accept(a, b, c) accept(a, (struct sockaddr *)b, c)
#define recvfrom(a, b, c, d, e, f) recvfrom(a, b, (socklen_t)c, d, e, f)
#endif
#endif

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#define CRYPTO_memcmp memcmp
#endif

#ifndef OPENSSL_NO_SECURE_MEMORY
/* unistd.h defines _POSIX_VERSION */
#if (defined(OPENSSL_SYS_UNIX)                                 \
    && ((defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) \
        || defined(__sun) || defined(__hpux) || defined(__sgi) \
        || defined(__osf__)))                                  \
    || defined(_WIN32)
/* secure memory is implemented */
#else
#define OPENSSL_NO_SECURE_MEMORY
#endif
#endif

/*
 * str[n]casecmp_l is defined in POSIX 2008-01. Value is taken accordingly
 * https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html
 * There are also equivalent functions on Windows.
 * There is no locale_t on NONSTOP.
 */
#if defined(OPENSSL_SYS_WINDOWS)
typedef _locale_t locale_t;
#define freelocale _free_locale
#define strcasecmp_l _stricmp_l
#define strncasecmp_l _strnicmp_l
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#elif !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L \
    || defined(OPENSSL_SYS_TANDEM)
#ifndef OPENSSL_NO_LOCALE
#define OPENSSL_NO_LOCALE
#endif
#endif

#endif

/*
 * Can we use a global destructor?  We can use a global destructor via
 * __attribute__ on anything like a modern gcc/clang.  We can also use
 * it via dllmain on anything win32/win64.
 *
 * Older things may not do this.
 * The assumption here is then if you don't have destructor support,
 * it is safe to call OPENSSL_cleanup before an application exits
 * because no library it is linked with will run code in a destructor
 * that will call into OpenSSL after exit() happens.
 *
 */
#if defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_WIN64)
#define OSSL_CLEANUP_USING_DESTRUCTOR
#define OSSL_DLLMAIN_DESTRUCTOR
/*
 * destructor will be installed in libcrypto's dllmain.c
 * This means effectively anything not win16 or dos will handle
 * this.
 */
void ossl_cleanup_destructor(void);
#else
#if defined(__has_attribute)
#if __has_attribute(destructor)
/*
 * This seems to have been a thing with any gcc or clang since the
 * early 2000's. So this could pretty much instead be just unconditional
 * on __GNUC__ or __clang__.
 */
#define OSSL_CLEANUP_USING_DESTRUCTOR
/* destructor is installed by compiler */
void ossl_cleanup_destructor(void) __attribute__((destructor));
#else
/* We are not using a destructor */
/*
 * So we are on something that is not close to Windows or being
 * compiled with a modern GCC/Clang derivative. either way
 * this probably means something like a toolchain that is
 * more than 20 years old.
 */
void ossl_cleanup_destructor(void);
#endif /* defined (__has_attribute(destructor) */
#endif /* defined (__has_attribute) */
#endif /* defined(OPENSSL_SYS_WIN32) || defined(OPENSSL_SYS_WIN64) */

/* e_os.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef HEADER_E_OS_H
# define HEADER_E_OS_H

# include <openssl/opensslconf.h>

# include <openssl/e_os2.h>
/*
 * <openssl/e_os2.h> contains what we can justify to make visible to the
 * outside; this file e_os.h is not part of the exported interface.
 */

#ifdef  __cplusplus
extern "C" {
#endif

/* Used to checking reference counts, most while doing perl5 stuff :-) */
# ifdef REF_PRINT
#  undef REF_PRINT
#  define REF_PRINT(a,b)  fprintf(stderr,"%08X:%4d:%s\n",(int)b,b->references,a)
# endif

# ifndef DEVRANDOM
/*
 * set this to a comma-separated list of 'random' device files to try out. My
 * default, we will try to read at least one of these files
 */
#  define DEVRANDOM "/dev/urandom","/dev/random","/dev/srandom"
# endif
# ifndef DEVRANDOM_EGD
/*
 * set this to a comma-seperated list of 'egd' sockets to try out. These
 * sockets will be tried in the order listed in case accessing the device
 * files listed in DEVRANDOM did not return enough entropy.
 */
#  define DEVRANDOM_EGD "/var/run/egd-pool","/dev/egd-pool","/etc/egd-pool","/etc/entropy"
# endif

# if defined(OPENSSL_SYS_VXWORKS)
#  define NO_SYS_PARAM_H
#  define NO_CHMOD
#  define NO_SYSLOG
# endif

# if defined(OPENSSL_SYS_MACINTOSH_CLASSIC)
#  if macintosh==1
#   ifndef MAC_OS_GUSI_SOURCE
#    define MAC_OS_pre_X
#    define NO_SYS_TYPES_H
#   endif
#   define NO_SYS_PARAM_H
#   define NO_CHMOD
#   define NO_SYSLOG
#   undef  DEVRANDOM
#   define GETPID_IS_MEANINGLESS
#  endif
# endif

/********************************************************************
 The Microsoft section
 ********************************************************************/
/*
 * The following is used because of the small stack in some Microsoft
 * operating systems
 */
# if defined(OPENSSL_SYS_MSDOS) && !defined(OPENSSL_SYSNAME_WIN32)
#  define MS_STATIC     static
# else
#  define MS_STATIC
# endif

# if defined(OPENSSL_SYS_WIN32) && !defined(WIN32)
#  define WIN32
# endif
# if defined(OPENSSL_SYS_WINDOWS) && !defined(WINDOWS)
#  define WINDOWS
# endif
# if defined(OPENSSL_SYS_MSDOS) && !defined(MSDOS)
#  define MSDOS
# endif

# if defined(MSDOS) && !defined(GETPID_IS_MEANINGLESS)
#  define GETPID_IS_MEANINGLESS
# endif

# ifdef WIN32
#  define get_last_sys_error()    GetLastError()
#  define clear_sys_error()       SetLastError(0)
#  if !defined(WINNT)
#   define WIN_CONSOLE_BUG
#  endif
# else
#  define get_last_sys_error()    errno
#  define clear_sys_error()       errno=0
# endif

# if defined(WINDOWS)
#  define get_last_socket_error() WSAGetLastError()
#  define clear_socket_error()    WSASetLastError(0)
#  define readsocket(s,b,n)       recv((s),(b),(n),0)
#  define writesocket(s,b,n)      send((s),(b),(n),0)
# elif defined(__DJGPP__)
#  define WATT32
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define closesocket(s)          close_s(s)
#  define readsocket(s,b,n)       read_s(s,b,n)
#  define writesocket(s,b,n)      send(s,b,n,0)
# elif defined(MAC_OS_pre_X)
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define closesocket(s)          MacSocket_close(s)
#  define readsocket(s,b,n)       MacSocket_recv((s),(b),(n),true)
#  define writesocket(s,b,n)      MacSocket_send((s),(b),(n))
# elif defined(OPENSSL_SYS_VMS)
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define ioctlsocket(a,b,c)      ioctl(a,b,c)
#  define closesocket(s)          close(s)
#  define readsocket(s,b,n)       recv((s),(b),(n),0)
#  define writesocket(s,b,n)      send((s),(b),(n),0)
# elif defined(OPENSSL_SYS_VXWORKS)
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define ioctlsocket(a,b,c)          ioctl((a),(b),(int)(c))
#  define closesocket(s)              close(s)
#  define readsocket(s,b,n)           read((s),(b),(n))
#  define writesocket(s,b,n)          write((s),(char *)(b),(n))
# elif defined(OPENSSL_SYS_BEOS_R5)
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define FIONBIO SO_NONBLOCK
#  define ioctlsocket(a,b,c)                setsockopt((a),SOL_SOCKET,(b),(c),sizeof(*(c)))
#  define readsocket(s,b,n)       recv((s),(b),(n),0)
#  define writesocket(s,b,n)      send((s),(b),(n),0)
# elif defined(OPENSSL_SYS_NETWARE)
#  if defined(NETWARE_BSDSOCK)
#   define get_last_socket_error() errno
#   define clear_socket_error()    errno=0
#   define closesocket(s)          close(s)
#   define ioctlsocket(a,b,c)      ioctl(a,b,c)
#   if defined(NETWARE_LIBC)
#    define readsocket(s,b,n)       recv((s),(b),(n),0)
#    define writesocket(s,b,n)      send((s),(b),(n),0)
#   else
#    define readsocket(s,b,n)       recv((s),(char*)(b),(n),0)
#    define writesocket(s,b,n)      send((s),(char*)(b),(n),0)
#   endif
#  else
#   define get_last_socket_error() WSAGetLastError()
#   define clear_socket_error()    WSASetLastError(0)
#   define readsocket(s,b,n)               recv((s),(b),(n),0)
#   define writesocket(s,b,n)              send((s),(b),(n),0)
#  endif
# else
#  define get_last_socket_error() errno
#  define clear_socket_error()    errno=0
#  define ioctlsocket(a,b,c)      ioctl(a,b,c)
#  define closesocket(s)          close(s)
#  define readsocket(s,b,n)       read((s),(b),(n))
#  define writesocket(s,b,n)      write((s),(b),(n))
# endif

# ifdef WIN16                   /* never the case */
#  define MS_CALLBACK   _far _loadds
#  define MS_FAR        _far
# else
#  define MS_CALLBACK
#  define MS_FAR
# endif

# ifdef OPENSSL_NO_STDIO
#  undef OPENSSL_NO_FP_API
#  define OPENSSL_NO_FP_API
# endif

# if (defined(WINDOWS) || defined(MSDOS))

#  ifdef __DJGPP__
#   include <unistd.h>
#   include <sys/stat.h>
#   include <sys/socket.h>
#   include <tcp.h>
#   include <netdb.h>
#   define _setmode setmode
#   define _O_TEXT O_TEXT
#   define _O_BINARY O_BINARY
#   undef DEVRANDOM
#   define DEVRANDOM "/dev/urandom\x24"
#  endif                        /* __DJGPP__ */

#  ifndef S_IFDIR
#   define S_IFDIR     _S_IFDIR
#  endif

#  ifndef S_IFMT
#   define S_IFMT      _S_IFMT
#  endif

#  if !defined(WINNT) && !defined(__DJGPP__)
#   define NO_SYSLOG
#  endif
#  define NO_DIRENT

#  ifdef WINDOWS
#   if !defined(_WIN32_WCE) && !defined(_WIN32_WINNT)
       /*
        * Defining _WIN32_WINNT here in e_os.h implies certain "discipline."
        * Most notably we ought to check for availability of each specific
        * routine with GetProcAddress() and/or guard NT-specific calls with
        * GetVersion() < 0x80000000. One can argue that in latter "or" case
        * we ought to /DELAYLOAD some .DLLs in order to protect ourselves
        * against run-time link errors. This doesn't seem to be necessary,
        * because it turned out that already Windows 95, first non-NT Win32
        * implementation, is equipped with at least NT 3.51 stubs, dummy
        * routines with same name, but which do nothing. Meaning that it's
        * apparently sufficient to guard "vanilla" NT calls with GetVersion
        * alone, while NT 4.0 and above interfaces ought to be linked with
        * GetProcAddress at run-time.
        */
#    define _WIN32_WINNT 0x0400
#   endif
#   if !defined(OPENSSL_NO_SOCK) && defined(_WIN32_WINNT)
       /*
        * Just like defining _WIN32_WINNT including winsock2.h implies
        * certain "discipline" for maintaining [broad] binary compatibility.
        * As long as structures are invariant among Winsock versions,
        * it's sufficient to check for specific Winsock2 API availability
        * at run-time [DSO_global_lookup is recommended]...
        */
#    include <winsock2.h>
#    include <ws2tcpip.h>
       /* yes, they have to be #included prior to <windows.h> */
#   endif
#   include <windows.h>
#   include <stdio.h>
#   include <stddef.h>
#   include <errno.h>
#   include <string.h>
#   ifdef _WIN64
#    define strlen(s) _strlen31(s)
/* cut strings to 2GB */
static __inline unsigned int _strlen31(const char *str)
{
    unsigned int len = 0;
    while (*str && len < 0x80000000U)
        str++, len++;
    return len & 0x7FFFFFFF;
}
#   endif
#   include <malloc.h>
#   if defined(_MSC_VER) && _MSC_VER<=1200 && defined(_MT) && defined(isspace)
       /* compensate for bug in VC6 ctype.h */
#    undef isspace
#    undef isdigit
#    undef isalnum
#    undef isupper
#    undef isxdigit
#   endif
#   if defined(_MSC_VER) && !defined(_DLL) && defined(stdin)
#    if _MSC_VER>=1300 && _MSC_VER<1600
#     undef stdin
#     undef stdout
#     undef stderr
FILE *__iob_func();
#     define stdin  (&__iob_func()[0])
#     define stdout (&__iob_func()[1])
#     define stderr (&__iob_func()[2])
#    elif _MSC_VER<1300 && defined(I_CAN_LIVE_WITH_LNK4049)
#     undef stdin
#     undef stdout
#     undef stderr
         /*
          * pre-1300 has __p__iob(), but it's available only in msvcrt.lib,
          * or in other words with /MD. Declaring implicit import, i.e. with
          * _imp_ prefix, works correctly with all compiler options, but
          * without /MD results in LINK warning LNK4049: 'locally defined
          * symbol "__iob" imported'.
          */
extern FILE *_imp___iob;
#     define stdin  (&_imp___iob[0])
#     define stdout (&_imp___iob[1])
#     define stderr (&_imp___iob[2])
#    endif
#   endif
#  endif
#  include <io.h>
#  include <fcntl.h>

#  ifdef OPENSSL_SYS_WINCE
#   define OPENSSL_NO_POSIX_IO
#  endif

#  if defined (__BORLANDC__)
#   define _setmode setmode
#   define _O_TEXT O_TEXT
#   define _O_BINARY O_BINARY
#   define _int64 __int64
#   define _kbhit kbhit
#  endif

#  define EXIT(n) exit(n)
#  define LIST_SEPARATOR_CHAR ';'
#  ifndef X_OK
#   define X_OK        0
#  endif
#  ifndef W_OK
#   define W_OK        2
#  endif
#  ifndef R_OK
#   define R_OK        4
#  endif
#  define OPENSSL_CONF  "openssl.cnf"
#  define SSLEAY_CONF   OPENSSL_CONF
#  define NUL_DEV       "nul"
#  define RFILE         ".rnd"
#  ifdef OPENSSL_SYS_WINCE
#   define DEFAULT_HOME  ""
#  else
#   define DEFAULT_HOME  "C:"
#  endif

/* Avoid Visual Studio 13 GetVersion deprecated problems */
#  if defined(_MSC_VER) && _MSC_VER>=1800
#   define check_winnt() (1)
#   define check_win_minplat(x) (1)
#  else
#   define check_winnt() (GetVersion() < 0x80000000)
#   define check_win_minplat(x) (LOBYTE(LOWORD(GetVersion())) >= (x))
#  endif

# else                          /* The non-microsoft world */

#  ifdef OPENSSL_SYS_VMS
#   define VMS 1
  /*
   * some programs don't include stdlib, so exit() and others give implicit
   * function warnings
   */
#   include <stdlib.h>
#   if defined(__DECC)
#    include <unistd.h>
#   else
#    include <unixlib.h>
#   endif
#   define OPENSSL_CONF        "openssl.cnf"
#   define SSLEAY_CONF         OPENSSL_CONF
#   define RFILE               ".rnd"
#   define LIST_SEPARATOR_CHAR ','
#   define NUL_DEV             "NLA0:"
  /* We don't have any well-defined random devices on VMS, yet... */
#   undef DEVRANDOM
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
     the status is tagged as an error, which I believe is what is wanted here.
     -- Richard Levitte
  */
#   define EXIT(n)             do { int __VMS_EXIT = n; \
                                     if (__VMS_EXIT == 0) \
                                       __VMS_EXIT = 1; \
                                     else \
                                       __VMS_EXIT = (n << 3) | 2; \
                                     __VMS_EXIT |= 0x10000000; \
                                     exit(__VMS_EXIT); } while(0)
#   define NO_SYS_PARAM_H

#  elif defined(OPENSSL_SYS_NETWARE)
#   include <fcntl.h>
#   include <unistd.h>
#   define NO_SYS_TYPES_H
#   undef  DEVRANDOM
#   ifdef NETWARE_CLIB
#    define getpid GetThreadID
extern int GetThreadID(void);
/* #      include <conio.h> */
extern int kbhit(void);
#   else
#    include <screen.h>
#   endif
#   define NO_SYSLOG
#   define _setmode setmode
#   define _kbhit kbhit
#   define _O_TEXT O_TEXT
#   define _O_BINARY O_BINARY
#   define OPENSSL_CONF   "openssl.cnf"
#   define SSLEAY_CONF    OPENSSL_CONF
#   define RFILE    ".rnd"
#   define LIST_SEPARATOR_CHAR ';'
#   define EXIT(n)  { if (n) printf("ERROR: %d\n", (int)n); exit(n); }

#  else
     /* !defined VMS */
#   ifdef OPENSSL_SYS_MPE
#    define NO_SYS_PARAM_H
#   endif
#   ifdef OPENSSL_UNISTD
#    include OPENSSL_UNISTD
#   else
#    include <unistd.h>
#   endif
#   ifndef NO_SYS_TYPES_H
#    include <sys/types.h>
#   endif
#   if defined(NeXT) || defined(OPENSSL_SYS_NEWS4)
#    define pid_t int           /* pid_t is missing on NEXTSTEP/OPENSTEP
                                 * (unless when compiling with
                                 * -D_POSIX_SOURCE, which doesn't work for
                                 * us) */
#   endif
#   ifdef OPENSSL_SYS_NEWS4     /* setvbuf is missing on mips-sony-bsd */
#    define setvbuf(a, b, c, d) setbuffer((a), (b), (d))
typedef unsigned long clock_t;
#   endif
#   ifdef OPENSSL_SYS_WIN32_CYGWIN
#    include <io.h>
#    include <fcntl.h>
#   endif

#   define OPENSSL_CONF        "openssl.cnf"
#   define SSLEAY_CONF         OPENSSL_CONF
#   define RFILE               ".rnd"
#   define LIST_SEPARATOR_CHAR ':'
#   define NUL_DEV             "/dev/null"
#   define EXIT(n)             exit(n)
#  endif

#  define SSLeay_getpid()       getpid()

# endif

/*************/

# ifdef USE_SOCKETS
#  if defined(WINDOWS) || defined(MSDOS)
      /* windows world */

#   ifdef OPENSSL_NO_SOCK
#    define SSLeay_Write(a,b,c)       (-1)
#    define SSLeay_Read(a,b,c)        (-1)
#    define SHUTDOWN(fd)              close(fd)
#    define SHUTDOWN2(fd)             close(fd)
#   elif !defined(__DJGPP__)
#    if defined(_WIN32_WCE) && _WIN32_WCE<410
#     define getservbyname _masked_declaration_getservbyname
#    endif
#    if !defined(IPPROTO_IP)
         /* winsock[2].h was included already? */
#     include <winsock.h>
#    endif
#    ifdef getservbyname
#     undef getservbyname
         /* this is used to be wcecompat/include/winsock_extras.h */
struct servent *PASCAL getservbyname(const char *, const char *);
#    endif

#    ifdef _WIN64
/*
 * Even though sizeof(SOCKET) is 8, it's safe to cast it to int, because
 * the value constitutes an index in per-process table of limited size
 * and not a real pointer.
 */
#     define socket(d,t,p)   ((int)socket(d,t,p))
#     define accept(s,f,l)   ((int)accept(s,f,l))
#    endif
#    define SSLeay_Write(a,b,c)       send((a),(b),(c),0)
#    define SSLeay_Read(a,b,c)        recv((a),(b),(c),0)
#    define SHUTDOWN(fd)              { shutdown((fd),0); closesocket(fd); }
#    define SHUTDOWN2(fd)             { shutdown((fd),2); closesocket(fd); }
#   else
#    define SSLeay_Write(a,b,c)       write_s(a,b,c,0)
#    define SSLeay_Read(a,b,c)        read_s(a,b,c)
#    define SHUTDOWN(fd)              close_s(fd)
#    define SHUTDOWN2(fd)             close_s(fd)
#   endif

#  elif defined(MAC_OS_pre_X)

#   include "MacSocket.h"
#   define SSLeay_Write(a,b,c)         MacSocket_send((a),(b),(c))
#   define SSLeay_Read(a,b,c)          MacSocket_recv((a),(b),(c),true)
#   define SHUTDOWN(fd)                MacSocket_close(fd)
#   define SHUTDOWN2(fd)               MacSocket_close(fd)

#  elif defined(OPENSSL_SYS_NETWARE)
         /*
          * NetWare uses the WinSock2 interfaces by default, but can be
          * configured for BSD
          */
#   if defined(NETWARE_BSDSOCK)
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <sys/time.h>
#    if defined(NETWARE_CLIB)
#     include <sys/bsdskt.h>
#    else
#     include <sys/select.h>
#    endif
#    define INVALID_SOCKET (int)(~0)
#   else
#    include <novsock2.h>
#   endif
#   define SSLeay_Write(a,b,c)   send((a),(b),(c),0)
#   define SSLeay_Read(a,b,c) recv((a),(b),(c),0)
#   define SHUTDOWN(fd)    { shutdown((fd),0); closesocket(fd); }
#   define SHUTDOWN2(fd)      { shutdown((fd),2); closesocket(fd); }

#  else

#   ifndef NO_SYS_PARAM_H
#    include <sys/param.h>
#   endif
#   ifdef OPENSSL_SYS_VXWORKS
#    include <time.h>
#   elif !defined(OPENSSL_SYS_MPE)
#    include <sys/time.h>       /* Needed under linux for FD_XXX */
#   endif

#   include <netdb.h>
#   if defined(OPENSSL_SYS_VMS_NODECC)
#    include <socket.h>
#    include <in.h>
#    include <inet.h>
#   else
#    include <sys/socket.h>
#    ifdef FILIO_H
#     include <sys/filio.h>     /* Added for FIONBIO under unixware */
#    endif
#    include <netinet/in.h>
#    if !defined(OPENSSL_SYS_BEOS_R5)
#     include <arpa/inet.h>
#    endif
#   endif

#   if defined(NeXT) || defined(_NEXT_SOURCE)
#    include <sys/fcntl.h>
#    include <sys/types.h>
#   endif

#   ifdef OPENSSL_SYS_AIX
#    include <sys/select.h>
#   endif

#   ifdef __QNX__
#    include <sys/select.h>
#   endif

#   if defined(__sun) || defined(sun)
#    include <sys/filio.h>
#   else
#    ifndef VMS
#     include <sys/ioctl.h>
#    else
         /* ioctl is only in VMS > 7.0 and when socketshr is not used */
#     if !defined(TCPIP_TYPE_SOCKETSHR) && defined(__VMS_VER) && (__VMS_VER > 70000000)
#      include <sys/ioctl.h>
#     endif
#    endif
#   endif

#   ifdef VMS
#    include <unixio.h>
#    if defined(TCPIP_TYPE_SOCKETSHR)
#     include <socketshr.h>
#    endif
#   endif

#   define SSLeay_Read(a,b,c)     read((a),(b),(c))
#   define SSLeay_Write(a,b,c)    write((a),(b),(c))
#   define SHUTDOWN(fd)    { shutdown((fd),0); closesocket((fd)); }
#   define SHUTDOWN2(fd)   { shutdown((fd),2); closesocket((fd)); }
#   ifndef INVALID_SOCKET
#    define INVALID_SOCKET      (-1)
#   endif                       /* INVALID_SOCKET */
#  endif

/*
 * Some IPv6 implementations are broken, disable them in known bad versions.
 */
#  if !defined(OPENSSL_USE_IPV6)
#   if defined(AF_INET6) && !defined(OPENSSL_SYS_BEOS_BONE) && !defined(NETWARE_CLIB)
#    define OPENSSL_USE_IPV6 1
#   else
#    define OPENSSL_USE_IPV6 0
#   endif
#  endif

# endif

# if (defined(__sun) || defined(sun)) && !defined(__svr4__) && !defined(__SVR4)
  /* include headers first, so our defines don't break it */
#  include <stdlib.h>
#  include <string.h>
  /* bcopy can handle overlapping moves according to SunOS 4.1.4 manpage */
#  define memmove(s1,s2,n) bcopy((s2),(s1),(n))
#  define strtoul(s,e,b) ((unsigned long int)strtol((s),(e),(b)))
extern char *sys_errlist[];
extern int sys_nerr;
#  define strerror(errnum) \
        (((errnum)<0 || (errnum)>=sys_nerr) ? NULL : sys_errlist[errnum])
  /* Being signed SunOS 4.x memcpy breaks ASN1_OBJECT table lookup */
#  include "crypto/o_str.h"
#  define memcmp OPENSSL_memcmp
# endif

# ifndef OPENSSL_EXIT
#  if defined(MONOLITH) && !defined(OPENSSL_C)
#   define OPENSSL_EXIT(n) return(n)
#  else
#   define OPENSSL_EXIT(n) do { EXIT(n); return(n); } while(0)
#  endif
# endif

/***********************************************/

# define DG_GCC_BUG             /* gcc < 2.6.3 on DGUX */

# ifdef sgi
#  define IRIX_CC_BUG           /* all version of IRIX I've tested (4.* 5.*) */
# endif
# ifdef OPENSSL_SYS_SNI
#  define IRIX_CC_BUG           /* CDS++ up to V2.0Bsomething suffered from
                                 * the same bug. */
# endif

# if defined(OPENSSL_SYS_WINDOWS)
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
# elif defined(OPENSSL_SYS_VMS)
/* VMS below version 7.0 doesn't have strcasecmp() */
#  include "o_str.h"
#  define strcasecmp OPENSSL_strcasecmp
#  define strncasecmp OPENSSL_strncasecmp
#  define OPENSSL_IMPLEMENTS_strncasecmp
# elif defined(OPENSSL_SYS_OS2) && defined(__EMX__)
#  define strcasecmp stricmp
#  define strncasecmp strnicmp
# elif defined(OPENSSL_SYS_NETWARE)
#  include <string.h>
#  if defined(NETWARE_CLIB)
#   define strcasecmp stricmp
#   define strncasecmp strnicmp
#  endif                        /* NETWARE_CLIB */
# endif

# if defined(OPENSSL_SYS_OS2) && defined(__EMX__)
#  include <io.h>
#  include <fcntl.h>
#  define NO_SYSLOG
# endif

/* vxworks */
# if defined(OPENSSL_SYS_VXWORKS)
#  include <ioLib.h>
#  include <tickLib.h>
#  include <sysLib.h>

#  define TTY_STRUCT int

#  define sleep(a) taskDelay((a) * sysClkRateGet())

#  include <vxWorks.h>
#  include <sockLib.h>
#  include <taskLib.h>

#  define getpid taskIdSelf

/*
 * NOTE: these are implemented by helpers in database app! if the database is
 * not linked, we need to implement them elswhere
 */
struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const char *addr, int length, int type);
struct servent *getservbyname(const char *name, const char *proto);

# endif
/* end vxworks */

/* beos */
# if defined(OPENSSL_SYS_BEOS_R5)
#  define SO_ERROR 0
#  define NO_SYS_UN
#  define IPPROTO_IP 0
#  include <OS.h>
# endif

# if !defined(inline) && !defined(__cplusplus)
#  if defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
   /* do nothing, inline works */
#  elif defined(__GNUC__) && __GNUC__>=2
#   define inline __inline__
#  elif defined(_MSC_VER)
  /*
   * Visual Studio: inline is available in C++ only, however
   * __inline is available for C, see
   * http://msdn.microsoft.com/en-us/library/z8y1yy88.aspx
   */
#   define inline __inline
#  else
#   define inline
#  endif
# endif

#ifdef  __cplusplus
}
#endif

#endif

#ifndef HEADER_CONFIG_DOS_H
#define HEADER_CONFIG_DOS_H


/* ================================================================ */
/*       ares/config-dos.h - Hand crafted config file for DOS       */
/* ================================================================ */

#define PACKAGE  "c-ares"

#define HAVE_GETTIMEOFDAY      1
#define HAVE_IOCTLSOCKET       1
#define HAVE_IOCTLSOCKET_FIONBIO   1
#define HAVE_LIMITS_H          1
#define HAVE_NET_IF_H          1
#define HAVE_RECV              1
#define HAVE_RECVFROM          1
#define HAVE_SEND              1
#define HAVE_STRDUP            1
#define HAVE_STRICMP           1
#define HAVE_STRUCT_IN6_ADDR   1
#define HAVE_STRUCT_TIMEVAL    1
#define HAVE_SYS_IOCTL_H       1
#define HAVE_SYS_SOCKET_H      1
#define HAVE_SYS_STAT_H        1
#define HAVE_SYS_TYPES_H       1
#define HAVE_TIME_H            1
#define HAVE_UNISTD_H          1

#define NEED_MALLOC_H          1

#define RETSIGTYPE             void
#define SIZEOF_INT             4
#define SIZEOF_SHORT           2
#define SIZEOF_SIZE_T          4
#define TIME_WITH_SYS_TIME     1

/* Qualifiers for send(), recv(), recvfrom() and getnameinfo(). */

#define SEND_TYPE_ARG1         int
#define SEND_QUAL_ARG2         const
#define SEND_TYPE_ARG2         void *
#define SEND_TYPE_ARG3         int
#define SEND_TYPE_ARG4         int
#define SEND_TYPE_RETV         int

#define RECV_TYPE_ARG1         int
#define RECV_TYPE_ARG2         void *
#define RECV_TYPE_ARG3         int
#define RECV_TYPE_ARG4         int
#define RECV_TYPE_RETV         int

#define RECVFROM_TYPE_ARG1     int
#define RECVFROM_TYPE_ARG2     void
#define RECVFROM_TYPE_ARG3     int
#define RECVFROM_TYPE_ARG4     int
#define RECVFROM_TYPE_ARG5     struct sockaddr
#define RECVFROM_TYPE_ARG6     int
#define RECVFROM_TYPE_RETV     int
#define RECVFROM_TYPE_ARG2_IS_VOID 1

#define BSD

#if defined(__HIGHC__) || \
    (defined(__GNUC__) && (__GNUC__ < 4))
#define ssize_t    int
#endif

/* Target HAVE_x section */

#if defined(DJGPP)
  #define HAVE_STRCASECMP           1
  #define HAVE_STRNCASECMP          1
  #define HAVE_SYS_TIME_H           1
  #define HAVE_VARIADIC_MACROS_GCC  1

  /* Because djgpp <= 2.03 doesn't have snprintf() etc. */
  #if (DJGPP_MINOR < 4)
    #define _MPRINTF_REPLACE
  #endif

#elif defined(__WATCOMC__)
  #define HAVE_STRCASECMP 1

#elif defined(__HIGHC__)
  #define HAVE_SYS_TIME_H 1
#endif

#ifdef WATT32
  #define HAVE_AF_INET6                    1
  #define HAVE_ARPA_INET_H                 1
  #define HAVE_ARPA_NAMESER_H              1
  #define HAVE_CLOSESOCKET_CAMEL           1
  #define HAVE_GETHOSTNAME                 1
  #define HAVE_NETDB_H                     1
  #define HAVE_NETINET_IN_H                1
  #define HAVE_NETINET_TCP_H               1
  #define HAVE_PF_INET6                    1
  #define HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID  1
  #define HAVE_STRUCT_ADDRINFO             1
  #define HAVE_STRUCT_IN6_ADDR             1
  #define HAVE_STRUCT_SOCKADDR_IN6         1
  #define HAVE_SYS_SOCKET_H                1
  #define HAVE_SYS_UIO_H                   1
  #define NS_INADDRSZ                      4
  #define HAVE_STRUCT_SOCKADDR_IN6         1
  #define CloseSocket(s)                   close_s((s))
#endif

#undef word
#undef byte

#endif /* HEADER_CONFIG_DOS_H */


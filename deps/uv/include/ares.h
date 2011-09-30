
/* Copyright 1998, 2009 by the Massachusetts Institute of Technology.
 * Copyright (C) 2007-2011 by Daniel Stenberg
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifndef ARES__H
#define ARES__H

#include "ares_version.h"  /* c-ares version defines   */

/*
 * Define WIN32 when build target is Win32 API
 */

#if (defined(_WIN32) || defined(__WIN32__)) && \
   !defined(WIN32) && !defined(__SYMBIAN32__)
#  define WIN32
#endif

/*************************** libuv patch ***************/

/*
 * We want to avoid autoconf altogether since there are a finite number of
 * operating systems and simply build c-ares. Therefore we do not want the
 * configurations provided by ares_build.h since we are always statically
 * linking c-ares into libuv. Having a system dependent ares_build.h forces
 * all users of ares.h to include the correct ares_build.h.  We do not care
 * about the linking checks provided by ares_rules.h. This would complicate
 * the libuv build process.
 */


#if defined(WIN32)
/* Configure process defines this to 1 when it finds out that system  */
/* header file ws2tcpip.h must be included by the external interface. */
/* #undef CARES_PULL_WS2TCPIP_H */
# include <winsock2.h>
# include <ws2tcpip.h>
# include <windows.h>

#else /* Not Windows */

# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
#endif

#if 0
/* The size of `long', as computed by sizeof. */
#define CARES_SIZEOF_LONG 4
#endif

/* Integral data type used for ares_socklen_t. */
#define CARES_TYPEOF_ARES_SOCKLEN_T socklen_t

#if 0
/* The size of `ares_socklen_t', as computed by sizeof. */
#define CARES_SIZEOF_ARES_SOCKLEN_T 4
#endif

/* Data type definition of ares_socklen_t. */
typedef int ares_socklen_t;

#if 0 /* libuv disabled */
#include "ares_rules.h"    /* c-ares rules enforcement */
#endif

/*********************** end libuv patch ***************/

#include <sys/types.h>

/* HP-UX systems version 9, 10 and 11 lack sys/select.h and so does oldish
   libc5-based Linux systems. Only include it on system that are known to
   require it! */
#if defined(_AIX) || defined(__NOVELL_LIBC__) || defined(__NetBSD__) || \
    defined(__minix) || defined(__SYMBIAN32__) || defined(__INTEGRITY)
#include <sys/select.h>
#endif
#if (defined(NETWARE) && !defined(__NOVELL_LIBC__))
#include <sys/bsdskt.h>
#endif

#if defined(WATT32)
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <tcp.h>
#elif defined(_WIN32_WCE)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winsock.h>
#elif defined(WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/*
** c-ares external API function linkage decorations.
*/

#if !defined(CARES_STATICLIB) && \
   (defined(WIN32) || defined(_WIN32) || defined(__SYMBIAN32__))
   /* __declspec function decoration for Win32 and Symbian DLL's */
#  if defined(CARES_BUILDING_LIBRARY)
#    define CARES_EXTERN  __declspec(dllexport)
#  else
#    define CARES_EXTERN  __declspec(dllimport)
#  endif
#else
   /* visibility function decoration for other cases */
#  if !defined(CARES_SYMBOL_HIDING) || \
     defined(WIN32) || defined(_WIN32) || defined(__SYMBIAN32__)
#    define CARES_EXTERN
#  else
#    define CARES_EXTERN CARES_SYMBOL_SCOPE_EXTERN
#  endif
#endif


#define ARES_SUCCESS            0

/* Server error codes (ARES_ENODATA indicates no relevant answer) */
#define ARES_ENODATA            1
#define ARES_EFORMERR           2
#define ARES_ESERVFAIL          3
#define ARES_ENOTFOUND          4
#define ARES_ENOTIMP            5
#define ARES_EREFUSED           6

/* Locally generated error codes */
#define ARES_EBADQUERY          7
#define ARES_EBADNAME           8
#define ARES_EBADFAMILY         9
#define ARES_EBADRESP           10
#define ARES_ECONNREFUSED       11
#define ARES_ETIMEOUT           12
#define ARES_EOF                13
#define ARES_EFILE              14
#define ARES_ENOMEM             15
#define ARES_EDESTRUCTION       16
#define ARES_EBADSTR            17

/* ares_getnameinfo error codes */
#define ARES_EBADFLAGS          18

/* ares_getaddrinfo error codes */
#define ARES_ENONAME            19
#define ARES_EBADHINTS          20

/* Uninitialized library error code */
#define ARES_ENOTINITIALIZED    21          /* introduced in 1.7.0 */

/* ares_library_init error codes */
#define ARES_ELOADIPHLPAPI           22     /* introduced in 1.7.0 */
#define ARES_EADDRGETNETWORKPARAMS   23     /* introduced in 1.7.0 */

/* More error codes */
#define ARES_ECANCELLED         24          /* introduced in 1.7.0 */

/* Flag values */
#define ARES_FLAG_USEVC         (1 << 0)
#define ARES_FLAG_PRIMARY       (1 << 1)
#define ARES_FLAG_IGNTC         (1 << 2)
#define ARES_FLAG_NORECURSE     (1 << 3)
#define ARES_FLAG_STAYOPEN      (1 << 4)
#define ARES_FLAG_NOSEARCH      (1 << 5)
#define ARES_FLAG_NOALIASES     (1 << 6)
#define ARES_FLAG_NOCHECKRESP   (1 << 7)

/* Option mask values */
#define ARES_OPT_FLAGS          (1 << 0)
#define ARES_OPT_TIMEOUT        (1 << 1)
#define ARES_OPT_TRIES          (1 << 2)
#define ARES_OPT_NDOTS          (1 << 3)
#define ARES_OPT_UDP_PORT       (1 << 4)
#define ARES_OPT_TCP_PORT       (1 << 5)
#define ARES_OPT_SERVERS        (1 << 6)
#define ARES_OPT_DOMAINS        (1 << 7)
#define ARES_OPT_LOOKUPS        (1 << 8)
#define ARES_OPT_SOCK_STATE_CB  (1 << 9)
#define ARES_OPT_SORTLIST       (1 << 10)
#define ARES_OPT_SOCK_SNDBUF    (1 << 11)
#define ARES_OPT_SOCK_RCVBUF    (1 << 12)
#define ARES_OPT_TIMEOUTMS      (1 << 13)
#define ARES_OPT_ROTATE         (1 << 14)

/* Nameinfo flag values */
#define ARES_NI_NOFQDN                  (1 << 0)
#define ARES_NI_NUMERICHOST             (1 << 1)
#define ARES_NI_NAMEREQD                (1 << 2)
#define ARES_NI_NUMERICSERV             (1 << 3)
#define ARES_NI_DGRAM                   (1 << 4)
#define ARES_NI_TCP                     0
#define ARES_NI_UDP                     ARES_NI_DGRAM
#define ARES_NI_SCTP                    (1 << 5)
#define ARES_NI_DCCP                    (1 << 6)
#define ARES_NI_NUMERICSCOPE            (1 << 7)
#define ARES_NI_LOOKUPHOST              (1 << 8)
#define ARES_NI_LOOKUPSERVICE           (1 << 9)
/* Reserved for future use */
#define ARES_NI_IDN                     (1 << 10)
#define ARES_NI_IDN_ALLOW_UNASSIGNED    (1 << 11)
#define ARES_NI_IDN_USE_STD3_ASCII_RULES (1 << 12)

/* Addrinfo flag values */
#define ARES_AI_CANONNAME               (1 << 0)
#define ARES_AI_NUMERICHOST             (1 << 1)
#define ARES_AI_PASSIVE                 (1 << 2)
#define ARES_AI_NUMERICSERV             (1 << 3)
#define ARES_AI_V4MAPPED                (1 << 4)
#define ARES_AI_ALL                     (1 << 5)
#define ARES_AI_ADDRCONFIG              (1 << 6)
/* Reserved for future use */
#define ARES_AI_IDN                     (1 << 10)
#define ARES_AI_IDN_ALLOW_UNASSIGNED    (1 << 11)
#define ARES_AI_IDN_USE_STD3_ASCII_RULES (1 << 12)
#define ARES_AI_CANONIDN                (1 << 13)

#define ARES_AI_MASK (ARES_AI_CANONNAME|ARES_AI_NUMERICHOST|ARES_AI_PASSIVE| \
                      ARES_AI_NUMERICSERV|ARES_AI_V4MAPPED|ARES_AI_ALL| \
                      ARES_AI_ADDRCONFIG)
#define ARES_GETSOCK_MAXNUM 16 /* ares_getsock() can return info about this
                                  many sockets */
#define ARES_GETSOCK_READABLE(bits,num) (bits & (1<< (num)))
#define ARES_GETSOCK_WRITABLE(bits,num) (bits & (1 << ((num) + \
                                         ARES_GETSOCK_MAXNUM)))

/* c-ares library initialization flag values */
#define ARES_LIB_INIT_NONE   (0)
#define ARES_LIB_INIT_WIN32  (1 << 0)
#define ARES_LIB_INIT_ALL    (ARES_LIB_INIT_WIN32)


/*
 * Typedef our socket type
 */

#ifndef ares_socket_typedef
#ifdef WIN32
typedef SOCKET ares_socket_t;
#define ARES_SOCKET_BAD INVALID_SOCKET
#else
typedef int ares_socket_t;
#define ARES_SOCKET_BAD -1
#endif
#define ares_socket_typedef
#endif /* ares_socket_typedef */

typedef void (*ares_sock_state_cb)(void *data,
                                   ares_socket_t socket_fd,
                                   int readable,
                                   int writable);

struct apattern;

/* NOTE about the ares_options struct to users and developers.

   This struct will remain looking like this. It will not be extended nor
   shrunk in future releases, but all new options will be set by ares_set_*()
   options instead of with the ares_init_options() function.

   Eventually (in a galaxy far far away), all options will be settable by
   ares_set_*() options and the ares_init_options() function will become
   deprecated.

   When new options are added to c-ares, they are not added to this
   struct. And they are not "saved" with the ares_save_options() function but
   instead we encourage the use of the ares_dup() function. Needless to say,
   if you add config options to c-ares you need to make sure ares_dup()
   duplicates this new option.

 */
struct ares_options {
  int flags;
  int timeout; /* in seconds or milliseconds, depending on options */
  int tries;
  int ndots;
  unsigned short udp_port;
  unsigned short tcp_port;
  int socket_send_buffer_size;
  int socket_receive_buffer_size;
  struct in_addr *servers;
  int nservers;
  char **domains;
  int ndomains;
  char *lookups;
  ares_sock_state_cb sock_state_cb;
  void *sock_state_cb_data;
  struct apattern *sortlist;
  int nsort;
};

struct hostent;
struct timeval;
struct sockaddr;
struct ares_channeldata;

typedef struct ares_channeldata *ares_channel;

typedef void (*ares_callback)(void *arg,
                              int status,
                              int timeouts,
                              unsigned char *abuf,
                              int alen);

typedef void (*ares_host_callback)(void *arg,
                                   int status,
                                   int timeouts,
                                   struct hostent *hostent);

typedef void (*ares_nameinfo_callback)(void *arg,
                                       int status,
                                       int timeouts,
                                       char *node,
                                       char *service);

typedef int  (*ares_sock_create_callback)(ares_socket_t socket_fd,
                                          int type,
                                          void *data);

CARES_EXTERN int ares_library_init(int flags);

CARES_EXTERN void ares_library_cleanup(void);

CARES_EXTERN const char *ares_version(int *version);

CARES_EXTERN int ares_init(ares_channel *channelptr);

CARES_EXTERN int ares_init_options(ares_channel *channelptr,
                                   struct ares_options *options,
                                   int optmask);

CARES_EXTERN int ares_save_options(ares_channel channel,
                                   struct ares_options *options,
                                   int *optmask);

CARES_EXTERN void ares_destroy_options(struct ares_options *options);

CARES_EXTERN int ares_dup(ares_channel *dest,
                          ares_channel src);

CARES_EXTERN void ares_destroy(ares_channel channel);

CARES_EXTERN void ares_cancel(ares_channel channel);

/* These next 3 configure local binding for the out-going socket
 * connection.  Use these to specify source IP and/or network device
 * on multi-homed systems.
 */
CARES_EXTERN void ares_set_local_ip4(ares_channel channel, unsigned int local_ip);

/* local_ip6 should be 16 bytes in length */
CARES_EXTERN void ares_set_local_ip6(ares_channel channel,
                                     const unsigned char* local_ip6);

/* local_dev_name should be null terminated. */
CARES_EXTERN void ares_set_local_dev(ares_channel channel,
                                     const char* local_dev_name);

CARES_EXTERN void ares_set_socket_callback(ares_channel channel,
                                           ares_sock_create_callback callback,
                                           void *user_data);

CARES_EXTERN void ares_send(ares_channel channel,
                            const unsigned char *qbuf,
                            int qlen,
                            ares_callback callback,
                            void *arg);

CARES_EXTERN void ares_query(ares_channel channel,
                             const char *name,
                             int dnsclass,
                             int type,
                             ares_callback callback,
                             void *arg);

CARES_EXTERN void ares_search(ares_channel channel,
                              const char *name,
                              int dnsclass,
                              int type,
                              ares_callback callback,
                              void *arg);

CARES_EXTERN void ares_gethostbyname(ares_channel channel,
                                     const char *name,
                                     int family,
                                     ares_host_callback callback,
                                     void *arg);

CARES_EXTERN int ares_gethostbyname_file(ares_channel channel,
                                         const char *name,
                                         int family,
                                         struct hostent **host);

CARES_EXTERN void ares_gethostbyaddr(ares_channel channel,
                                     const void *addr,
                                     int addrlen,
                                     int family,
                                     ares_host_callback callback,
                                     void *arg);

CARES_EXTERN void ares_getnameinfo(ares_channel channel,
                                   const struct sockaddr *sa,
                                   ares_socklen_t salen,
                                   int flags,
                                   ares_nameinfo_callback callback,
                                   void *arg);

CARES_EXTERN int ares_fds(ares_channel channel,
                          fd_set *read_fds,
                          fd_set *write_fds);

CARES_EXTERN int ares_getsock(ares_channel channel,
                              ares_socket_t *socks,
                              int numsocks);

CARES_EXTERN struct timeval *ares_timeout(ares_channel channel,
                                          struct timeval *maxtv,
                                          struct timeval *tv);

CARES_EXTERN void ares_process(ares_channel channel,
                               fd_set *read_fds,
                               fd_set *write_fds);

CARES_EXTERN void ares_process_fd(ares_channel channel,
                                  ares_socket_t read_fd,
                                  ares_socket_t write_fd);

CARES_EXTERN int ares_mkquery(const char *name,
                              int dnsclass,
                              int type,
                              unsigned short id,
                              int rd,
                              unsigned char **buf,
                              int *buflen);

CARES_EXTERN int ares_expand_name(const unsigned char *encoded,
                                  const unsigned char *abuf,
                                  int alen,
                                  char **s,
                                  long *enclen);

CARES_EXTERN int ares_expand_string(const unsigned char *encoded,
                                    const unsigned char *abuf,
                                    int alen,
                                    unsigned char **s,
                                    long *enclen);

/*
 * NOTE: before c-ares 1.7.0 we would most often use the system in6_addr
 * struct below when ares itself was built, but many apps would use this
 * private version since the header checked a HAVE_* define for it. Starting
 * with 1.7.0 we always declare and use our own to stop relying on the
 * system's one.
 */
struct ares_in6_addr {
  union {
    unsigned char _S6_u8[16];
  } _S6_un;
};

struct ares_addrttl {
  struct in_addr ipaddr;
  int            ttl;
};

struct ares_addr6ttl {
  struct ares_in6_addr ip6addr;
  int             ttl;
};

struct ares_srv_reply {
  struct ares_srv_reply  *next;
  char                   *host;
  unsigned short          priority;
  unsigned short          weight;
  unsigned short          port;
};

struct ares_mx_reply {
  struct ares_mx_reply   *next;
  char                   *host;
  unsigned short          priority;
};

struct ares_txt_reply {
  struct ares_txt_reply  *next;
  unsigned char          *txt;
  size_t                  length;  /* length excludes null termination */
};

/*
** Parse the buffer, starting at *abuf and of length alen bytes, previously
** obtained from an ares_search call.  Put the results in *host, if nonnull.
** Also, if addrttls is nonnull, put up to *naddrttls IPv4 addresses along with
** their TTLs in that array, and set *naddrttls to the number of addresses
** so written.
*/

CARES_EXTERN int ares_parse_a_reply(const unsigned char *abuf,
                                    int alen,
                                    struct hostent **host,
                                    struct ares_addrttl *addrttls,
                                    int *naddrttls);

CARES_EXTERN int ares_parse_aaaa_reply(const unsigned char *abuf,
                                       int alen,
                                       struct hostent **host,
                                       struct ares_addr6ttl *addrttls,
                                       int *naddrttls);

CARES_EXTERN int ares_parse_ptr_reply(const unsigned char *abuf,
                                      int alen,
                                      const void *addr,
                                      int addrlen,
                                      int family,
                                      struct hostent **host);

CARES_EXTERN int ares_parse_ns_reply(const unsigned char *abuf,
                                     int alen,
                                     struct hostent **host);

CARES_EXTERN int ares_parse_srv_reply(const unsigned char* abuf,
                                      int alen,
                                      struct ares_srv_reply** srv_out);

CARES_EXTERN int ares_parse_mx_reply(const unsigned char* abuf,
                                      int alen,
                                      struct ares_mx_reply** mx_out);

CARES_EXTERN int ares_parse_txt_reply(const unsigned char* abuf,
                                      int alen,
                                      struct ares_txt_reply** txt_out);

CARES_EXTERN void ares_free_string(void *str);

CARES_EXTERN void ares_free_hostent(struct hostent *host);

CARES_EXTERN void ares_free_data(void *dataptr);

CARES_EXTERN const char *ares_strerror(int code);

/* TODO:  Hold port here as well. */
struct ares_addr_node {
  struct ares_addr_node *next;
  int family;
  union {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
  } addr;
};

CARES_EXTERN int ares_set_servers(ares_channel channel,
                                  struct ares_addr_node *servers);

/* Incomming string format: host[:port][,host[:port]]... */
CARES_EXTERN int ares_set_servers_csv(ares_channel channel,
                                      const char* servers);

CARES_EXTERN int ares_get_servers(ares_channel channel,
                                  struct ares_addr_node **servers);

#ifdef  __cplusplus
}
#endif

#endif /* ARES__H */

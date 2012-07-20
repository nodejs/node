#ifndef __ARES_PRIVATE_H
#define __ARES_PRIVATE_H


/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2004-2010 by Daniel Stenberg
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

/*
 * Define WIN32 when build target is Win32 API
 */

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#define WIN32
#endif

#include <stdio.h>
#include <sys/types.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef WATT32
#include <tcp.h>
#include <sys/ioctl.h>
#define writev(s,v,c)     writev_s(s,v,c)
#define HAVE_WRITEV 1
#endif

#ifdef NETWARE
#include <time.h>
#endif

#define DEFAULT_TIMEOUT         5000 /* milliseconds */
#define DEFAULT_TRIES           4
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#if defined(WIN32) && !defined(WATT32)

#define WIN_NS_9X      "System\\CurrentControlSet\\Services\\VxD\\MSTCP"
#define WIN_NS_NT_KEY  "System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define NAMESERVER     "NameServer"
#define DHCPNAMESERVER "DhcpNameServer"
#define DATABASEPATH   "DatabasePath"
#define WIN_PATH_HOSTS  "\\hosts"

#elif defined(WATT32)

#define PATH_RESOLV_CONF "/dev/ENV/etc/resolv.conf"

#elif defined(NETWARE)

#define PATH_RESOLV_CONF "sys:/etc/resolv.cfg"
#define PATH_HOSTS              "sys:/etc/hosts"

#elif defined(__riscos__)

#define PATH_HOSTS             "InetDBase:Hosts"

#else

#define PATH_RESOLV_CONF        "/etc/resolv.conf"
#ifdef ETC_INET
#define PATH_HOSTS              "/etc/inet/hosts"
#else
#define PATH_HOSTS              "/etc/hosts"
#endif

#endif

#define ARES_ID_KEY_LEN 31

#include "ares_ipv6.h"
#include "ares_llist.h"

#ifndef HAVE_GETENV
#  include "ares_getenv.h"
#  define getenv(ptr) ares_getenv(ptr)
#endif

#ifndef HAVE_STRDUP
#  include "ares_strdup.h"
#  define strdup(ptr) ares_strdup(ptr)
#endif

#ifndef HAVE_STRCASECMP
#  include "ares_strcasecmp.h"
#  define strcasecmp(p1,p2) ares_strcasecmp(p1,p2)
#endif

#ifndef HAVE_STRNCASECMP
#  include "ares_strcasecmp.h"
#  define strncasecmp(p1,p2,n) ares_strncasecmp(p1,p2,n)
#endif

#ifndef HAVE_WRITEV
#  include "ares_writev.h"
#  define writev(s,ptr,cnt) ares_writev(s,ptr,cnt)
#endif

struct ares_addr {
  int family;
  union {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
  } addr;
};
#define addrV4 addr.addr4
#define addrV6 addr.addr6

struct query;

struct send_request {
  /* Remaining data to send */
  const unsigned char *data;
  size_t len;

  /* The query for which we're sending this data */
  struct query* owner_query;
  /* The buffer we're using, if we have our own copy of the packet */
  unsigned char *data_storage;

  /* Next request in queue */
  struct send_request *next;
};

struct server_state {
  struct ares_addr addr;
  ares_socket_t udp_socket;
  ares_socket_t tcp_socket;

  /* Mini-buffer for reading the length word */
  unsigned char tcp_lenbuf[2];
  int tcp_lenbuf_pos;
  int tcp_length;

  /* Buffer for reading actual TCP data */
  unsigned char *tcp_buffer;
  int tcp_buffer_pos;

  /* TCP output queue */
  struct send_request *qhead;
  struct send_request *qtail;

  /* Which incarnation of this connection is this? We don't want to
   * retransmit requests into the very same socket, but if the server
   * closes on us and we re-open the connection, then we do want to
   * re-send. */
  int tcp_connection_generation;

  /* Circular, doubly-linked list of outstanding queries to this server */
  struct list_node queries_to_server;

  /* Link back to owning channel */
  ares_channel channel;

  /* Is this server broken? We mark connections as broken when a
   * request that is queued for sending times out.
   */
  int is_broken;
};

/* State to represent a DNS query */
struct query {
  /* Query ID from qbuf, for faster lookup, and current timeout */
  unsigned short qid;
  struct timeval timeout;

  /*
   * Links for the doubly-linked lists in which we insert a query.
   * These circular, doubly-linked lists that are hash-bucketed based
   * the attributes we care about, help making most important
   * operations O(1).
   */
  struct list_node queries_by_qid;    /* hopefully in same cache line as qid */
  struct list_node queries_by_timeout;
  struct list_node queries_to_server;
  struct list_node all_queries;

  /* Query buf with length at beginning, for TCP transmission */
  unsigned char *tcpbuf;
  int tcplen;

  /* Arguments passed to ares_send() (qbuf points into tcpbuf) */
  const unsigned char *qbuf;
  int qlen;
  ares_callback callback;
  void *arg;

  /* Query status */
  int try_count; /* Number of times we tried this query already. */
  int server; /* Server this query has last been sent to. */
  struct query_server_info *server_info;   /* per-server state */
  int using_tcp;
  int error_status;
  int timeouts; /* number of timeouts we saw for this request */
};

/* Per-server state for a query */
struct query_server_info {
  int skip_server;  /* should we skip server, due to errors, etc? */
  int tcp_connection_generation;  /* into which TCP connection did we send? */
};

/* An IP address pattern; matches an IP address X if X & mask == addr */
#define PATTERN_MASK 0x1
#define PATTERN_CIDR 0x2

struct apattern {
  union
  {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
  } addr;
  union
  {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
    unsigned short       bits;
  } mask;
  int family;
  unsigned short type;
};

typedef struct rc4_key
{
  unsigned char state[256];
  unsigned char x;
  unsigned char y;
} rc4_key;

struct ares_channeldata {
  /* Configuration data */
  int flags;
  int timeout; /* in milliseconds */
  int tries;
  int ndots;
  int rotate; /* if true, all servers specified are used */
  int udp_port;
  int tcp_port;
  int socket_send_buffer_size;
  int socket_receive_buffer_size;
  char **domains;
  int ndomains;
  struct apattern *sortlist;
  int nsort;
  char *lookups;

  /* For binding to local devices and/or IP addresses.  Leave
   * them null/zero for no binding.
   */
  char local_dev_name[32];
  unsigned int local_ip4;
  unsigned char local_ip6[16];

  int optmask; /* the option bitfield passed in at init time */

  /* Server addresses and communications state */
  struct server_state *servers;
  int nservers;

  /* ID to use for next query */
  unsigned short next_id;
  /* key to use when generating new ids */
  rc4_key id_key;

  /* Generation number to use for the next TCP socket open/close */
  int tcp_connection_generation;

  /* The time at which we last called process_timeouts(). Uses integer seconds
     just to draw the line somewhere. */
  time_t last_timeout_processed;

  /* Last server we sent a query to. */
  int last_server;

  /* Circular, doubly-linked list of queries, bucketed various ways.... */
  /* All active queries in a single list: */
  struct list_node all_queries;
  /* Queries bucketed by qid, for quickly dispatching DNS responses: */
#define ARES_QID_TABLE_SIZE 2048
  struct list_node queries_by_qid[ARES_QID_TABLE_SIZE];
  /* Queries bucketed by timeout, for quickly handling timeouts: */
#define ARES_TIMEOUT_TABLE_SIZE 1024
  struct list_node queries_by_timeout[ARES_TIMEOUT_TABLE_SIZE];

  ares_sock_state_cb sock_state_cb;
  void *sock_state_cb_data;

  ares_sock_create_callback sock_create_cb;
  void *sock_create_cb_data;
};

/* return true if now is exactly check time or later */
int ares__timedout(struct timeval *now,
                   struct timeval *check);
/* add the specific number of milliseconds to the time in the first argument */
int ares__timeadd(struct timeval *now,
                  int millisecs);
/* return time offset between now and (future) check, in milliseconds */
long ares__timeoffset(struct timeval *now,
                      struct timeval *check);
/* returns ARES_SUCCESS if library has been initialized */
int ares_library_initialized(void);
void ares__rc4(rc4_key* key,unsigned char *buffer_ptr, int buffer_len);
void ares__send_query(ares_channel channel, struct query *query,
                      struct timeval *now);
void ares__close_sockets(ares_channel channel, struct server_state *server);
int ares__get_hostent(FILE *fp, int family, struct hostent **host);
int ares__read_line(FILE *fp, char **buf, size_t *bufsize);
void ares__free_query(struct query *query);
unsigned short ares__generate_new_id(rc4_key* key);
struct timeval ares__tvnow(void);
int ares__expand_name_for_response(const unsigned char *encoded,
                                   const unsigned char *abuf, int alen,
                                   char **s, long *enclen);
void ares__init_servers_state(ares_channel channel);
void ares__destroy_servers_state(ares_channel channel);
#if 0 /* Not used */
long ares__tvdiff(struct timeval t1, struct timeval t2);
#endif

#define ARES_SWAP_BYTE(a,b) \
  { unsigned char swapByte = *(a);  *(a) = *(b);  *(b) = swapByte; }

#define SOCK_STATE_CALLBACK(c, s, r, w)                                 \
  do {                                                                  \
    if ((c)->sock_state_cb)                                             \
      (c)->sock_state_cb((c)->sock_state_cb_data, (s), (r), (w));       \
  } WHILE_FALSE

#ifdef CURLDEBUG
/* This is low-level hard-hacking memory leak tracking and similar. Using the
   libcurl lowlevel code from within library is ugly and only works when
   c-ares is built and linked with a similarly curldebug-enabled libcurl,
   but we do this anyway for convenience. */
#include "../lib/memdebug.h"
#endif

#endif /* __ARES_PRIVATE_H */

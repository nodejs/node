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

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef WATT32
#include <tcp.h>
#include <sys/ioctl.h>
#define writev(s,v,c)     writev_s(s,v,c)
#define HAVE_WRITEV 1
#endif

#define DEFAULT_TIMEOUT         5000 /* milliseconds */
#define DEFAULT_TRIES           4
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifdef CARES_EXPOSE_STATICS
/* Make some internal functions visible for testing */
#define STATIC_TESTABLE
#else
#define STATIC_TESTABLE static
#endif

/* By using a double cast, we can get rid of the bogus warning of
 * warning: cast from 'const struct sockaddr *' to 'const struct sockaddr_in6 *' increases required alignment from 1 to 4 [-Wcast-align]
 */
#define CARES_INADDR_CAST(type, var) ((type)((void *)var))

#if defined(WIN32) && !defined(WATT32)

#define WIN_NS_9X            "System\\CurrentControlSet\\Services\\VxD\\MSTCP"
#define WIN_NS_NT_KEY        "System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define WIN_DNSCLIENT        "Software\\Policies\\Microsoft\\System\\DNSClient"
#define WIN_NT_DNSCLIENT     "Software\\Policies\\Microsoft\\Windows NT\\DNSClient"
#define NAMESERVER           "NameServer"
#define DHCPNAMESERVER       "DhcpNameServer"
#define DATABASEPATH         "DatabasePath"
#define WIN_PATH_HOSTS       "\\hosts"
#define SEARCHLIST_KEY       "SearchList"
#define PRIMARYDNSSUFFIX_KEY "PrimaryDNSSuffix"
#define INTERFACES_KEY       "Interfaces"
#define DOMAIN_KEY           "Domain"
#define DHCPDOMAIN_KEY       "DhcpDomain"

#elif defined(WATT32)

#define PATH_RESOLV_CONF "/dev/ENV/etc/resolv.conf"
W32_FUNC const char *_w32_GetHostsFile (void);

#elif defined(NETWARE)

#define PATH_RESOLV_CONF "sys:/etc/resolv.cfg"
#define PATH_HOSTS              "sys:/etc/hosts"

#elif defined(__riscos__)

#define PATH_HOSTS             "InetDBase:Hosts"

#elif defined(__HAIKU__)

#define PATH_RESOLV_CONF "/system/settings/network/resolv.conf"
#define PATH_HOSTS              "/system/settings/network/hosts"

#else

#define PATH_RESOLV_CONF        "/etc/resolv.conf"
#ifdef ETC_INET
#define PATH_HOSTS              "/etc/inet/hosts"
#else
#define PATH_HOSTS              "/etc/hosts"
#endif

#endif

#include "ares_ipv6.h"
#include "ares_llist.h"

#ifndef HAVE_GETENV
#  include "ares_getenv.h"
#  define getenv(ptr) ares_getenv(ptr)
#endif

#include "ares_strdup.h"
#include "ares_strsplit.h"

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

/********* EDNS defines section ******/
#define EDNSPACKETSZ   1280  /* Reasonable UDP payload size, as suggested
                                in RFC2671 */
#define MAXENDSSZ      4096  /* Maximum (local) limit for edns packet size */
#define EDNSFIXEDSZ    11    /* Size of EDNS header */
/********* EDNS defines section ******/

struct ares_addr {
  int family;
  union {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
  } addr;
  int udp_port;  /* stored in network order */
  int tcp_port;  /* stored in network order */
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

struct ares_rand_state;
typedef struct ares_rand_state ares_rand_state;

struct ares_channeldata {
  /* Configuration data */
  int flags;
  int timeout; /* in milliseconds */
  int tries;
  int ndots;
  int rotate; /* if true, all servers specified are used */
  int udp_port; /* stored in network order */
  int tcp_port; /* stored in network order */
  int socket_send_buffer_size;
  int socket_receive_buffer_size;
  char **domains;
  int ndomains;
  struct apattern *sortlist;
  int nsort;
  char *lookups;
  int ednspsz;

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
  /* random state to use when generating new ids */
  ares_rand_state *rand_state;

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

  ares_sock_config_callback sock_config_cb;
  void *sock_config_cb_data;

  const struct ares_socket_functions * sock_funcs;
  void *sock_func_cb_data;

  /* Path for resolv.conf file, configurable via ares_options */
  char *resolvconf_path;

  /* Path for hosts file, configurable via ares_options */
  char *hosts_path;
};

/* Does the domain end in ".onion" or ".onion."? Case-insensitive. */
int ares__is_onion_domain(const char *name);

/* Memory management functions */
extern void *(*ares_malloc)(size_t size);
extern void *(*ares_realloc)(void *ptr, size_t size);
extern void (*ares_free)(void *ptr);

/* return true if now is exactly check time or later */
int ares__timedout(struct timeval *now,
                   struct timeval *check);

void ares__send_query(ares_channel channel, struct query *query,
                      struct timeval *now);
void ares__close_sockets(ares_channel channel, struct server_state *server);
int ares__get_hostent(FILE *fp, int family, struct hostent **host);
int ares__read_line(FILE *fp, char **buf, size_t *bufsize);
void ares__free_query(struct query *query);

ares_rand_state *ares__init_rand_state(void);
void ares__destroy_rand_state(ares_rand_state *state);
unsigned short ares__generate_new_id(ares_rand_state *state);
struct timeval ares__tvnow(void);
int ares__expand_name_validated(const unsigned char *encoded,
                                const unsigned char *abuf,
                                int alen, char **s, long *enclen,
                                int is_hostname);
int ares__expand_name_for_response(const unsigned char *encoded,
                                   const unsigned char *abuf, int alen,
                                   char **s, long *enclen, int is_hostname);
void ares__init_servers_state(ares_channel channel);
void ares__destroy_servers_state(ares_channel channel);
int ares__parse_qtype_reply(const unsigned char* abuf, int alen, int* qtype);
int ares__single_domain(ares_channel channel, const char *name, char **s);
int ares__cat_domain(const char *name, const char *domain, char **s);
int ares__sortaddrinfo(ares_channel channel, struct ares_addrinfo_node *ai_node);
int ares__readaddrinfo(FILE *fp, const char *name, unsigned short port,
                       const struct ares_addrinfo_hints *hints,
                       struct ares_addrinfo *ai);

struct ares_addrinfo *ares__malloc_addrinfo(void);

struct ares_addrinfo_node *ares__malloc_addrinfo_node(void);
void ares__freeaddrinfo_nodes(struct ares_addrinfo_node *ai_node);

struct ares_addrinfo_node *ares__append_addrinfo_node(struct ares_addrinfo_node **ai_node);
void ares__addrinfo_cat_nodes(struct ares_addrinfo_node **head,
                              struct ares_addrinfo_node *tail);

struct ares_addrinfo_cname *ares__malloc_addrinfo_cname(void);
void ares__freeaddrinfo_cnames(struct ares_addrinfo_cname *ai_cname);

struct ares_addrinfo_cname *ares__append_addrinfo_cname(struct ares_addrinfo_cname **ai_cname);

int ares_append_ai_node(int aftype, unsigned short port, int ttl,
                        const void *adata,
                        struct ares_addrinfo_node **nodes);

void ares__addrinfo_cat_cnames(struct ares_addrinfo_cname **head,
                               struct ares_addrinfo_cname *tail);

int ares__parse_into_addrinfo(const unsigned char *abuf,
                              int alen, int cname_only_is_enodata,
                              unsigned short port,
                              struct ares_addrinfo *ai);

int ares__addrinfo2hostent(const struct ares_addrinfo *ai, int family,
                           struct hostent **host);
int ares__addrinfo2addrttl(const struct ares_addrinfo *ai, int family,
                           int req_naddrttls, struct ares_addrttl *addrttls,
                           struct ares_addr6ttl *addr6ttls, int *naddrttls);
int ares__addrinfo_localhost(const char *name, unsigned short port,
                             const struct ares_addrinfo_hints *hints,
                             struct ares_addrinfo *ai);

#if 0 /* Not used */
long ares__tvdiff(struct timeval t1, struct timeval t2);
#endif

ares_socket_t ares__open_socket(ares_channel channel,
                                int af, int type, int protocol);
void ares__close_socket(ares_channel, ares_socket_t);
int ares__connect_socket(ares_channel channel,
                         ares_socket_t sockfd,
                         const struct sockaddr *addr,
                         ares_socklen_t addrlen);

#define ARES_SWAP_BYTE(a,b) \
  { unsigned char swapByte = *(a);  *(a) = *(b);  *(b) = swapByte; }

#define SOCK_STATE_CALLBACK(c, s, r, w)                                 \
  do {                                                                  \
    if ((c)->sock_state_cb)                                             \
      (c)->sock_state_cb((c)->sock_state_cb_data, (s), (r), (w));       \
  } WHILE_FALSE

#endif /* __ARES_PRIVATE_H */

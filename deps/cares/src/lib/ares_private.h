/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2010 Daniel Stenberg
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
#ifndef __ARES_PRIVATE_H
#define __ARES_PRIVATE_H

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
#endif

#define DEFAULT_TIMEOUT         2000 /* milliseconds */
#define DEFAULT_TRIES           3
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

struct ares_rand_state;
typedef struct ares_rand_state ares_rand_state;

#include "ares__llist.h"
#include "ares__slist.h"
#include "ares__htable_stvp.h"
#include "ares__htable_asvp.h"
#include "ares__buf.h"

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

struct server_state;

struct server_connection {
  struct server_state *server;
  ares_socket_t        fd;
  int                  is_tcp;
  /* total number of queries run on this connection since it was established */
  size_t               total_queries;
  /* list of outstanding queries to this connection */
  ares__llist_t       *queries_to_conn;
};

struct server_state {
  size_t idx; /* index for server in ares_channel */
  struct ares_addr addr;

  ares__llist_t            *connections;
  struct server_connection *tcp_conn;

  /* TCP buffer since multiple responses can come back in one read, or partial
   * in a read */
  ares__buf_t              *tcp_parser;

  /* TCP output queue */
  ares__buf_t              *tcp_send;

  /* Which incarnation of this connection is this? We don't want to
   * retransmit requests into the very same socket, but if the server
   * closes on us and we re-open the connection, then we do want to
   * re-send. */
  int tcp_connection_generation;

  /* Link back to owning channel */
  ares_channel channel;
};

/* State to represent a DNS query */
struct query {
  /* Query ID from qbuf, for faster lookup, and current timeout */
  unsigned short qid; /* host byte order */
  struct timeval timeout;
  ares_channel channel;

  /*
   * Node object for each list entry the query belongs to in order to
   * make removal operations O(1).
   */
  ares__slist_node_t *node_queries_by_timeout;
  ares__llist_node_t *node_queries_to_conn;
  ares__llist_node_t *node_all_queries;

  /* connection handle for validation purposes */
  const struct server_connection *conn;

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
  int no_retries; /* do not perform any additional retries, this is set when
                   * a query is to be canceled */
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

  /* random state to use when generating new ids */
  ares_rand_state *rand_state;

  /* Generation number to use for the next TCP socket open/close */
  int tcp_connection_generation;

  /* Last server we sent a query to. */
  int last_server;

  /* All active queries in a single list */
  ares__llist_t   *all_queries;
  /* Queries bucketed by qid, for quickly dispatching DNS responses: */
  ares__htable_stvp_t *queries_by_qid;

  /* Queries bucketed by timeout, for quickly handling timeouts: */
  ares__slist_t   *queries_by_timeout;

  /* Map linked list node member for connection to file descriptor.  We use
   * the node instead of the connection object itself so we can quickly look
   * up a connection and remove it if necessary (as otherwise we'd have to
   * scan all connections) */
  ares__htable_asvp_t *connnode_by_socket;

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

  /* Maximum UDP queries per connection allowed */
  int udp_max_queries;
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

/* Returns one of the normal ares status codes like ARES_SUCCESS */
int ares__send_query(ares_channel channel, struct query *query,
                     struct timeval *now);

/* Identical to ares_query, but returns a normal ares return code like
 * ARES_SUCCESS, and can be passed the qid by reference which will be
 * filled in on ARES_SUCCESS */
int ares_query_qid(ares_channel channel, const char *name,
                              int dnsclass, int type, ares_callback callback,
                              void *arg, unsigned short *qid);
/* Identical to ares_send() except returns normal ares return codes like
 * ARES_SUCCESS */
int ares_send_ex(ares_channel channel, const unsigned char *qbuf, int qlen,
                 ares_callback callback, void *arg);
void ares__close_connection(struct server_connection *conn);
void ares__close_sockets(struct server_state *server);
void ares__check_cleanup_conn(ares_channel channel, ares_socket_t fd);
int ares__get_hostent(FILE *fp, int family, struct hostent **host);
int ares__read_line(FILE *fp, char **buf, size_t *bufsize);
void ares__free_query(struct query *query);

ares_rand_state *ares__init_rand_state(void);
void ares__destroy_rand_state(ares_rand_state *state);
void ares__rand_bytes(ares_rand_state *state, unsigned char *buf, size_t len);

unsigned short ares__generate_new_id(ares_rand_state *state);
struct timeval ares__tvnow(void);
int ares__expand_name_validated(const unsigned char *encoded,
                                const unsigned char *abuf,
                                int alen, char **s, long *enclen,
                                int is_hostname);
int ares__expand_name_for_response(const unsigned char *encoded,
                                   const unsigned char *abuf, int alen,
                                   char **s, long *enclen, int is_hostname);
int ares__init_servers_state(ares_channel channel);
void ares__destroy_servers_state(ares_channel channel);
int ares__parse_qtype_reply(const unsigned char* abuf, int alen, int* qtype);
int ares__single_domain(ares_channel channel, const char *name, char **s);
int ares__cat_domain(const char *name, const char *domain, char **s);
int ares__sortaddrinfo(ares_channel channel, struct ares_addrinfo_node *ai_node);
int ares__readaddrinfo(FILE *fp, const char *name, unsigned short port,
                       const struct ares_addrinfo_hints *hints,
                       struct ares_addrinfo *ai);

void ares__freeaddrinfo_nodes(struct ares_addrinfo_node *ai_node);

struct ares_addrinfo_node *ares__append_addrinfo_node(struct ares_addrinfo_node **ai_node);
void ares__addrinfo_cat_nodes(struct ares_addrinfo_node **head,
                              struct ares_addrinfo_node *tail);

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

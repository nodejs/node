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
#  define WIN32
#endif

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#ifdef WATT32
#  include <tcp.h>
#  include <sys/ioctl.h>
#endif

#define DEFAULT_TIMEOUT 2000 /* milliseconds */
#define DEFAULT_TRIES   3
#ifndef INADDR_NONE
#  define INADDR_NONE 0xffffffff
#endif

#ifdef CARES_EXPOSE_STATICS
/* Make some internal functions visible for testing */
#  define STATIC_TESTABLE
#else
#  define STATIC_TESTABLE static
#endif

/* By using a double cast, we can get rid of the bogus warning of
 * warning: cast from 'const struct sockaddr *' to 'const struct sockaddr_in6 *'
 * increases required alignment from 1 to 4 [-Wcast-align]
 */
#define CARES_INADDR_CAST(type, var) ((type)((void *)var))

#if defined(WIN32) && !defined(WATT32)

#  define WIN_NS_9X     "System\\CurrentControlSet\\Services\\VxD\\MSTCP"
#  define WIN_NS_NT_KEY "System\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#  define WIN_DNSCLIENT "Software\\Policies\\Microsoft\\System\\DNSClient"
#  define WIN_NT_DNSCLIENT \
    "Software\\Policies\\Microsoft\\Windows NT\\DNSClient"
#  define NAMESERVER           "NameServer"
#  define DHCPNAMESERVER       "DhcpNameServer"
#  define DATABASEPATH         "DatabasePath"
#  define WIN_PATH_HOSTS       "\\hosts"
#  define SEARCHLIST_KEY       "SearchList"
#  define PRIMARYDNSSUFFIX_KEY "PrimaryDNSSuffix"
#  define INTERFACES_KEY       "Interfaces"
#  define DOMAIN_KEY           "Domain"
#  define DHCPDOMAIN_KEY       "DhcpDomain"
#  define PATH_RESOLV_CONF     ""
#elif defined(WATT32)

#  define PATH_RESOLV_CONF "/dev/ENV/etc/resolv.conf"
W32_FUNC const char *_w32_GetHostsFile(void);

#elif defined(NETWARE)

#  define PATH_RESOLV_CONF "sys:/etc/resolv.cfg"
#  define PATH_HOSTS       "sys:/etc/hosts"

#elif defined(__riscos__)

#  define PATH_RESOLV_CONF ""
#  define PATH_HOSTS       "InetDBase:Hosts"

#elif defined(__HAIKU__)

#  define PATH_RESOLV_CONF "/system/settings/network/resolv.conf"
#  define PATH_HOSTS       "/system/settings/network/hosts"

#else

#  define PATH_RESOLV_CONF "/etc/resolv.conf"
#  ifdef ETC_INET
#    define PATH_HOSTS "/etc/inet/hosts"
#  else
#    define PATH_HOSTS "/etc/hosts"
#  endif

#endif

#include "ares_ipv6.h"

struct ares_rand_state;
typedef struct ares_rand_state ares_rand_state;

#include "ares__llist.h"
#include "ares__slist.h"
#include "ares__htable_strvp.h"
#include "ares__htable_szvp.h"
#include "ares__htable_asvp.h"
#include "ares__buf.h"
#include "ares_dns_private.h"

#ifndef HAVE_GETENV
#  include "ares_getenv.h"
#  define getenv(ptr) ares_getenv(ptr)
#endif

#include "ares_str.h"
#include "ares_strsplit.h"

#ifndef HAVE_STRCASECMP
#  include "ares_strcasecmp.h"
#  define strcasecmp(p1, p2) ares_strcasecmp(p1, p2)
#endif

#ifndef HAVE_STRNCASECMP
#  include "ares_strcasecmp.h"
#  define strncasecmp(p1, p2, n) ares_strncasecmp(p1, p2, n)
#endif

/********* EDNS defines section ******/
#define EDNSPACKETSZ                                                  \
  1280                   /* Reasonable UDP payload size, as suggested \
                            in RFC2671 */
#define MAXENDSSZ   4096 /* Maximum (local) limit for edns packet size */
#define EDNSFIXEDSZ 11   /* Size of EDNS header */

/********* EDNS defines section ******/


struct query;

struct server_state;

struct server_connection {
  struct server_state *server;
  ares_socket_t        fd;
  ares_bool_t          is_tcp;
  /* total number of queries run on this connection since it was established */
  size_t               total_queries;
  /* list of outstanding queries to this connection */
  ares__llist_t       *queries_to_conn;
};

struct server_state {
  size_t                    idx; /* index for server in system configuration */
  size_t                    consec_failures; /* Consecutive query failure count
                                              * can be hard errors or timeouts
                                              */
  struct ares_addr          addr;
  unsigned short            udp_port;        /* host byte order */
  unsigned short            tcp_port;        /* host byte order */

  ares__llist_t            *connections;
  struct server_connection *tcp_conn;

  /* TCP buffer since multiple responses can come back in one read, or partial
   * in a read */
  ares__buf_t              *tcp_parser;

  /* TCP output queue */
  ares__buf_t              *tcp_send;

  /* Link back to owning channel */
  ares_channel_t           *channel;
};

/* State to represent a DNS query */
struct query {
  /* Query ID from qbuf, for faster lookup, and current timeout */
  unsigned short            qid; /* host byte order */
  struct timeval            timeout;
  ares_channel_t           *channel;

  /*
   * Node object for each list entry the query belongs to in order to
   * make removal operations O(1).
   */
  ares__slist_node_t       *node_queries_by_timeout;
  ares__llist_node_t       *node_queries_to_conn;
  ares__llist_node_t       *node_all_queries;

  /* connection handle query is associated with */
  struct server_connection *conn;

  /* Arguments passed to ares_send() */
  unsigned char            *qbuf;
  size_t                    qlen;

  ares_callback             callback;
  void                     *arg;

  /* Query status */
  size_t        try_count; /* Number of times we tried this query already. */
  ares_bool_t   using_tcp;
  ares_status_t error_status;
  size_t        timeouts; /* number of timeouts we saw for this request */
  ares_bool_t no_retries; /* do not perform any additional retries, this is set
                           * when a query is to be canceled */
};

/* An IP address pattern; matches an IP address X if X & mask == addr */
#define PATTERN_MASK 0x1
#define PATTERN_CIDR 0x2

struct apattern {
  union {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
  } addr;

  union {
    struct in_addr       addr4;
    struct ares_in6_addr addr6;
    unsigned short       bits;
  } mask;

  int            family;
  unsigned short type;
};

struct ares__qcache;
typedef struct ares__qcache ares__qcache_t;

struct ares_hosts_file;
typedef struct ares_hosts_file ares_hosts_file_t;

struct ares__thread_mutex;
typedef struct ares__thread_mutex ares__thread_mutex_t;

struct ares_channeldata {
  /* Configuration data */
  unsigned int          flags;
  size_t                timeout; /* in milliseconds */
  size_t                tries;
  size_t                ndots;
  size_t                maxtimeout;              /* in milliseconds */
  ares_bool_t           rotate;
  unsigned short        udp_port;                /* stored in network order */
  unsigned short        tcp_port;                /* stored in network order */
  int                   socket_send_buffer_size; /* setsockopt takes int */
  int                   socket_receive_buffer_size; /* setsockopt takes int */
  char                **domains;
  size_t                ndomains;
  struct apattern      *sortlist;
  size_t                nsort;
  char                 *lookups;
  size_t                ednspsz;
  unsigned int          qcache_max_ttl;
  unsigned int          optmask;

  /* For binding to local devices and/or IP addresses.  Leave
   * them null/zero for no binding.
   */
  char                  local_dev_name[32];
  unsigned int          local_ip4;
  unsigned char         local_ip6[16];

  /* Thread safety lock */
  ares__thread_mutex_t *lock;

  /* Server addresses and communications state. Sorted by least consecutive
   * failures, followed by the configuration order if failures are equal. */
  ares__slist_t        *servers;

  /* random state to use when generating new ids and generating retry penalties
   */
  ares_rand_state      *rand_state;

  /* All active queries in a single list */
  ares__llist_t        *all_queries;
  /* Queries bucketed by qid, for quickly dispatching DNS responses: */
  ares__htable_szvp_t  *queries_by_qid;

  /* Queries bucketed by timeout, for quickly handling timeouts: */
  ares__slist_t        *queries_by_timeout;

  /* Map linked list node member for connection to file descriptor.  We use
   * the node instead of the connection object itself so we can quickly look
   * up a connection and remove it if necessary (as otherwise we'd have to
   * scan all connections) */
  ares__htable_asvp_t  *connnode_by_socket;

  ares_sock_state_cb    sock_state_cb;
  void                 *sock_state_cb_data;

  ares_sock_create_callback           sock_create_cb;
  void                               *sock_create_cb_data;

  ares_sock_config_callback           sock_config_cb;
  void                               *sock_config_cb_data;

  const struct ares_socket_functions *sock_funcs;
  void                               *sock_func_cb_data;

  /* Path for resolv.conf file, configurable via ares_options */
  char                               *resolvconf_path;

  /* Path for hosts file, configurable via ares_options */
  char                               *hosts_path;

  /* Maximum UDP queries per connection allowed */
  size_t                              udp_max_queries;

  /* Cache of local hosts file */
  ares_hosts_file_t                  *hf;

  /* Query Cache */
  ares__qcache_t                     *qcache;
};

/* Does the domain end in ".onion" or ".onion."? Case-insensitive. */
ares_bool_t   ares__is_onion_domain(const char *name);

/* Memory management functions */
extern void  *(*ares_malloc)(size_t size);
extern void  *(*ares_realloc)(void *ptr, size_t size);
extern void   (*ares_free)(void *ptr);
void         *ares_malloc_zero(size_t size);
void         *ares_realloc_zero(void *ptr, size_t orig_size, size_t new_size);

/* return true if now is exactly check time or later */
ares_bool_t   ares__timedout(const struct timeval *now,
                             const struct timeval *check);

/* Returns one of the normal ares status codes like ARES_SUCCESS */
ares_status_t ares__send_query(struct query *query, struct timeval *now);
ares_status_t ares__requeue_query(struct query *query, struct timeval *now);

/* Identical to ares_query, but returns a normal ares return code like
 * ARES_SUCCESS, and can be passed the qid by reference which will be
 * filled in on ARES_SUCCESS */
ares_status_t ares_query_qid(ares_channel_t *channel, const char *name,
                             int dnsclass, int type, ares_callback callback,
                             void *arg, unsigned short *qid);
/* Identical to ares_send() except returns normal ares return codes like
 * ARES_SUCCESS */
ares_status_t ares_send_ex(ares_channel_t *channel, const unsigned char *qbuf,
                           size_t qlen, ares_callback callback, void *arg,
                           unsigned short *qid);
void          ares__close_connection(struct server_connection *conn);
void          ares__close_sockets(struct server_state *server);
void          ares__check_cleanup_conn(const ares_channel_t     *channel,
                                       struct server_connection *conn);
ares_status_t ares__read_line(FILE *fp, char **buf, size_t *bufsize);
void          ares__free_query(struct query *query);

ares_rand_state *ares__init_rand_state(void);
void             ares__destroy_rand_state(ares_rand_state *state);
void ares__rand_bytes(ares_rand_state *state, unsigned char *buf, size_t len);

unsigned short ares__generate_new_id(ares_rand_state *state);
struct timeval ares__tvnow(void);
ares_status_t  ares__expand_name_validated(const unsigned char *encoded,
                                           const unsigned char *abuf,
                                           size_t alen, char **s, size_t *enclen,
                                           ares_bool_t is_hostname);
ares_status_t  ares__expand_name_for_response(const unsigned char *encoded,
                                              const unsigned char *abuf,
                                              size_t alen, char **s,
                                              size_t     *enclen,
                                              ares_bool_t is_hostname);
ares_status_t  ares_expand_string_ex(const unsigned char *encoded,
                                     const unsigned char *abuf, size_t alen,
                                     unsigned char **s, size_t *enclen);
ares_status_t  ares__init_servers_state(ares_channel_t *channel);
ares_status_t  ares__init_by_options(ares_channel_t            *channel,
                                     const struct ares_options *options,
                                     int                        optmask);
ares_status_t  ares__init_by_sysconfig(ares_channel_t *channel);

typedef struct {
  ares__llist_t   *sconfig;
  struct apattern *sortlist;
  size_t           nsortlist;
  char           **domains;
  size_t           ndomains;
  char            *lookups;
  size_t           ndots;
  size_t           tries;
  ares_bool_t      rotate;
  size_t           timeout_ms;
} ares_sysconfig_t;

ares_status_t ares__init_by_environment(ares_sysconfig_t *sysconfig);

ares_status_t ares__init_sysconfig_files(const ares_channel_t *channel,
                                         ares_sysconfig_t     *sysconfig);
ares_status_t ares__parse_sortlist(struct apattern **sortlist, size_t *nsort,
                                   const char *str);

void          ares__destroy_servers_state(ares_channel_t *channel);
ares_status_t ares__single_domain(const ares_channel_t *channel,
                                  const char *name, char **s);
ares_status_t ares__cat_domain(const char *name, const char *domain, char **s);
ares_status_t ares__sortaddrinfo(ares_channel_t            *channel,
                                 struct ares_addrinfo_node *ai_node);

void          ares__freeaddrinfo_nodes(struct ares_addrinfo_node *ai_node);
ares_bool_t   ares__is_localhost(const char *name);

struct ares_addrinfo_node    *
  ares__append_addrinfo_node(struct ares_addrinfo_node **ai_node);
void ares__addrinfo_cat_nodes(struct ares_addrinfo_node **head,
                              struct ares_addrinfo_node  *tail);

void ares__freeaddrinfo_cnames(struct ares_addrinfo_cname *ai_cname);

struct ares_addrinfo_cname *
  ares__append_addrinfo_cname(struct ares_addrinfo_cname **ai_cname);

ares_status_t ares_append_ai_node(int aftype, unsigned short port,
                                  unsigned int ttl, const void *adata,
                                  struct ares_addrinfo_node **nodes);

void          ares__addrinfo_cat_cnames(struct ares_addrinfo_cname **head,
                                        struct ares_addrinfo_cname  *tail);

ares_status_t ares__parse_into_addrinfo(const unsigned char *abuf, size_t alen,
                                        ares_bool_t    cname_only_is_enodata,
                                        unsigned short port,
                                        struct ares_addrinfo *ai);

ares_status_t ares__addrinfo2hostent(const struct ares_addrinfo *ai, int family,
                                     struct hostent **host);
ares_status_t ares__addrinfo2addrttl(const struct ares_addrinfo *ai, int family,
                                     size_t                req_naddrttls,
                                     struct ares_addrttl  *addrttls,
                                     struct ares_addr6ttl *addr6ttls,
                                     size_t               *naddrttls);
ares_status_t ares__addrinfo_localhost(const char *name, unsigned short port,
                                       const struct ares_addrinfo_hints *hints,
                                       struct ares_addrinfo             *ai);
ares_status_t ares__open_connection(ares_channel_t      *channel,
                                    struct server_state *server,
                                    ares_bool_t          is_tcp);
ares_socket_t ares__open_socket(ares_channel_t *channel, int af, int type,
                                int protocol);
ares_ssize_t  ares__socket_write(ares_channel_t *channel, ares_socket_t s,
                                 const void *data, size_t len);
ares_ssize_t  ares__socket_recvfrom(ares_channel_t *channel, ares_socket_t s,
                                    void *data, size_t data_len, int flags,
                                    struct sockaddr *from,
                                    ares_socklen_t  *from_len);
ares_ssize_t  ares__socket_recv(ares_channel_t *channel, ares_socket_t s,
                                void *data, size_t data_len);
void          ares__close_socket(ares_channel, ares_socket_t);
int         ares__connect_socket(ares_channel_t *channel, ares_socket_t sockfd,
                                 const struct sockaddr *addr, ares_socklen_t addrlen);
ares_bool_t ares__is_hostnamech(int ch);
void        ares__destroy_server(struct server_state *server);

ares_status_t ares__servers_update(ares_channel_t *channel,
                                   ares__llist_t  *server_list,
                                   ares_bool_t     user_specified);
ares_status_t ares__sconfig_append(ares__llist_t         **sconfig,
                                   const struct ares_addr *addr,
                                   unsigned short          udp_port,
                                   unsigned short          tcp_port);
ares_status_t ares__sconfig_append_fromstr(ares__llist_t **sconfig,
                                           const char     *str);
ares_status_t ares_in_addr_to_server_config_llist(const struct in_addr *servers,
                                                  size_t          nservers,
                                                  ares__llist_t **llist);

struct ares_hosts_entry;
typedef struct ares_hosts_entry ares_hosts_entry_t;

void                            ares__hosts_file_destroy(ares_hosts_file_t *hf);
ares_status_t ares__hosts_search_ipaddr(ares_channel_t *channel,
                                        ares_bool_t use_env, const char *ipaddr,
                                        const ares_hosts_entry_t **entry);
ares_status_t ares__hosts_search_host(ares_channel_t *channel,
                                      ares_bool_t use_env, const char *host,
                                      const ares_hosts_entry_t **entry);
ares_status_t ares__hosts_entry_to_hostent(const ares_hosts_entry_t *entry,
                                           int                       family,
                                           struct hostent          **hostent);
ares_status_t ares__hosts_entry_to_addrinfo(const ares_hosts_entry_t *entry,
                                            const char *name, int family,
                                            unsigned short        port,
                                            ares_bool_t           want_cnames,
                                            struct ares_addrinfo *ai);
ares_bool_t   ares__isprint(int ch);


/*! Parse a compressed DNS name as defined in RFC1035 starting at the current
 *  offset within the buffer.
 *
 *  It is assumed that either a const buffer is being used, or before
 *  the message processing was started that ares__buf_reclaim() was called.
 *
 *  \param[in]  buf        Initialized buffer object
 *  \param[out] name       Pointer passed by reference to be filled in with
 *                         allocated string of the parsed name that must be
 *                         ares_free()'d by the caller.
 *  \param[in] is_hostname if ARES_TRUE, will validate the character set for
 *                         a valid hostname or will return error.
 *  \return ARES_SUCCESS on success
 */
ares_status_t ares__dns_name_parse(ares__buf_t *buf, char **name,
                                   ares_bool_t is_hostname);

/*! Write the DNS name to the buffer in the DNS domain-name syntax as a
 *  series of labels.  The maximum domain name length is 255 characters with
 *  each label being a maximum of 63 characters.  If the validate_hostname
 *  flag is set, it will strictly validate the character set.
 *
 *  \param[in,out]  buf   Initialized buffer object to write name to
 *  \param[in,out]  list  Pointer passed by reference to maintain a list of
 *                        domain name to indexes used for name compression.
 *                        Pass NULL (not by reference) if name compression isn't
 *                        desired.  Otherwise the list will be automatically
 *                        created upon first entry.
 *  \param[in]      validate_hostname Validate the hostname character set.
 *  \param[in]      name              Name to write out, it may have escape
 *                                    sequences.
 *  \return ARES_SUCCESS on success, most likely ARES_EBADNAME if the name is
 *          bad.
 */
ares_status_t ares__dns_name_write(ares__buf_t *buf, ares__llist_t **list,
                                   ares_bool_t validate_hostname,
                                   const char *name);

#define ARES_SWAP_BYTE(a, b)           \
  do {                                 \
    unsigned char swapByte = *(a);     \
    *(a)                   = *(b);     \
    *(b)                   = swapByte; \
  } while (0)

#define SOCK_STATE_CALLBACK(c, s, r, w)                           \
  do {                                                            \
    if ((c)->sock_state_cb) {                                     \
      (c)->sock_state_cb((c)->sock_state_cb_data, (s), (r), (w)); \
    }                                                             \
  } while (0)

#define ARES_CONFIG_CHECK(x)                                             \
  (x && x->lookups && ares__slist_len(x->servers) > 0 && x->ndots > 0 && \
   x->timeout > 0 && x->tries > 0)

size_t        ares__round_up_pow2(size_t n);
size_t        ares__log2(size_t n);
size_t        ares__pow(size_t x, size_t y);
size_t        ares__count_digits(size_t n);
size_t        ares__count_hexdigits(size_t n);
void          ares__qcache_destroy(ares__qcache_t *cache);
ares_status_t ares__qcache_create(ares_rand_state *rand_state,
                                  unsigned int     max_ttl,
                                  ares__qcache_t **cache_out);
void          ares__qcache_flush(ares__qcache_t *cache);
ares_status_t ares_qcache_insert(ares_channel_t       *channel,
                                 const struct timeval *now,
                                 const struct query   *query,
                                 ares_dns_record_t    *dnsrec);
ares_status_t ares_qcache_fetch(ares_channel_t       *channel,
                                const struct timeval *now,
                                const unsigned char *qbuf, size_t qlen,
                                unsigned char **abuf, size_t *alen);

ares_status_t ares__channel_threading_init(ares_channel_t *channel);
void          ares__channel_threading_destroy(ares_channel_t *channel);
void          ares__channel_lock(ares_channel_t *channel);
void          ares__channel_unlock(ares_channel_t *channel);

#ifdef _MSC_VER
typedef __int64          ares_int64_t;
typedef unsigned __int64 ares_uint64_t;
#else
typedef long long          ares_int64_t;
typedef unsigned long long ares_uint64_t;
#endif

#endif /* __ARES_PRIVATE_H */

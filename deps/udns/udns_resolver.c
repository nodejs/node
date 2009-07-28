/* $Id: udns_resolver.c,v 1.98 2007/01/10 13:32:33 mjt Exp $
   resolver stuff (main module)

   Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
   This file is part of UDNS library, an async DNS stub resolver.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library, in file named COPYING.LGPL; if not,
   write to the Free Software Foundation, Inc., 59 Temple Place,
   Suite 330, Boston, MA  02111-1307  USA

 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef WINDOWS
# include <winsock2.h>          /* includes <windows.h> */
# include <ws2tcpip.h>          /* needed for struct in6_addr */
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/time.h>
# ifdef HAVE_POLL
#  include <sys/poll.h>
# else
#  ifdef HAVE_SYS_SELECT_H
#   include <sys/select.h>
#  endif
# endif
# ifdef HAVE_TIMES
#  include <sys/times.h>
# endif
# define closesocket(sock) close(sock)
#endif	/* !WINDOWS */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include "udns.h"

#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EINVAL
#endif
#ifndef MSG_DONTWAIT
# define MSG_DONTWAIT 0
#endif

struct dns_qlink {
  struct dns_query *next, *prev;
};

struct dns_query {
  struct dns_qlink dnsq_link;		/* list entry (should be first) */
  unsigned dnsq_origdnl0;		/* original query DN len w/o last 0 */
  unsigned dnsq_flags;			/* control flags for this query */
  unsigned dnsq_servi;			/* index of next server to try */
  unsigned dnsq_servwait;		/* bitmask: servers left to wait */
  unsigned dnsq_servskip;		/* bitmask: servers to skip */
  unsigned dnsq_servnEDNS0;		/* bitmask: servers refusing EDNS0 */
  unsigned dnsq_try;			/* number of tries made so far */
  dnscc_t *dnsq_nxtsrch;		/* next search pointer @dnsc_srchbuf */
  time_t dnsq_deadline;			/* when current try will expire */
  dns_parse_fn *dnsq_parse;		/* parse: raw => application */
  dns_query_fn *dnsq_cbck;		/* the callback to call when done */
  void *dnsq_cbdata;			/* user data for the callback */
#ifndef NDEBUG
  struct dns_ctx *dnsq_ctx;		/* the resolver context */
#endif
  /* char fields at the end to avoid padding */
  dnsc_t dnsq_id[2];			/* query ID */
  dnsc_t dnsq_typcls[4];		/* requested RR type+class */
  dnsc_t dnsq_dn[DNS_MAXDN+DNS_DNPAD];	/* the query DN +alignment */
};

/* working with dns_query lists */

static __inline void qlist_init(struct dns_qlink *list) {
  list->next = list->prev = (struct dns_query *)list;
}

static __inline int qlist_isempty(const struct dns_qlink *list) {
  return list->next == (const struct dns_query *)list ? 1 : 0;
}

static __inline struct dns_query *qlist_first(struct dns_qlink *list) {
  return list->next == (struct dns_query *)list ? 0 : list->next;
}

static __inline void qlist_remove(struct dns_query *q) {
  q->dnsq_link.next->dnsq_link.prev = q->dnsq_link.prev;
  q->dnsq_link.prev->dnsq_link.next = q->dnsq_link.next;
}

/* insert q between prev and next */
static __inline void
qlist_insert(struct dns_query *q,
             struct dns_query *prev, struct dns_query *next) {
  q->dnsq_link.next = next;
  q->dnsq_link.prev = prev;
  prev->dnsq_link.next = next->dnsq_link.prev = q;
}

static __inline void
qlist_insert_after(struct dns_query *q, struct dns_query *prev) {
  qlist_insert(q, prev, prev->dnsq_link.next);
}

static __inline void
qlist_insert_before(struct dns_query *q, struct dns_query *next) {
  qlist_insert(q, next->dnsq_link.prev, next);
}

static __inline void
qlist_add_tail(struct dns_query *q, struct dns_qlink *top) {
  qlist_insert_before(q, (struct dns_query *)top);
}

static __inline void
qlist_add_head(struct dns_query *q, struct dns_qlink *top) {
  qlist_insert_after(q, (struct dns_query *)top);
}

#define QLIST_FIRST(list, direction) ((list)->direction)
#define QLIST_ISLAST(list, q) ((q) == (struct dns_query*)(list))
#define QLIST_NEXT(q, direction) ((q)->dnsq_link.direction)

#define QLIST_FOR_EACH(list, q, direction) \
  for(q = QLIST_FIRST(list, direction); \
      !QLIST_ISLAST(list, q); q = QLIST_NEXT(q, direction))

union sockaddr_ns {
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef HAVE_IPv6
  struct sockaddr_in6 sin6;
#endif
};

#define sin_eq(a,b) \
	((a).sin_port == (b).sin_port && \
	 (a).sin_addr.s_addr == (b).sin_addr.s_addr)
#define sin6_eq(a,b) \
	((a).sin6_port == (b).sin6_port && \
	 memcmp(&(a).sin6_addr, &(b).sin6_addr, sizeof(struct in6_addr)) == 0)

struct dns_ctx {		/* resolver context */
  /* settings */
  unsigned dnsc_flags;			/* various flags */
  unsigned dnsc_timeout;		/* timeout (base value) for queries */
  unsigned dnsc_ntries;			/* number of retries */
  unsigned dnsc_ndots;			/* ndots to assume absolute name */
  unsigned dnsc_port;			/* default port (DNS_PORT) */
  unsigned dnsc_udpbuf;			/* size of UDP buffer */
  /* array of nameserver addresses */
  union sockaddr_ns dnsc_serv[DNS_MAXSERV];
  unsigned dnsc_nserv;			/* number of nameservers */
  unsigned dnsc_salen;			/* length of socket addresses */
  dnsc_t dnsc_srchbuf[1024];		/* buffer for searchlist */
  dnsc_t *dnsc_srchend;			/* current end of srchbuf */

  dns_utm_fn *dnsc_utmfn;		/* register/cancel timer events */
  void *dnsc_utmctx;			/* user timer context for utmfn() */
  time_t dnsc_utmexp;			/* when user timer expires */

  dns_dbgfn *dnsc_udbgfn;		/* debugging function */

  /* dynamic data */
  unsigned dnsc_nextid;		/* next queue ID to use */
  int dnsc_udpsock;			/* UDP socket */
  struct dns_qlink dnsc_qactive;	/* active list sorted by deadline */
  int dnsc_nactive;			/* number entries in dnsc_qactive */
  dnsc_t *dnsc_pbuf;			/* packet buffer (udpbuf size) */
  int dnsc_qstatus;			/* last query status value */
};

static const struct {
  const char *name;
  enum dns_opt opt;
  unsigned offset;
  unsigned min, max;
} dns_opts[] = {
#define opt(name,opt,field,min,max) \
	{name,opt,offsetof(struct dns_ctx,field),min,max}
  opt("retrans", DNS_OPT_TIMEOUT, dnsc_timeout, 1,300),
  opt("timeout", DNS_OPT_TIMEOUT, dnsc_timeout, 1,300),
  opt("retry",    DNS_OPT_NTRIES, dnsc_ntries, 1,50),
  opt("attempts", DNS_OPT_NTRIES, dnsc_ntries, 1,50),
  opt("ndots", DNS_OPT_NDOTS, dnsc_ndots, 0,1000),
  opt("port", DNS_OPT_PORT, dnsc_port, 1,0xffff),
  opt("udpbuf", DNS_OPT_UDPSIZE, dnsc_udpbuf, DNS_MAXPACKET,65536),
#undef opt
};
#define dns_ctxopt(ctx,idx) (*((unsigned*)(((char*)ctx)+dns_opts[idx].offset)))

#define ISSPACE(x) (x == ' ' || x == '\t' || x == '\r' || x == '\n')

struct dns_ctx dns_defctx;

#define SETCTX(ctx) if (!ctx) ctx = &dns_defctx
#define SETCTXINITED(ctx) SETCTX(ctx); assert(CTXINITED(ctx))
#define CTXINITED(ctx) (ctx->dnsc_flags & DNS_INITED)
#define SETCTXFRESH(ctx) SETCTXINITED(ctx); assert(!CTXOPEN(ctx))
#define SETCTXINACTIVE(ctx) \
		SETCTXINITED(ctx); assert(!ctx->dnsc_nactive)
#define SETCTXOPEN(ctx) SETCTXINITED(ctx); assert(CTXOPEN(ctx))
#define CTXOPEN(ctx) (ctx->dnsc_udpsock >= 0)

#if defined(NDEBUG) || !defined(DEBUG)
#define dns_assert_ctx(ctx)
#else
static void dns_assert_ctx(const struct dns_ctx *ctx) {
  int nactive = 0;
  const struct dns_query *q;
  QLIST_FOR_EACH(&ctx->dnsc_qactive, q, next) {
    assert(q->dnsq_ctx == ctx);
    assert(q->dnsq_link.next->dnsq_link.prev == q);
    assert(q->dnsq_link.prev->dnsq_link.next == q);
    ++nactive;
  }
  assert(nactive == ctx->dnsc_nactive);
}
#endif

enum {
  DNS_INTERNAL		= 0xffff, /* internal flags mask */
  DNS_INITED		= 0x0001, /* the context is initialized */
  DNS_ASIS_DONE		= 0x0002, /* search: skip the last as-is query */
  DNS_SEEN_NODATA	= 0x0004, /* search: NODATA has been received */
};

int dns_add_serv(struct dns_ctx *ctx, const char *serv) {
  union sockaddr_ns *sns;
  SETCTXFRESH(ctx);
  if (!serv)
    return (ctx->dnsc_nserv = 0);
  if (ctx->dnsc_nserv >= DNS_MAXSERV)
    return errno = ENFILE, -1;
  sns = &ctx->dnsc_serv[ctx->dnsc_nserv];
  memset(sns, 0, sizeof(*sns));
  if (dns_pton(AF_INET, serv, &sns->sin.sin_addr) > 0) {
    sns->sin.sin_family = AF_INET;
    return ++ctx->dnsc_nserv;
  }
#ifdef HAVE_IPv6
  if (dns_pton(AF_INET6, serv, &sns->sin6.sin6_addr) > 0) {
    sns->sin6.sin6_family = AF_INET6;
    return ++ctx->dnsc_nserv;
  }
#endif
  errno = EINVAL;
  return -1;
}

int dns_add_serv_s(struct dns_ctx *ctx, const struct sockaddr *sa) {
  SETCTXFRESH(ctx);
  if (!sa)
    return (ctx->dnsc_nserv = 0);
  if (ctx->dnsc_nserv >= DNS_MAXSERV)
    return errno = ENFILE, -1;
#ifdef HAVE_IPv6
  else if (sa->sa_family == AF_INET6)
    ctx->dnsc_serv[ctx->dnsc_nserv].sin6 = *(struct sockaddr_in6*)sa;
#endif
  else if (sa->sa_family == AF_INET)
    ctx->dnsc_serv[ctx->dnsc_nserv].sin = *(struct sockaddr_in*)sa;
  else
    return errno = EAFNOSUPPORT, -1;
  return ++ctx->dnsc_nserv;
}

int dns_set_opts(struct dns_ctx *ctx, const char *opts) {
  unsigned i, v;
  SETCTXINACTIVE(ctx);
  for(;;) {
    while(ISSPACE(*opts)) ++opts;
    if (!*opts) break;
    for(i = 0; i < sizeof(dns_opts)/sizeof(dns_opts[0]); ++i) {
      v = strlen(dns_opts[i].name);
      if (strncmp(dns_opts[i].name, opts, v) != 0 ||
          (opts[v] != ':' && opts[v] != '='))
        continue;
      opts += v + 1;
      v = 0;
      if (*opts < '0' || *opts > '9') break;
      do v = v * 10 + (*opts++ - '0');
      while (*opts >= '0' && *opts <= '9');
      if (v < dns_opts[i].min) v = dns_opts[i].min;
      if (v > dns_opts[i].max) v = dns_opts[i].max;
      dns_ctxopt(ctx, i) = v;
      break;
    }
    while(*opts && !ISSPACE(*opts)) ++opts;
  }
  return 0;
}

int dns_set_opt(struct dns_ctx *ctx, enum dns_opt opt, int val) {
  int prev;
  unsigned i;
  SETCTXINACTIVE(ctx);
  for(i = 0; i < sizeof(dns_opts)/sizeof(dns_opts[0]); ++i) {
    if (dns_opts[i].opt != opt) continue;
    prev = dns_ctxopt(ctx, i);
    if (val >= 0) {
      unsigned v = val;
      if (v < dns_opts[i].min || v > dns_opts[i].max) {
        errno = EINVAL;
        return -1;
      }
      dns_ctxopt(ctx, i) = v;
    }
    return prev;
  }
  if (opt == DNS_OPT_FLAGS) {
    prev = ctx->dnsc_flags & ~DNS_INTERNAL;
    if (val >= 0)
      ctx->dnsc_flags =
        (ctx->dnsc_flags & DNS_INTERNAL) | (val & ~DNS_INTERNAL);
    return prev;
  }
  errno = ENOSYS;
  return -1;
}

int dns_add_srch(struct dns_ctx *ctx, const char *srch) {
  int dnl;
  SETCTXINACTIVE(ctx);
  if (!srch) {
    memset(ctx->dnsc_srchbuf, 0, sizeof(ctx->dnsc_srchbuf));
    ctx->dnsc_srchend = ctx->dnsc_srchbuf;
    return 0;
  }
  dnl =
    sizeof(ctx->dnsc_srchbuf) - (ctx->dnsc_srchend - ctx->dnsc_srchbuf) - 1;
  dnl = dns_sptodn(srch, ctx->dnsc_srchend, dnl);
  if (dnl > 0)
    ctx->dnsc_srchend += dnl;
  ctx->dnsc_srchend[0] = '\0';	/* we ensure the list is always ends at . */
  if (dnl > 0)
    return 0;
  errno = EINVAL;
  return -1;
}

static void dns_drop_utm(struct dns_ctx *ctx) {
  if (ctx->dnsc_utmfn)
    ctx->dnsc_utmfn(NULL, -1, ctx->dnsc_utmctx);
  ctx->dnsc_utmctx = NULL;
  ctx->dnsc_utmexp = -1;
}

static void
_dns_request_utm(struct dns_ctx *ctx, time_t now) {
  struct dns_query *q;
  time_t deadline;
  int timeout;
  q = qlist_first(&ctx->dnsc_qactive);
  if (!q)
    deadline = -1, timeout = -1;
  else if (!now || q->dnsq_deadline <= now)
    deadline = 0, timeout = 0;
  else
    deadline = q->dnsq_deadline, timeout = (int)(deadline - now);
  if (ctx->dnsc_utmexp == deadline)
    return;
  ctx->dnsc_utmfn(ctx, timeout, ctx->dnsc_utmctx);
  ctx->dnsc_utmexp = deadline;
}

static __inline void
dns_request_utm(struct dns_ctx *ctx, time_t now) {
  if (ctx->dnsc_utmfn)
    _dns_request_utm(ctx, now);
}

void dns_set_dbgfn(struct dns_ctx *ctx, dns_dbgfn *dbgfn) {
  SETCTXINITED(ctx);
  ctx->dnsc_udbgfn = dbgfn;
}

void
dns_set_tmcbck(struct dns_ctx *ctx, dns_utm_fn *fn, void *data) {
  SETCTXINITED(ctx);
  dns_drop_utm(ctx);
  ctx->dnsc_utmfn = fn;
  ctx->dnsc_utmctx = data;
  if (CTXOPEN(ctx))
    dns_request_utm(ctx, 0);
}

unsigned dns_random16(void) {
#ifdef WINDOWS
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
#define x (ft.dwLowDateTime)
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
#define x (tv.tv_usec)
#endif
  return ((unsigned)x ^ ((unsigned)x >> 16)) & 0xffff;
#undef x
}

void dns_close(struct dns_ctx *ctx) {
  struct dns_query *q;
  SETCTX(ctx);
  if (CTXINITED(ctx)) {
    if (ctx->dnsc_udpsock >= 0)
      closesocket(ctx->dnsc_udpsock);
    ctx->dnsc_udpsock = -1;
    if (ctx->dnsc_pbuf)
      free(ctx->dnsc_pbuf);
    ctx->dnsc_pbuf = NULL;
    while((q = qlist_first(&ctx->dnsc_qactive)) != NULL) {
      qlist_remove(q);
      free(q);
    }
    ctx->dnsc_nactive = 0;
    dns_drop_utm(ctx);
  }
}

void dns_reset(struct dns_ctx *ctx) {
  SETCTX(ctx);
  dns_close(ctx);
  memset(ctx, 0, sizeof(*ctx));
  ctx->dnsc_timeout = 4;
  ctx->dnsc_ntries = 3;
  ctx->dnsc_ndots = 1;
  ctx->dnsc_udpbuf = DNS_EDNS0PACKET;
  ctx->dnsc_port = DNS_PORT;
  ctx->dnsc_udpsock = -1;
  ctx->dnsc_srchend = ctx->dnsc_srchbuf;
  qlist_init(&ctx->dnsc_qactive);
  ctx->dnsc_nextid = dns_random16();
  ctx->dnsc_flags = DNS_INITED;
}

struct dns_ctx *dns_new(const struct dns_ctx *copy) {
  struct dns_ctx *ctx;
  SETCTXINITED(copy);
  dns_assert_ctx(copy);
  ctx = malloc(sizeof(*ctx));
  if (!ctx)
    return NULL;
  *ctx = *copy;
  ctx->dnsc_udpsock = -1;
  qlist_init(&ctx->dnsc_qactive);
  ctx->dnsc_nactive = 0;
  ctx->dnsc_pbuf = NULL;
  ctx->dnsc_qstatus = 0;
  ctx->dnsc_utmfn = NULL;
  ctx->dnsc_utmctx = NULL;
  ctx->dnsc_nextid = dns_random16();
  return ctx;
}

void dns_free(struct dns_ctx *ctx) {
  assert(ctx != NULL && ctx != &dns_defctx);
  dns_reset(ctx);
  free(ctx);
}

int dns_open(struct dns_ctx *ctx) {
  int sock;
  unsigned i;
  int port;
  union sockaddr_ns *sns;
#ifdef HAVE_IPv6
  unsigned have_inet6 = 0;
#endif

  SETCTXINITED(ctx);
  assert(!CTXOPEN(ctx));

  port = htons((unsigned short)ctx->dnsc_port);
  /* ensure we have at least one server */
  if (!ctx->dnsc_nserv) {
    sns = ctx->dnsc_serv;
    sns->sin.sin_family = AF_INET;
    sns->sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ctx->dnsc_nserv = 1;
  }

  for (i = 0; i < ctx->dnsc_nserv; ++i) {
    sns = &ctx->dnsc_serv[i];
    /* set port for each sockaddr */
#ifdef HAVE_IPv6
    if (sns->sa.sa_family == AF_INET6) {
      if (!sns->sin6.sin6_port) sns->sin6.sin6_port = (unsigned short)port;
      ++have_inet6;
    }
    else
#endif
    {
      assert(sns->sa.sa_family == AF_INET);
      if (!sns->sin.sin_port) sns->sin.sin_port = (unsigned short)port;
    }
  }

#ifdef HAVE_IPv6
  if (have_inet6 && have_inet6 < ctx->dnsc_nserv) {
    /* convert all IPv4 addresses to IPv6 V4MAPPED */
    struct sockaddr_in6 sin6;
    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    /* V4MAPPED: ::ffff:1.2.3.4 */
    sin6.sin6_addr.s6_addr[10] = 0xff;
    sin6.sin6_addr.s6_addr[11] = 0xff;
    for(i = 0; i < ctx->dnsc_nserv; ++i) {
      sns = &ctx->dnsc_serv[i];
      if (sns->sa.sa_family == AF_INET) {
        sin6.sin6_port = sns->sin.sin_port;
        ((struct in_addr*)&sin6.sin6_addr)[3] = sns->sin.sin_addr;
        sns->sin6 = sin6;
      }
    }
  }

  ctx->dnsc_salen = have_inet6 ?
    sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

  if (have_inet6)
    sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  else
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else /* !HAVE_IPv6 */
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  ctx->dnsc_salen = sizeof(struct sockaddr_in);
#endif /* HAVE_IPv6 */

  if (sock < 0) {
    ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
    return -1;
  }
#ifdef WINDOWS
  { unsigned long on = 1;
    if (ioctlsocket(sock, FIONBIO, &on) == SOCKET_ERROR) {
      closesocket(sock);
      ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
      return -1;
    }
  }
#else	/* !WINDOWS */
  if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0 ||
      fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
    closesocket(sock);
    ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
    return -1;
  }
#endif	/* WINDOWS */
  /* allocate the packet buffer */
  if ((ctx->dnsc_pbuf = malloc(ctx->dnsc_udpbuf)) == NULL) {
    closesocket(sock);
    ctx->dnsc_qstatus = DNS_E_NOMEM;
    errno = ENOMEM;
    return -1;
  }

  ctx->dnsc_udpsock = sock;
  dns_request_utm(ctx, 0);
  return sock;
}

int dns_sock(const struct dns_ctx *ctx) {
  SETCTXINITED(ctx);
  return ctx->dnsc_udpsock;
}

int dns_active(const struct dns_ctx *ctx) {
  SETCTXINITED(ctx);
  dns_assert_ctx(ctx);
  return ctx->dnsc_nactive;
}

int dns_status(const struct dns_ctx *ctx) {
  SETCTX(ctx);
  return ctx->dnsc_qstatus;
}
void dns_setstatus(struct dns_ctx *ctx, int status) {
  SETCTX(ctx);
  ctx->dnsc_qstatus = status;
}

/* End the query: disconnect it from the active list, free it,
 * and return the result to the caller.
 */
static void
dns_end_query(struct dns_ctx *ctx, struct dns_query *q,
              int status, void *result) {
  dns_query_fn *cbck = q->dnsq_cbck;
  void *cbdata = q->dnsq_cbdata;
  ctx->dnsc_qstatus = status;
  assert((status < 0 && result == 0) || (status >= 0 && result != 0));
  assert(cbck != 0);	/*XXX callback may be NULL */
  assert(ctx->dnsc_nactive > 0);
  --ctx->dnsc_nactive;
  qlist_remove(q);
  /* force the query to be unconnected */
  /*memset(q, 0, sizeof(*q));*/
#ifndef NDEBUG
  q->dnsq_ctx = NULL;
#endif
  free(q);
  cbck(ctx, result, cbdata);
}

#define DNS_DBG(ctx, code, sa, slen, pkt, plen) \
  do { \
    if (ctx->dnsc_udbgfn) \
      ctx->dnsc_udbgfn(code, (sa), slen, pkt, plen, 0, 0); \
  } while(0)
#define DNS_DBGQ(ctx, q, code, sa, slen, pkt, plen) \
  do { \
    if (ctx->dnsc_udbgfn) \
      ctx->dnsc_udbgfn(code, (sa), slen, pkt, plen, q, q->dnsq_cbdata); \
  } while(0)

static void dns_newid(struct dns_ctx *ctx, struct dns_query *q) {
  /* this is how we choose an identifier for a new query (qID).
   * For now, it's just sequential number, incremented for every query, and
   * thus obviously trivial to guess.
   * There are two choices:
   *  a) use sequential numbers.  It is plain insecure. In DNS, there are two
   *   places where random numbers are (or can) be used to increase security:
   *   random qID and random source port number.  Without this randomness
   *   (udns uses fixed port for all queries), or when the randomness is weak,
   *   it's trivial to spoof query replies.  With randomness however, it
   *   becomes a bit more difficult task.  Too bad we only have 16 bits for
   *   our security, as qID is only two bytes.  It isn't a security per se,
   *   to rely on those 16 bits - an attacker can just flood us with fake
   *   replies with all possible qIDs (only 65536 of them), and in this case,
   *   even if we'll use true random qIDs, we'll be in trouble (not protected
   *   against spoofing).  Yes, this is only possible on a high-speed network
   *   (probably on the LAN only, since usually a border router for a LAN
   *   protects internal machines from packets with spoofed local addresses
   *   from outside, and usually a nameserver resides on LAN), but it's
   *   still very well possible to send us fake replies.
   *   In other words: there's nothing a DNS (stub) resolver can do against
   *   spoofing attacks, unless DNSSEC is in use, which helps here alot.
   *   Too bad that DNSSEC isn't widespread, so relying on it isn't an
   *   option in almost all cases...
   *  b) use random qID, based on some random-number generation mechanism.
   *   This way, we increase our protection a bit (see above - it's very weak
   *   still), but we also increase risk of qID reuse and matching late replies
   *   that comes to queries we've sent before against new queries.  There are
   *   some more corner cases around that, as well - for example, normally,
   *   udns tries to find the query for a given reply by qID, *and* by
   *   verifying that the query DN and other parameters are also the same
   *   (so if the new query is against another domain name, old reply will
   *   be ignored automatically).  But certain types of replies which we now
   *   handle - for example, FORMERR reply from servers which refuses to
   *   process EDNS0-enabled packets - comes without all the query parameters
   *   but the qID - so we're forced to use qID only when determining which
   *   query the given reply corresponds to.  This makes us even more
   *   vulnerable to spoofing attacks, because an attacker don't even need to
   *   know which queries we perform to spoof the replies - he only needs to
   *   flood us with fake FORMERR "replies".
   *
   * That all to say: using sequential (or any other trivially guessable)
   * numbers for qIDs is insecure, but the whole thing is inherently insecure
   * as well, and this "extra weakness" that comes from weak qID choosing
   * algorithm adds almost nothing to the underlying problem.
   *
   * It CAN NOT be made secure.  Period.  That's it.
   * Unless we choose to implement DNSSEC, which is a whole different story.
   * Forcing TCP mode makes it better, but who uses TCP for DNS anyway?
   * (and it's hardly possible because of huge impact on the recursive
   * nameservers).
   *
   * Note that ALL stub resolvers (again, unless they implement and enforce
   * DNSSEC) suffers from this same problem.
   *
   * So, instead of trying to be more secure (which actually is not - false
   * sense of security is - I think - is worse than no security), I'm trying
   * to be more robust here (by preventing qID reuse, which helps in normal
   * conditions).  And use sequential qID generation scheme.
   */
  dns_put16(q->dnsq_id, ctx->dnsc_nextid++);
  /* reset all parameters relevant for previous query lifetime */
  q->dnsq_try = 0;
  q->dnsq_servi = 0;
  /*XXX probably should keep dnsq_servnEDNS0 bits?
   * See also comments in dns_ioevent() about FORMERR case */
  q->dnsq_servwait = q->dnsq_servskip = q->dnsq_servnEDNS0 = 0;
}

/* Find next search suffix and fills in q->dnsq_dn.
 * Return 0 if no more to try. */
static int dns_next_srch(struct dns_ctx *ctx, struct dns_query *q) {
  unsigned dnl;

  for(;;) {
    if (q->dnsq_nxtsrch > ctx->dnsc_srchend)
      return 0;
    dnl = dns_dnlen(q->dnsq_nxtsrch);
    if (dnl + q->dnsq_origdnl0 <= DNS_MAXDN &&
        (*q->dnsq_nxtsrch || !(q->dnsq_flags & DNS_ASIS_DONE)))
      break;
    q->dnsq_nxtsrch += dnl;
  }
  memcpy(q->dnsq_dn + q->dnsq_origdnl0, q->dnsq_nxtsrch, dnl);
  if (!*q->dnsq_nxtsrch)
    q->dnsq_flags |= DNS_ASIS_DONE;
  q->dnsq_nxtsrch += dnl;
  dns_newid(ctx, q); /* new ID for new qDN */
  return 1;
}

/* find the server to try for current iteration.
 * Note that current dnsq_servi may point to a server we should skip --
 * in that case advance to the next server.
 * Return true if found, false if all tried.
 */
static int dns_find_serv(const struct dns_ctx *ctx, struct dns_query *q) {
  while(q->dnsq_servi < ctx->dnsc_nserv) {
    if (!(q->dnsq_servskip & (1 << q->dnsq_servi)))
      return 1;
    ++q->dnsq_servi;
  }
  return 0;
}

/* format and send the query to a given server.
 * In case of network problem (sendto() fails), return -1,
 * else return 0.
 */
static int
dns_send_this(struct dns_ctx *ctx, struct dns_query *q,
              unsigned servi, time_t now) {
  unsigned qlen;
  unsigned tries;

  { /* format the query buffer */
    dnsc_t *p = ctx->dnsc_pbuf;
    memset(p, 0, DNS_HSIZE);
    if (!(q->dnsq_flags & DNS_NORD)) p[DNS_H_F1] |= DNS_HF1_RD;
    if (q->dnsq_flags & DNS_AAONLY) p[DNS_H_F1] |= DNS_HF1_AA;
    p[DNS_H_QDCNT2] = 1;
    memcpy(p + DNS_H_QID, q->dnsq_id, 2);
    p = dns_payload(p);
    /* copy query dn */
    p += dns_dntodn(q->dnsq_dn, p, DNS_MAXDN);
    /* query type and class */
    memcpy(p, q->dnsq_typcls, 4); p += 4;
    /* add EDNS0 size record */
    if (ctx->dnsc_udpbuf > DNS_MAXPACKET &&
        !(q->dnsq_servnEDNS0 & (1 << servi))) {
      *p++ = 0;			/* empty (root) DN */
      p = dns_put16(p, DNS_T_OPT);
      p = dns_put16(p, ctx->dnsc_udpbuf);
      /* EDNS0 RCODE & VERSION; rest of the TTL field; RDLEN */
      memset(p, 0, 2+2+2); p += 2+2+2;
      ctx->dnsc_pbuf[DNS_H_ARCNT2] = 1;
    }
    qlen = p - ctx->dnsc_pbuf;
    assert(qlen <= ctx->dnsc_udpbuf);
  }

  /* send the query */
  tries = 10;
  while (sendto(ctx->dnsc_udpsock, (void*)ctx->dnsc_pbuf, qlen, 0,
                &ctx->dnsc_serv[servi].sa, ctx->dnsc_salen) < 0) {
    /*XXX just ignore the sendto() error for now and try again.
     * In the future, it may be possible to retrieve the error code
     * and find which operation/query failed.
     *XXX try the next server too? (if ENETUNREACH is returned immediately)
     */
    if (--tries) continue;
    /* if we can't send the query, fail it. */
    dns_end_query(ctx, q, DNS_E_TEMPFAIL, 0);
    return -1;
  }
  DNS_DBGQ(ctx, q, 1,
           &ctx->dnsc_serv[servi].sa, sizeof(union sockaddr_ns),
           ctx->dnsc_pbuf, qlen);
  q->dnsq_servwait |= 1 << servi;	/* expect reply from this ns */

  q->dnsq_deadline = now +
    (dns_find_serv(ctx, q) ? 1 : ctx->dnsc_timeout << q->dnsq_try);

  /* move the query to the proper place, according to the new deadline */
  qlist_remove(q);
  { /* insert from the tail */
    struct dns_query *p;
    QLIST_FOR_EACH(&ctx->dnsc_qactive, p, prev)
      if (p->dnsq_deadline <= q->dnsq_deadline)
	break;
    qlist_insert_after(q, p);
  }

  return 0;
}

/* send the query out using next available server
 * and add it to the active list, or, if no servers available,
 * end it.
 */
static void
dns_send(struct dns_ctx *ctx, struct dns_query *q, time_t now) {

  /* if we can't send the query, return TEMPFAIL even when searching:
   * we can't be sure whenever the name we tried to search exists or not,
   * so don't continue searching, or we may find the wrong name. */

  if (!dns_find_serv(ctx, q)) {
    /* no more servers in this iteration.  Try the next cycle */
    q->dnsq_servi = 0;	/* reset */
    q->dnsq_try++;	/* next try */
    if (q->dnsq_try >= ctx->dnsc_ntries ||
        !dns_find_serv(ctx, q)) {
      /* no more servers and tries, fail the query */
      /* return TEMPFAIL even when searching: no more tries for this
       * searchlist, and no single definitive reply (handled in dns_ioevent()
       * in NOERROR or NXDOMAIN cases) => all nameservers failed to process
       * current search list element, so we don't know whenever the name exists.
       */
      dns_end_query(ctx, q, DNS_E_TEMPFAIL, 0);
      return;
    }
  }

  dns_send_this(ctx, q, q->dnsq_servi++, now);
}

static void dns_dummy_cb(struct dns_ctx *ctx, void *result, void *data) {
  if (result) free(result);
  data = ctx = 0;	/* used */
}

/* The (only, main, real) query submission routine.
 * Allocate new query structure, initialize it, check validity of
 * parameters, and add it to the head of the active list, without
 * trying to send it (to be picked up on next event).
 * Error return (without calling the callback routine) -
 *  no memory or wrong parameters.
 *XXX The `no memory' case probably should go to the callback anyway...
 */
struct dns_query *
dns_submit_dn(struct dns_ctx *ctx,
              dnscc_t *dn, int qcls, int qtyp, int flags,
              dns_parse_fn *parse, dns_query_fn *cbck, void *data) {
  struct dns_query *q;
  SETCTXOPEN(ctx);
  dns_assert_ctx(ctx);

  q = calloc(sizeof(*q), 1);
  if (!q) {
    ctx->dnsc_qstatus = DNS_E_NOMEM;
    return NULL;
  }

#ifndef NDEBUG
  q->dnsq_ctx = ctx;
#endif
  q->dnsq_parse = parse;
  q->dnsq_cbck = cbck ? cbck : dns_dummy_cb;
  q->dnsq_cbdata = data;

  q->dnsq_origdnl0 = dns_dntodn(dn, q->dnsq_dn, sizeof(q->dnsq_dn));
  assert(q->dnsq_origdnl0 > 0);
  --q->dnsq_origdnl0;		/* w/o the trailing 0 */
  dns_put16(q->dnsq_typcls+0, qtyp);
  dns_put16(q->dnsq_typcls+2, qcls);
  q->dnsq_flags = (flags | ctx->dnsc_flags) & ~DNS_INTERNAL;

  if (flags & DNS_NOSRCH ||
      dns_dnlabels(q->dnsq_dn) > ctx->dnsc_ndots) {
    q->dnsq_nxtsrch = flags & DNS_NOSRCH ?
      ctx->dnsc_srchend /* end of the search list if no search requested */ :
      ctx->dnsc_srchbuf /* beginning of the list, but try as-is first */;
    q->dnsq_flags |= DNS_ASIS_DONE;
    dns_newid(ctx, q);
  }
  else {
    q->dnsq_nxtsrch = ctx->dnsc_srchbuf;
    dns_next_srch(ctx, q);
  }

  qlist_add_head(q, &ctx->dnsc_qactive);
  ++ctx->dnsc_nactive;
  dns_request_utm(ctx, 0);

  return q;
}

struct dns_query *
dns_submit_p(struct dns_ctx *ctx,
             const char *name, int qcls, int qtyp, int flags,
             dns_parse_fn *parse, dns_query_fn *cbck, void *data) {
  int isabs;
  SETCTXOPEN(ctx);
  if (dns_ptodn(name, 0, ctx->dnsc_pbuf, DNS_MAXDN, &isabs) <= 0) {
    ctx->dnsc_qstatus = DNS_E_BADQUERY;
    return NULL;
  }
  if (isabs)
    flags |= DNS_NOSRCH;
  return
    dns_submit_dn(ctx, ctx->dnsc_pbuf, qcls, qtyp, flags, parse, cbck, data);
}

/* process readable fd condition.
 * To be usable in edge-triggered environment, the routine
 * should consume all input so it should loop over.
 * Note it isn't really necessary to loop here, because
 * an application may perform the loop just fine by it's own,
 * but in this case we should return some sensitive result,
 * to indicate when to stop calling and error conditions.
 * Note also we may encounter all sorts of recvfrom()
 * errors which aren't fatal, and at the same time we may
 * loop forever if an error IS fatal.
 */
void dns_ioevent(struct dns_ctx *ctx, time_t now) {
  int r;
  unsigned servi;
  struct dns_query *q;
  dnsc_t *pbuf;
  dnscc_t *pend, *pcur;
  void *result;
  union sockaddr_ns sns;
  socklen_t slen;

  SETCTX(ctx);
  if (!CTXOPEN(ctx))
    return;
  dns_assert_ctx(ctx);
  pbuf = ctx->dnsc_pbuf;

  if (!now) now = time(NULL);

again: /* receive the reply */

  slen = sizeof(sns);
  r = recvfrom(ctx->dnsc_udpsock, (void*)pbuf, ctx->dnsc_udpbuf,
               MSG_DONTWAIT, &sns.sa, &slen);
  if (r < 0) {
    /*XXX just ignore recvfrom() errors for now.
     * in the future it may be possible to determine which
     * query failed and requeue it.
     * Note there may be various error conditions, triggered
     * by both local problems and remote problems.  It isn't
     * quite trivial to determine whenever an error is local
     * or remote.  On local errors, we should stop, while
     * remote errors should be ignored (for now anyway).
     */
#ifdef WINDOWS
    if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
    if (errno == EAGAIN)
#endif
    {
      dns_request_utm(ctx, now);
      return;
    }
    goto again;
  }

  pend = pbuf + r;
  pcur = dns_payload(pbuf);

  /* check reply header */
  if (pcur > pend || dns_numqd(pbuf) > 1 || dns_opcode(pbuf) != 0) {
    DNS_DBG(ctx, -1/*bad reply*/, &sns.sa, slen, pbuf, r);
    goto again;
  }

  /* find the matching query, by qID */
  for (q = QLIST_FIRST(&ctx->dnsc_qactive, next);; q = QLIST_NEXT(q, next)) {
    if (QLIST_ISLAST(&ctx->dnsc_qactive, q)) {
      /* no more requests: old reply? */
      DNS_DBG(ctx, -5/*no matching query*/, &sns.sa, slen, pbuf, r);
      goto again;
    }
    if (pbuf[DNS_H_QID1] == q->dnsq_id[0] &&
        pbuf[DNS_H_QID2] == q->dnsq_id[1])
      break;
  }

  /* if we have numqd, compare with our query qDN */
  if (dns_numqd(pbuf)) {
    /* decode the qDN */
    dnsc_t dn[DNS_MAXDN];
    if (dns_getdn(pbuf, &pcur, pend, dn, sizeof(dn)) < 0 ||
        pcur + 4 > pend) {
      DNS_DBG(ctx, -1/*bad reply*/, &sns.sa, slen, pbuf, r);
      goto again;
    }
    if (!dns_dnequal(dn, q->dnsq_dn) ||
        memcmp(pcur, q->dnsq_typcls, 4) != 0) {
      /* not this query */
      DNS_DBG(ctx, -5/*no matching query*/, &sns.sa, slen, pbuf, r);
      goto again;
    }
    /* here, query match, and pcur points past qDN in query section in pbuf */
  }
  /* if no numqd, we only allow FORMERR rcode */
  else if (dns_rcode(pbuf) != DNS_R_FORMERR) {
    /* treat it as bad reply if !FORMERR */
    DNS_DBG(ctx, -1/*bad reply*/, &sns.sa, slen, pbuf, r);
    goto again;
  }
  else {
    /* else it's FORMERR, handled below */
  }

  /* find server */
#ifdef HAVE_IPv6
  if (sns.sa.sa_family == AF_INET6 && slen >= sizeof(sns.sin6)) {
    for(servi = 0; servi < ctx->dnsc_nserv; ++servi)
      if (sin6_eq(ctx->dnsc_serv[servi].sin6, sns.sin6))
        break;
  }
  else
#endif
  if (sns.sa.sa_family == AF_INET && slen >= sizeof(sns.sin)) {
    for(servi = 0; servi < ctx->dnsc_nserv; ++servi)
      if (sin_eq(ctx->dnsc_serv[servi].sin, sns.sin))
        break;
  }
  else
    servi = ctx->dnsc_nserv;

  /* check if we expect reply from this server.
   * Note we can receive reply from first try if we're already at next */
  if (!(q->dnsq_servwait & (1 << servi))) { /* if ever asked this NS */
    DNS_DBG(ctx, -2/*wrong server*/, &sns.sa, slen, pbuf, r);
    goto again;
  }

  /* we got (some) reply for our query */

  DNS_DBGQ(ctx, q, 0, &sns.sa, slen, pbuf, r);
  q->dnsq_servwait &= ~(1 << servi);	/* don't expect reply from this serv */

  /* process the RCODE */
  switch(dns_rcode(pbuf)) {

  case DNS_R_NOERROR:
    if (dns_tc(pbuf)) {
      /* possible truncation.  We can't deal with it. */
      /*XXX for now, treat TC bit the same as SERVFAIL.
       * It is possible to:
       *  a) try to decode the reply - may be ANSWER section is ok;
       *  b) check if server understands EDNS0, and if it is, and
       *   answer still don't fit, end query.
       */
      break;
    }
    if (!dns_numan(pbuf)) {	/* no data of requested type */
      if (dns_next_srch(ctx, q)) {
        /* if we're searching, try next searchlist element,
         * but remember NODATA reply. */
        q->dnsq_flags |= DNS_SEEN_NODATA;
        dns_send(ctx, q, now);
      }
      else
        /* else - nothing to search any more - finish the query.
         * It will be NODATA since we've seen a NODATA reply. */
        dns_end_query(ctx, q, DNS_E_NODATA, 0);
    }
    /* we've got a positive reply here */
    else if (q->dnsq_parse) {
      /* if we have parsing routine, call it and return whatever it returned */
      /* don't try to re-search if NODATA here.  For example,
       * if we asked for A but only received CNAME.  Unless we'll
       * someday do recursive queries.  And that's problematic too, since
       * we may be dealing with specific AA-only nameservers for a given
       * domain, but CNAME points elsewhere...
       */
      r = q->dnsq_parse(q->dnsq_dn, pbuf, pcur, pend, &result);
      dns_end_query(ctx, q, r, r < 0 ? NULL : result);
    }
    /* else just malloc+copy the raw DNS reply */
    else if ((result = malloc(r)) == NULL)
      dns_end_query(ctx, q, DNS_E_NOMEM, NULL);
    else {
      memcpy(result, pbuf, r);
      dns_end_query(ctx, q, r, result);
    }
    goto again;

  case DNS_R_NXDOMAIN:	/* Non-existing domain. */
    if (dns_next_srch(ctx, q))
      /* more search entries exists, try them. */
      dns_send(ctx, q, now);
    else
      /* nothing to search anymore. End the query, returning either NODATA
       * if we've seen it before, or NXDOMAIN if not. */
      dns_end_query(ctx, q,
           q->dnsq_flags & DNS_SEEN_NODATA ? DNS_E_NODATA : DNS_E_NXDOMAIN, 0);
    goto again;

  case DNS_R_FORMERR:
  case DNS_R_NOTIMPL:
    /* for FORMERR and NOTIMPL rcodes, if we tried EDNS0-enabled query,
     * try w/o EDNS0. */
    if (ctx->dnsc_udpbuf > DNS_MAXPACKET &&
        !(q->dnsq_servnEDNS0 & (1 << servi))) {
      /* we always trying EDNS0 first if enabled, and retry a given query
       * if not available. Maybe it's better to remember inavailability of
       * EDNS0 in ctx as a per-NS flag, and never try again for this NS.
       * For long-running applications.. maybe they will change the nameserver
       * while we're running? :)  Also, since FORMERR is the only rcode we
       * allow to be header-only, and in this case the only check we do to
       * find a query it belongs to is qID (not qDN+qCLS+qTYP), it's much
       * easier to spoof and to force us to perform non-EDNS0 queries only...
       */
      q->dnsq_servnEDNS0 |= 1 << servi;
      dns_send_this(ctx, q, servi, now);
      goto again;
    }
    /* else we handle it the same as SERVFAIL etc */

  case DNS_R_SERVFAIL:
  case DNS_R_REFUSED:
    /* for these rcodes, advance this request
     * to the next server and reschedule */
  default: /* unknown rcode? hmmm... */
    break;
  }

  /* here, we received unexpected reply */
  q->dnsq_servskip |= (1 << servi);	/* don't retry this server */

  /* we don't expect replies from this server anymore.
   * But there may be other servers.  Some may be still processing our
   * query, and some may be left to try.
   * We just ignore this reply and wait a bit more if some NSes haven't
   * replied yet (dnsq_servwait != 0), and let the situation to be handled
   * on next event processing.  Timeout for this query is set correctly,
   * if not taking into account the one-second difference - we can try
   * next server in the same iteration sooner.
   */

  /* try next server */
  if (!q->dnsq_servwait) {
    /* next retry: maybe some other servers will reply next time.
     * dns_send() will end the query for us if no more servers to try.
     * Note we can't continue with the next searchlist element here:
     * we don't know if the current qdn exists or not, there's no definitive
     * answer yet (which is seen in cases above).
     *XXX standard resolver also tries as-is query in case all nameservers
     * failed to process our query and if not tried before.  We don't do it.
     */
    dns_send(ctx, q, now);
  }
  else {
    /* else don't do anything - not all servers replied yet */
  }
  goto again;

}

/* handle all timeouts */
int dns_timeouts(struct dns_ctx *ctx, int maxwait, time_t now) {
  /* this is a hot routine */
  struct dns_query *q;

  SETCTX(ctx);
  dns_assert_ctx(ctx);

  /* Pick up first entry from query list.
   * If its deadline has passed, (re)send it
   * (dns_send() will move it next in the list).
   * If not, this is the query which determines the closest deadline.
   */

  q = qlist_first(&ctx->dnsc_qactive);
  if (!q)
    return maxwait;
  if (!now)
    now = time(NULL);
  do {
    if (q->dnsq_deadline > now) { /* first non-expired query */
      int w = (int)(q->dnsq_deadline - now);
      if (maxwait < 0 || maxwait > w)
        maxwait = w;
      break;
    }
    else {
      /* process expired deadline */
      dns_send(ctx, q, now);
    }
  } while((q = qlist_first(&ctx->dnsc_qactive)) != NULL);

  dns_request_utm(ctx, now); /* update timer with new deadline */
  return maxwait;
}

struct dns_resolve_data {
  int   dnsrd_done;
  void *dnsrd_result;
};

static void dns_resolve_cb(struct dns_ctx *ctx, void *result, void *data) {
  struct dns_resolve_data *d = data;
  d->dnsrd_result = result;
  d->dnsrd_done = 1;
  ctx = ctx;
}

void *dns_resolve(struct dns_ctx *ctx, struct dns_query *q) {
  time_t now;
  struct dns_resolve_data d;
  int n;
  SETCTXOPEN(ctx);

  if (!q)
    return NULL;

  assert(ctx == q->dnsq_ctx);
  dns_assert_ctx(ctx);
  /* do not allow re-resolving syncronous queries */
  assert(q->dnsq_cbck != dns_resolve_cb && "can't resolve syncronous query");
  if (q->dnsq_cbck == dns_resolve_cb) {
    ctx->dnsc_qstatus = DNS_E_BADQUERY;
    return NULL;
  }
  q->dnsq_cbck = dns_resolve_cb;
  q->dnsq_cbdata = &d;
  d.dnsrd_done = 0;

  now = time(NULL);
  while(!d.dnsrd_done && (n = dns_timeouts(ctx, -1, now)) >= 0) {
#ifdef HAVE_POLL
    struct pollfd pfd;
    pfd.fd = ctx->dnsc_udpsock;
    pfd.events = POLLIN;
    n = poll(&pfd, 1, n * 1000);
#else
    fd_set rfd;
    struct timeval tv;
    FD_ZERO(&rfd);
    FD_SET(ctx->dnsc_udpsock, &rfd);
    tv.tv_sec = n; tv.tv_usec = 0;
    n = select(ctx->dnsc_udpsock + 1, &rfd, NULL, NULL, &tv);
#endif
    now = time(NULL);
    if (n > 0)
      dns_ioevent(ctx, now);
  }

  return d.dnsrd_result;
}

void *dns_resolve_dn(struct dns_ctx *ctx,
                     dnscc_t *dn, int qcls, int qtyp, int flags,
                     dns_parse_fn *parse) {
  return
    dns_resolve(ctx,
      dns_submit_dn(ctx, dn, qcls, qtyp, flags, parse, NULL, NULL));
}

void *dns_resolve_p(struct dns_ctx *ctx,
                    const char *name, int qcls, int qtyp, int flags,
                    dns_parse_fn *parse) {
  return
    dns_resolve(ctx,
      dns_submit_p(ctx, name, qcls, qtyp, flags, parse, NULL, NULL));
}

int dns_cancel(struct dns_ctx *ctx, struct dns_query *q) {
  SETCTX(ctx);
  dns_assert_ctx(ctx);
  assert(q->dnsq_ctx == ctx);
  /* do not allow cancelling syncronous queries */
  assert(q->dnsq_cbck != dns_resolve_cb && "can't cancel syncronous query");
  if (q->dnsq_cbck == dns_resolve_cb)
    return (ctx->dnsc_qstatus = DNS_E_BADQUERY);
  qlist_remove(q);
  --ctx->dnsc_nactive;
  dns_request_utm(ctx, 0);
  return 0;
}


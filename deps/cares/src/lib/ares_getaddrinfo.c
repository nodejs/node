/* MIT License
 *
 * Copyright (c) 1998, 2011, 2013 Massachusetts Institute of Technology
 * Copyright (c) 2017 Christian Ammer
 * Copyright (c) 2019 Andrew Selivanov
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

#include "ares_setup.h"

#ifdef HAVE_GETSERVBYNAME_R
#  if !defined(GETSERVBYNAME_R_ARGS) || (GETSERVBYNAME_R_ARGS < 4) || \
    (GETSERVBYNAME_R_ARGS > 6)
#    error "you MUST specifiy a valid number of arguments for getservbyname_r"
#  endif
#endif

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares_nameser.h"

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <assert.h>

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "ares.h"
#include "bitncmp.h"
#include "ares_private.h"
#include "ares_dns.h"

#ifdef WATT32
#  undef WIN32
#endif
#ifdef WIN32
#  include "ares_platform.h"
#endif

struct host_query {
  ares_channel               channel;
  char                      *name;
  unsigned short             port; /* in host order */
  ares_addrinfo_callback     callback;
  void                      *arg;
  struct ares_addrinfo_hints hints;
  int         sent_family; /* this family is what was is being used */
  size_t      timeouts;    /* number of timeouts we saw for this request */
  const char *remaining_lookups;  /* types of lookup we need to perform ("fb" by
                                     default, file and dns respectively) */
  struct ares_addrinfo *ai;       /* store results between lookups */
  unsigned short        qid_a;    /* qid for A request */
  unsigned short        qid_aaaa; /* qid for AAAA request */
  size_t                remaining;   /* number of DNS answers waiting for */
  ares_ssize_t          next_domain; /* next search domain to try */
  size_t
    nodata_cnt; /* Track nodata responses to possibly override final result */
};

static const struct ares_addrinfo_hints default_hints = {
  0,         /* ai_flags */
  AF_UNSPEC, /* ai_family */
  0,         /* ai_socktype */
  0,         /* ai_protocol */
};

static const struct ares_addrinfo_cname empty_addrinfo_cname = {
  INT_MAX, /* ttl */
  NULL,    /* alias */
  NULL,    /* name */
  NULL,    /* next */
};

static const struct ares_addrinfo_node empty_addrinfo_node = {
  0,    /* ai_ttl */
  0,    /* ai_flags */
  0,    /* ai_family */
  0,    /* ai_socktype */
  0,    /* ai_protocol */
  0,    /* ai_addrlen */
  NULL, /* ai_addr */
  NULL  /* ai_next */
};

static const struct ares_addrinfo empty_addrinfo = {
  NULL, /* cnames */
  NULL, /* nodes */
  NULL  /* name */
};

/* forward declarations */
static void        host_callback(void *arg, int status, int timeouts,
                                 unsigned char *abuf, int alen);
static ares_bool_t as_is_first(const struct host_query *hquery);
static ares_bool_t as_is_only(const struct host_query *hquery);
static ares_bool_t next_dns_lookup(struct host_query *hquery);

static struct ares_addrinfo_cname *ares__malloc_addrinfo_cname(void)
{
  struct ares_addrinfo_cname *cname =
    ares_malloc(sizeof(struct ares_addrinfo_cname));
  if (!cname) {
    return NULL;
  }

  *cname = empty_addrinfo_cname;
  return cname;
}

struct ares_addrinfo_cname *
  ares__append_addrinfo_cname(struct ares_addrinfo_cname **head)
{
  struct ares_addrinfo_cname *tail = ares__malloc_addrinfo_cname();
  struct ares_addrinfo_cname *last = *head;
  if (!last) {
    *head = tail;
    return tail;
  }

  while (last->next) {
    last = last->next;
  }

  last->next = tail;
  return tail;
}

void ares__addrinfo_cat_cnames(struct ares_addrinfo_cname **head,
                               struct ares_addrinfo_cname  *tail)
{
  struct ares_addrinfo_cname *last = *head;
  if (!last) {
    *head = tail;
    return;
  }

  while (last->next) {
    last = last->next;
  }

  last->next = tail;
}

static struct ares_addrinfo *ares__malloc_addrinfo(void)
{
  struct ares_addrinfo *ai = ares_malloc(sizeof(struct ares_addrinfo));
  if (!ai) {
    return NULL;
  }

  *ai = empty_addrinfo;
  return ai;
}

static struct ares_addrinfo_node *ares__malloc_addrinfo_node(void)
{
  struct ares_addrinfo_node *node = ares_malloc(sizeof(*node));
  if (!node) {
    return NULL;
  }

  *node = empty_addrinfo_node;
  return node;
}

/* Allocate new addrinfo and append to the tail. */
struct ares_addrinfo_node *
  ares__append_addrinfo_node(struct ares_addrinfo_node **head)
{
  struct ares_addrinfo_node *tail = ares__malloc_addrinfo_node();
  struct ares_addrinfo_node *last = *head;
  if (!last) {
    *head = tail;
    return tail;
  }

  while (last->ai_next) {
    last = last->ai_next;
  }

  last->ai_next = tail;
  return tail;
}

void ares__addrinfo_cat_nodes(struct ares_addrinfo_node **head,
                              struct ares_addrinfo_node  *tail)
{
  struct ares_addrinfo_node *last = *head;
  if (!last) {
    *head = tail;
    return;
  }

  while (last->ai_next) {
    last = last->ai_next;
  }

  last->ai_next = tail;
}

/* Resolve service name into port number given in host byte order.
 * If not resolved, return 0.
 */
static unsigned short lookup_service(const char *service, int flags)
{
  const char     *proto;
  struct servent *sep;
#ifdef HAVE_GETSERVBYNAME_R
  struct servent se;
  char           tmpbuf[4096];
#endif

  if (service) {
    if (flags & ARES_NI_UDP) {
      proto = "udp";
    } else if (flags & ARES_NI_SCTP) {
      proto = "sctp";
    } else if (flags & ARES_NI_DCCP) {
      proto = "dccp";
    } else {
      proto = "tcp";
    }
#ifdef HAVE_GETSERVBYNAME_R
    memset(&se, 0, sizeof(se));
    sep = &se;
    memset(tmpbuf, 0, sizeof(tmpbuf));
#  if GETSERVBYNAME_R_ARGS == 6
    if (getservbyname_r(service, proto, &se, (void *)tmpbuf, sizeof(tmpbuf),
                        &sep) != 0) {
      sep = NULL; /* LCOV_EXCL_LINE: buffer large so this never fails */
    }
#  elif GETSERVBYNAME_R_ARGS == 5
    sep = getservbyname_r(service, proto, &se, (void *)tmpbuf, sizeof(tmpbuf));
#  elif GETSERVBYNAME_R_ARGS == 4
    if (getservbyname_r(service, proto, &se, (void *)tmpbuf) != 0) {
      sep = NULL;
    }
#  else
    /* Lets just hope the OS uses TLS! */
    sep = getservbyname(service, proto);
#  endif
#else
    /* Lets just hope the OS uses TLS! */
#  if (defined(NETWARE) && !defined(__NOVELL_LIBC__))
    sep = getservbyname(service, (char *)proto);
#  else
    sep = getservbyname(service, proto);
#  endif
#endif
    return (sep ? ntohs((unsigned short)sep->s_port) : 0);
  }
  return 0;
}

/* If the name looks like an IP address or an error occured,
 * fake up a host entry, end the query immediately, and return true.
 * Otherwise return false.
 */
static ares_bool_t fake_addrinfo(const char *name, unsigned short port,
                                 const struct ares_addrinfo_hints *hints,
                                 struct ares_addrinfo             *ai,
                                 ares_addrinfo_callback callback, void *arg)
{
  struct ares_addrinfo_cname *cname;
  ares_status_t               status = ARES_SUCCESS;
  ares_bool_t                 result = ARES_FALSE;
  int                         family = hints->ai_family;
  if (family == AF_INET || family == AF_INET6 || family == AF_UNSPEC) {
    /* It only looks like an IP address if it's all numbers and dots. */
    size_t      numdots = 0;
    ares_bool_t valid   = ARES_TRUE;
    const char *p;
    for (p = name; *p; p++) {
      if (!ISDIGIT(*p) && *p != '.') {
        valid = ARES_FALSE;
        break;
      } else if (*p == '.') {
        numdots++;
      }
    }

    /* if we don't have 3 dots, it is illegal
     * (although inet_pton doesn't think so).
     */
    if (numdots != 3 || !valid) {
      result = ARES_FALSE;
    } else {
      struct in_addr addr4;
      result =
        ares_inet_pton(AF_INET, name, &addr4) < 1 ? ARES_FALSE : ARES_TRUE;
      if (result) {
        status = ares_append_ai_node(AF_INET, port, 0, &addr4, &ai->nodes);
        if (status != ARES_SUCCESS) {
          callback(arg, (int)status, 0, NULL);
          return ARES_TRUE;
        }
      }
    }
  }

  if (!result && (family == AF_INET6 || family == AF_UNSPEC)) {
    struct ares_in6_addr addr6;
    result =
      ares_inet_pton(AF_INET6, name, &addr6) < 1 ? ARES_FALSE : ARES_TRUE;
    if (result) {
      status = ares_append_ai_node(AF_INET6, port, 0, &addr6, &ai->nodes);
      if (status != ARES_SUCCESS) {
        callback(arg, (int)status, 0, NULL);
        return ARES_TRUE;
      }
    }
  }

  if (!result) {
    return ARES_FALSE;
  }

  if (hints->ai_flags & ARES_AI_CANONNAME) {
    cname = ares__append_addrinfo_cname(&ai->cnames);
    if (!cname) {
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return ARES_TRUE;
    }

    /* Duplicate the name, to avoid a constness violation. */
    cname->name = ares_strdup(name);
    if (!cname->name) {
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return ARES_TRUE;
    }
  }

  ai->nodes->ai_socktype = hints->ai_socktype;
  ai->nodes->ai_protocol = hints->ai_protocol;

  callback(arg, ARES_SUCCESS, 0, ai);
  return ARES_TRUE;
}

static void end_hquery(struct host_query *hquery, ares_status_t status)
{
  struct ares_addrinfo_node  sentinel;
  struct ares_addrinfo_node *next;

  if (status == ARES_SUCCESS) {
    if (!(hquery->hints.ai_flags & ARES_AI_NOSORT) && hquery->ai->nodes) {
      sentinel.ai_next = hquery->ai->nodes;
      ares__sortaddrinfo(hquery->channel, &sentinel);
      hquery->ai->nodes = sentinel.ai_next;
    }
    next = hquery->ai->nodes;

    while (next) {
      next->ai_socktype = hquery->hints.ai_socktype;
      next->ai_protocol = hquery->hints.ai_protocol;
      next              = next->ai_next;
    }
  } else {
    /* Clean up what we have collected by so far. */
    ares_freeaddrinfo(hquery->ai);
    hquery->ai = NULL;
  }

  hquery->callback(hquery->arg, (int)status, (int)hquery->timeouts, hquery->ai);
  ares_free(hquery->name);
  ares_free(hquery);
}

static ares_bool_t is_localhost(const char *name)
{
  /* RFC6761 6.3 says : The domain "localhost." and any names falling within
   * ".localhost." */
  size_t len;

  if (name == NULL) {
    return ARES_FALSE;
  }

  if (strcmp(name, "localhost") == 0) {
    return ARES_TRUE;
  }

  len = ares_strlen(name);
  if (len < 10 /* strlen(".localhost") */) {
    return ARES_FALSE;
  }

  if (strcmp(name + (len - 10 /* strlen(".localhost") */), ".localhost") == 0) {
    return ARES_TRUE;
  }

  return ARES_FALSE;
}

static ares_status_t file_lookup(struct host_query *hquery)
{
  FILE         *fp;
  int           error;
  ares_status_t status;
  char         *path_hosts = NULL;

  if (hquery->channel->hosts_path) {
    path_hosts = ares_strdup(hquery->channel->hosts_path);
    if (!path_hosts) {
      return ARES_ENOMEM;
    }
  }

  if (hquery->hints.ai_flags & ARES_AI_ENVHOSTS) {
    if (path_hosts) {
      ares_free(path_hosts);
    }

    path_hosts = ares_strdup(getenv("CARES_HOSTS"));
    if (!path_hosts) {
      return ARES_ENOMEM;
    }
  }

  if (!path_hosts) {
#ifdef WIN32
    char         PATH_HOSTS[MAX_PATH];
    win_platform platform;

    PATH_HOSTS[0] = '\0';

    platform = ares__getplatform();

    if (platform == WIN_NT) {
      char tmp[MAX_PATH];
      HKEY hkeyHosts;

      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0, KEY_READ,
                        &hkeyHosts) == ERROR_SUCCESS) {
        DWORD dwLength = MAX_PATH;
        RegQueryValueExA(hkeyHosts, DATABASEPATH, NULL, NULL, (LPBYTE)tmp,
                         &dwLength);
        ExpandEnvironmentStringsA(tmp, PATH_HOSTS, MAX_PATH);
        RegCloseKey(hkeyHosts);
      }
    } else if (platform == WIN_9X) {
      GetWindowsDirectoryA(PATH_HOSTS, MAX_PATH);
    } else {
      return ARES_ENOTFOUND;
    }

    strcat(PATH_HOSTS, WIN_PATH_HOSTS);
#elif defined(WATT32)
    const char *PATH_HOSTS = _w32_GetHostsFile();

    if (!PATH_HOSTS) {
      return ARES_ENOTFOUND;
    }
#endif
    path_hosts = ares_strdup(PATH_HOSTS);
    if (!path_hosts) {
      return ARES_ENOMEM;
    }
  }

  fp = fopen(path_hosts, "r");
  if (!fp) {
    error = ERRNO;
    switch (error) {
      case ENOENT:
      case ESRCH:
        status = ARES_ENOTFOUND;
        break;
      default:
        DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n", error,
                       strerror(error)));
        DEBUGF(fprintf(stderr, "Error opening file: %s\n", path_hosts));
        status = ARES_EFILE;
        break;
    }
  } else {
    status = ares__readaddrinfo(fp, hquery->name, hquery->port, &hquery->hints,
                                hquery->ai);
    fclose(fp);
  }
  ares_free(path_hosts);

  /* RFC6761 section 6.3 #3 states that "Name resolution APIs and libraries
   * SHOULD recognize localhost names as special and SHOULD always return the
   * IP loopback address for address queries".
   * We will also ignore ALL errors when trying to resolve localhost, such
   * as permissions errors reading /etc/hosts or a malformed /etc/hosts */
  if (status != ARES_SUCCESS && is_localhost(hquery->name)) {
    return ares__addrinfo_localhost(hquery->name, hquery->port, &hquery->hints,
                                    hquery->ai);
  }

  return status;
}

static void next_lookup(struct host_query *hquery, ares_status_t status)
{
  switch (*hquery->remaining_lookups) {
    case 'b':
      /* RFC6761 section 6.3 #3 says "Name resolution APIs SHOULD NOT send
       * queries for localhost names to their configured caching DNS
       * server(s)."
       * Otherwise, DNS lookup. */
      if (!is_localhost(hquery->name) && next_dns_lookup(hquery)) {
        break;
      }

      hquery->remaining_lookups++;
      next_lookup(hquery, status);
      break;

    case 'f':
      /* Host file lookup */
      if (file_lookup(hquery) == ARES_SUCCESS) {
        end_hquery(hquery, ARES_SUCCESS);
        break;
      }
      hquery->remaining_lookups++;
      next_lookup(hquery, status);
      break;
    default:
      /* No lookup left */
      end_hquery(hquery, status);
      break;
  }
}

static void terminate_retries(struct host_query *hquery, unsigned short qid)
{
  unsigned short term_qid =
    (qid == hquery->qid_a) ? hquery->qid_aaaa : hquery->qid_a;
  ares_channel  channel = hquery->channel;
  struct query *query   = NULL;

  /* No other outstanding queries, nothing to do */
  if (!hquery->remaining) {
    return;
  }

  query = ares__htable_stvp_get_direct(channel->queries_by_qid, term_qid);
  if (query == NULL) {
    return;
  }

  query->no_retries = ARES_TRUE;
}

static void host_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen)
{
  struct host_query *hquery         = (struct host_query *)arg;
  ares_status_t      addinfostatus  = ARES_SUCCESS;
  unsigned short     qid            = 0;
  hquery->timeouts                 += (size_t)timeouts;
  hquery->remaining--;

  if (status == ARES_SUCCESS) {
    if (alen < 0) {
      addinfostatus = ARES_EBADRESP;
    } else {
      addinfostatus = ares__parse_into_addrinfo(abuf, (size_t)alen, ARES_TRUE,
                                                hquery->port, hquery->ai);
    }
    if (addinfostatus == ARES_SUCCESS && alen >= HFIXEDSZ) {
      qid = DNS_HEADER_QID(abuf); /* Converts to host byte order */
      terminate_retries(hquery, qid);
    }
  }

  if (!hquery->remaining) {
    if (addinfostatus != ARES_SUCCESS && addinfostatus != ARES_ENODATA) {
      /* error in parsing result e.g. no memory */
      if (addinfostatus == ARES_EBADRESP && hquery->ai->nodes) {
        /* We got a bad response from server, but at least one query
         * ended with ARES_SUCCESS */
        end_hquery(hquery, ARES_SUCCESS);
      } else {
        end_hquery(hquery, addinfostatus);
      }
    } else if (hquery->ai->nodes) {
      /* at least one query ended with ARES_SUCCESS */
      end_hquery(hquery, ARES_SUCCESS);
    } else if (status == ARES_EDESTRUCTION || status == ARES_ECANCELLED) {
      /* must make sure we don't do next_lookup() on destroy or cancel */
      end_hquery(hquery, (ares_status_t)status);
    } else if (status == ARES_ENOTFOUND || status == ARES_ENODATA ||
               addinfostatus == ARES_ENODATA) {
      if (status == ARES_ENODATA || addinfostatus == ARES_ENODATA) {
        hquery->nodata_cnt++;
      }
      next_lookup(hquery,
                  hquery->nodata_cnt ? ARES_ENODATA : (ares_status_t)status);
    } else {
      end_hquery(hquery, (ares_status_t)status);
    }
  }

  /* at this point we keep on waiting for the next query to finish */
}

void ares_getaddrinfo(ares_channel channel, const char *name,
                      const char                       *service,
                      const struct ares_addrinfo_hints *hints,
                      ares_addrinfo_callback callback, void *arg)
{
  struct host_query    *hquery;
  unsigned short        port = 0;
  int                   family;
  struct ares_addrinfo *ai;
  char                 *alias_name = NULL;
  ares_status_t         status;

  if (!hints) {
    hints = &default_hints;
  }

  family = hints->ai_family;

  /* Right now we only know how to look up Internet addresses
     and unspec means try both basically. */
  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC) {
    callback(arg, ARES_ENOTIMP, 0, NULL);
    return;
  }

  if (ares__is_onion_domain(name)) {
    callback(arg, ARES_ENOTFOUND, 0, NULL);
    return;
  }

  /* perform HOSTALIAS resolution (technically this function does some other
   * things we are going to ignore) */
  status = ares__single_domain(channel, name, &alias_name);
  if (status != ARES_SUCCESS) {
    callback(arg, (int)status, 0, NULL);
    return;
  }

  if (alias_name) {
    name = alias_name;
  }

  if (service) {
    if (hints->ai_flags & ARES_AI_NUMERICSERV) {
      unsigned long val;
      errno = 0;
      val   = strtoul(service, NULL, 0);
      if ((val == 0 && errno != 0) || val > 65535) {
        ares_free(alias_name);
        callback(arg, ARES_ESERVICE, 0, NULL);
        return;
      }
      port = (unsigned short)val;
    } else {
      port = lookup_service(service, 0);
      if (!port) {
        unsigned long val;
        errno = 0;
        val   = strtoul(service, NULL, 0);
        if ((val == 0 && errno != 0) || val > 65535) {
          ares_free(alias_name);
          callback(arg, ARES_ESERVICE, 0, NULL);
          return;
        }
        port = (unsigned short)val;
      }
    }
  }

  ai = ares__malloc_addrinfo();
  if (!ai) {
    ares_free(alias_name);
    callback(arg, ARES_ENOMEM, 0, NULL);
    return;
  }

  if (fake_addrinfo(name, port, hints, ai, callback, arg)) {
    ares_free(alias_name);
    return;
  }

  /* Allocate and fill in the host query structure. */
  hquery = ares_malloc(sizeof(*hquery));
  if (!hquery) {
    ares_free(alias_name);
    ares_freeaddrinfo(ai);
    callback(arg, ARES_ENOMEM, 0, NULL);
    return;
  }
  memset(hquery, 0, sizeof(*hquery));
  hquery->name = ares_strdup(name);
  ares_free(alias_name);
  if (!hquery->name) {
    ares_free(hquery);
    ares_freeaddrinfo(ai);
    callback(arg, ARES_ENOMEM, 0, NULL);
    return;
  }

  hquery->port              = port;
  hquery->channel           = channel;
  hquery->hints             = *hints;
  hquery->sent_family       = -1; /* nothing is sent yet */
  hquery->callback          = callback;
  hquery->arg               = arg;
  hquery->remaining_lookups = channel->lookups;
  hquery->ai                = ai;
  hquery->next_domain       = -1;

  /* Start performing lookups according to channel->lookups. */
  next_lookup(hquery, ARES_ECONNREFUSED /* initial error code */);
}

static ares_bool_t next_dns_lookup(struct host_query *hquery)
{
  char         *s              = NULL;
  ares_bool_t   is_s_allocated = ARES_FALSE;
  ares_status_t status;

  /* if next_domain == -1 and as_is_first is true, try hquery->name */
  if (hquery->next_domain == -1) {
    if (as_is_first(hquery)) {
      s = hquery->name;
    }
    hquery->next_domain = 0;
  }

  /* if as_is_first is false, try hquery->name at last */
  if (!s && (size_t)hquery->next_domain == hquery->channel->ndomains) {
    if (!as_is_first(hquery)) {
      s = hquery->name;
    }
    hquery->next_domain++;
  }

  if (!s && (size_t)hquery->next_domain < hquery->channel->ndomains &&
      !as_is_only(hquery)) {
    status = ares__cat_domain(
      hquery->name, hquery->channel->domains[hquery->next_domain++], &s);
    if (status == ARES_SUCCESS) {
      is_s_allocated = ARES_TRUE;
    }
  }

  if (s) {
    /* NOTE: hquery may be invalidated during the call to ares_query_qid(),
     *       so should not be referenced after this point */
    switch (hquery->hints.ai_family) {
      case AF_INET:
        hquery->remaining += 1;
        ares_query_qid(hquery->channel, s, C_IN, T_A, host_callback, hquery,
                       &hquery->qid_a);
        break;
      case AF_INET6:
        hquery->remaining += 1;
        ares_query_qid(hquery->channel, s, C_IN, T_AAAA, host_callback, hquery,
                       &hquery->qid_aaaa);
        break;
      case AF_UNSPEC:
        hquery->remaining += 2;
        ares_query_qid(hquery->channel, s, C_IN, T_A, host_callback, hquery,
                       &hquery->qid_a);
        ares_query_qid(hquery->channel, s, C_IN, T_AAAA, host_callback, hquery,
                       &hquery->qid_aaaa);
        break;
      default:
        break;
    }
    if (is_s_allocated) {
      ares_free(s);
    }
    return ARES_TRUE;
  } else {
    assert(!hquery->ai->nodes);
    return ARES_FALSE;
  }
}

static ares_bool_t as_is_first(const struct host_query *hquery)
{
  const char *p;
  size_t      ndots = 0;
  size_t      nname = ares_strlen(hquery->name);
  for (p = hquery->name; p && *p; p++) {
    if (*p == '.') {
      ndots++;
    }
  }
  if (hquery->name != NULL && nname && hquery->name[nname - 1] == '.') {
    /* prevent ARES_EBADNAME for valid FQDN, where ndots < channel->ndots  */
    return ARES_TRUE;
  }
  return ndots >= hquery->channel->ndots ? ARES_TRUE : ARES_FALSE;
}

static ares_bool_t as_is_only(const struct host_query *hquery)
{
  size_t nname = ares_strlen(hquery->name);
  if (hquery->name != NULL && nname && hquery->name[nname - 1] == '.') {
    return ARES_TRUE;
  }
  return ARES_FALSE;
}

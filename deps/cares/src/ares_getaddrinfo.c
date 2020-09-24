
/* Copyright 1998, 2011, 2013 by the Massachusetts Institute of Technology.
 * Copyright (C) 2017 - 2018 by Christian Ammer
 * Copyright (C) 2019 by Andrew Selivanov
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

#include "ares_setup.h"

#ifdef HAVE_GETSERVBYNAME_R
#  if !defined(GETSERVBYNAME_R_ARGS) || \
     (GETSERVBYNAME_R_ARGS < 4) || (GETSERVBYNAME_R_ARGS > 6)
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
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <assert.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "ares.h"
#include "bitncmp.h"
#include "ares_private.h"

#ifdef WATT32
#undef WIN32
#endif
#ifdef WIN32
#  include "ares_platform.h"
#endif

struct host_query
{
  ares_channel channel;
  char *name;
  unsigned short port; /* in host order */
  ares_addrinfo_callback callback;
  void *arg;
  struct ares_addrinfo_hints hints;
  int sent_family; /* this family is what was is being used */
  int timeouts;    /* number of timeouts we saw for this request */
  const char *remaining_lookups; /* types of lookup we need to perform ("fb" by
                                    default, file and dns respectively) */
  struct ares_addrinfo *ai;      /* store results between lookups */
  int remaining;   /* number of DNS answers waiting for */
  int next_domain; /* next search domain to try */
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
  NULL  /* nodes */
};

/* forward declarations */
static void host_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen);
static int as_is_first(const struct host_query *hquery);
static int next_dns_lookup(struct host_query *hquery);

struct ares_addrinfo_cname *ares__malloc_addrinfo_cname()
{
  struct ares_addrinfo_cname *cname = ares_malloc(sizeof(struct ares_addrinfo_cname));
  if (!cname)
    return NULL;

  *cname = empty_addrinfo_cname;
  return cname;
}

struct ares_addrinfo_cname *ares__append_addrinfo_cname(struct ares_addrinfo_cname **head)
{
  struct ares_addrinfo_cname *tail = ares__malloc_addrinfo_cname();
  struct ares_addrinfo_cname *last = *head;
  if (!last)
    {
      *head = tail;
      return tail;
    }

  while (last->next)
    {
      last = last->next;
    }

  last->next = tail;
  return tail;
}

void ares__addrinfo_cat_cnames(struct ares_addrinfo_cname **head,
                               struct ares_addrinfo_cname *tail)
{
  struct ares_addrinfo_cname *last = *head;
  if (!last)
    {
      *head = tail;
      return;
    }

  while (last->next)
    {
      last = last->next;
    }

  last->next = tail;
}

struct ares_addrinfo *ares__malloc_addrinfo()
{
  struct ares_addrinfo *ai = ares_malloc(sizeof(struct ares_addrinfo));
  if (!ai)
    return NULL;

  *ai = empty_addrinfo;
  return ai;
}

struct ares_addrinfo_node *ares__malloc_addrinfo_node()
{
  struct ares_addrinfo_node *node =
      ares_malloc(sizeof(struct ares_addrinfo_node));
  if (!node)
    return NULL;

  *node = empty_addrinfo_node;
  return node;
}

/* Allocate new addrinfo and append to the tail. */
struct ares_addrinfo_node *ares__append_addrinfo_node(struct ares_addrinfo_node **head)
{
  struct ares_addrinfo_node *tail = ares__malloc_addrinfo_node();
  struct ares_addrinfo_node *last = *head;
  if (!last)
    {
      *head = tail;
      return tail;
    }

  while (last->ai_next)
    {
      last = last->ai_next;
    }

  last->ai_next = tail;
  return tail;
}

void ares__addrinfo_cat_nodes(struct ares_addrinfo_node **head,
                              struct ares_addrinfo_node *tail)
{
  struct ares_addrinfo_node *last = *head;
  if (!last)
    {
      *head = tail;
      return;
    }

  while (last->ai_next)
    {
      last = last->ai_next;
    }

  last->ai_next = tail;
}

/* Resolve service name into port number given in host byte order.
 * If not resolved, return 0.
 */
static unsigned short lookup_service(const char *service, int flags)
{
  const char *proto;
  struct servent *sep;
#ifdef HAVE_GETSERVBYNAME_R
  struct servent se;
  char tmpbuf[4096];
#endif

  if (service)
    {
      if (flags & ARES_NI_UDP)
        proto = "udp";
      else if (flags & ARES_NI_SCTP)
        proto = "sctp";
      else if (flags & ARES_NI_DCCP)
        proto = "dccp";
      else
        proto = "tcp";
#ifdef HAVE_GETSERVBYNAME_R
      memset(&se, 0, sizeof(se));
      sep = &se;
      memset(tmpbuf, 0, sizeof(tmpbuf));
#if GETSERVBYNAME_R_ARGS == 6
      if (getservbyname_r(service, proto, &se, (void *)tmpbuf, sizeof(tmpbuf),
                          &sep) != 0)
        sep = NULL; /* LCOV_EXCL_LINE: buffer large so this never fails */
#elif GETSERVBYNAME_R_ARGS == 5
      sep =
          getservbyname_r(service, proto, &se, (void *)tmpbuf, sizeof(tmpbuf));
#elif GETSERVBYNAME_R_ARGS == 4
      if (getservbyname_r(service, proto, &se, (void *)tmpbuf) != 0)
        sep = NULL;
#else
      /* Lets just hope the OS uses TLS! */
      sep = getservbyname(service, proto);
#endif
#else
        /* Lets just hope the OS uses TLS! */
#if (defined(NETWARE) && !defined(__NOVELL_LIBC__))
      sep = getservbyname(service, (char *)proto);
#else
      sep = getservbyname(service, proto);
#endif
#endif
      return (sep ? ntohs((unsigned short)sep->s_port) : 0);
    }
  return 0;
}

/* If the name looks like an IP address or an error occured,
 * fake up a host entry, end the query immediately, and return true.
 * Otherwise return false.
 */
static int fake_addrinfo(const char *name,
                         unsigned short port,
                         const struct ares_addrinfo_hints *hints,
                         struct ares_addrinfo *ai,
                         ares_addrinfo_callback callback,
                         void *arg)
{
  struct ares_addrinfo_cname *cname;
  struct ares_addrinfo_node *node;
  ares_sockaddr addr;
  size_t addrlen;
  int result = 0;
  int family = hints->ai_family;
  if (family == AF_INET || family == AF_INET6 || family == AF_UNSPEC)
    {
      /* It only looks like an IP address if it's all numbers and dots. */
      int numdots = 0, valid = 1;
      const char *p;
      for (p = name; *p; p++)
        {
          if (!ISDIGIT(*p) && *p != '.')
            {
              valid = 0;
              break;
            }
          else if (*p == '.')
            {
              numdots++;
            }
        }

      memset(&addr, 0, sizeof(addr));

      /* if we don't have 3 dots, it is illegal
       * (although inet_pton doesn't think so).
       */
      if (numdots != 3 || !valid)
        result = 0;
      else
        result =
            (ares_inet_pton(AF_INET, name, &addr.sa4.sin_addr) < 1 ? 0 : 1);

      if (result)
        {
          family = addr.sa.sa_family = AF_INET;
          addr.sa4.sin_port = htons(port);
          addrlen = sizeof(addr.sa4);
        }
    }

  if (family == AF_INET6 || family == AF_UNSPEC)
    {
      result =
          (ares_inet_pton(AF_INET6, name, &addr.sa6.sin6_addr) < 1 ? 0 : 1);
      addr.sa6.sin6_family = AF_INET6;
      addr.sa6.sin6_port = htons(port);
      addrlen = sizeof(addr.sa6);
    }

  if (!result)
    return 0;

  node = ares__malloc_addrinfo_node();
  if (!node)
    {
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return 1;
    }

  ai->nodes = node;

  node->ai_addr = ares_malloc(addrlen);
  if (!node->ai_addr)
    {
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return 1;
    }

  node->ai_addrlen = (unsigned int)addrlen;
  node->ai_family = addr.sa.sa_family;
  if (addr.sa.sa_family == AF_INET)
    memcpy(node->ai_addr, &addr.sa4, sizeof(addr.sa4));
  else
    memcpy(node->ai_addr, &addr.sa6, sizeof(addr.sa6));

  if (hints->ai_flags & ARES_AI_CANONNAME)
    {
      cname = ares__append_addrinfo_cname(&ai->cnames);
      if (!cname)
        {
          ares_freeaddrinfo(ai);
          callback(arg, ARES_ENOMEM, 0, NULL);
          return 1;
        }

      /* Duplicate the name, to avoid a constness violation. */
      cname->name = ares_strdup(name);
      if (!cname->name)
        {
          ares_freeaddrinfo(ai);
          callback(arg, ARES_ENOMEM, 0, NULL);
          return 1;
        }
    }

  callback(arg, ARES_SUCCESS, 0, ai);
  return 1;
}

static void end_hquery(struct host_query *hquery, int status)
{
  struct ares_addrinfo_node sentinel;
  struct ares_addrinfo_node *next;
  if (status == ARES_SUCCESS)
    {
      if (!(hquery->hints.ai_flags & ARES_AI_NOSORT))
        {
          sentinel.ai_next = hquery->ai->nodes;
          ares__sortaddrinfo(hquery->channel, &sentinel);
          hquery->ai->nodes = sentinel.ai_next;
        }
      next = hquery->ai->nodes;
      /* Set port into each address (resolved separately). */
      while (next)
        {
          if (next->ai_family == AF_INET)
            {
              (CARES_INADDR_CAST(struct sockaddr_in *, next->ai_addr))->sin_port = htons(hquery->port);
            }
          else
            {
              (CARES_INADDR_CAST(struct sockaddr_in6 *, next->ai_addr))->sin6_port = htons(hquery->port);
            }
          next = next->ai_next;
        }
    }
  else
    {
      /* Clean up what we have collected by so far. */
      ares_freeaddrinfo(hquery->ai);
      hquery->ai = NULL;
    }

  hquery->callback(hquery->arg, status, hquery->timeouts, hquery->ai);
  ares_free(hquery->name);
  ares_free(hquery);
}

static int file_lookup(struct host_query *hquery)
{
  FILE *fp;
  int error;
  int status;
  const char *path_hosts = NULL;

  if (hquery->hints.ai_flags & ARES_AI_ENVHOSTS)
    {
      path_hosts = getenv("CARES_HOSTS");
    }

  if (!path_hosts)
    {
#ifdef WIN32
      char PATH_HOSTS[MAX_PATH];
      win_platform platform;

      PATH_HOSTS[0] = '\0';

      platform = ares__getplatform();

      if (platform == WIN_NT)
        {
          char tmp[MAX_PATH];
          HKEY hkeyHosts;

          if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0, KEY_READ,
                           &hkeyHosts) == ERROR_SUCCESS)
            {
              DWORD dwLength = MAX_PATH;
              RegQueryValueExA(hkeyHosts, DATABASEPATH, NULL, NULL, (LPBYTE)tmp,
                              &dwLength);
              ExpandEnvironmentStringsA(tmp, PATH_HOSTS, MAX_PATH);
              RegCloseKey(hkeyHosts);
            }
        }
      else if (platform == WIN_9X)
        GetWindowsDirectoryA(PATH_HOSTS, MAX_PATH);
      else
        return ARES_ENOTFOUND;

      strcat(PATH_HOSTS, WIN_PATH_HOSTS);
      path_hosts = PATH_HOSTS;

#elif defined(WATT32)
      const char *PATH_HOSTS = _w32_GetHostsFile();

      if (!PATH_HOSTS)
        return ARES_ENOTFOUND;
#endif
      path_hosts = PATH_HOSTS;
    }

  fp = fopen(path_hosts, "r");
  if (!fp)
    {
      error = ERRNO;
      switch (error)
        {
        case ENOENT:
        case ESRCH:
          return ARES_ENOTFOUND;
        default:
          DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n", error,
                         strerror(error)));
          DEBUGF(fprintf(stderr, "Error opening file: %s\n", path_hosts));
          return ARES_EFILE;
        }
    }
  status = ares__readaddrinfo(fp, hquery->name, hquery->port, &hquery->hints, hquery->ai);
  fclose(fp);
  return status;
}

static void next_lookup(struct host_query *hquery, int status)
{
  switch (*hquery->remaining_lookups)
    {
      case 'b':
          /* DNS lookup */
          if (next_dns_lookup(hquery))
            break;
          hquery->remaining_lookups++;
          next_lookup(hquery, status);
          break;

      case 'f':
          /* Host file lookup */
          if (file_lookup(hquery) == ARES_SUCCESS)
            {
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

static void host_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen)
{
  struct host_query *hquery = (struct host_query*)arg;
  int addinfostatus = ARES_SUCCESS;
  hquery->timeouts += timeouts;
  hquery->remaining--;

  if (status == ARES_SUCCESS)
    {
      addinfostatus = ares__parse_into_addrinfo(abuf, alen, hquery->ai);
    }
  else if (status == ARES_EDESTRUCTION)
    {
      end_hquery(hquery, status);
      return;
    }

  if (!hquery->remaining)
    {
      if (addinfostatus != ARES_SUCCESS)
        {
          /* error in parsing result e.g. no memory */
          end_hquery(hquery, addinfostatus);
        }
      else if (hquery->ai->nodes)
        {
          /* at least one query ended with ARES_SUCCESS */
          end_hquery(hquery, ARES_SUCCESS);
        }
      else if (status == ARES_ENOTFOUND)
        {
          next_lookup(hquery, status);
        }
      else
        {
          end_hquery(hquery, status);
        }
    }

  /* at this point we keep on waiting for the next query to finish */
}

void ares_getaddrinfo(ares_channel channel,
                      const char* name, const char* service,
                      const struct ares_addrinfo_hints* hints,
                      ares_addrinfo_callback callback, void* arg)
{
  struct host_query *hquery;
  unsigned short port = 0;
  int family;
  struct ares_addrinfo *ai;

  if (!hints)
    {
      hints = &default_hints;
    }

  family = hints->ai_family;

  /* Right now we only know how to look up Internet addresses
     and unspec means try both basically. */
  if (family != AF_INET &&
      family != AF_INET6 &&
      family != AF_UNSPEC)
    {
      callback(arg, ARES_ENOTIMP, 0, NULL);
      return;
    }

  if (ares__is_onion_domain(name))
    {
      callback(arg, ARES_ENOTFOUND, 0, NULL);
      return;
    }

  if (service)
    {
      if (hints->ai_flags & ARES_AI_NUMERICSERV)
        {
          port = (unsigned short)strtoul(service, NULL, 0);
          if (!port)
            {
              callback(arg, ARES_ESERVICE, 0, NULL);
              return;
            }
        }
      else
        {
          port = lookup_service(service, 0);
          if (!port)
            {
              port = (unsigned short)strtoul(service, NULL, 0);
              if (!port)
                {
                  callback(arg, ARES_ESERVICE, 0, NULL);
                  return;
                }
            }
        }
    }

  ai = ares__malloc_addrinfo();
  if (!ai)
    {
      callback(arg, ARES_ENOMEM, 0, NULL);
      return;
    }

  if (fake_addrinfo(name, port, hints, ai, callback, arg))
    {
      return;
    }

  /* Allocate and fill in the host query structure. */
  hquery = ares_malloc(sizeof(struct host_query));
  if (!hquery)
    {
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return;
    }

  hquery->name = ares_strdup(name);
  if (!hquery->name)
    {
      ares_free(hquery);
      ares_freeaddrinfo(ai);
      callback(arg, ARES_ENOMEM, 0, NULL);
      return;
    }

  hquery->port = port;
  hquery->channel = channel;
  hquery->hints = *hints;
  hquery->sent_family = -1; /* nothing is sent yet */
  hquery->callback = callback;
  hquery->arg = arg;
  hquery->remaining_lookups = channel->lookups;
  hquery->timeouts = 0;
  hquery->ai = ai;
  hquery->next_domain = -1;
  hquery->remaining = 0;

  /* Start performing lookups according to channel->lookups. */
  next_lookup(hquery, ARES_ECONNREFUSED /* initial error code */);
}

static int next_dns_lookup(struct host_query *hquery)
{
  char *s = NULL;
  int is_s_allocated = 0;
  int status;

  /* if next_domain == -1 and as_is_first is true, try hquery->name */
  if (hquery->next_domain == -1)
    {
      if (as_is_first(hquery))
        {
          s = hquery->name;
        }
      hquery->next_domain = 0;
    }

  /* if as_is_first is false, try hquery->name at last */
  if (!s && hquery->next_domain == hquery->channel->ndomains) {
    if (!as_is_first(hquery))
      {
        s = hquery->name;
      }
    hquery->next_domain++;
  }

  if (!s && hquery->next_domain < hquery->channel->ndomains)
    {
      status = ares__cat_domain(
          hquery->name,
          hquery->channel->domains[hquery->next_domain++],
          &s);
      if (status == ARES_SUCCESS)
        {
          is_s_allocated = 1;
        }
    }

  if (s)
    {
      switch (hquery->hints.ai_family)
        {
          case AF_INET:
            hquery->remaining += 1;
            ares_query(hquery->channel, s, C_IN, T_A, host_callback, hquery);
            break;
          case AF_INET6:
            hquery->remaining += 1;
            ares_query(hquery->channel, s, C_IN, T_AAAA, host_callback, hquery);
            break;
          case AF_UNSPEC:
            hquery->remaining += 2;
            ares_query(hquery->channel, s, C_IN, T_A, host_callback, hquery);
            ares_query(hquery->channel, s, C_IN, T_AAAA, host_callback, hquery);
            break;
          default: break;
        }
      if (is_s_allocated)
        {
          ares_free(s);
        }
      return 1;
    }
  else
    {
      assert(!hquery->ai->nodes);
      return 0;
    }
}

static int as_is_first(const struct host_query* hquery)
{
  char* p;
  int ndots = 0;
  for (p = hquery->name; *p; p++)
    {
      if (*p == '.')
        {
          ndots++;
        }
    }
  return ndots >= hquery->channel->ndots;
}

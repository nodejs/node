/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2005 Dominick Meglio
 * Copyright (c) 2019 Andrew Selivanov
 * Copyright (c) 2021 Brad House
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

#include "ares_private.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

static size_t hostent_nalias(const struct hostent *host)
{
  size_t i;
  for (i=0; host->h_aliases != NULL && host->h_aliases[i] != NULL; i++)
    ;

  return i;
}

static size_t ai_nalias(const struct ares_addrinfo *ai)
{
  const struct ares_addrinfo_cname *cname;
  size_t                            i     = 0;

  for (cname = ai->cnames; cname != NULL; cname=cname->next) {
    i++;
  }

  return i;
}

static size_t hostent_naddr(const struct hostent *host)
{
  size_t i;
  for (i=0; host->h_addr_list != NULL && host->h_addr_list[i] != NULL; i++)
    ;

  return i;
}

static size_t ai_naddr(const struct ares_addrinfo *ai, int af)
{
  const struct ares_addrinfo_node *node;
  size_t                           i     = 0;

  for (node = ai->nodes; node != NULL; node=node->ai_next) {
    if (af != AF_UNSPEC && af != node->ai_family)
      continue;
    i++;
  }

  return i;
}

ares_status_t ares_addrinfo2hostent(const struct ares_addrinfo *ai, int family,
                                    struct hostent **host)
{
  struct ares_addrinfo_node  *next;
  char                      **aliases  = NULL;
  char                      **addrs    = NULL;
  size_t                      naliases = 0;
  size_t                      naddrs   = 0;
  size_t                      i;
  size_t                      ealiases = 0;
  size_t                      eaddrs   = 0;

  if (ai == NULL || host == NULL) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  /* Use either the host set in the passed in hosts to be filled in, or the
   * first node of the response as the family, since hostent can only
   * represent one family.  We assume getaddrinfo() returned a sorted list if
   * the user requested AF_UNSPEC. */
  if (family == AF_UNSPEC) {
    if (*host != NULL && (*host)->h_addrtype != AF_UNSPEC) {
      family = (*host)->h_addrtype;
    } else if (ai->nodes != NULL) {
      family = ai->nodes->ai_family;
    }
  }

  if (family != AF_INET && family != AF_INET6) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  if (*host == NULL) {
    *host = ares_malloc_zero(sizeof(**host));
    if (!(*host)) {
      goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
    }
  }

  (*host)->h_addrtype = (HOSTENT_ADDRTYPE_TYPE)family;
  if (family == AF_INET) {
    (*host)->h_length = sizeof(struct in_addr);
  } else if (family == AF_INET6) {
    (*host)->h_length = sizeof(struct ares_in6_addr);
  }

  if ((*host)->h_name == NULL) {
    if (ai->cnames) {
      (*host)->h_name = ares_strdup(ai->cnames->name);
      if ((*host)->h_name == NULL && ai->cnames->name) {
        goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
      }
    } else {
      (*host)->h_name = ares_strdup(ai->name);
      if ((*host)->h_name == NULL && ai->name) {
        goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
      }
    }
  }

  naliases = ai_nalias(ai);
  ealiases = hostent_nalias(*host);
  aliases  = ares_realloc_zero((*host)->h_aliases,
                               ealiases * sizeof(char *),
                               (naliases + ealiases + 1) * sizeof(char *));
  if (!aliases) {
    goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
  }
  (*host)->h_aliases = aliases;

  if (naliases) {
    const struct ares_addrinfo_cname *cname;
    i = ealiases;
    for (cname = ai->cnames; cname != NULL; cname = cname->next) {
      if (cname->alias == NULL) {
        continue;
      }
      (*host)->h_aliases[i] = ares_strdup(cname->alias);
      if ((*host)->h_aliases[i] == NULL) {
        goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
      }
      i++;
    }
  }

  naddrs = ai_naddr(ai, family);
  eaddrs = hostent_naddr(*host);
  addrs  = ares_realloc_zero((*host)->h_addr_list, eaddrs * sizeof(char *),
                             (naddrs + eaddrs + 1) * sizeof(char *));
  if (addrs == NULL) {
    goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
  }
  (*host)->h_addr_list = addrs;

  if (naddrs) {
    i = eaddrs;
    for (next = ai->nodes; next != NULL; next = next->ai_next) {
      if (next->ai_family != family) {
        continue;
      }
      (*host)->h_addr_list[i] = ares_malloc_zero((size_t)(*host)->h_length);
      if ((*host)->h_addr_list[i] == NULL) {
        goto enomem; /* LCOV_EXCL_LINE: OutOfMemory */
      }
      if (family == AF_INET6) {
        memcpy((*host)->h_addr_list[i],
               &(CARES_INADDR_CAST(const struct sockaddr_in6 *, next->ai_addr)
                   ->sin6_addr),
               (size_t)(*host)->h_length);
      }
      if (family == AF_INET) {
        memcpy((*host)->h_addr_list[i],
               &(CARES_INADDR_CAST(const struct sockaddr_in *, next->ai_addr)
                   ->sin_addr),
               (size_t)(*host)->h_length);
      }
      i++;
    }
  }

  if (naddrs + eaddrs == 0 && naliases + ealiases == 0) {
    ares_free_hostent(*host);
    *host = NULL;
    return ARES_ENODATA;
  }

  return ARES_SUCCESS;

/* LCOV_EXCL_START: OutOfMemory */
enomem:
  ares_free_hostent(*host);
  *host = NULL;
  return ARES_ENOMEM;
  /* LCOV_EXCL_STOP */
}

ares_status_t ares_addrinfo2addrttl(const struct ares_addrinfo *ai, int family,
                                    size_t                req_naddrttls,
                                    struct ares_addrttl  *addrttls,
                                    struct ares_addr6ttl *addr6ttls,
                                    size_t               *naddrttls)
{
  struct ares_addrinfo_node  *next;
  struct ares_addrinfo_cname *next_cname;
  int                         cname_ttl = INT_MAX;

  if (family != AF_INET && family != AF_INET6) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  if (ai == NULL || naddrttls == NULL) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  if (family == AF_INET && addrttls == NULL) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  if (family == AF_INET6 && addr6ttls == NULL) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  if (req_naddrttls == 0) {
    return ARES_EBADQUERY; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  *naddrttls = 0;

  next_cname = ai->cnames;
  while (next_cname) {
    if (next_cname->ttl < cname_ttl) {
      cname_ttl = next_cname->ttl;
    }
    next_cname = next_cname->next;
  }

  for (next = ai->nodes; next != NULL; next = next->ai_next) {
    if (next->ai_family != family) {
      continue;
    }

    if (*naddrttls >= req_naddrttls) {
      break;
    }

    if (family == AF_INET6) {
      if (next->ai_ttl > cname_ttl) {
        addr6ttls[*naddrttls].ttl = cname_ttl;
      } else {
        addr6ttls[*naddrttls].ttl = next->ai_ttl;
      }

      memcpy(&addr6ttls[*naddrttls].ip6addr,
             &(CARES_INADDR_CAST(const struct sockaddr_in6 *, next->ai_addr)
                 ->sin6_addr),
             sizeof(struct ares_in6_addr));
    } else {
      if (next->ai_ttl > cname_ttl) {
        addrttls[*naddrttls].ttl = cname_ttl;
      } else {
        addrttls[*naddrttls].ttl = next->ai_ttl;
      }
      memcpy(&addrttls[*naddrttls].ipaddr,
             &(CARES_INADDR_CAST(const struct sockaddr_in *, next->ai_addr)
                 ->sin_addr),
             sizeof(struct in_addr));
    }
    (*naddrttls)++;
  }

  return ARES_SUCCESS;
}

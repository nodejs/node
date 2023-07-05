/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright 2005 Dominick Meglio
 * Copyright (C) 2019 by Andrew Selivanov
 * Copyright (C) 2021 by Brad House
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

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "ares.h"
#include "ares_dns.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

int ares__addrinfo2hostent(const struct ares_addrinfo *ai, int family,
                           struct hostent **host)
{
  struct ares_addrinfo_node *next;
  struct ares_addrinfo_cname *next_cname;
  char **aliases = NULL;
  char *addrs = NULL;
  int naliases = 0, naddrs = 0, alias = 0, i;

  if (ai == NULL || host == NULL)
    return ARES_EBADQUERY;

  *host = ares_malloc(sizeof(**host));
  if (!(*host))
    {
      goto enomem;
    }
  memset(*host, 0, sizeof(**host));

  /* Use the first node of the response as the family, since hostent can only
   * represent one family.  We assume getaddrinfo() returned a sorted list if
   * the user requested AF_UNSPEC. */
  if (family == AF_UNSPEC && ai->nodes)
    family = ai->nodes->ai_family;

  next = ai->nodes;
  while (next)
    {
      if(next->ai_family == family)
        {
          ++naddrs;
        }
      next = next->ai_next;
    }

  next_cname = ai->cnames;
  while (next_cname)
    {
      if(next_cname->alias)
        ++naliases;
      next_cname = next_cname->next;
    }

  aliases = ares_malloc((naliases + 1) * sizeof(char *));
  if (!aliases)
    {
      goto enomem;
    }
  (*host)->h_aliases = aliases;
  memset(aliases, 0, (naliases + 1) * sizeof(char *));

  if (naliases)
    {
      next_cname = ai->cnames;
      while (next_cname)
        {
          if(next_cname->alias) {
            aliases[alias] = ares_strdup(next_cname->alias);
            if (!aliases[alias]) {
              goto enomem;
            }
            alias++;
          }
          next_cname = next_cname->next;
        }
    }


  (*host)->h_addr_list = ares_malloc((naddrs + 1) * sizeof(char *));
  if (!(*host)->h_addr_list)
    {
      goto enomem;
    }

  memset((*host)->h_addr_list, 0, (naddrs + 1) * sizeof(char *));

  if (ai->cnames)
    {
      (*host)->h_name = ares_strdup(ai->cnames->name);
      if ((*host)->h_name == NULL && ai->cnames->name)
        {
          goto enomem;
        }
    }
  else
    {
      (*host)->h_name = ares_strdup(ai->name);
      if ((*host)->h_name == NULL && ai->name)
        {
          goto enomem;
        }
    }

  (*host)->h_addrtype = family;
  (*host)->h_length = (family == AF_INET)?
     sizeof(struct in_addr):sizeof(struct ares_in6_addr);

  if (naddrs)
    {
      addrs = ares_malloc(naddrs * (*host)->h_length);
      if (!addrs)
        {
          goto enomem;
        }

      i = 0;
      next = ai->nodes;
      while (next)
        {
          if(next->ai_family == family)
            {
              (*host)->h_addr_list[i] = addrs + (i * (*host)->h_length);
              if (family == AF_INET6)
                {
                  memcpy((*host)->h_addr_list[i],
                     &(CARES_INADDR_CAST(struct sockaddr_in6 *, next->ai_addr)->sin6_addr),
                     (*host)->h_length);
                }
              else
                {
                  memcpy((*host)->h_addr_list[i],
                     &(CARES_INADDR_CAST(struct sockaddr_in *, next->ai_addr)->sin_addr),
                     (*host)->h_length);
                }
              ++i;
            }
          next = next->ai_next;
        }

      if (i == 0)
        {
          ares_free(addrs);
        }
    }

  if (naddrs == 0 && naliases == 0)
    {
      ares_free_hostent(*host);
      *host = NULL;
      return ARES_ENODATA;
    }

  return ARES_SUCCESS;

enomem:
  ares_free_hostent(*host);
  *host = NULL;
  return ARES_ENOMEM;
}


int ares__addrinfo2addrttl(const struct ares_addrinfo *ai, int family,
                           int req_naddrttls, struct ares_addrttl *addrttls,
                           struct ares_addr6ttl *addr6ttls, int *naddrttls)
{
  struct ares_addrinfo_node *next;
  struct ares_addrinfo_cname *next_cname;
  int cname_ttl = INT_MAX;

  if (family != AF_INET && family != AF_INET6)
    return ARES_EBADQUERY;

  if (ai == NULL || naddrttls == NULL)
    return ARES_EBADQUERY;

  if (family == AF_INET && addrttls == NULL)
    return ARES_EBADQUERY;

  if (family == AF_INET6 && addr6ttls == NULL)
    return ARES_EBADQUERY;

  if (req_naddrttls == 0)
    return ARES_EBADQUERY;

  *naddrttls = 0;

  next_cname = ai->cnames;
  while (next_cname)
    {
      if(next_cname->ttl < cname_ttl)
        cname_ttl = next_cname->ttl;
      next_cname = next_cname->next;
    }

  next = ai->nodes;
  while (next)
    {
      if(next->ai_family == family)
        {
          if (*naddrttls < req_naddrttls)
            {
                if (family == AF_INET6)
                  {
                    if(next->ai_ttl > cname_ttl)
                      addr6ttls[*naddrttls].ttl = cname_ttl;
                    else
                      addr6ttls[*naddrttls].ttl = next->ai_ttl;

                    memcpy(&addr6ttls[*naddrttls].ip6addr,
                           &(CARES_INADDR_CAST(struct sockaddr_in6 *, next->ai_addr)->sin6_addr),
                           sizeof(struct ares_in6_addr));
                  }
                else
                  {
                    if(next->ai_ttl > cname_ttl)
                      addrttls[*naddrttls].ttl = cname_ttl;
                    else
                      addrttls[*naddrttls].ttl = next->ai_ttl;
                    memcpy(&addrttls[*naddrttls].ipaddr,
                           &(CARES_INADDR_CAST(struct sockaddr_in *, next->ai_addr)->sin_addr),
                           sizeof(struct in_addr));
                  }
                (*naddrttls)++;
             }
        }
      next = next->ai_next;
    }

  return ARES_SUCCESS;
}


/* Copyright (C) 2021
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

#if defined(_WIN32) && defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
#include <ws2ipdef.h>
#include <iphlpapi.h>
#endif

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_nowarn.h"
#include "ares_private.h"

int ares_append_ai_node(int aftype,
                        unsigned short port,
                        int ttl,
                        const void *adata,
                        struct ares_addrinfo_node **nodes)
{
  struct ares_addrinfo_node *node;

  node = ares__append_addrinfo_node(nodes);
  if (!node)
    {
      return ARES_ENOMEM;
    }

  memset(node, 0, sizeof(*node));

  if (aftype == AF_INET)
    {
      struct sockaddr_in *sin = ares_malloc(sizeof(*sin));
      if (!sin)
        {
          return ARES_ENOMEM;
        }

      memset(sin, 0, sizeof(*sin));
      memcpy(&sin->sin_addr.s_addr, adata, sizeof(sin->sin_addr.s_addr));
      sin->sin_family = AF_INET;
      sin->sin_port = htons(port);

      node->ai_addr = (struct sockaddr *)sin;
      node->ai_family = AF_INET;
      node->ai_addrlen = sizeof(*sin);
      node->ai_addr = (struct sockaddr *)sin;
      node->ai_ttl = ttl;
    }

  if (aftype == AF_INET6)
    {
      struct sockaddr_in6 *sin6 = ares_malloc(sizeof(*sin6));
      if (!sin6)
        {
          return ARES_ENOMEM;
        }

      memset(sin6, 0, sizeof(*sin6));
      memcpy(&sin6->sin6_addr.s6_addr, adata, sizeof(sin6->sin6_addr.s6_addr));
      sin6->sin6_family = AF_INET6;
      sin6->sin6_port = htons(port);

      node->ai_addr = (struct sockaddr *)sin6;
      node->ai_family = AF_INET6;
      node->ai_addrlen = sizeof(*sin6);
      node->ai_addr = (struct sockaddr *)sin6;
      node->ai_ttl = ttl;
    }

  return ARES_SUCCESS;
}


static int ares__default_loopback_addrs(int aftype,
                                        unsigned short port,
                                        struct ares_addrinfo_node **nodes)
{
  int status = ARES_SUCCESS;

  if (aftype == AF_UNSPEC || aftype == AF_INET6)
    {
      struct ares_in6_addr addr6;
      ares_inet_pton(AF_INET6, "::1", &addr6);
      status = ares_append_ai_node(AF_INET6, port, 0, &addr6, nodes);
      if (status != ARES_SUCCESS)
        {
          return status;
       }
    }

  if (aftype == AF_UNSPEC || aftype == AF_INET)
    {
      struct in_addr addr4;
      ares_inet_pton(AF_INET, "127.0.0.1", &addr4);
      status = ares_append_ai_node(AF_INET, port, 0, &addr4, nodes);
      if (status != ARES_SUCCESS)
        {
          return status;
       }
    }

  return status;
}


static int ares__system_loopback_addrs(int aftype,
                                       unsigned short port,
                                       struct ares_addrinfo_node **nodes)
{
#if defined(_WIN32) && defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600 && !defined(__WATCOMC__)
  PMIB_UNICASTIPADDRESS_TABLE table;
  unsigned int i;
  int status;

  *nodes = NULL;

  if (GetUnicastIpAddressTable(aftype, &table) != NO_ERROR)
    return ARES_ENOTFOUND;

  for (i=0; i<table->NumEntries; i++)
    {
      if (table->Table[i].InterfaceLuid.Info.IfType !=
          IF_TYPE_SOFTWARE_LOOPBACK)
        {
          continue;
        }

      if (table->Table[i].Address.si_family == AF_INET)
        {
          status = ares_append_ai_node(table->Table[i].Address.si_family, port, 0,
                                       &table->Table[i].Address.Ipv4.sin_addr,
                                       nodes);
        }
      else if (table->Table[i].Address.si_family == AF_INET6)
        {
          status = ares_append_ai_node(table->Table[i].Address.si_family, port, 0,
                                       &table->Table[i].Address.Ipv6.sin6_addr,
                                       nodes);
        }
      else
        {
          /* Ignore any others */
          continue;
        }

      if (status != ARES_SUCCESS)
        {
          goto fail;
        }
    }

    if (*nodes == NULL)
      status = ARES_ENOTFOUND;

fail:
  FreeMibTable(table);

  if (status != ARES_SUCCESS)
    {
      ares__freeaddrinfo_nodes(*nodes);
      *nodes = NULL;
    }

  return status;

#else
  (void)aftype;
  (void)port;
  (void)nodes;
  /* Not supported on any other OS at this time */
  return ARES_ENOTFOUND;
#endif
}


int ares__addrinfo_localhost(const char *name,
                             unsigned short port,
                             const struct ares_addrinfo_hints *hints,
                             struct ares_addrinfo *ai)
{
  struct ares_addrinfo_node *nodes = NULL;
  int result;

  /* Validate family */
  switch (hints->ai_family) {
    case AF_INET:
    case AF_INET6:
    case AF_UNSPEC:
      break;
    default:
      return ARES_EBADFAMILY;
  }

  ai->name = ares_strdup(name);
  if(!ai->name)
    {
      goto enomem;
    }

  result = ares__system_loopback_addrs(hints->ai_family, port, &nodes);

  if (result == ARES_ENOTFOUND)
    {
      result = ares__default_loopback_addrs(hints->ai_family, port, &nodes);
    }

  ares__addrinfo_cat_nodes(&ai->nodes, nodes);

  return result;

enomem:
  ares__freeaddrinfo_nodes(nodes);
  ares_free(ai->name);
  ai->name = NULL;
  return ARES_ENOMEM;
}

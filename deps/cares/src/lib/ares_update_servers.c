/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2008 Daniel Stenberg
 * Copyright (c) 2023 Brad House
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

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares.h"
#include "ares_data.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

typedef struct {
  struct ares_addr addr;
  unsigned short   tcp_port;
  unsigned short   udp_port;
} ares_sconfig_t;

static ares_bool_t ares__addr_match(const struct ares_addr *addr1,
                                    const struct ares_addr *addr2)
{
  if (addr1 == NULL && addr2 == NULL) {
    return ARES_TRUE;
  }

  if (addr1 == NULL || addr2 == NULL) {
    return ARES_FALSE;
  }

  if (addr1->family != addr2->family) {
    return ARES_FALSE;
  }

  if (addr1->family == AF_INET && memcmp(&addr1->addr.addr4, &addr2->addr.addr4,
                                         sizeof(addr1->addr.addr4)) == 0) {
    return ARES_TRUE;
  }

  if (addr1->family == AF_INET6 &&
      memcmp(&addr1->addr.addr6._S6_un._S6_u8, &addr2->addr.addr6._S6_un._S6_u8,
             sizeof(addr1->addr.addr6._S6_un._S6_u8)) == 0) {
    return ARES_TRUE;
  }

  return ARES_FALSE;
}

/* Validate that the ip address matches the subnet (network base and network
 * mask) specified. Addresses are specified in standard Network Byte Order as
 * 16 bytes, and the netmask is 0 to 128 (bits).
 */
static ares_bool_t ares_ipv6_subnet_matches(const unsigned char  netbase[16],
                                            unsigned char        netmask,
                                            const unsigned char *ipaddr)
{
  unsigned char mask[16] = { 0 };
  unsigned char i;

  /* Misuse */
  if (netmask > 128) {
    return ARES_FALSE;
  }

  /* Quickly set whole bytes */
  memset(mask, 0xFF, netmask / 8);

  /* Set remaining bits */
  if (netmask % 8 && netmask < 128 /* Silence coverity */) {
    mask[netmask / 8] = (unsigned char)(0xff << (8 - (netmask % 8)));
  }

  for (i = 0; i < 16; i++) {
    if ((netbase[i] & mask[i]) != (ipaddr[i] & mask[i])) {
      return ARES_FALSE;
    }
  }

  return ARES_TRUE;
}

static ares_bool_t ares_server_blacklisted(const struct ares_addr *addr)
{
  /* A list of blacklisted IPv6 subnets. */
  const struct {
    const unsigned char netbase[16];
    unsigned char       netmask;
  } blacklist_v6[] = {
  /* fec0::/10 was deprecated by [RFC3879] in September 2004. Formerly a
  * Site-Local scoped address prefix.  These are never valid DNS servers,
  * but are known to be returned at least sometimes on Windows and Android.
  */
    {{ 0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00 },
     10}
  };

  size_t i;

  if (addr->family != AF_INET6) {
    return ARES_FALSE;
  }

  /* See if ipaddr matches any of the entries in the blacklist. */
  for (i = 0; i < sizeof(blacklist_v6) / sizeof(*blacklist_v6); i++) {
    if (ares_ipv6_subnet_matches(blacklist_v6[i].netbase,
                                 blacklist_v6[i].netmask,
                                 (const unsigned char *)&addr->addr.addr6)) {
      return ARES_TRUE;
    }
  }
  return ARES_FALSE;
}

/* Parse address and port in these formats, either ipv4 or ipv6 addresses
 * are allowed:
 *   ipaddr
 *   [ipaddr]
 *   [ipaddr]:port
 *
 * If a port is not specified, will set port to 0.
 *
 * Will fail if an IPv6 nameserver as detected by
 * ares_ipv6_server_blacklisted()
 *
 * Returns an error code on failure, else ARES_SUCCESS
 */
static ares_status_t parse_dnsaddrport(const char *str, size_t len,
                                       struct ares_addr *host,
                                       unsigned short   *port)
{
  char        ipaddr[INET6_ADDRSTRLEN] = "";
  char        ipport[6]                = "";
  size_t      mylen;
  const char *addr_start = NULL;
  const char *addr_end   = NULL;
  const char *port_start = NULL;
  const char *port_end   = NULL;

  /* Must start with [, hex digit or : */
  if (len == 0 || (*str != '[' && !isxdigit(*str) && *str != ':')) {
    return ARES_EBADSTR;
  }

  /* If it starts with a bracket, must end with a bracket */
  if (*str == '[') {
    const char *ptr;
    addr_start = str + 1;
    ptr        = memchr(addr_start, ']', len - 1);
    if (ptr == NULL) {
      return ARES_EBADSTR;
    }
    addr_end = ptr - 1;

    /* Try to pull off port */
    if ((size_t)(ptr - str) < len) {
      ptr++;
      if (*ptr != ':') {
        return ARES_EBADSTR;
      }

      /* Missing port number */
      if ((size_t)(ptr - str) == len) {
        return ARES_EBADSTR;
      }

      port_start = ptr + 1;
      port_end   = str + (len - 1);
    }
  } else {
    addr_start = str;
    addr_end   = str + (len - 1);
  }

  mylen = (size_t)(addr_end - addr_start) + 1;
  /* Larger than buffer with null term */
  if (mylen + 1 > sizeof(ipaddr)) {
    return ARES_EBADSTR;
  }

  memset(ipaddr, 0, sizeof(ipaddr));
  memcpy(ipaddr, addr_start, mylen);

  if (port_start) {
    mylen = (size_t)(port_end - port_start) + 1;
    /* Larger than buffer with null term */
    if (mylen + 1 > sizeof(ipport)) {
      return ARES_EBADSTR;
    }
    memset(ipport, 0, sizeof(ipport));
    memcpy(ipport, port_start, mylen);
  } else {
    snprintf(ipport, sizeof(ipport), "0");
  }

  /* Convert textual address to binary format. */
  if (ares_inet_pton(AF_INET, ipaddr, &host->addr.addr4) == 1) {
    host->family = AF_INET;
  } else if (ares_inet_pton(AF_INET6, ipaddr, &host->addr.addr6) == 1) {
    host->family = AF_INET6;
  } else {
    return ARES_EBADSTR;
  }

  *port = (unsigned short)atoi(ipport);
  return ARES_SUCCESS;
}

ares_status_t ares__sconfig_append(ares__llist_t         **sconfig,
                                   const struct ares_addr *addr,
                                   unsigned short          udp_port,
                                   unsigned short          tcp_port)
{
  ares_sconfig_t *s;
  ares_status_t   status;

  if (sconfig == NULL || addr == NULL) {
    return ARES_EFORMERR;
  }

  /* Silently skip blacklisted IPv6 servers. */
  if (ares_server_blacklisted(addr)) {
    return ARES_SUCCESS;
  }

  s = ares_malloc_zero(sizeof(*s));
  if (s == NULL) {
    return ARES_ENOMEM;
  }

  if (*sconfig == NULL) {
    *sconfig = ares__llist_create(ares_free);
    if (*sconfig == NULL) {
      status = ARES_ENOMEM;
      goto fail;
    }
  }

  memcpy(&s->addr, addr, sizeof(s->addr));
  s->udp_port = udp_port;
  s->tcp_port = tcp_port;

  if (ares__llist_insert_last(*sconfig, s) == NULL) {
    status = ARES_ENOMEM;
    goto fail;
  }

  return ARES_SUCCESS;

fail:
  ares_free(s);

  return status;
}

/* Add the IPv4 or IPv6 nameservers in str (separated by commas or spaces) to
 * the servers list, updating servers and nservers as required.
 *
 * If a nameserver is encapsulated in [ ] it may optionally include a port
 * suffix, e.g.:
 *    [127.0.0.1]:59591
 *
 * The extended format is required to support OpenBSD's resolv.conf format:
 *   https://man.openbsd.org/OpenBSD-5.1/resolv.conf.5
 * As well as MacOS libresolv that may include a non-default port number.
 *
 * This will silently ignore blacklisted IPv6 nameservers as detected by
 * ares_ipv6_server_blacklisted().
 *
 * Returns an error code on failure, else ARES_SUCCESS.
 */
ares_status_t ares__sconfig_append_fromstr(ares__llist_t **sconfig,
                                           const char     *str)
{
  struct ares_addr host;
  const char      *p;
  const char      *txtaddr;
  ares_status_t    status;

  /* On Windows, there may be more than one nameserver specified in the same
   * registry key, so we parse input as a space or comma separated list.
   */
  for (p = str; p;) {
    unsigned short port;

    /* Skip whitespace and commas. */
    while (*p && (ISSPACE(*p) || (*p == ','))) {
      p++;
    }
    if (!*p) {
      /* No more input, done. */
      break;
    }

    /* Pointer to start of IPv4 or IPv6 address part. */
    txtaddr = p;

    /* Advance past this address. */
    while (*p && !ISSPACE(*p) && (*p != ',')) {
      p++;
    }

    if (parse_dnsaddrport(txtaddr, (size_t)(p - txtaddr), &host, &port) !=
        ARES_SUCCESS) {
      continue;
    }

    status = ares__sconfig_append(sconfig, &host, port, port);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  return ARES_SUCCESS;
}

static unsigned short ares__sconfig_get_port(const ares_channel_t *channel,
                                             const ares_sconfig_t *s,
                                             ares_bool_t           is_tcp)
{
  unsigned short port = is_tcp ? s->tcp_port : s->udp_port;

  if (port == 0) {
    port = is_tcp ? channel->tcp_port : channel->udp_port;
  }

  if (port == 0) {
    port = 53;
  }

  return port;
}

static ares__slist_node_t *ares__server_find(ares_channel_t       *channel,
                                             const ares_sconfig_t *s)
{
  ares__slist_node_t *node;

  for (node = ares__slist_node_first(channel->servers); node != NULL;
       node = ares__slist_node_next(node)) {
    const struct server_state *server = ares__slist_node_val(node);

    if (!ares__addr_match(&server->addr, &s->addr)) {
      continue;
    }

    if (server->tcp_port != ares__sconfig_get_port(channel, s, ARES_TRUE)) {
      continue;
    }

    if (server->udp_port != ares__sconfig_get_port(channel, s, ARES_FALSE)) {
      continue;
    }

    return node;
  }
  return NULL;
}

static ares_bool_t ares__server_isdup(const ares_channel_t *channel,
                                      ares__llist_node_t   *s)
{
  /* Scan backwards to see if this is a duplicate */
  ares__llist_node_t   *prev;
  const ares_sconfig_t *server = ares__llist_node_val(s);

  for (prev = ares__llist_node_prev(s); prev != NULL;
       prev = ares__llist_node_prev(prev)) {
    const ares_sconfig_t *p = ares__llist_node_val(prev);

    if (!ares__addr_match(&server->addr, &p->addr)) {
      continue;
    }

    if (ares__sconfig_get_port(channel, server, ARES_TRUE) !=
        ares__sconfig_get_port(channel, p, ARES_TRUE)) {
      continue;
    }

    if (ares__sconfig_get_port(channel, server, ARES_FALSE) !=
        ares__sconfig_get_port(channel, p, ARES_FALSE)) {
      continue;
    }

    return ARES_TRUE;
  }

  return ARES_FALSE;
}

static ares_status_t ares__server_create(ares_channel_t       *channel,
                                         const ares_sconfig_t *sconfig,
                                         size_t                idx)
{
  ares_status_t        status;
  struct server_state *server = ares_malloc_zero(sizeof(*server));

  if (server == NULL) {
    return ARES_ENOMEM;
  }

  server->idx         = idx;
  server->channel     = channel;
  server->udp_port    = ares__sconfig_get_port(channel, sconfig, ARES_FALSE);
  server->tcp_port    = ares__sconfig_get_port(channel, sconfig, ARES_TRUE);
  server->addr.family = sconfig->addr.family;

  if (sconfig->addr.family == AF_INET) {
    memcpy(&server->addr.addr.addr4, &sconfig->addr.addr.addr4,
           sizeof(server->addr.addr.addr4));
  } else if (sconfig->addr.family == AF_INET6) {
    memcpy(&server->addr.addr.addr6, &sconfig->addr.addr.addr6,
           sizeof(server->addr.addr.addr6));
  }

  server->tcp_parser = ares__buf_create();
  if (server->tcp_parser == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  server->tcp_send = ares__buf_create();
  if (server->tcp_send == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  server->connections = ares__llist_create(NULL);
  if (server->connections == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  if (ares__slist_insert(channel->servers, server) == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  status = ARES_SUCCESS;

done:
  if (status != ARES_SUCCESS) {
    ares__destroy_server(server);
  }

  return status;
}

static ares_bool_t ares__server_in_newconfig(const struct server_state *server,
                                             ares__llist_t             *srvlist)
{
  ares__llist_node_t   *node;
  const ares_channel_t *channel = server->channel;

  for (node = ares__llist_node_first(srvlist); node != NULL;
       node = ares__llist_node_next(node)) {
    const ares_sconfig_t *s = ares__llist_node_val(node);

    if (!ares__addr_match(&server->addr, &s->addr)) {
      continue;
    }

    if (server->tcp_port != ares__sconfig_get_port(channel, s, ARES_TRUE)) {
      continue;
    }

    if (server->udp_port != ares__sconfig_get_port(channel, s, ARES_FALSE)) {
      continue;
    }

    return ARES_TRUE;
  }

  return ARES_FALSE;
}

static void ares__servers_remove_stale(ares_channel_t *channel,
                                       ares__llist_t  *srvlist)
{
  ares__slist_node_t *snode = ares__slist_node_first(channel->servers);

  while (snode != NULL) {
    ares__slist_node_t        *snext  = ares__slist_node_next(snode);
    const struct server_state *server = ares__slist_node_val(snode);
    if (!ares__server_in_newconfig(server, srvlist)) {
      /* This will clean up all server state via the destruction callback and
       * move any queries to new servers */
      ares__slist_node_destroy(snode);
    }
    snode = snext;
  }
}

static void ares__servers_trim_single(ares_channel_t *channel)
{
  while (ares__slist_len(channel->servers) > 1) {
    ares__slist_node_destroy(ares__slist_node_last(channel->servers));
  }
}

ares_status_t ares__servers_update(ares_channel_t *channel,
                                   ares__llist_t  *server_list,
                                   ares_bool_t     user_specified)
{
  ares__llist_node_t *node;
  size_t              idx = 0;
  ares_status_t       status;

  if (channel == NULL) {
    return ARES_EFORMERR;
  }

  ares__channel_lock(channel);

  /* NOTE: a NULL or zero entry server list is considered valid due to
   *       real-world people needing support for this for their test harnesses
   */

  /* Add new entries */
  for (node = ares__llist_node_first(server_list); node != NULL;
       node = ares__llist_node_next(node)) {
    const ares_sconfig_t *sconfig = ares__llist_node_val(node);
    ares__slist_node_t   *snode;

    /* Don't add duplicate servers! */
    if (ares__server_isdup(channel, node)) {
      continue;
    }

    snode = ares__server_find(channel, sconfig);
    if (snode != NULL) {
      struct server_state *server = ares__slist_node_val(snode);

      if (server->idx != idx) {
        server->idx = idx;
        /* Index changed, reinsert node, doesn't require any memory
         * allocations so can't fail. */
        ares__slist_node_reinsert(snode);
      }
    } else {
      status = ares__server_create(channel, sconfig, idx);
      if (status != ARES_SUCCESS) {
        goto done;
      }
    }

    idx++;
  }

  /* Remove any servers that don't exist in the current configuration */
  ares__servers_remove_stale(channel, server_list);

  /* Trim to one server if ARES_FLAG_PRIMARY is set. */
  if (channel->flags & ARES_FLAG_PRIMARY) {
    ares__servers_trim_single(channel);
  }

  if (user_specified) {
    /* Save servers as if they were passed in as an option */
    channel->optmask |= ARES_OPT_SERVERS;
  }

  /* Clear any cached query results */
  ares__qcache_flush(channel->qcache);

  status = ARES_SUCCESS;

done:
  ares__channel_unlock(channel);
  return status;
}

static ares_status_t
  ares_addr_node_to_server_config_llist(const struct ares_addr_node *servers,
                                        ares__llist_t              **llist)
{
  const struct ares_addr_node *node;
  ares__llist_t               *s;

  *llist = NULL;

  s = ares__llist_create(ares_free);
  if (s == NULL) {
    goto fail;
  }

  for (node = servers; node != NULL; node = node->next) {
    ares_sconfig_t *sconfig;

    /* Invalid entry */
    if (node->family != AF_INET && node->family != AF_INET6) {
      continue;
    }

    sconfig = ares_malloc_zero(sizeof(*sconfig));
    if (sconfig == NULL) {
      goto fail;
    }

    sconfig->addr.family = node->family;
    if (node->family == AF_INET) {
      memcpy(&sconfig->addr.addr.addr4, &node->addr.addr4,
             sizeof(sconfig->addr.addr.addr4));
    } else if (sconfig->addr.family == AF_INET6) {
      memcpy(&sconfig->addr.addr.addr6, &node->addr.addr6,
             sizeof(sconfig->addr.addr.addr6));
    }

    if (ares__llist_insert_last(s, sconfig) == NULL) {
      ares_free(sconfig);
      goto fail;
    }
  }

  *llist = s;
  return ARES_SUCCESS;

fail:
  ares__llist_destroy(s);
  return ARES_ENOMEM;
}

static ares_status_t ares_addr_port_node_to_server_config_llist(
  const struct ares_addr_port_node *servers, ares__llist_t **llist)
{
  const struct ares_addr_port_node *node;
  ares__llist_t                    *s;

  *llist = NULL;

  s = ares__llist_create(ares_free);
  if (s == NULL) {
    goto fail;
  }

  for (node = servers; node != NULL; node = node->next) {
    ares_sconfig_t *sconfig;

    /* Invalid entry */
    if (node->family != AF_INET && node->family != AF_INET6) {
      continue;
    }

    sconfig = ares_malloc_zero(sizeof(*sconfig));
    if (sconfig == NULL) {
      goto fail;
    }

    sconfig->addr.family = node->family;
    if (node->family == AF_INET) {
      memcpy(&sconfig->addr.addr.addr4, &node->addr.addr4,
             sizeof(sconfig->addr.addr.addr4));
    } else if (sconfig->addr.family == AF_INET6) {
      memcpy(&sconfig->addr.addr.addr6, &node->addr.addr6,
             sizeof(sconfig->addr.addr.addr6));
    }

    sconfig->tcp_port = (unsigned short)node->tcp_port;
    sconfig->udp_port = (unsigned short)node->udp_port;

    if (ares__llist_insert_last(s, sconfig) == NULL) {
      ares_free(sconfig);
      goto fail;
    }
  }

  *llist = s;
  return ARES_SUCCESS;

fail:
  ares__llist_destroy(s);
  return ARES_ENOMEM;
}

ares_status_t ares_in_addr_to_server_config_llist(const struct in_addr *servers,
                                                  size_t          nservers,
                                                  ares__llist_t **llist)
{
  size_t         i;
  ares__llist_t *s;

  *llist = NULL;

  s = ares__llist_create(ares_free);
  if (s == NULL) {
    goto fail;
  }

  for (i = 0; servers != NULL && i < nservers; i++) {
    ares_sconfig_t *sconfig;

    sconfig = ares_malloc_zero(sizeof(*sconfig));
    if (sconfig == NULL) {
      goto fail;
    }

    sconfig->addr.family = AF_INET;
    memcpy(&sconfig->addr.addr.addr4, &servers[i],
           sizeof(sconfig->addr.addr.addr4));

    if (ares__llist_insert_last(s, sconfig) == NULL) {
      goto fail;
    }
  }

  *llist = s;
  return ARES_SUCCESS;

fail:
  ares__llist_destroy(s);
  return ARES_ENOMEM;
}

int ares_get_servers(ares_channel_t *channel, struct ares_addr_node **servers)
{
  struct ares_addr_node *srvr_head = NULL;
  struct ares_addr_node *srvr_last = NULL;
  struct ares_addr_node *srvr_curr;
  ares_status_t          status = ARES_SUCCESS;
  ares__slist_node_t    *node;

  if (channel == NULL) {
    return ARES_ENODATA;
  }

  ares__channel_lock(channel);

  for (node = ares__slist_node_first(channel->servers); node != NULL;
       node = ares__slist_node_next(node)) {
    const struct server_state *server = ares__slist_node_val(node);

    /* Allocate storage for this server node appending it to the list */
    srvr_curr = ares_malloc_data(ARES_DATATYPE_ADDR_NODE);
    if (!srvr_curr) {
      status = ARES_ENOMEM;
      break;
    }
    if (srvr_last) {
      srvr_last->next = srvr_curr;
    } else {
      srvr_head = srvr_curr;
    }
    srvr_last = srvr_curr;

    /* Fill this server node data */
    srvr_curr->family = server->addr.family;
    if (srvr_curr->family == AF_INET) {
      memcpy(&srvr_curr->addr.addr4, &server->addr.addr.addr4,
             sizeof(srvr_curr->addr.addr4));
    } else {
      memcpy(&srvr_curr->addr.addr6, &server->addr.addr.addr6,
             sizeof(srvr_curr->addr.addr6));
    }
  }

  if (status != ARES_SUCCESS) {
    ares_free_data(srvr_head);
    srvr_head = NULL;
  }

  *servers = srvr_head;

  ares__channel_unlock(channel);

  return (int)status;
}

int ares_get_servers_ports(ares_channel_t              *channel,
                           struct ares_addr_port_node **servers)
{
  struct ares_addr_port_node *srvr_head = NULL;
  struct ares_addr_port_node *srvr_last = NULL;
  struct ares_addr_port_node *srvr_curr;
  ares_status_t               status = ARES_SUCCESS;
  ares__slist_node_t         *node;

  if (channel == NULL) {
    return ARES_ENODATA;
  }

  ares__channel_lock(channel);

  for (node = ares__slist_node_first(channel->servers); node != NULL;
       node = ares__slist_node_next(node)) {
    const struct server_state *server = ares__slist_node_val(node);

    /* Allocate storage for this server node appending it to the list */
    srvr_curr = ares_malloc_data(ARES_DATATYPE_ADDR_PORT_NODE);
    if (!srvr_curr) {
      status = ARES_ENOMEM;
      break;
    }
    if (srvr_last) {
      srvr_last->next = srvr_curr;
    } else {
      srvr_head = srvr_curr;
    }
    srvr_last = srvr_curr;

    /* Fill this server node data */
    srvr_curr->family   = server->addr.family;
    srvr_curr->udp_port = server->udp_port;
    srvr_curr->tcp_port = server->tcp_port;

    if (srvr_curr->family == AF_INET) {
      memcpy(&srvr_curr->addr.addr4, &server->addr.addr.addr4,
             sizeof(srvr_curr->addr.addr4));
    } else {
      memcpy(&srvr_curr->addr.addr6, &server->addr.addr.addr6,
             sizeof(srvr_curr->addr.addr6));
    }
  }

  if (status != ARES_SUCCESS) {
    ares_free_data(srvr_head);
    srvr_head = NULL;
  }

  *servers = srvr_head;

  ares__channel_unlock(channel);
  return (int)status;
}

int ares_set_servers(ares_channel_t              *channel,
                     const struct ares_addr_node *servers)
{
  ares__llist_t *slist;
  ares_status_t  status;

  if (channel == NULL) {
    return ARES_ENODATA;
  }

  status = ares_addr_node_to_server_config_llist(servers, &slist);
  if (status != ARES_SUCCESS) {
    return (int)status;
  }

  /* NOTE: lock is in ares__servers_update() */
  status = ares__servers_update(channel, slist, ARES_TRUE);

  ares__llist_destroy(slist);

  return (int)status;
}

int ares_set_servers_ports(ares_channel_t                   *channel,
                           const struct ares_addr_port_node *servers)
{
  ares__llist_t *slist;
  ares_status_t  status;

  if (channel == NULL) {
    return ARES_ENODATA;
  }

  status = ares_addr_port_node_to_server_config_llist(servers, &slist);
  if (status != ARES_SUCCESS) {
    return (int)status;
  }

  /* NOTE: lock is in ares__servers_update() */
  status = ares__servers_update(channel, slist, ARES_TRUE);

  ares__llist_destroy(slist);

  return (int)status;
}

/* Incoming string format: host[:port][,host[:port]]... */
/* IPv6 addresses with ports require square brackets [fe80::1]:53 */
static ares_status_t set_servers_csv(ares_channel_t *channel, const char *_csv,
                                     int use_port)
{
  size_t                      i;
  char                       *csv = NULL;
  char                       *ptr;
  const char                 *start_host;
  int                         cc      = 0;
  ares_status_t               status  = ARES_SUCCESS;
  struct ares_addr_port_node *servers = NULL;
  struct ares_addr_port_node *last    = NULL;

  if (ares_library_initialized() != ARES_SUCCESS) {
    return ARES_ENOTINITIALIZED; /* LCOV_EXCL_LINE: n/a on non-WinSock */
  }

  if (!channel) {
    return ARES_ENODATA;
  }

  /* NOTE: lock is in ares__servers_update() */

  i = ares_strlen(_csv);
  if (i == 0) {
    /* blank all servers */
    return (ares_status_t)ares_set_servers_ports(channel, NULL);
  }

  csv = ares_malloc(i + 2);
  if (!csv) {
    return ARES_ENOMEM;
  }

  ares_strcpy(csv, _csv, i + 2);
  if (csv[i - 1] != ',') { /* make parsing easier by ensuring ending ',' */
    csv[i]     = ',';
    csv[i + 1] = 0;
  }

  start_host = csv;
  for (ptr = csv; *ptr; ptr++) {
    if (*ptr == ':') {
      /* count colons to determine if we have an IPv6 number or IPv4 with
         port */
      cc++;
    } else if (*ptr == '[') {
      /* move start_host if an open square bracket is found wrapping an IPv6
         address */
      start_host = ptr + 1;
    } else if (*ptr == ',') {
      char                       *pp   = ptr - 1;
      char                       *p    = ptr;
      int                         port = 0;
      struct in_addr              in4;
      struct ares_in6_addr        in6;
      struct ares_addr_port_node *s = NULL;

      *ptr = 0; /* null terminate host:port string */
      /* Got an entry..see if the port was specified. */
      if (cc > 0) {
        while (pp > start_host) {
          /* a single close square bracket followed by a colon, ']:' indicates
             an IPv6 address with port */
          if ((*pp == ']') && (*p == ':')) {
            break; /* found port */
          }
          /* a single colon, ':' indicates an IPv4 address with port */
          if ((*pp == ':') && (cc == 1)) {
            break; /* found port */
          }
          if (!(ISDIGIT(*pp) || (*pp == ':'))) {
            /* Found end of digits before we found :, so wasn't a port */
            /* must allow ':' for IPv6 case of ']:' indicates we found a port */
            pp = p = ptr;
            break;
          }
          pp--;
          p--;
        }
        if ((pp != start_host) && ((pp + 1) < ptr)) {
          /* Found it. Parse over the port number */
          /* when an IPv6 address is wrapped with square brackets the port
             starts at pp + 2 */
          if (*pp == ']') {
            p++; /* move p before ':' */
          }
          /* p will point to the start of the port */
          port = (int)strtol(p, NULL, 10);
          *pp  = 0; /* null terminate host */
        }
      }
      /* resolve host, try ipv4 first, rslt is in network byte order */
      if (!ares_inet_pton(AF_INET, start_host, &in4)) {
        /* Ok, try IPv6 then */
        if (!ares_inet_pton(AF_INET6, start_host, &in6)) {
          status = ARES_EBADSTR;
          goto out;
        }
        /* was ipv6, add new server */
        s = ares_malloc(sizeof(*s));
        if (!s) {
          status = ARES_ENOMEM;
          goto out;
        }
        s->family = AF_INET6;
        memcpy(&s->addr, &in6, sizeof(struct ares_in6_addr));
      } else {
        /* was ipv4, add new server */
        s = ares_malloc(sizeof(*s));
        if (!s) {
          status = ARES_ENOMEM;
          goto out;
        }
        s->family = AF_INET;
        memcpy(&s->addr, &in4, sizeof(struct in_addr));
      }
      if (s) {
        s->udp_port = use_port ? port : 0;
        s->tcp_port = s->udp_port;
        s->next     = NULL;
        if (last) {
          last->next = s;
          /* need to move last to maintain the linked list */
          last = last->next;
        } else {
          servers = s;
          last    = s;
        }
      }

      /* Set up for next one */
      start_host = ptr + 1;
      cc         = 0;
    }
  }

  status = (ares_status_t)ares_set_servers_ports(channel, servers);

out:
  if (csv) {
    ares_free(csv);
  }
  while (servers) {
    struct ares_addr_port_node *s = servers;
    servers                       = servers->next;
    ares_free(s);
  }

  return status;
}

int ares_set_servers_csv(ares_channel_t *channel, const char *_csv)
{
  /* NOTE: lock is in ares__servers_update() */
  return (int)set_servers_csv(channel, _csv, ARES_FALSE);
}

int ares_set_servers_ports_csv(ares_channel_t *channel, const char *_csv)
{
  /* NOTE: lock is in ares__servers_update() */
  return (int)set_servers_csv(channel, _csv, ARES_TRUE);
}

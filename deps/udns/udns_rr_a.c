/* $Id: udns_rr_a.c,v 1.16 2007/01/09 04:44:51 mjt Exp $
   parse/query A/AAAA IN records

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

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifndef WINDOWS
# include <sys/types.h>
# include <netinet/in.h>
#endif
#include "udns.h"

/* here, we use common routine to parse both IPv4 and IPv6 addresses.
 */

/* this structure should match dns_rr_a[46] */
struct dns_rr_a {
  dns_rr_common(dnsa);
  unsigned char *dnsa_addr;
};

static int
dns_parse_a(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
            void **result, unsigned dsize) {
  struct dns_rr_a *ret;
  struct dns_parse p;
  struct dns_rr rr;
  int r;

  /* first, validate and count number of addresses */
  dns_initparse(&p, qdn, pkt, cur, end);
  while((r = dns_nextrr(&p, &rr)) > 0)
    if (rr.dnsrr_dsz != dsize)
      return DNS_E_PROTOCOL;
  if (r < 0)
    return DNS_E_PROTOCOL;
  else if (!p.dnsp_nrr)
    return DNS_E_NODATA;

  ret = malloc(sizeof(*ret) + dsize * p.dnsp_nrr + dns_stdrr_size(&p));
  if (!ret)
    return DNS_E_NOMEM;

  ret->dnsa_nrr = p.dnsp_nrr;
  ret->dnsa_addr = (unsigned char*)(ret+1);

  /* copy the RRs */
  for (dns_rewind(&p, qdn), r = 0; dns_nextrr(&p, &rr); ++r)
    memcpy(ret->dnsa_addr + dsize * r, rr.dnsrr_dptr, dsize);

  dns_stdrr_finish((struct dns_rr_null *)ret,
                   (char *)(ret->dnsa_addr + dsize * p.dnsp_nrr), &p);
  *result = ret;
  return 0;
}

int
dns_parse_a4(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
             void **result) {
#ifdef AF_INET
  assert(sizeof(struct in_addr) == 4);
#endif
  assert(dns_get16(cur+2) == DNS_C_IN && dns_get16(cur+0) == DNS_T_A);
  return dns_parse_a(qdn, pkt, cur, end, result, 4);
}

struct dns_query *
dns_submit_a4(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_a4_fn *cbck, void *data) {
  return
    dns_submit_p(ctx, name, DNS_C_IN, DNS_T_A, flags,
                 dns_parse_a4, (dns_query_fn*)cbck, data);
}

struct dns_rr_a4 *
dns_resolve_a4(struct dns_ctx *ctx, const char *name, int flags) {
  return (struct dns_rr_a4 *)
    dns_resolve_p(ctx, name, DNS_C_IN, DNS_T_A, flags, dns_parse_a4);
}

int
dns_parse_a6(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
             void **result) {
#ifdef AF_INET6
  assert(sizeof(struct in6_addr) == 16);
#endif
  assert(dns_get16(cur+2) == DNS_C_IN && dns_get16(cur+0) == DNS_T_AAAA);
  return dns_parse_a(qdn, pkt, cur, end, result, 16);
}

struct dns_query *
dns_submit_a6(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_a6_fn *cbck, void *data) {
  return
    dns_submit_p(ctx, name, DNS_C_IN, DNS_T_AAAA, flags,
                 dns_parse_a6, (dns_query_fn*)cbck, data);
}

struct dns_rr_a6 *
dns_resolve_a6(struct dns_ctx *ctx, const char *name, int flags) {
  return (struct dns_rr_a6 *)
    dns_resolve_p(ctx, name, DNS_C_IN, DNS_T_AAAA, flags, dns_parse_a6);
}

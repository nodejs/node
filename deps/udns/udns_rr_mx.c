/* $Id: udns_rr_mx.c,v 1.13 2005/04/20 06:44:34 mjt Exp $
   parse/query MX IN records

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
#include "udns.h"

int
dns_parse_mx(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
             void **result) {
  struct dns_rr_mx *ret;
  struct dns_parse p;
  struct dns_rr rr;
  int r, l;
  char *sp;
  dnsc_t mx[DNS_MAXDN];

  assert(dns_get16(cur+2) == DNS_C_IN && dns_get16(cur+0) == DNS_T_MX);

  /* first, validate the answer and count size of the result */
  l = 0;
  dns_initparse(&p, qdn, pkt, cur, end);
  while((r = dns_nextrr(&p, &rr)) > 0) {
    cur = rr.dnsrr_dptr + 2;
    r = dns_getdn(pkt, &cur, end, mx, sizeof(mx));
    if (r <= 0 || cur != rr.dnsrr_dend)
      return DNS_E_PROTOCOL;
    l += dns_dntop_size(mx);
  }
  if (r < 0)
    return DNS_E_PROTOCOL;
  if (!p.dnsp_nrr)
    return DNS_E_NODATA;

  /* next, allocate and set up result */
  l += dns_stdrr_size(&p);
  ret = malloc(sizeof(*ret) + sizeof(struct dns_mx) * p.dnsp_nrr + l);
  if (!ret)
    return DNS_E_NOMEM;
  ret->dnsmx_nrr = p.dnsp_nrr;
  ret->dnsmx_mx = (struct dns_mx *)(ret+1);

  /* and 3rd, fill in result, finally */
  sp = (char*)(ret->dnsmx_mx + p.dnsp_nrr);
  for (dns_rewind(&p, qdn), r = 0; dns_nextrr(&p, &rr); ++r) {
    ret->dnsmx_mx[r].name = sp;
    cur = rr.dnsrr_dptr;
    ret->dnsmx_mx[r].priority = dns_get16(cur);
    cur += 2;
    dns_getdn(pkt, &cur, end, mx, sizeof(mx));
    sp += dns_dntop(mx, sp, DNS_MAXNAME);
  }
  dns_stdrr_finish((struct dns_rr_null *)ret, sp, &p);
  *result = ret;
  return 0;
}

struct dns_query *
dns_submit_mx(struct dns_ctx *ctx, const char *name, int flags,
              dns_query_mx_fn *cbck, void *data) {
  return
    dns_submit_p(ctx, name, DNS_C_IN, DNS_T_MX, flags,
                 dns_parse_mx, (dns_query_fn *)cbck, data);
}

struct dns_rr_mx *
dns_resolve_mx(struct dns_ctx *ctx, const char *name, int flags) {
  return (struct dns_rr_mx *)
    dns_resolve_p(ctx, name, DNS_C_IN, DNS_T_MX, flags, dns_parse_mx);
}

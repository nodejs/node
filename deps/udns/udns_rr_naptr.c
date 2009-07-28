/* $Id: udns_rr_naptr.c,v 1.1 2006/11/28 22:58:04 mjt Exp $
   parse/query NAPTR IN records

   Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
   Copyright (C) 2006  Mikael Magnusson <mikma@users.sourceforge.net>
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

/* Get a single string for NAPTR record, pretty much like a DN label.
 * String length is in first byte in *cur, so it can't be >255.
 */
static int dns_getstr(dnscc_t **cur, dnscc_t *ep, char *buf)
{
  unsigned l;
  dnscc_t *cp = *cur;

  l = *cp++;
  if (cp + l > ep)
    return DNS_E_PROTOCOL;
  if (buf) {
    memcpy(buf, cp, l);
    buf[l] = '\0';
  }
  cp += l;

  *cur = cp;
  return l + 1;
}

int
dns_parse_naptr(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
                void **result) {
  struct dns_rr_naptr *ret;
  struct dns_parse p;
  struct dns_rr rr;
  int r, l;
  char *sp;
  dnsc_t dn[DNS_MAXDN];

  assert(dns_get16(cur+2) == DNS_C_IN && dns_get16(cur+0) == DNS_T_NAPTR);

  /* first, validate the answer and count size of the result */
  l = 0;
  dns_initparse(&p, qdn, pkt, cur, end);
  while((r = dns_nextrr(&p, &rr)) > 0) {
    int i;
    dnscc_t *ep = rr.dnsrr_dend;

    /* first 4 bytes: order & preference */
    cur = rr.dnsrr_dptr + 4;

    /* flags, services and regexp */
    for (i = 0; i < 3; i++) {
      r = dns_getstr(&cur, ep, NULL);
      if (r < 0)
        return r;
      l += r;
    }
    /* replacement */
    r = dns_getdn(pkt, &cur, end, dn, sizeof(dn));
    if (r <= 0 || cur != rr.dnsrr_dend)
      return DNS_E_PROTOCOL;
    l += dns_dntop_size(dn);
  }
  if (r < 0)
    return DNS_E_PROTOCOL;
  if (!p.dnsp_nrr)
    return DNS_E_NODATA;

  /* next, allocate and set up result */
  l += dns_stdrr_size(&p);
  ret = malloc(sizeof(*ret) + sizeof(struct dns_naptr) * p.dnsp_nrr + l);
  if (!ret)
    return DNS_E_NOMEM;
  ret->dnsnaptr_nrr = p.dnsp_nrr;
  ret->dnsnaptr_naptr = (struct dns_naptr *)(ret+1);

  /* and 3rd, fill in result, finally */
  sp = (char*)(&ret->dnsnaptr_naptr[p.dnsp_nrr]);
  for (dns_rewind(&p, qdn), r = 0; dns_nextrr(&p, &rr); ++r) {
    cur = rr.dnsrr_dptr;
    ret->dnsnaptr_naptr[r].order = dns_get16(cur); cur += 2;
    ret->dnsnaptr_naptr[r].preference = dns_get16(cur); cur += 2;
    sp += dns_getstr(&cur, end, (ret->dnsnaptr_naptr[r].flags = sp));
    sp += dns_getstr(&cur, end, (ret->dnsnaptr_naptr[r].service = sp));
    sp += dns_getstr(&cur, end, (ret->dnsnaptr_naptr[r].regexp = sp));
    dns_getdn(pkt, &cur, end, dn, sizeof(dn));
    sp += dns_dntop(dn, (ret->dnsnaptr_naptr[r].replacement = sp), DNS_MAXNAME);
  }
  dns_stdrr_finish((struct dns_rr_null *)ret, sp, &p);
  *result = ret;
  return 0;
}

struct dns_query *
dns_submit_naptr(struct dns_ctx *ctx, const char *name, int flags,
                 dns_query_naptr_fn *cbck, void *data) {
  return
    dns_submit_p(ctx, name, DNS_C_IN, DNS_T_NAPTR, flags,
                 dns_parse_naptr, (dns_query_fn *)cbck, data);
}

struct dns_rr_naptr *
dns_resolve_naptr(struct dns_ctx *ctx, const char *name, int flags) {
  return (struct dns_rr_naptr *)
    dns_resolve_p(ctx, name, DNS_C_IN, DNS_T_NAPTR, flags, dns_parse_naptr);
}

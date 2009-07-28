/* $Id: udns_rr_txt.c,v 1.15 2006/11/28 22:45:20 mjt Exp $
   parse/query TXT records

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
dns_parse_txt(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
              void **result) {
  struct dns_rr_txt *ret;
  struct dns_parse p;
  struct dns_rr rr;
  int r, l;
  dnsc_t *sp;
  dnscc_t *cp, *ep;

  assert(dns_get16(cur+0) == DNS_T_TXT);

  /* first, validate the answer and count size of the result */
  l = 0;
  dns_initparse(&p, qdn, pkt, cur, end);
  while((r = dns_nextrr(&p, &rr)) > 0) {
    cp = rr.dnsrr_dptr; ep = rr.dnsrr_dend;
    while(cp < ep) {
      r = *cp++;
      if (cp + r > ep)
        return DNS_E_PROTOCOL;
      l += r;
      cp += r;
    }
  }
  if (r < 0)
    return DNS_E_PROTOCOL;
  if (!p.dnsp_nrr)
    return DNS_E_NODATA;

  /* next, allocate and set up result */
  l +=  (sizeof(struct dns_txt) + 1) * p.dnsp_nrr + dns_stdrr_size(&p);
  ret = malloc(sizeof(*ret) + l);
  if (!ret)
    return DNS_E_NOMEM;
  ret->dnstxt_nrr = p.dnsp_nrr;
  ret->dnstxt_txt = (struct dns_txt *)(ret+1);

  /* and 3rd, fill in result, finally */
  sp = (dnsc_t*)(ret->dnstxt_txt + p.dnsp_nrr);
  for(dns_rewind(&p, qdn), r = 0; dns_nextrr(&p, &rr) > 0; ++r) {
    ret->dnstxt_txt[r].txt = sp;
    cp = rr.dnsrr_dptr; ep = rr.dnsrr_dend;
    while(cp < ep) {
      l = *cp++;
      memcpy(sp, cp, l);
      sp += l;
      cp += l;
    }
    ret->dnstxt_txt[r].len = sp - ret->dnstxt_txt[r].txt;
    *sp++ = '\0';
  }
  dns_stdrr_finish((struct dns_rr_null *)ret, (char*)sp, &p);
  *result = ret;
  return 0;
}

struct dns_query *
dns_submit_txt(struct dns_ctx *ctx, const char *name, int qcls, int flags,
               dns_query_txt_fn *cbck, void *data) {
  return
    dns_submit_p(ctx, name, qcls, DNS_T_TXT, flags,
                 dns_parse_txt, (dns_query_fn *)cbck, data);
}

struct dns_rr_txt *
dns_resolve_txt(struct dns_ctx *ctx, const char *name, int qcls, int flags) {
  return (struct dns_rr_txt *)
    dns_resolve_p(ctx, name, qcls, DNS_T_TXT, flags, dns_parse_txt);
}

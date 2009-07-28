/* $Id: udns_parse.c,v 1.14 2005/09/12 10:55:21 mjt Exp $
   raw DNS packet parsing routines

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
#include <assert.h>
#include "udns.h"

dnscc_t *dns_skipdn(dnscc_t *cur, dnscc_t *end) {
  unsigned c;
  for(;;) {
    if (cur >= end)
      return NULL;
    c = *cur++;
    if (!c)
      return cur;
    if (c & 192)		/* jump */
      return cur + 1 >= end ? NULL : cur + 1;
    cur += c;
  }
}

int
dns_getdn(dnscc_t *pkt, dnscc_t **cur, dnscc_t *end,
          register dnsc_t *dn, unsigned dnsiz) {
  unsigned c;
  dnscc_t *pp = *cur;		/* current packet pointer */
  dnsc_t *dp = dn;		/* current dn pointer */
  dnsc_t *const de		/* end of the DN dest */
       = dn + (dnsiz < DNS_MAXDN ? dnsiz : DNS_MAXDN);
  dnscc_t *jump = NULL;		/* ptr after first jump if any */
  unsigned loop = 100;		/* jump loop counter */

  for(;;) {		/* loop by labels */
    if (pp >= end)		/* reached end of packet? */
      return -1;
    c = *pp++;			/* length of the label */
    if (!c) {			/* empty label: terminate */
      if (dn >= de)		/* can't fit terminator */
        goto noroom;
      *dp++ = 0;
      /* return next pos: either after the first jump or current */
      *cur = jump ? jump : pp;
      return dp - dn;
    }
    if (c & 192) {		/* jump */
      if (pp >= end)		/* eop instead of jump pos */
        return -1;
      if (!jump) jump = pp + 1;	/* remember first jump */
      else if (!--loop) return -1; /* too many jumps */
      c = ((c & ~192) << 8) | *pp; /* new pos */
      if (c < DNS_HSIZE)	/* don't allow jump into the header */
        return -1;
      pp = pkt + c;
      continue;
    }
    if (c > DNS_MAXLABEL)	/* too long label? */
      return -1;
    if (pp + c > end)		/* label does not fit in packet? */
      return -1;
    if (dp + c + 1 > de)	/* if enouth room for the label */
      goto noroom;
    *dp++ = c;			/* label length */
    memcpy(dp, pp, c);		/* and the label itself */
    dp += c;
    pp += c;			/* advance to the next label */
  }
noroom:
  return dnsiz < DNS_MAXDN ? 0 : -1;
}

void dns_rewind(struct dns_parse *p, dnscc_t *qdn) {
  p->dnsp_qdn = qdn;
  p->dnsp_cur = p->dnsp_ans;
  p->dnsp_rrl = dns_numan(p->dnsp_pkt);
  p->dnsp_ttl = 0xffffffffu;
  p->dnsp_nrr = 0;
}

void
dns_initparse(struct dns_parse *p, dnscc_t *qdn,
              dnscc_t *pkt, dnscc_t *cur, dnscc_t *end) {
  p->dnsp_pkt = pkt;
  p->dnsp_end = end;
  p->dnsp_rrl = dns_numan(pkt);
  p->dnsp_qdn = qdn;
  assert(cur + 4 <= end);
  if ((p->dnsp_qtyp = dns_get16(cur+0)) == DNS_T_ANY) p->dnsp_qtyp = 0;
  if ((p->dnsp_qcls = dns_get16(cur+2)) == DNS_C_ANY) p->dnsp_qcls = 0;
  p->dnsp_cur = p->dnsp_ans = cur + 4;
  p->dnsp_ttl = 0xffffffffu;
  p->dnsp_nrr = 0;
}

int dns_nextrr(struct dns_parse *p, struct dns_rr *rr) {
  dnscc_t *cur = p->dnsp_cur;
  while(p->dnsp_rrl > 0) {
    --p->dnsp_rrl;
    if (dns_getdn(p->dnsp_pkt, &cur, p->dnsp_end,
                  rr->dnsrr_dn, sizeof(rr->dnsrr_dn)) <= 0)
      return -1;
    if (cur + 10 > p->dnsp_end)
      return -1;
    rr->dnsrr_typ = dns_get16(cur);
    rr->dnsrr_cls = dns_get16(cur+2);
    rr->dnsrr_ttl = dns_get32(cur+4);
    rr->dnsrr_dsz = dns_get16(cur+8);
    rr->dnsrr_dptr = cur = cur + 10;
    rr->dnsrr_dend = cur = cur + rr->dnsrr_dsz;
    if (cur > p->dnsp_end)
      return -1;
    if (p->dnsp_qdn && !dns_dnequal(p->dnsp_qdn, rr->dnsrr_dn))
      continue;
    if ((!p->dnsp_qcls || p->dnsp_qcls == rr->dnsrr_cls) &&
        (!p->dnsp_qtyp || p->dnsp_qtyp == rr->dnsrr_typ)) {
      p->dnsp_cur = cur;
      ++p->dnsp_nrr;
      if (p->dnsp_ttl > rr->dnsrr_ttl) p->dnsp_ttl = rr->dnsrr_ttl;
      return 1;
    }
    if (p->dnsp_qdn && rr->dnsrr_typ == DNS_T_CNAME && !p->dnsp_nrr) {
      if (dns_getdn(p->dnsp_pkt, &rr->dnsrr_dptr, p->dnsp_end,
                    p->dnsp_dnbuf, sizeof(p->dnsp_dnbuf)) <= 0 ||
          rr->dnsrr_dptr != rr->dnsrr_dend)
        return -1;
      p->dnsp_qdn = p->dnsp_dnbuf;
      if (p->dnsp_ttl > rr->dnsrr_ttl) p->dnsp_ttl = rr->dnsrr_ttl;
    }
  }
  p->dnsp_cur = cur;
  return 0;
}

int dns_stdrr_size(const struct dns_parse *p) {
  return
    dns_dntop_size(p->dnsp_qdn) +
    (p->dnsp_qdn == dns_payload(p->dnsp_pkt) ? 0 :
     dns_dntop_size(dns_payload(p->dnsp_pkt)));
}

void *dns_stdrr_finish(struct dns_rr_null *ret, char *cp,
                       const struct dns_parse *p) {
  cp += dns_dntop(p->dnsp_qdn, (ret->dnsn_cname = cp), DNS_MAXNAME);
  if (p->dnsp_qdn == dns_payload(p->dnsp_pkt))
    ret->dnsn_qname = ret->dnsn_cname;
  else
    dns_dntop(dns_payload(p->dnsp_pkt), (ret->dnsn_qname = cp), DNS_MAXNAME);
  ret->dnsn_ttl = p->dnsp_ttl;
  return ret;
}

/* $Id: udns_rr_srv.c,v 1.2 2005/09/12 12:26:22 mjt Exp $
   parse/query SRV IN (rfc2782) records

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

   Copyright 2005 Thadeu Lima de Souza Cascardo <cascardo@minaslivre.org>

   2005-09-11:
   Changed MX parser file into a SRV parser file

 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "udns.h"

int
dns_parse_srv(dnscc_t *qdn, dnscc_t *pkt, dnscc_t *cur, dnscc_t *end,
              void **result) {
  struct dns_rr_srv *ret;
  struct dns_parse p;
  struct dns_rr rr;
  int r, l;
  char *sp;
  dnsc_t srv[DNS_MAXDN];

  assert(dns_get16(cur+2) == DNS_C_IN && dns_get16(cur+0) == DNS_T_SRV);

  /* first, validate the answer and count size of the result */
  l = 0;
  dns_initparse(&p, qdn, pkt, cur, end);
  while((r = dns_nextrr(&p, &rr)) > 0) {
    cur = rr.dnsrr_dptr + 6;
    r = dns_getdn(pkt, &cur, end, srv, sizeof(srv));
    if (r <= 0 || cur != rr.dnsrr_dend)
      return DNS_E_PROTOCOL;
    l += dns_dntop_size(srv);
  }
  if (r < 0)
    return DNS_E_PROTOCOL;
  if (!p.dnsp_nrr)
    return DNS_E_NODATA;

  /* next, allocate and set up result */
  l += dns_stdrr_size(&p);
  ret = malloc(sizeof(*ret) + sizeof(struct dns_srv) * p.dnsp_nrr + l);
  if (!ret)
    return DNS_E_NOMEM;
  ret->dnssrv_nrr = p.dnsp_nrr;
  ret->dnssrv_srv = (struct dns_srv *)(ret+1);

  /* and 3rd, fill in result, finally */
  sp = (char*)(ret->dnssrv_srv + p.dnsp_nrr);
  for (dns_rewind(&p, qdn), r = 0; dns_nextrr(&p, &rr); ++r) {
    ret->dnssrv_srv[r].name = sp;
    cur = rr.dnsrr_dptr;
    ret->dnssrv_srv[r].priority = dns_get16(cur);
    ret->dnssrv_srv[r].weight = dns_get16(cur+2);
    ret->dnssrv_srv[r].port = dns_get16(cur+4);
    cur += 6;
    dns_getdn(pkt, &cur, end, srv, sizeof(srv));
    sp += dns_dntop(srv, sp, DNS_MAXNAME);
  }
  dns_stdrr_finish((struct dns_rr_null *)ret, sp, &p);
  *result = ret;
  return 0;
}

/* Add a single service or proto name prepending an undescore (_),
 * according to rfc2782 rules.
 * Return 0 or the label length.
 * Routing assumes dn holds enouth space for a single DN label. */
static unsigned add_sname(dnsc_t *dn, const char *sn) {
  unsigned l;
  l = dns_ptodn(sn, 0, dn + 1, DNS_MAXLABEL-1, NULL);
  if (l <= 1 || l - 2 != dn[1])
    /* Should we really check if sn is exactly one label?  Do we care? */
    return 0;
  dn[0] = l - 1;
  dn[1] = '_';
  return l;
}

/* Construct a domain name for SRV query from the given name, service and
 * protocol (service may be NULL in which case protocol isn't used).
 * Return negative value on error (malformed query),
 * or addition query flag(s) to use.
 */
static int
build_srv_dn(dnsc_t *dn, const char *name, const char *srv, const char *proto)
{
  unsigned p = 0, l;
  int isabs;
  if (srv) {
    l = add_sname(dn + p, srv);
    if (!l)
      return -1;
    p += l;
    l = add_sname(dn + p, proto);
    if (!l)
      return -1;
    p += l;
  }
  l = dns_ptodn(name, 0, dn + p, DNS_MAXDN - p, &isabs);
  if (!l)
    return -1;
  return isabs ? DNS_NOSRCH : 0;
}

struct dns_query *
dns_submit_srv(struct dns_ctx *ctx,
               const char *name, const char *srv, const char *proto,
               int flags, dns_query_srv_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  int r = build_srv_dn(dn, name, srv, proto);
  if (r < 0) {
    dns_setstatus (ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_SRV, flags | r,
                  dns_parse_srv, (dns_query_fn *)cbck, data);
}

struct dns_rr_srv *
dns_resolve_srv(struct dns_ctx *ctx,
                const char *name, const char *srv, const char *proto, int flags)
{
  dnsc_t dn[DNS_MAXDN];
  int r = build_srv_dn(dn, name, srv, proto);
  if (r < 0) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return (struct dns_rr_srv *)
    dns_resolve_dn(ctx, dn, DNS_C_IN, DNS_T_SRV, flags | r, dns_parse_srv);
}

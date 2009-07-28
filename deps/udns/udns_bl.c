/* $Id: udns_bl.c,v 1.10 2005/09/12 10:55:21 mjt Exp $
   DNSBL stuff

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

#include "udns.h"
#ifndef NULL
# define NULL 0
#endif

struct dns_query *
dns_submit_a4dnsbl(struct dns_ctx *ctx,
                   const struct in_addr *addr, const char *dnsbl,
                   dns_query_a4_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (dns_a4ptodn(addr, dnsbl, dn, sizeof(dn)) <= 0) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_A, DNS_NOSRCH,
                  dns_parse_a4, (dns_query_fn*)cbck, data);
}

struct dns_query *
dns_submit_a4dnsbl_txt(struct dns_ctx *ctx,
                       const struct in_addr *addr, const char *dnsbl,
                       dns_query_txt_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (dns_a4ptodn(addr, dnsbl, dn, sizeof(dn)) <= 0) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_TXT, DNS_NOSRCH,
                  dns_parse_txt, (dns_query_fn*)cbck, data);
}

struct dns_rr_a4 *
dns_resolve_a4dnsbl(struct dns_ctx *ctx,
                    const struct in_addr *addr, const char *dnsbl) {
  return (struct dns_rr_a4 *)
    dns_resolve(ctx, dns_submit_a4dnsbl(ctx, addr, dnsbl, 0, 0));
}

struct dns_rr_txt *
dns_resolve_a4dnsbl_txt(struct dns_ctx *ctx,
                        const struct in_addr *addr, const char *dnsbl) {
  return (struct dns_rr_txt *)
    dns_resolve(ctx, dns_submit_a4dnsbl_txt(ctx, addr, dnsbl, 0, 0));
}


struct dns_query *
dns_submit_a6dnsbl(struct dns_ctx *ctx,
                   const struct in6_addr *addr, const char *dnsbl,
                   dns_query_a4_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (dns_a6ptodn(addr, dnsbl, dn, sizeof(dn)) <= 0) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_A, DNS_NOSRCH,
                  dns_parse_a4, (dns_query_fn*)cbck, data);
}

struct dns_query *
dns_submit_a6dnsbl_txt(struct dns_ctx *ctx,
                       const struct in6_addr *addr, const char *dnsbl,
                       dns_query_txt_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (dns_a6ptodn(addr, dnsbl, dn, sizeof(dn)) <= 0) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_TXT, DNS_NOSRCH,
                  dns_parse_txt, (dns_query_fn*)cbck, data);
}

struct dns_rr_a4 *
dns_resolve_a6dnsbl(struct dns_ctx *ctx,
                    const struct in6_addr *addr, const char *dnsbl) {
  return (struct dns_rr_a4 *)
    dns_resolve(ctx, dns_submit_a6dnsbl(ctx, addr, dnsbl, 0, 0));
}

struct dns_rr_txt *
dns_resolve_a6dnsbl_txt(struct dns_ctx *ctx,
                        const struct in6_addr *addr, const char *dnsbl) {
  return (struct dns_rr_txt *)
    dns_resolve(ctx, dns_submit_a6dnsbl_txt(ctx, addr, dnsbl, 0, 0));
}

static int
dns_rhsbltodn(const char *name, const char *rhsbl, dnsc_t dn[DNS_MAXDN])
{
  int l = dns_sptodn(name, dn, DNS_MAXDN);
  if (l <= 0) return 0;
  l = dns_sptodn(rhsbl, dn+l-1, DNS_MAXDN-l+1);
  if (l <= 0) return 0;
  return 1;
}

struct dns_query *
dns_submit_rhsbl(struct dns_ctx *ctx, const char *name, const char *rhsbl,
                 dns_query_a4_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (!dns_rhsbltodn(name, rhsbl, dn)) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_A, DNS_NOSRCH,
                  dns_parse_a4, (dns_query_fn*)cbck, data);
}
struct dns_query *
dns_submit_rhsbl_txt(struct dns_ctx *ctx, const char *name, const char *rhsbl,
                     dns_query_txt_fn *cbck, void *data) {
  dnsc_t dn[DNS_MAXDN];
  if (!dns_rhsbltodn(name, rhsbl, dn)) {
    dns_setstatus(ctx, DNS_E_BADQUERY);
    return NULL;
  }
  return
    dns_submit_dn(ctx, dn, DNS_C_IN, DNS_T_TXT, DNS_NOSRCH,
                  dns_parse_txt, (dns_query_fn*)cbck, data);
}

struct dns_rr_a4 *
dns_resolve_rhsbl(struct dns_ctx *ctx, const char *name, const char *rhsbl) {
  return (struct dns_rr_a4*)
    dns_resolve(ctx, dns_submit_rhsbl(ctx, name, rhsbl, 0, 0));
}

struct dns_rr_txt *
dns_resolve_rhsbl_txt(struct dns_ctx *ctx, const char *name, const char *rhsbl)
{
  return (struct dns_rr_txt*)
    dns_resolve(ctx, dns_submit_rhsbl_txt(ctx, name, rhsbl, 0, 0));
}

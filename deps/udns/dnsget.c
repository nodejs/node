/* $Id: dnsget.c,v 1.31 2007/01/08 01:14:44 mjt Exp $
   simple host/dig-like application using UDNS library

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef WINDOWS
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udns.h"

#ifndef HAVE_GETOPT
# include "getopt.c"
#endif

#ifndef AF_INET6
# define AF_INET6 10
#endif

static char *progname;
static int verbose = 1;
static int errors;
static int notfound;

/* verbosity level:
 * <0 - bare result
 *  0 - bare result and error messages
 *  1 - readable result
 *  2 - received packet contents and `trying ...' stuff
 *  3 - sent and received packet contents
 */

static void die(int errnum, const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "%s: ", progname);
  va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
  if (errnum) fprintf(stderr, ": %s\n", strerror(errnum));
  else putc('\n', stderr);
  fflush(stderr);
  exit(1);
}

static const char *dns_xntop(int af, const void *src) {
  static char buf[6*5+4*4];
  return dns_ntop(af, src, buf, sizeof(buf));
}

struct query {
  const char *name;		/* original query string */
  unsigned char *dn;		/* the DN being looked up */
  enum dns_type qtyp;		/* type of the query */
};

static void query_free(struct query *q) {
  free(q->dn);
  free(q);
}

static struct query *
query_new(const char *name, const unsigned char *dn, enum dns_type qtyp) {
  struct query *q = malloc(sizeof(*q));
  unsigned l = dns_dnlen(dn);
  unsigned char *cdn = malloc(l);
  if (!q || !cdn) die(0, "out of memory");
  memcpy(cdn, dn, l);
  q->name = name;
  q->dn = cdn;
  q->qtyp = qtyp;
  return q;
}

static enum dns_class qcls = DNS_C_IN;

static void
dnserror(struct query *q, int errnum) {
  if (verbose >= 0)
    fprintf(stderr, "%s: unable to lookup %s record for %s: %s\n", progname,
            dns_typename(q->qtyp), dns_dntosp(q->dn), dns_strerror(errnum));
  if (errnum == DNS_E_NXDOMAIN || errnum == DNS_E_NODATA)
    ++notfound;
  else
    ++errors;
  query_free(q);
}

static const unsigned char *
printtxt(const unsigned char *c) {
  unsigned n = *c++;
  const unsigned char *e = c + n;
  if (verbose > 0) while(c < e) {
    if (*c < ' ' || *c >= 127) printf("\\%02x", *c);
    else if (*c == '\\' || *c == '"') printf("\\%c", *c);
    else putchar(*c);
    ++c;
  }
  else
   fwrite(c, n, 1, stdout);
  return e;
}

static void
printhex(const unsigned char *c, const unsigned char *e) {
  while(c < e)
    printf("%02x", *c++);
}

static unsigned char to_b64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void
printb64(const unsigned char *c, const unsigned char *e) {
  while(c < e) {
    putchar(to_b64[c[0] >> 2]);
    if (c+1 < e) {
      putchar(to_b64[(c[0] & 0x3) << 4 | c[1] >> 4]);
      if (c+2 < e) {
        putchar(to_b64[(c[1] & 0xf) << 2 | c[2] >> 6]);
        putchar(to_b64[c[2] & 0x3f]);
      }
      else {
        putchar(to_b64[(c[1] & 0xf) << 2]);
	putchar('=');
	break;
      }
    }
    else {
      putchar(to_b64[(c[0] & 0x3) << 4]);
      putchar('=');
      putchar('=');
      break;
    }
    c += 3;
  }
}

static void
printdate(time_t time) {
  struct tm *tm = gmtime(&time);
  printf("%04d%02d%02d%02d%02d%02d",
    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
    tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void
printrr(const struct dns_parse *p, struct dns_rr *rr) {
  const unsigned char *pkt = p->dnsp_pkt;
  const unsigned char *end = p->dnsp_end;
  const unsigned char *dptr = rr->dnsrr_dptr;
  const unsigned char *dend = rr->dnsrr_dend;
  unsigned char *dn = rr->dnsrr_dn;
  const unsigned char *c;
  unsigned n;

  if (verbose > 0) {
    if (verbose > 1) {
      if (!p->dnsp_rrl && !rr->dnsrr_dn[0] && rr->dnsrr_typ == DNS_T_OPT) {
        printf(";EDNS0 OPT record (UDPsize: %d): %d bytes\n",
               rr->dnsrr_cls, rr->dnsrr_dsz);
        return;
      }
      n = printf("%s.", dns_dntosp(rr->dnsrr_dn));
      printf("%s%u\t%s\t%s\t",
             n > 15 ? "\t" : n > 7 ? "\t\t" : "\t\t\t",
             rr->dnsrr_ttl,
             dns_classname(rr->dnsrr_cls),
             dns_typename(rr->dnsrr_typ));
    }
    else
      printf("%s. %s ", dns_dntosp(rr->dnsrr_dn), dns_typename(rr->dnsrr_typ));
  }

  switch(rr->dnsrr_typ) {

  case DNS_T_CNAME:
  case DNS_T_PTR:
  case DNS_T_NS:
  case DNS_T_MB:
  case DNS_T_MD:
  case DNS_T_MF:
  case DNS_T_MG:
  case DNS_T_MR:
    if (dns_getdn(pkt, &dptr, end, dn, DNS_MAXDN) <= 0) goto xperr;
    printf("%s.", dns_dntosp(dn));
    break;

  case DNS_T_A:
    if (rr->dnsrr_dsz != 4) goto xperr;
    printf("%d.%d.%d.%d", dptr[0], dptr[1], dptr[2], dptr[3]);
    break;

  case DNS_T_AAAA:
    if (rr->dnsrr_dsz != 16) goto xperr;
    printf("%s", dns_xntop(AF_INET6, dptr));
    break;

  case DNS_T_MX:
    c = dptr + 2;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 || c != dend) goto xperr;
    printf("%d %s.", dns_get16(dptr), dns_dntosp(dn));
    break;

  case DNS_T_TXT:
    /* first verify it */
    for(c = dptr; c < dend; c += n) {
      n = *c++;
      if (c + n > dend) goto xperr;
    }
    c = dptr; n = 0;
    while (c < dend) {
      if (verbose > 0) printf(n++ ? "\" \"":"\"");
      c = printtxt(c);
    }
    if (verbose > 0) putchar('"');
    break;

  case DNS_T_HINFO:	/* CPU, OS */
    c = dptr;
    n = *c++; if ((c += n) >= dend) goto xperr;
    n = *c++; if ((c += n) != dend) goto xperr;
    c = dptr;
    if (verbose > 0) putchar('"');
    c = printtxt(c);
    if (verbose > 0) printf("\" \""); else putchar(' ');
    printtxt(c);
    if (verbose > 0) putchar('"');
    break;

  case DNS_T_WKS:
    c = dptr;
    if (dptr + 4 + 2 >= end) goto xperr;
    printf("%s %d", dns_xntop(AF_INET, dptr), dptr[4]);
    c = dptr + 5;
    for (n = 0; c < dend; ++c, n += 8) {
      if (*c) {
        unsigned b;
        for (b = 0; b < 8; ++b)
          if (*c & (1 << (7-b))) printf(" %d", n + b);
      }
    }
    break;

  case DNS_T_SRV:	/* prio weight port targetDN */
    c = dptr;
    c += 2 + 2 + 2;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 || c != dend) goto xperr;
    c = dptr;
    printf("%d %d %d %s.",
           dns_get16(c+0), dns_get16(c+2), dns_get16(c+4),
           dns_dntosp(dn));
    break;

  case DNS_T_NAPTR:	/* order pref flags serv regexp repl */
    c = dptr;
    c += 4;	/* order, pref */
    for (n = 0; n < 3; ++n)
      if (c >= dend) goto xperr;
      else c += *c + 1;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 || c != dend) goto xperr;
    c = dptr;
    printf("%u %u", dns_get16(c+0), dns_get16(c+2));
    c += 4;
    for(n = 0; n < 3; ++n) {
      putchar(' ');
      if (verbose > 0) putchar('"');
      c = printtxt(c);
      if (verbose > 0) putchar('"');
    } 
    printf(" %s.", dns_dntosp(dn));
    break;

  case DNS_T_KEY: /* flags(2) proto(1) algo(1) pubkey */
    c = dptr;
    if (c + 2 + 1 + 1 > dend) goto xperr;
    printf("%d %d %d", dns_get16(c), c[2], c[3]);
    c += 2 + 1 + 1;
    if (c < dend) {
      putchar(' ');
      printb64(c, dend);
    }
    break;

  case DNS_T_SIG:
    /* type(2) algo(1) labels(1) ottl(4) sexp(4) sinc(4) tag(2) sdn sig */
    c = dptr;
    c += 2 + 1 + 1 + 4 + 4 + 4 + 2;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0) goto xperr;
    printf("%d %u %u %u ",
           dns_get16(dptr), dptr[2], dptr[3], dns_get32(dptr+4));
    printdate(dns_get32(dptr+8));
    putchar(' ');
    printdate(dns_get32(dptr+12));
    printf(" %d %s. ", dns_get16(dptr+10), dns_dntosp(dn));
    printb64(c, dend);
    break;

#if 0	/* unused RR types? */
  case DNS_T_DS:
    c = dptr;
    if (c + 2 + 2 >= dend) goto xperr;
    printf("%u %u %u ", dns_get16(c), c[2], c[3]);
    printhex(c + 4, dend);
    break;

  case DNS_T_NSEC:
    c = dptr;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0) goto xperr;
    printf("%s.", dns_dntosp(dn));
    unfinished.
    break;
#endif


  case DNS_T_SOA:
    c = dptr;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 ||
        dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 ||
        c + 4*5 != dend)
      goto xperr;
    dns_getdn(pkt, &dptr, end, dn, DNS_MAXDN);
    printf("%s. ", dns_dntosp(dn));
    dns_getdn(pkt, &dptr, end, dn, DNS_MAXDN);
    printf("%s. ", dns_dntosp(dn));
    printf("%u %u %u %u %u",
           dns_get32(dptr), dns_get32(dptr+4), dns_get32(dptr+8),
           dns_get32(dptr+12), dns_get32(dptr+16));
    break;

  case DNS_T_MINFO:
    c = dptr;
    if (dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 ||
        dns_getdn(pkt, &c, end, dn, DNS_MAXDN) <= 0 ||
	c != dend)
      goto xperr;
    dns_getdn(pkt, &dptr, end, dn, DNS_MAXDN);
    printf("%s. ", dns_dntosp(dn));
    dns_getdn(pkt, &dptr, end, dn, DNS_MAXDN);
    printf("%s.", dns_dntosp(dn));
    break;

  case DNS_T_NULL:
  default:
    printhex(dptr, dend);
    break;
  }
  putchar('\n');
  return;

xperr:
  printf("<parse error>\n");
  ++errors;
}

static int
printsection(struct dns_parse *p, int nrr, const char *sname) {
  struct dns_rr rr;
  int r;
  if (!nrr) return 0;
  if (verbose > 1) printf("\n;; %s section (%d):\n", sname, nrr);

  p->dnsp_rrl = nrr;
  while((r = dns_nextrr(p, &rr)) > 0)
    printrr(p, &rr);
  if (r < 0) printf("<<ERROR>>\n");
  return r;
}

/* dbgcb will only be called if verbose > 1 */
static void
dbgcb(int code, const struct sockaddr *sa, unsigned slen,
      const unsigned char *pkt, int r,
      const struct dns_query *unused_q, void *unused_data) {
  struct dns_parse p;
  const unsigned char *cur, *end;
  int numqd;

  if (code > 0)	{
    printf(";; trying %s.\n", dns_dntosp(dns_payload(pkt)));
    printf(";; sending %d bytes query to ", r);
  }
  else
    printf(";; received %d bytes response from ", r);
  if (sa->sa_family == AF_INET && slen >= sizeof(struct sockaddr_in))
    printf("%s port %d\n",
           dns_xntop(AF_INET, &((struct sockaddr_in*)sa)->sin_addr),
           htons(((struct sockaddr_in*)sa)->sin_port));
#ifdef HAVE_IPv6
  else if (sa->sa_family == AF_INET6 && slen >= sizeof(struct sockaddr_in6))
    printf("%s port %d\n",
           dns_xntop(AF_INET6, &((struct sockaddr_in6*)sa)->sin6_addr),
           htons(((struct sockaddr_in6*)sa)->sin6_port));
#endif
  else
    printf("<<unknown socket type %d>>\n", sa->sa_family);
  if (code > 0 && verbose < 3) {
    putchar('\n');
    return;
  }

  if (code == -2) printf(";; reply from unexpected source\n");
  if (code == -5) printf(";; reply to a query we didn't sent (or old)\n");
  if (r < DNS_HSIZE) {
    printf(";; short packet (%d bytes)\n", r);
    return;
  }
  if (dns_opcode(pkt) != 0)
    printf(";; unexpected opcode %d\n", dns_opcode(pkt));
  if (dns_tc(pkt) != 0)
    printf(";; warning: TC bit set, probably incomplete reply\n");

  printf(";; ->>HEADER<<- opcode: ");
  switch(dns_opcode(pkt)) {
  case 0: printf("QUERY"); break;
  case 1: printf("IQUERY"); break;
  case 2: printf("STATUS"); break;
  default: printf("UNKNOWN(%u)", dns_opcode(pkt)); break;
  }
  printf(", status: %s, id: %d, size: %d\n;; flags:",
         dns_rcodename(dns_rcode(pkt)), dns_qid(pkt), r);
  if (dns_qr(pkt)) printf(" qr");
  if (dns_rd(pkt)) printf(" rd");
  if (dns_ra(pkt)) printf(" ra");
  if (dns_aa(pkt)) printf(" aa");
  if (dns_tc(pkt)) printf(" tc");
  numqd = dns_numqd(pkt);
  printf("; QUERY: %d, ANSWER: %d, AUTHORITY: %d, ADDITIONAL: %d\n",
         numqd, dns_numan(pkt), dns_numns(pkt), dns_numar(pkt));
  if (numqd != 1)
    printf(";; unexpected number of entries in QUERY section: %d\n",
           numqd);
  printf("\n;; QUERY SECTION (%d):\n", numqd);
  cur = dns_payload(pkt);
  end = pkt + r;
  while(numqd--) {
    if (dns_getdn(pkt, &cur, end, p.dnsp_dnbuf, DNS_MAXDN) <= 0 ||
        cur + 4 > end) {
      printf("; invalid query section\n");
      return;
    }
    r = printf(";%s.", dns_dntosp(p.dnsp_dnbuf));
    printf("%s%s\t%s\n",
           r > 23 ? "\t" : r > 15 ? "\t\t" : r > 7 ? "\t\t\t" : "\t\t\t\t",
           dns_classname(dns_get16(cur+2)), dns_typename(dns_get16(cur)));
    cur += 4;
  }

  p.dnsp_pkt = pkt;
  p.dnsp_cur = p.dnsp_ans = cur;
  p.dnsp_end = end;
  p.dnsp_qdn = NULL;
  p.dnsp_qcls = p.dnsp_qtyp = 0;
  p.dnsp_ttl = 0xffffffffu;
  p.dnsp_nrr = 0;

  r = printsection(&p, dns_numan(pkt), "ANSWER");
  if (r == 0)
    r = printsection(&p, dns_numns(pkt), "AUTHORITY");
  if (r == 0)
    r = printsection(&p, dns_numar(pkt), "ADDITIONAL");
  putchar('\n');
}

static void dnscb(struct dns_ctx *ctx, void *result, void *data) {
  int r = dns_status(ctx);
  struct query *q = data;
  struct dns_parse p;
  struct dns_rr rr;
  unsigned nrr;
  unsigned char dn[DNS_MAXDN];
  const unsigned char *pkt, *cur, *end;
  if (!result) {
    dnserror(q, r);
    return;
  }
  pkt = result; end = pkt + r; cur = dns_payload(pkt);
  dns_getdn(pkt, &cur, end, dn, sizeof(dn));
  dns_initparse(&p, NULL, pkt, cur, end);
  p.dnsp_qcls = p.dnsp_qtyp = 0;
  nrr = 0;
  while((r = dns_nextrr(&p, &rr)) > 0) {
    if (!dns_dnequal(dn, rr.dnsrr_dn)) continue;
    if ((qcls == DNS_C_ANY || qcls == rr.dnsrr_cls) &&
        (q->qtyp == DNS_T_ANY || q->qtyp == rr.dnsrr_typ))
      ++nrr;
    else if (rr.dnsrr_typ == DNS_T_CNAME && !nrr) {
      if (dns_getdn(pkt, &rr.dnsrr_dptr, end,
                    p.dnsp_dnbuf, sizeof(p.dnsp_dnbuf)) <= 0 ||
          rr.dnsrr_dptr != rr.dnsrr_dend) {
        r = DNS_E_PROTOCOL;
        break;
      }
      else {
        if (verbose == 1) {
          printf("%s.", dns_dntosp(dn));
          printf(" CNAME %s.\n", dns_dntosp(p.dnsp_dnbuf));
        }
        dns_dntodn(p.dnsp_dnbuf, dn, sizeof(dn));
      }
    }
  }
  if (!r && !nrr)
    r = DNS_E_NODATA;
  if (r < 0) {
    dnserror(q, r);
    free(result);
    return;
  }
  if (verbose < 2) {	/* else it is already printed by dbgfn */
    dns_rewind(&p, NULL);
    p.dnsp_qtyp = q->qtyp == DNS_T_ANY ? 0 : q->qtyp;
    p.dnsp_qcls = qcls == DNS_C_ANY ? 0 : qcls;
    while(dns_nextrr(&p, &rr))
      printrr(&p, &rr);
  }
  free(result);
  query_free(q);
}

int main(int argc, char **argv) {
  int i;
  int fd;
  fd_set fds;
  struct timeval tv;
  time_t now;
  char *ns[DNS_MAXSERV];
  int nns = 0;
  struct query *q;
  enum dns_type qtyp = 0;
  struct dns_ctx *nctx = NULL;

  if (!(progname = strrchr(argv[0], '/'))) progname = argv[0];
  else argv[0] = ++progname;

  if (argc <= 1)
    die(0, "try `%s -h' for help", progname);

  if (dns_init(NULL, 0) < 0 || !(nctx = dns_new(NULL)))
    die(errno, "unable to initialize dns library");
  /* we keep two dns contexts: one may be needed to resolve
   * nameservers if given as names, using default options.
   */

  while((i = getopt(argc, argv, "vqt:c:an:o:h")) != EOF) switch(i) {
  case 'v': ++verbose; break;
  case 'q': --verbose; break;
  case 't':
    if (optarg[0] == '*' && !optarg[1])
      i = DNS_T_ANY;
    else if ((i = dns_findtypename(optarg)) <= 0)
      die(0, "unrecognized query type `%s'", optarg);
    qtyp = i;
    break;
  case 'c':
    if (optarg[0] == '*' && !optarg[1])
      i = DNS_C_ANY;
    else if ((i = dns_findclassname(optarg)) < 0)
      die(0, "unrecognized query class `%s'", optarg);
    qcls = i;
    break;
  case 'a':
    qtyp = DNS_T_ANY;
    ++verbose;
    break;
  case 'n':
    if (nns >= DNS_MAXSERV)
      die(0, "too many nameservers, %d max", DNS_MAXSERV);
    ns[nns++] = optarg;
    break;
  case 'o':
    if (dns_set_opts(NULL, optarg) != 0)
      die(0, "invalid option string: `%s'", optarg);
    break;
  case 'h':
    printf(
"%s: simple DNS query tool (using udns version %s)\n"
"Usage: %s [options] domain-name...\n"
"where options are:\n"
" -h - print this help and exit\n"
" -v - be more verbose\n"
" -q - be less verbose\n"
" -t type - set query type (A, AAA, PTR etc)\n"
" -c class - set query class (IN (default), CH, HS, *)\n"
" -a - equivalent to -t ANY -v\n"
" -n ns - use given nameserver(s) instead of default\n"
"  (may be specified multiple times)\n"
" -o option:value - set resovler option (the same as setting $RES_OPTIONS):\n"
"  timeout:sec  - initial query timeout\n"
"  attempts:num - number of attempt to resovle a query\n"
"  ndots:num    - if name has more than num dots, lookup it before search\n"
"  port:num     - port number for queries instead of default 53\n"
"  udpbuf:num   - size of UDP buffer (use EDNS0 if >512)\n"
"  (may be specified more than once)\n"
      , progname, dns_version(), progname);
    return 0;
  default:
    die(0, "try `%s -h' for help", progname);
  }

  argc -= optind; argv += optind;
  if (!argc)
    die(0, "no name(s) to query specified");

  if (nns) {
    /* if nameservers given as names, resolve them.
     * We only allow IPv4 nameservers as names for now.
     * Ok, it is easy enouth to try both AAAA and A,
     * but the question is what to do by default.
     */
    struct sockaddr_in sin;
    int j, r = 0, opened = 0;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(dns_set_opt(NULL, DNS_OPT_PORT, -1));
    dns_add_serv(NULL, NULL);
    for(i = 0; i < nns; ++i) {
      if (dns_pton(AF_INET, ns[i], &sin.sin_addr) <= 0) {
        struct dns_rr_a4 *rr;
        if (!opened) {
          if (dns_open(nctx) < 0)
            die(errno, "unable to initialize dns context");
          opened = 1;
        }
        rr = dns_resolve_a4(nctx, ns[i], 0);
        if (!rr)
          die(0, "unable to resolve nameserver %s: %s",
              ns[i], dns_strerror(dns_status(nctx)));
        for(j = 0; j < rr->dnsa4_nrr; ++j) {
          sin.sin_addr = rr->dnsa4_addr[j];
          if ((r = dns_add_serv_s(NULL, (struct sockaddr *)&sin)) < 0)
            break;
        }
        free(rr);
      }
      else
        r = dns_add_serv_s(NULL, (struct sockaddr *)&sin);
      if (r < 0)
        die(errno, "unable to add nameserver %s",
             dns_xntop(AF_INET, &sin.sin_addr));
    }
  }
  dns_free(nctx);

  fd = dns_open(NULL);
  if (fd < 0)
    die(errno, "unable to initialize dns context");

  if (verbose > 1)
    dns_set_dbgfn(NULL, dbgcb);

  for (i = 0; i < argc; ++i) {
    char *name = argv[i];
    union {
      struct in_addr addr;
      struct in6_addr addr6;
    } a;
    unsigned char dn[DNS_MAXDN];
    enum dns_type l_qtyp = 0;
    int abs;
    if (dns_pton(AF_INET, name, &a.addr) > 0) {
      dns_a4todn(&a.addr, 0, dn, sizeof(dn));
      l_qtyp = DNS_T_PTR;
      abs = 1;
    }
#ifdef HAVE_IPv6
    else if (dns_pton(AF_INET6, name, &a.addr6) > 0) {
      dns_a6todn(&a.addr6, 0, dn, sizeof(dn));
      l_qtyp = DNS_T_PTR;
      abs = 1;
    }
#endif
    else if (!dns_ptodn(name, strlen(name), dn, sizeof(dn), &abs))
      die(0, "invalid name `%s'\n", name);
    else
      l_qtyp = DNS_T_A;
    if (qtyp) l_qtyp = qtyp;
    q = query_new(name, dn, l_qtyp);
    if (abs) abs = DNS_NOSRCH;
    if (!dns_submit_dn(NULL, dn, qcls, l_qtyp, abs, 0, dnscb, q))
      dnserror(q, dns_status(NULL));
  }

  FD_ZERO(&fds);
  now = 0;
  while((i = dns_timeouts(NULL, -1, now)) > 0) {
    FD_SET(fd, &fds);
    tv.tv_sec = i;
    tv.tv_usec = 0;
    i = select(fd+1, &fds, 0, 0, &tv);
    now = time(NULL);
    if (i > 0) dns_ioevent(NULL, now);
  }

  return errors ? 1 : notfound ? 100 : 0;
}

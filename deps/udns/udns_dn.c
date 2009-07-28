/* $Id: udns_dn.c,v 1.7 2006/11/28 22:45:20 mjt Exp $
   domain names manipulation routines

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
#include "udns.h"

unsigned dns_dnlen(dnscc_t *dn) {
  register dnscc_t *d = dn;
  while(*d)
    d += 1 + *d;
  return (unsigned)(d - dn) + 1;
}

unsigned dns_dnlabels(register dnscc_t *dn) {
  register unsigned l = 0;
  while(*dn)
    ++l, dn += 1 + *dn;
  return l;
}

unsigned dns_dnequal(register dnscc_t *dn1, register dnscc_t *dn2) {
  register unsigned c;
  dnscc_t *dn = dn1;
  for(;;) {
    if ((c = *dn1++) != *dn2++)
      return 0;
    if (!c)
      return (unsigned)(dn1 - dn);
    while(c--) {
      if (DNS_DNLC(*dn1) != DNS_DNLC(*dn2))
        return 0;
      ++dn1; ++dn2;
    }
  }
}

unsigned
dns_dntodn(dnscc_t *sdn, dnsc_t *ddn, unsigned ddnsiz) {
  unsigned sdnlen = dns_dnlen(sdn);
  if (ddnsiz < sdnlen)
    return 0;
  memcpy(ddn, sdn, sdnlen);
  return sdnlen;
}

int
dns_ptodn(const char *name, unsigned namelen,
          dnsc_t *dn, unsigned dnsiz, int *isabs)
{
  dnsc_t *dp;		/* current position in dn (len byte first) */
  dnsc_t *const de	/* end of dn: last byte that can be filled up */
      = dn + (dnsiz >= DNS_MAXDN ? DNS_MAXDN : dnsiz) - 1;
  dnscc_t *np = (dnscc_t *)name;
  dnscc_t *ne = np + (namelen ? namelen : strlen((char*)np));
  dnsc_t *llab;		/* start of last label (llab[-1] will be length) */
  unsigned c;		/* next input character, or length of last label */

  if (!dnsiz)
    return 0;
  dp = llab = dn + 1;

  while(np < ne) {

    if (*np == '.') {	/* label delimiter */
      c = dp - llab;		/* length of the label */
      if (!c) {			/* empty label */
        if (np == (dnscc_t *)name && np + 1 == ne) {
          /* special case for root dn, aka `.' */
          ++np;
          break;
        }
        return -1;		/* zero label */
      }
      if (c > DNS_MAXLABEL)
        return -1;		/* label too long */
      llab[-1] = (dnsc_t)c;	/* update len of last label */
      llab = ++dp; /* start new label, llab[-1] will be len of it */
      ++np;
      continue;
    }

    /* check whenever we may put out one more byte */
    if (dp >= de) /* too long? */
      return dnsiz >= DNS_MAXDN ? -1 : 0;
    if (*np != '\\') { /* non-escape, simple case */
      *dp++ = *np++;
      continue;
    }
    /* handle \-style escape */
    /* note that traditionally, domain names (gethostbyname etc)
     * used decimal \dd notation, not octal \ooo (RFC1035), so
     * we're following this tradition here.
     */
    if (++np == ne)
      return -1;			/* bad escape */
    else if (*np >= '0' && *np <= '9') { /* decimal number */
      /* we allow not only exactly 3 digits as per RFC1035,
       * but also 2 or 1, for better usability. */
      c = *np++ - '0';
      if (np < ne && *np >= '0' && *np <= '9') { /* 2digits */
        c = c * 10 + *np++ - '0';
        if (np < ne && *np >= '0' && *np <= '9') {
          c = c * 10 + *np++ - '0';
          if (c > 255)
            return -1;			/* bad escape */
        }
      }
    }
    else
      c = *np++;
    *dp++ = (dnsc_t)c;	/* place next out byte */
  }

  if ((c = dp - llab) > DNS_MAXLABEL)
    return -1;				/* label too long */
  if ((llab[-1] = (dnsc_t)c) != 0) {
    *dp++ = 0;
    if (isabs)
      *isabs = 0;
  }
  else if (isabs)
    *isabs = 1;

  return dp - dn;
}

dnscc_t dns_inaddr_arpa_dn[14] = "\07in-addr\04arpa";

dnsc_t *
dns_a4todn_(const struct in_addr *addr, dnsc_t *dn, dnsc_t *dne) {
  dnsc_t *p;
  unsigned n;
  dnscc_t *s = ((dnscc_t *)addr) + 4;
  while(--s >= (dnscc_t *)addr) {
    n = *s;
    p = dn + 1;
    if (n > 99) {
      if (p + 2 > dne) return 0;
      *p++ = n / 100 + '0';
      *p++ = (n % 100 / 10) + '0';
      *p = n % 10 + '0';
    }
    else if (n > 9) {
      if (p + 1 > dne) return 0;
      *p++ = n / 10 + '0';
      *p = n % 10 + '0';
    }
    else {
      if (p > dne) return 0;
      *p = n + '0';
    }
    *dn = p - dn;
    dn = p + 1;
  }
  return dn;
}

int dns_a4todn(const struct in_addr *addr, dnscc_t *tdn,
               dnsc_t *dn, unsigned dnsiz) {
  dnsc_t *dne = dn + (dnsiz > DNS_MAXDN ? DNS_MAXDN : dnsiz);
  dnsc_t *p;
  unsigned l;
  p = dns_a4todn_(addr, dn, dne);
  if (!p) return 0;
  if (!tdn)
    tdn = dns_inaddr_arpa_dn;
  l = dns_dnlen(tdn);
  if (p + l > dne) return dnsiz >= DNS_MAXDN ? -1 : 0;
  memcpy(p, tdn, l);
  return (p + l) - dn;
}

int dns_a4ptodn(const struct in_addr *addr, const char *tname,
                dnsc_t *dn, unsigned dnsiz) {
  dnsc_t *p;
  int r;
  if (!tname)
    return dns_a4todn(addr, NULL, dn, dnsiz);
  p = dns_a4todn_(addr, dn, dn + dnsiz);
  if (!p) return 0;
  r = dns_sptodn(tname, p, dnsiz - (p - dn));
  return r != 0 ? r : dnsiz >= DNS_MAXDN ? -1 : 0;
}

dnscc_t dns_ip6_arpa_dn[10] = "\03ip6\04arpa";

dnsc_t *
dns_a6todn_(const struct in6_addr *addr, dnsc_t *dn, dnsc_t *dne) {
  unsigned n;
  dnscc_t *s = ((dnscc_t *)addr) + 16;
  if (dn + 64 > dne) return 0;
  while(--s >= (dnscc_t *)addr) {
    *dn++ = 1;
    n = *s & 0x0f;
    *dn++ = n > 9 ? n + 'a' - 10 : n + '0';
    *dn++ = 1;
    n = *s >> 4;
    *dn++ = n > 9 ? n + 'a' - 10 : n + '0';
  }
  return dn;
}

int dns_a6todn(const struct in6_addr *addr, dnscc_t *tdn,
               dnsc_t *dn, unsigned dnsiz) {
  dnsc_t *dne = dn + (dnsiz > DNS_MAXDN ? DNS_MAXDN : dnsiz);
  dnsc_t *p;
  unsigned l;
  p = dns_a6todn_(addr, dn, dne);
  if (!p) return 0;
  if (!tdn)
    tdn = dns_ip6_arpa_dn;
  l = dns_dnlen(tdn);
  if (p + l > dne) return dnsiz >= DNS_MAXDN ? -1 : 0;
  memcpy(p, tdn, l);
  return (p + l) - dn;
}

int dns_a6ptodn(const struct in6_addr *addr, const char *tname,
                dnsc_t *dn, unsigned dnsiz) {
  dnsc_t *p;
  int r;
  if (!tname)
    return dns_a6todn(addr, NULL, dn, dnsiz);
  p = dns_a6todn_(addr, dn, dn + dnsiz);
  if (!p) return 0;
  r = dns_sptodn(tname, p, dnsiz - (p - dn));
  return r != 0 ? r : dnsiz >= DNS_MAXDN ? -1 : 0;
}

/* return size of buffer required to convert the dn into asciiz string.
 * Keep in sync with dns_dntop() below.
 */
unsigned dns_dntop_size(dnscc_t *dn) {
  unsigned size = 0;		/* the size reqd */
  dnscc_t *le;			/* label end */

  while(*dn) {
    /* *dn is the length of the next label, non-zero */
    if (size)
      ++size;		/* for the dot */
    le = dn + *dn + 1;
    ++dn;
    do {
      switch(*dn) {
      case '.':
      case '\\':
      /* Special modifiers in zone files. */
      case '"':
      case ';':
      case '@':
      case '$':
        size += 2;
        break;
      default:
        if (*dn <= 0x20 || *dn >= 0x7f)
          /* \ddd decimal notation */
          size += 4;
        else
          size += 1;
      }
    } while(++dn < le);
  }
  size += 1;	/* zero byte at the end - string terminator */
  return size > DNS_MAXNAME ? 0 : size;
}

/* Convert the dn into asciiz string.
 * Keep in sync with dns_dntop_size() above.
 */
int dns_dntop(dnscc_t *dn, char *name, unsigned namesiz) {
  char *np = name;			/* current name ptr */
  char *const ne = name + namesiz;	/* end of name */
  dnscc_t *le;		/* label end */

  while(*dn) {
    /* *dn is the length of the next label, non-zero */
    if (np != name) {
      if (np >= ne) goto toolong;
      *np++ = '.';
    }
    le = dn + *dn + 1;
    ++dn;
    do {
      switch(*dn) {
      case '.':
      case '\\':
      /* Special modifiers in zone files. */
      case '"':
      case ';':
      case '@':
      case '$':
        if (np + 2 > ne) goto toolong;
        *np++ = '\\';
        *np++ = *dn;
        break;
      default:
        if (*dn <= 0x20 || *dn >= 0x7f) {
          /* \ddd decimal notation */
          if (np + 4 >= ne) goto toolong;
          *np++ = '\\';
          *np++ = '0' + (*dn / 100);
          *np++ = '0' + ((*dn % 100) / 10);
          *np++ = '0' + (*dn % 10);
        }
        else {
          if (np >= ne) goto toolong;
          *np++ = *dn;
        }
      }
    } while(++dn < le);
  }
  if (np >= ne) goto toolong;
  *np++ = '\0';
  return np - name;
toolong:
  return namesiz >= DNS_MAXNAME ? -1 : 0;
}

#ifdef TEST
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  int i;
  int sz;
  dnsc_t dn[DNS_MAXDN+10];
  dnsc_t *dl, *dp;
  int isabs;

  sz = (argc > 1) ? atoi(argv[1]) : 0;

  for(i = 2; i < argc; ++i) {
    int r = dns_ptodn(argv[i], 0, dn, sz, &isabs);
    printf("%s: ", argv[i]);
    if (r < 0) printf("error\n");
    else if (!r) printf("buffer too small\n");
    else {
      printf("len=%d dnlen=%d size=%d name:",
             r, dns_dnlen(dn), dns_dntop_size(dn));
      dl = dn;
      while(*dl) {
        printf(" %d=", *dl);
        dp = dl + 1;
        dl = dp + *dl;
        while(dp < dl) {
          if (*dp <= ' ' || *dp >= 0x7f)
            printf("\\%03d", *dp);
          else if (*dp == '.' || *dp == '\\')
            printf("\\%c", *dp);
          else
            putchar(*dp);
          ++dp;
        }
      }
      if (isabs) putchar('.');
      putchar('\n');
    }
  }
  return 0;
}

#endif /* TEST */

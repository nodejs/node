/* $Id: ex-rdns.c,v 1.8 2007/01/07 22:46:47 mjt Exp $
   parallel rDNS resolver example - read IP addresses from stdin,
   write domain names to stdout
 
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "udns.h"

static int curq;

static const char *n2ip(const unsigned char *c) {
  static char b[sizeof("255.255.255.255")];
  sprintf(b, "%u.%u.%u.%u", c[0], c[1], c[2], c[3]);
  return b;
}
static void dnscb(struct dns_ctx *ctx, struct dns_rr_ptr *rr, void *data) {
  const char *ip = n2ip((unsigned char *)&data);
  int i;
  --curq;
  if (rr) {
    printf("%s", ip);
    for(i = 0; i < rr->dnsptr_nrr; ++i)
      printf(" %s", rr->dnsptr_ptr[i]);
    putchar('\n');
    free(rr);
  }
  else
    fprintf(stderr, "%s: %s\n", ip, dns_strerror(dns_status(ctx)));
}

int main(int argc, char **argv) {
  int c, t;
  time_t now;
  int maxq = 10;
  struct pollfd pfd;
  char linebuf[1024];
  char *eol;
  int eof;

  if (dns_init(NULL, 1) < 0) {
    fprintf(stderr, "unable to initialize dns library\n");
    return 1;
  }
  while((c = getopt(argc, argv, "m:r")) != EOF) switch(c) {
  case 'm': maxq = atoi(optarg); break;
  case 'r':
     dns_set_opt(0, DNS_OPT_FLAGS,
                 dns_set_opt(0, DNS_OPT_FLAGS, -1) | DNS_NORD);
     break;
  default: return 1;
  }
  if (argc != optind) return 1;

  pfd.fd = dns_sock(0);
  pfd.events = POLLIN;
  now = time(NULL);
  c = optind;
  eof = 0;
  while(curq || !eof) {
    if (!eof && curq < maxq) {
      union { struct in_addr a; void *p; } pa;
      if (!fgets(linebuf, sizeof(linebuf), stdin)) {
        eof = 1;
        continue;
      }
      eol = strchr(linebuf, '\n');
      if (eol) *eol = '\0';
      if (!linebuf[0]) continue;
      if (dns_pton(AF_INET, linebuf, &pa.a) <= 0)
        fprintf(stderr, "%s: invalid address\n", linebuf);
      else if (dns_submit_a4ptr(0, &pa.a, dnscb, pa.p) == 0)
        fprintf(stderr, "%s: unable to submit query: %s\n",
                linebuf, dns_strerror(dns_status(0)));
      else
        ++curq;
      continue;
    }
    if (curq) {
      t = dns_timeouts(0, -1, now);
      t = poll(&pfd, 1, c * 1000);
      now = time(NULL);
      if (t) dns_ioevent(0, now);
    }
  }
  return 0;
}

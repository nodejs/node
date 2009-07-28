/* $Id: udns_misc.c,v 1.8 2005/04/05 22:51:32 mjt Exp $
   miscellaneous routines

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

int dns_findname(const struct dns_nameval *nv, const char *name) {
  register const char *a, *b;
  for(; nv->name; ++nv)
    for(a = name, b = nv->name; ; ++a, ++b)
      if (DNS_DNUC(*a) != *b) break;
      else if (!*a) return nv->val;
  return -1;
}

const char *_dns_format_code(char *buf, const char *prefix, int code) {
  char *bp = buf;
  unsigned c, n;
  do *bp++ = DNS_DNUC(*prefix);
  while(*++prefix);
  *bp++ = '#';
  if (code < 0) code = -code, *bp++ = '-';
  n = 0; c = code;
  do ++n;
  while((c /= 10));
  c = code;
  bp[n--] = '\0';
  do bp[n--] = c % 10 + '0';
  while((c /= 10));
  return buf;
}

const char *dns_strerror(int err) {
  if (err >= 0) return "successeful completion";
  switch(err) {
  case DNS_E_TEMPFAIL:	return "temporary failure in name resolution";
  case DNS_E_PROTOCOL:	return "protocol error";
  case DNS_E_NXDOMAIN:	return "domain name does not exist";
  case DNS_E_NODATA:	return "valid domain but no data of requested type";
  case DNS_E_NOMEM:	return "out of memory";
  case DNS_E_BADQUERY:	return "malformed query";
  default:		return "unknown error";
  }
}

const char *dns_version(void) {
  return UDNS_VERSION;
}


/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HAVE_BITNCMP

#include "ares_setup.h"
#include "bitncmp.h"

/*
 * int
 * bitncmp(l, r, n)
 *	compare bit masks l and r, for n bits.
 * return:
 *	<0, >0, or 0 in the libc tradition.
 * note:
 *	network byte order assumed.  this means 192.5.5.240/28 has
 *	0x11110000 in its fourth octet.
 * author:
 *	Paul Vixie (ISC), June 1996
 */
int ares__bitncmp(const void *l, const void *r, int n)
{
  unsigned int lb, rb;
  int x, b;

  b = n / 8;
  x = memcmp(l, r, b);
  if (x || (n % 8) == 0)
    return (x);

  lb = ((const unsigned char *)l)[b];
  rb = ((const unsigned char *)r)[b];
  for (b = n % 8; b > 0; b--) {
    if ((lb & 0x80) != (rb & 0x80)) {
      if (lb & 0x80)
        return (1);
      return (-1);
    }
    lb <<= 1;
    rb <<= 1;
  }
  return (0);
}
#endif

/* Copyright (c) 2011, 2018 Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Derived from https://github.com/bnoordhuis/punycode
 * but updated to support IDNA 2008.
 */

#include "uv.h"
#include "idna.h"
#include <assert.h>
#include <string.h>
#include <limits.h> /* UINT_MAX */

static unsigned uv__utf8_decode1_slow(const char** p,
                                      const char* pe,
                                      unsigned a) {
  unsigned b;
  unsigned c;
  unsigned d;
  unsigned min;

  if (a > 0xF7)
    return -1;

  switch (pe - *p) {
  default:
    if (a > 0xEF) {
      min = 0x10000;
      a = a & 7;
      b = (unsigned char) *(*p)++;
      c = (unsigned char) *(*p)++;
      d = (unsigned char) *(*p)++;
      break;
    }
    /* Fall through. */
  case 2:
    if (a > 0xDF) {
      min = 0x800;
      b = 0x80 | (a & 15);
      c = (unsigned char) *(*p)++;
      d = (unsigned char) *(*p)++;
      a = 0;
      break;
    }
    /* Fall through. */
  case 1:
    if (a > 0xBF) {
      min = 0x80;
      b = 0x80;
      c = 0x80 | (a & 31);
      d = (unsigned char) *(*p)++;
      a = 0;
      break;
    }
    /* Fall through. */
  case 0:
    return -1;  /* Invalid continuation byte. */
  }

  if (0x80 != (0xC0 & (b ^ c ^ d)))
    return -1;  /* Invalid sequence. */

  b &= 63;
  c &= 63;
  d &= 63;
  a = (a << 18) | (b << 12) | (c << 6) | d;

  if (a < min)
    return -1;  /* Overlong sequence. */

  if (a > 0x10FFFF)
    return -1;  /* Four-byte sequence > U+10FFFF. */

  if (a >= 0xD800 && a <= 0xDFFF)
    return -1;  /* Surrogate pair. */

  return a;
}

unsigned uv__utf8_decode1(const char** p, const char* pe) {
  unsigned a;

  assert(*p < pe);

  a = (unsigned char) *(*p)++;

  if (a < 128)
    return a;  /* ASCII, common case. */

  return uv__utf8_decode1_slow(p, pe, a);
}

static int uv__idna_toascii_label(const char* s, const char* se,
                                  char** d, char* de) {
  static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  const char* ss;
  unsigned c;
  unsigned h;
  unsigned k;
  unsigned n;
  unsigned m;
  unsigned q;
  unsigned t;
  unsigned x;
  unsigned y;
  unsigned bias;
  unsigned delta;
  unsigned todo;
  int first;

  h = 0;
  ss = s;
  todo = 0;

  /* Note: after this loop we've visited all UTF-8 characters and know
   * they're legal so we no longer need to check for decode errors.
   */
  while (s < se) {
    c = uv__utf8_decode1(&s, se);

    if (c == UINT_MAX)
      return UV_EINVAL;

    if (c < 128)
      h++;
    else
      todo++;
  }

  /* Only write "xn--" when there are non-ASCII characters. */
  if (todo > 0) {
    if (*d < de) *(*d)++ = 'x';
    if (*d < de) *(*d)++ = 'n';
    if (*d < de) *(*d)++ = '-';
    if (*d < de) *(*d)++ = '-';
  }

  /* Write ASCII characters. */
  x = 0;
  s = ss;
  while (s < se) {
    c = uv__utf8_decode1(&s, se);
    assert(c != UINT_MAX);

    if (c > 127)
      continue;

    if (*d < de)
      *(*d)++ = c;

    if (++x == h)
      break;  /* Visited all ASCII characters. */
  }

  if (todo == 0)
    return h;

  /* Only write separator when we've written ASCII characters first. */
  if (h > 0)
    if (*d < de)
      *(*d)++ = '-';

  n = 128;
  bias = 72;
  delta = 0;
  first = 1;

  while (todo > 0) {
    m = -1;
    s = ss;

    while (s < se) {
      c = uv__utf8_decode1(&s, se);
      assert(c != UINT_MAX);

      if (c >= n)
        if (c < m)
          m = c;
    }

    x = m - n;
    y = h + 1;

    if (x > ~delta / y)
      return UV_E2BIG;  /* Overflow. */

    delta += x * y;
    n = m;

    s = ss;
    while (s < se) {
      c = uv__utf8_decode1(&s, se);
      assert(c != UINT_MAX);

      if (c < n)
        if (++delta == 0)
          return UV_E2BIG;  /* Overflow. */

      if (c != n)
        continue;

      for (k = 36, q = delta; /* empty */; k += 36) {
        t = 1;

        if (k > bias)
          t = k - bias;

        if (t > 26)
          t = 26;

        if (q < t)
          break;

        /* TODO(bnoordhuis) Since 1 <= t <= 26 and therefore
         * 10 <= y <= 35, we can optimize the long division
         * into a table-based reciprocal multiplication.
         */
        x = q - t;
        y = 36 - t;  /* 10 <= y <= 35 since 1 <= t <= 26. */
        q = x / y;
        t = t + x % y;  /* 1 <= t <= 35 because of y. */

        if (*d < de)
          *(*d)++ = alphabet[t];
      }

      if (*d < de)
        *(*d)++ = alphabet[q];

      delta /= 2;

      if (first) {
        delta /= 350;
        first = 0;
      }

      /* No overflow check is needed because |delta| was just
       * divided by 2 and |delta+delta >= delta + delta/h|.
       */
      h++;
      delta += delta / h;

      for (bias = 0; delta > 35 * 26 / 2; bias += 36)
        delta /= 35;

      bias += 36 * delta / (delta + 38);
      delta = 0;
      todo--;
    }

    delta++;
    n++;
  }

  return 0;
}

long uv__idna_toascii(const char* s, const char* se, char* d, char* de) {
  const char* si;
  const char* st;
  unsigned c;
  char* ds;
  int rc;

  ds = d;

  si = s;
  while (si < se) {
    st = si;
    c = uv__utf8_decode1(&si, se);

    if (c == UINT_MAX)
      return UV_EINVAL;

    if (c != '.')
      if (c != 0x3002)  /* 。 */
        if (c != 0xFF0E)  /* ． */
          if (c != 0xFF61)  /* ｡ */
            continue;

    rc = uv__idna_toascii_label(s, st, &d, de);

    if (rc < 0)
      return rc;

    if (d < de)
      *d++ = '.';

    s = si;
  }

  if (s < se) {
    rc = uv__idna_toascii_label(s, se, &d, de);

    if (rc < 0)
      return rc;
  }

  if (d < de)
    *d++ = '\0';

  return d - ds;  /* Number of bytes written. */
}

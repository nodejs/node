/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "ngtcp2_str.h"

#include <string.h>

#include "ngtcp2_macro.h"

void *ngtcp2_cpymem(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
  return (uint8_t *)dest + n;
}

uint8_t *ngtcp2_setmem(uint8_t *dest, uint8_t b, size_t n) {
  memset(dest, b, n);
  return dest + n;
}

const void *ngtcp2_get_bytes(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
  return (uint8_t *)src + n;
}

#define LOWER_XDIGITS "0123456789abcdef"

uint8_t *ngtcp2_encode_hex(uint8_t *dest, const uint8_t *data, size_t len) {
  size_t i;

  for (i = 0; i < len; ++i) {
    *dest++ = (uint8_t)LOWER_XDIGITS[data[i] >> 4];
    *dest++ = (uint8_t)LOWER_XDIGITS[data[i] & 0xf];
  }

  return dest;
}

char *ngtcp2_encode_hex_cstr(char *dest, const uint8_t *data, size_t len) {
  uint8_t *p = ngtcp2_encode_hex((uint8_t *)dest, data, len);

  *p = '\0';

  return dest;
}

char *ngtcp2_encode_printable_ascii_cstr(char *dest, const uint8_t *data,
                                         size_t len) {
  size_t i;
  char *p = dest;
  uint8_t c;

  for (i = 0; i < len; ++i) {
    c = data[i];
    if (0x20 <= c && c <= 0x7e) {
      *p++ = (char)c;
    } else {
      *p++ = '.';
    }
  }

  *p = '\0';

  return dest;
}

char *ngtcp2_encode_ipv4_cstr(char *dest, const uint8_t *addr) {
  size_t i;
  char *p = dest;

  p = (char *)ngtcp2_encode_uint((uint8_t *)p, addr[0]);

  for (i = 1; i < 4; ++i) {
    *p++ = '.';
    p = (char *)ngtcp2_encode_uint((uint8_t *)p, addr[i]);
  }

  *p = '\0';

  return dest;
}

/*
 * write_hex_zsup writes the content of buffer pointed by |data| of
 * length |len| to |dest| in hex string.  Any leading zeros are
 * suppressed.  It returns |dest| plus the number of bytes written.
 */
static char *write_hex_zsup(char *dest, const uint8_t *data, size_t len) {
  size_t i;
  char *p = dest;
  uint8_t d;

  for (i = 0; i < len; ++i) {
    d = data[i];
    if (d >> 4) {
      break;
    }

    d &= 0xf;

    if (d) {
      *p++ = LOWER_XDIGITS[d];
      ++i;
      break;
    }
  }

  if (p == dest && i == len) {
    *p++ = '0';
    return p;
  }

  for (; i < len; ++i) {
    d = data[i];
    *p++ = LOWER_XDIGITS[d >> 4];
    *p++ = LOWER_XDIGITS[d & 0xf];
  }

  return p;
}

char *ngtcp2_encode_ipv6_cstr(char *dest, const uint8_t *addr) {
  uint16_t blks[8];
  size_t i;
  size_t zlen, zoff;
  size_t max_zlen = 0, max_zoff = 8;
  char *p = dest;

  for (i = 0; i < 16; i += sizeof(uint16_t)) {
    /* Copy in network byte order. */
    memcpy(&blks[i / sizeof(uint16_t)], addr + i, sizeof(uint16_t));
  }

  for (i = 0; i < 8;) {
    if (blks[i]) {
      ++i;
      continue;
    }

    zlen = 1;
    zoff = i;

    ++i;
    for (; i < 8 && blks[i] == 0; ++i, ++zlen)
      ;
    if (zlen > max_zlen) {
      max_zlen = zlen;
      max_zoff = zoff;
    }
  }

  /* Do not suppress a single '0' block */
  if (max_zlen == 1) {
    max_zoff = 8;
  }

  if (max_zoff != 0) {
    p = write_hex_zsup(p, (const uint8_t *)blks, sizeof(uint16_t));

    for (i = 1; i < max_zoff; ++i) {
      *p++ = ':';
      p = write_hex_zsup(p, (const uint8_t *)(blks + i), sizeof(uint16_t));
    }
  }

  if (max_zoff != 8) {
    *p++ = ':';

    if (max_zoff + max_zlen == 8) {
      *p++ = ':';
    } else {
      for (i = max_zoff + max_zlen; i < 8; ++i) {
        *p++ = ':';
        p = write_hex_zsup(p, (const uint8_t *)(blks + i), sizeof(uint16_t));
      }
    }
  }

  *p = '\0';

  return dest;
}

int ngtcp2_cmemeq(const uint8_t *a, const uint8_t *b, size_t n) {
  size_t i;
  int rv = 0;

  for (i = 0; i < n; ++i) {
    rv |= a[i] ^ b[i];
  }

  return rv == 0;
}

/* countl_zero counts the number of leading zeros in |x|.  It is
   undefined if |x| is 0. */
static int countl_zero(uint64_t x) {
#ifdef __GNUC__
  return __builtin_clzll(x);
#else  /* !defined(__GNUC__) */
  /* This is the same implementation of Go's LeadingZeros64 in
     math/bits package. */
  static const uint8_t len8tab[] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  };
  int n = 0;

  if (x >= 1ull << 32) {
    x >>= 32;
    n += 32;
  }

  if (x >= 1 << 16) {
    x >>= 16;
    n += 16;
  }

  if (x >= 1 << 8) {
    x >>= 8;
    n += 8;
  }

  return 64 - (n + len8tab[x]);
#endif /* !defined(__GNUC__) */
}

/*
 * count_digit returns the minimum number of digits to represent |x|
 * in base 10.
 *
 * credit:
 * https://lemire.me/blog/2025/01/07/counting-the-digits-of-64-bit-integers/
 */
static size_t count_digit(uint64_t x) {
  static const uint64_t count_digit_tbl[] = {
    9ull,
    99ull,
    999ull,
    9999ull,
    99999ull,
    999999ull,
    9999999ull,
    99999999ull,
    999999999ull,
    9999999999ull,
    99999999999ull,
    999999999999ull,
    9999999999999ull,
    99999999999999ull,
    999999999999999ull,
    9999999999999999ull,
    99999999999999999ull,
    999999999999999999ull,
    9999999999999999999ull,
  };
  size_t y = (size_t)(19 * (63 - countl_zero(x | 1)) >> 6);

  y += x > count_digit_tbl[y];

  return y + 1;
}

uint8_t *ngtcp2_encode_uint(uint8_t *dest, uint64_t n) {
  static const uint8_t uint_digits[] =
    "00010203040506070809101112131415161718192021222324252627282930313233343536"
    "37383940414243444546474849505152535455565758596061626364656667686970717273"
    "7475767778798081828384858687888990919293949596979899";
  uint8_t *p;
  const uint8_t *tp;

  if (n < 10) {
    *dest++ = (uint8_t)('0' + n);
    return dest;
  }

  if (n < 100) {
    tp = &uint_digits[n * 2];
    *dest++ = *tp++;
    *dest++ = *tp;
    return dest;
  }

  dest += count_digit(n);
  p = dest;

  for (; n >= 100; n /= 100) {
    p -= 2;
    tp = &uint_digits[(n % 100) * 2];
    p[0] = *tp++;
    p[1] = *tp;
  }

  if (n < 10) {
    *--p = (uint8_t)('0' + n);
    return dest;
  }

  p -= 2;
  tp = &uint_digits[n * 2];
  p[0] = *tp++;
  p[1] = *tp;

  return dest;
}

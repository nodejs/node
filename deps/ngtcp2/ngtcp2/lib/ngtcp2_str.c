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
#include <assert.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_unreachable.h"

void *ngtcp2_cpymem(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
  return (uint8_t *)dest + n;
}

uint8_t *ngtcp2_setmem(uint8_t *dest, uint8_t b, size_t n) {
  memset(dest, b, n);
  return dest + n;
}

void ngtcp2_secure_clear(void *data, size_t len) {
#ifdef WIN32
  SecureZeroMemory(data, len);
#elif defined(HAVE_EXPLICIT_BZERO)
  explicit_bzero(data, len);
#elif defined(HAVE_MEMSET_S)
  memset_s(data, len, 0, len);
#else  /* !defined(WIN32) && !defined(HAVE_EXPLICIT_BZERO) &&                  \
          !defined(HAVE_MEMSET_S) */
  static void *(*volatile memset_ptr)(void *, int, size_t) = memset;

  memset_ptr(data, 0, len);
#endif /* !defined(WIN32) && !defined(HAVE_EXPLICIT_BZERO) &&                  \
          !defined(HAVE_MEMSET_S) */
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
    *dest++ = (uint8_t)LOWER_XDIGITS[data[i] & 0xFU];
  }

  return dest;
}

size_t ngtcp2_encode_uint_hexlen(uint64_t n) {
  size_t i;
  uint8_t d;

  if (n == 0) {
    return 1;
  }

  for (i = 0; i < sizeof(n); ++i) {
    d = (uint8_t)(n >> (sizeof(n) - 1 - i) * 8);
    if (!d) {
      continue;
    }

    if (d >> 4) {
      return (sizeof(n) - i) * 2;
    }

    return (sizeof(n) - i) * 2 - 1;
  }

  ngtcp2_unreachable();
}

uint8_t *ngtcp2_encode_uint_hex(uint8_t *dest, uint64_t n) {
  size_t i;
  uint8_t d;

  if (n == 0) {
    *dest++ = '0';

    return dest;
  }

  for (i = 0; i < sizeof(n); ++i) {
    d = (uint8_t)(n >> (sizeof(n) - 1 - i) * 8);
    if (d) {
      if (d >> 4) {
        *dest++ = (uint8_t)LOWER_XDIGITS[d >> 4];
      }

      *dest++ = (uint8_t)LOWER_XDIGITS[d & 0xFU];
      ++i;

      break;
    }
  }

  for (; i < sizeof(n); ++i) {
    d = (uint8_t)(n >> (sizeof(n) - 1 - i) * 8);

    *dest++ = (uint8_t)LOWER_XDIGITS[d >> 4];
    *dest++ = (uint8_t)LOWER_XDIGITS[d & 0xFU];
  }

  return dest;
}

uint8_t *ngtcp2_encode_printable_ascii(uint8_t *dest, const uint8_t *data,
                                       size_t len) {
  size_t i;
  uint8_t c;

  for (i = 0; i < len; ++i) {
    c = data[i];
    if (0x20 <= c && c <= 0x7E) {
      *dest++ = c;
    } else {
      *dest++ = '.';
    }
  }

  return dest;
}

uint8_t *ngtcp2_encode_ipv4(uint8_t *dest, const ngtcp2_in_addr *addr) {
  size_t i;
  const uint8_t *in = (const uint8_t *)addr;

  dest = ngtcp2_encode_uint(dest, in[0]);

  for (i = 1; i < 4; ++i) {
    *dest++ = '.';
    dest = ngtcp2_encode_uint(dest, in[i]);
  }

  return dest;
}

/*
 * write_hex_zsup writes the content of buffer pointed by |data| of
 * length |len| to |dest| in hex string.  Any leading zeros are
 * suppressed.  It returns |dest| plus the number of bytes written.
 */
static uint8_t *write_hex_zsup(uint8_t *dest, const uint8_t *data, size_t len) {
  size_t i;
  uint8_t *p = dest;
  uint8_t d;

  for (i = 0; i < len; ++i) {
    d = data[i];
    if (d >> 4) {
      break;
    }

    d &= 0xFU;

    if (d) {
      *p++ = (uint8_t)LOWER_XDIGITS[d];
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
    *p++ = (uint8_t)LOWER_XDIGITS[d >> 4];
    *p++ = (uint8_t)LOWER_XDIGITS[d & 0xFU];
  }

  return p;
}

uint8_t *ngtcp2_encode_ipv6(uint8_t *dest, const ngtcp2_in6_addr *addr) {
  uint16_t blks[8];
  size_t i;
  size_t zlen, zoff;
  size_t max_zlen = 0, max_zoff = 8;
  const uint8_t *in = (const uint8_t *)addr;

  for (i = 0; i < 16; i += sizeof(uint16_t)) {
    /* Copy in network byte order. */
    memcpy(&blks[i / sizeof(uint16_t)], in + i, sizeof(uint16_t));
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
    dest = write_hex_zsup(dest, (const uint8_t *)blks, sizeof(uint16_t));

    for (i = 1; i < max_zoff; ++i) {
      *dest++ = ':';
      dest =
        write_hex_zsup(dest, (const uint8_t *)(blks + i), sizeof(uint16_t));
    }
  }

  if (max_zoff != 8) {
    *dest++ = ':';

    if (max_zoff + max_zlen == 8) {
      *dest++ = ':';
    } else {
      for (i = max_zoff + max_zlen; i < 8; ++i) {
        *dest++ = ':';
        dest =
          write_hex_zsup(dest, (const uint8_t *)(blks + i), sizeof(uint16_t));
      }
    }
  }

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

  if (x >= 1ULL << 32) {
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
    9ULL,
    99ULL,
    999ULL,
    9999ULL,
    99999ULL,
    999999ULL,
    9999999ULL,
    99999999ULL,
    999999999ULL,
    9999999999ULL,
    99999999999ULL,
    999999999999ULL,
    9999999999999ULL,
    99999999999999ULL,
    999999999999999ULL,
    9999999999999999ULL,
    99999999999999999ULL,
    999999999999999999ULL,
    9999999999999999999ULL,
  };
  size_t y = (size_t)(19 * (63 - countl_zero(x | 1)) >> 6);

  y += x > count_digit_tbl[y];

  return y + 1;
}

size_t ngtcp2_encode_uintlen(uint64_t n) { return count_digit(n); }

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

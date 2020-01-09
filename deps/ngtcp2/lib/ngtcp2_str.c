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

void *ngtcp2_cpymem(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
  return (uint8_t *)dest + n;
}

uint8_t *ngtcp2_setmem(uint8_t *dest, uint8_t b, size_t n) {
  memset(dest, b, n);
  return dest + n;
}

#define LOWER_XDIGITS "0123456789abcdef"

uint8_t *ngtcp2_encode_hex(uint8_t *dest, const uint8_t *data, size_t len) {
  size_t i;
  uint8_t *p = dest;

  for (i = 0; i < len; ++i) {
    *p++ = (uint8_t)LOWER_XDIGITS[data[i] >> 4];
    *p++ = (uint8_t)LOWER_XDIGITS[data[i] & 0xf];
  }

  *p = '\0';

  return dest;
}

char *ngtcp2_encode_printable_ascii(char *dest, const uint8_t *data,
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

int ngtcp2_verify_stateless_reset_token(const uint8_t *want,
                                        const uint8_t *got) {
  return !ngtcp2_check_invalid_stateless_reset_token(got) &&
                 ngtcp2_cmemeq(want, got, NGTCP2_STATELESS_RESET_TOKENLEN)
             ? 0
             : NGTCP2_ERR_INVALID_ARGUMENT;
}

int ngtcp2_check_invalid_stateless_reset_token(const uint8_t *token) {
  static uint8_t invalid_token[NGTCP2_STATELESS_RESET_TOKENLEN] = {0};

  return 0 == memcmp(invalid_token, token, NGTCP2_STATELESS_RESET_TOKENLEN);
}

int ngtcp2_cmemeq(const uint8_t *a, const uint8_t *b, size_t n) {
  size_t i;
  int rv = 0;

  for (i = 0; i < n; ++i) {
    rv |= a[i] ^ b[i];
  }

  return rv == 0;
}

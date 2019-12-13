/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#include "ngtcp2_addr.h"

#include <string.h>

ngtcp2_addr *ngtcp2_addr_init(ngtcp2_addr *dest, const void *addr,
                              size_t addrlen, void *user_data) {
  dest->addrlen = addrlen;
  dest->addr = (uint8_t *)addr;
  dest->user_data = user_data;
  return dest;
}

void ngtcp2_addr_copy(ngtcp2_addr *dest, const ngtcp2_addr *src) {
  dest->addrlen = src->addrlen;
  if (src->addrlen) {
    memcpy(dest->addr, src->addr, src->addrlen);
  }
  dest->user_data = src->user_data;
}

void ngtcp2_addr_copy_byte(ngtcp2_addr *dest, const void *addr,
                           size_t addrlen) {
  dest->addrlen = addrlen;
  if (addrlen) {
    memcpy(dest->addr, addr, addrlen);
  }
}

int ngtcp2_addr_eq(const ngtcp2_addr *a, const ngtcp2_addr *b) {
  return a->addrlen == b->addrlen && memcmp(a->addr, b->addr, a->addrlen) == 0;
}

int ngtcp2_addr_empty(const ngtcp2_addr *addr) { return addr->addrlen == 0; }

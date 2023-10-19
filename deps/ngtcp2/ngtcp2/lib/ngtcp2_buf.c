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
#include "ngtcp2_buf.h"
#include "ngtcp2_mem.h"

void ngtcp2_buf_init(ngtcp2_buf *buf, uint8_t *begin, size_t len) {
  buf->begin = buf->pos = buf->last = begin;
  buf->end = begin + len;
}

void ngtcp2_buf_reset(ngtcp2_buf *buf) { buf->pos = buf->last = buf->begin; }

size_t ngtcp2_buf_cap(const ngtcp2_buf *buf) {
  return (size_t)(buf->end - buf->begin);
}

int ngtcp2_buf_chain_new(ngtcp2_buf_chain **pbufchain, size_t len,
                         const ngtcp2_mem *mem) {
  *pbufchain = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_buf_chain) + len);
  if (*pbufchain == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  (*pbufchain)->next = NULL;

  ngtcp2_buf_init(&(*pbufchain)->buf,
                  (uint8_t *)(*pbufchain) + sizeof(ngtcp2_buf_chain), len);

  return 0;
}

void ngtcp2_buf_chain_del(ngtcp2_buf_chain *bufchain, const ngtcp2_mem *mem) {
  ngtcp2_mem_free(mem, bufchain);
}

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

void ngtcp2_buf_init(ngtcp2_buf *buf, uint8_t *begin, size_t len) {
  buf->begin = buf->pos = buf->last = begin;
  buf->end = begin + len;
}

void ngtcp2_buf_reset(ngtcp2_buf *buf) { buf->pos = buf->last = buf->begin; }

size_t ngtcp2_buf_left(const ngtcp2_buf *buf) {
  return (size_t)(buf->end - buf->last);
}

size_t ngtcp2_buf_len(const ngtcp2_buf *buf) {
  return (size_t)(buf->last - buf->pos);
}

size_t ngtcp2_buf_cap(const ngtcp2_buf *buf) {
  return (size_t)(buf->end - buf->begin);
}

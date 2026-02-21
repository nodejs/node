/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "nghttp3_buf.h"

void nghttp3_buf_init(nghttp3_buf *buf) {
  buf->begin = buf->end = buf->pos = buf->last = NULL;
}

void nghttp3_buf_wrap_init(nghttp3_buf *buf, uint8_t *src, size_t len) {
  buf->begin = buf->pos = buf->last = src;
  buf->end = buf->begin + len;
}

void nghttp3_buf_free(nghttp3_buf *buf, const nghttp3_mem *mem) {
  nghttp3_mem_free(mem, buf->begin);
}

size_t nghttp3_buf_left(const nghttp3_buf *buf) {
  return (size_t)(buf->end - buf->last);
}

size_t nghttp3_buf_len(const nghttp3_buf *buf) {
  return (size_t)(buf->last - buf->pos);
}

size_t nghttp3_buf_cap(const nghttp3_buf *buf) {
  return (size_t)(buf->end - buf->begin);
}

size_t nghttp3_buf_offset(const nghttp3_buf *buf) {
  return (size_t)(buf->pos - buf->begin);
}

void nghttp3_buf_reset(nghttp3_buf *buf) { buf->pos = buf->last = buf->begin; }

int nghttp3_buf_reserve(nghttp3_buf *buf, size_t size, const nghttp3_mem *mem) {
  uint8_t *p;
  nghttp3_ssize pos_offset, last_offset;

  if ((size_t)(buf->end - buf->begin) >= size) {
    return 0;
  }

  pos_offset = buf->pos - buf->begin;
  last_offset = buf->last - buf->begin;

  p = nghttp3_mem_realloc(mem, buf->begin, size);
  if (p == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  buf->begin = p;
  buf->end = p + size;
  buf->pos = p + pos_offset;
  buf->last = p + last_offset;

  return 0;
}

void nghttp3_buf_swap(nghttp3_buf *a, nghttp3_buf *b) {
  nghttp3_buf c = *a;

  *a = *b;
  *b = c;
}

void nghttp3_typed_buf_init(nghttp3_typed_buf *tbuf, const nghttp3_buf *buf,
                            nghttp3_buf_type type) {
  tbuf->buf = *buf;
  tbuf->type = type;
  tbuf->buf.begin = tbuf->buf.pos;
}

void nghttp3_typed_buf_shared_init(nghttp3_typed_buf *tbuf,
                                   const nghttp3_buf *chunk) {
  tbuf->buf = *chunk;
  tbuf->type = NGHTTP3_BUF_TYPE_SHARED;
  tbuf->buf.begin = tbuf->buf.pos = tbuf->buf.last;
}

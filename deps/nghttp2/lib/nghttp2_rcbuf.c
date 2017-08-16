/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2016 Tatsuhiro Tsujikawa
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
#include "nghttp2_rcbuf.h"

#include <string.h>
#include <assert.h>

#include "nghttp2_mem.h"
#include "nghttp2_helper.h"

int nghttp2_rcbuf_new(nghttp2_rcbuf **rcbuf_ptr, size_t size,
                      nghttp2_mem *mem) {
  uint8_t *p;

  p = nghttp2_mem_malloc(mem, sizeof(nghttp2_rcbuf) + size);
  if (p == NULL) {
    return NGHTTP2_ERR_NOMEM;
  }

  *rcbuf_ptr = (void *)p;

  (*rcbuf_ptr)->mem_user_data = mem->mem_user_data;
  (*rcbuf_ptr)->free = mem->free;
  (*rcbuf_ptr)->base = p + sizeof(nghttp2_rcbuf);
  (*rcbuf_ptr)->len = size;
  (*rcbuf_ptr)->ref = 1;

  return 0;
}

int nghttp2_rcbuf_new2(nghttp2_rcbuf **rcbuf_ptr, const uint8_t *src,
                       size_t srclen, nghttp2_mem *mem) {
  int rv;

  rv = nghttp2_rcbuf_new(rcbuf_ptr, srclen + 1, mem);
  if (rv != 0) {
    return rv;
  }

  (*rcbuf_ptr)->len = srclen;
  *nghttp2_cpymem((*rcbuf_ptr)->base, src, srclen) = '\0';

  return 0;
}

/*
 * Frees |rcbuf| itself, regardless of its reference cout.
 */
void nghttp2_rcbuf_del(nghttp2_rcbuf *rcbuf) {
  nghttp2_mem_free2(rcbuf->free, rcbuf, rcbuf->mem_user_data);
}

void nghttp2_rcbuf_incref(nghttp2_rcbuf *rcbuf) {
  if (rcbuf->ref == -1) {
    return;
  }

  ++rcbuf->ref;
}

void nghttp2_rcbuf_decref(nghttp2_rcbuf *rcbuf) {
  if (rcbuf == NULL || rcbuf->ref == -1) {
    return;
  }

  assert(rcbuf->ref > 0);

  if (--rcbuf->ref == 0) {
    nghttp2_rcbuf_del(rcbuf);
  }
}

nghttp2_vec nghttp2_rcbuf_get_buf(nghttp2_rcbuf *rcbuf) {
  nghttp2_vec res = {rcbuf->base, rcbuf->len};
  return res;
}

/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2016 nghttp2 contributors
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
#include "nghttp3_rcbuf.h"

#include <assert.h>

#include "nghttp3_mem.h"
#include "nghttp3_str.h"

int nghttp3_rcbuf_new(nghttp3_rcbuf **rcbuf_ptr, size_t size,
                      const nghttp3_mem *mem) {
  uint8_t *p;

  p = nghttp3_mem_malloc(mem, sizeof(nghttp3_rcbuf) + size);
  if (p == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  *rcbuf_ptr = (void *)p;

  (*rcbuf_ptr)->mem = mem;
  (*rcbuf_ptr)->base = p + sizeof(nghttp3_rcbuf);
  (*rcbuf_ptr)->len = size;
  (*rcbuf_ptr)->ref = 1;

  return 0;
}

int nghttp3_rcbuf_new2(nghttp3_rcbuf **rcbuf_ptr, const uint8_t *src,
                       size_t srclen, const nghttp3_mem *mem) {
  int rv;
  uint8_t *p;

  rv = nghttp3_rcbuf_new(rcbuf_ptr, srclen + 1, mem);
  if (rv != 0) {
    return rv;
  }

  (*rcbuf_ptr)->len = srclen;
  p = (*rcbuf_ptr)->base;

  if (srclen) {
    p = nghttp3_cpymem(p, src, srclen);
  }

  *p = '\0';

  return 0;
}

/*
 * Frees |rcbuf| itself, regardless of its reference cout.
 */
void nghttp3_rcbuf_del(nghttp3_rcbuf *rcbuf) {
  nghttp3_mem_free(rcbuf->mem, rcbuf);
}

void nghttp3_rcbuf_incref(nghttp3_rcbuf *rcbuf) {
  if (rcbuf->ref == -1) {
    return;
  }

  ++rcbuf->ref;
}

void nghttp3_rcbuf_decref(nghttp3_rcbuf *rcbuf) {
  if (rcbuf == NULL || rcbuf->ref == -1) {
    return;
  }

  assert(rcbuf->ref > 0);

  if (--rcbuf->ref == 0) {
    nghttp3_rcbuf_del(rcbuf);
  }
}

nghttp3_vec nghttp3_rcbuf_get_buf(const nghttp3_rcbuf *rcbuf) {
  nghttp3_vec res = {rcbuf->base, rcbuf->len};
  return res;
}

int nghttp3_rcbuf_is_static(const nghttp3_rcbuf *rcbuf) {
  return rcbuf->ref == -1;
}

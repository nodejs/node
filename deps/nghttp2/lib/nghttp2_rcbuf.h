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
#ifndef NGHTTP2_RCBUF_H
#define NGHTTP2_RCBUF_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>

struct nghttp2_rcbuf {
  /* custom memory allocator belongs to the mem parameter when
     creating this object. */
  void *mem_user_data;
  nghttp2_free free;
  /* The pointer to the underlying buffer */
  uint8_t *base;
  /* Size of buffer pointed by |base|. */
  size_t len;
  /* Reference count */
  int32_t ref;
};

/*
 * Allocates nghttp2_rcbuf object with |size| as initial buffer size.
 * When the function succeeds, the reference count becomes 1.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM:
 *     Out of memory.
 */
int nghttp2_rcbuf_new(nghttp2_rcbuf **rcbuf_ptr, size_t size, nghttp2_mem *mem);

/*
 * Like nghttp2_rcbuf_new(), but initializes the buffer with |src| of
 * length |srclen|.  This function allocates additional byte at the
 * end and puts '\0' into it, so that the resulting buffer could be
 * used as NULL-terminated string.  Still (*rcbuf_ptr)->len equals to
 * |srclen|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM:
 *     Out of memory.
 */
int nghttp2_rcbuf_new2(nghttp2_rcbuf **rcbuf_ptr, const uint8_t *src,
                       size_t srclen, nghttp2_mem *mem);

/*
 * Frees |rcbuf| itself, regardless of its reference cout.
 */
void nghttp2_rcbuf_del(nghttp2_rcbuf *rcbuf);

#endif /* NGHTTP2_RCBUF_H */

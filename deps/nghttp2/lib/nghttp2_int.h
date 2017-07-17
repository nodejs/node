/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
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
#ifndef NGHTTP2_INT_H
#define NGHTTP2_INT_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>

/* Macros, types and constants for internal use */

/* "less" function, return nonzero if |lhs| is less than |rhs|. */
typedef int (*nghttp2_less)(const void *lhs, const void *rhs);

/* Internal error code. They must be in the range [-499, -100],
   inclusive. */
typedef enum {
  NGHTTP2_ERR_CREDENTIAL_PENDING = -101,
  NGHTTP2_ERR_IGN_HEADER_BLOCK = -103,
  NGHTTP2_ERR_IGN_PAYLOAD = -104,
  /*
   * Invalid HTTP header field was received but it can be treated as
   * if it was not received because of compatibility reasons.
   */
  NGHTTP2_ERR_IGN_HTTP_HEADER = -105,
  /*
   * Invalid HTTP header field was received, and it is ignored.
   * Unlike NGHTTP2_ERR_IGN_HTTP_HEADER, this does not invoke
   * nghttp2_on_invalid_header_callback.
   */
  NGHTTP2_ERR_REMOVE_HTTP_HEADER = -106
} nghttp2_internal_error;

#endif /* NGHTTP2_INT_H */

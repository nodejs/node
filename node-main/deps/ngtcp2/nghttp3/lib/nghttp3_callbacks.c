/*
 * nghttp3
 *
 * Copyright (c) 2025 nghttp3 contributors
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
#include "nghttp3_callbacks.h"

#include <string.h>
#include <assert.h>

#include "nghttp3_unreachable.h"

static void callbacks_copy(nghttp3_callbacks *dest,
                           const nghttp3_callbacks *src,
                           int callbacks_version) {
  assert(callbacks_version != NGHTTP3_CALLBACKS_VERSION);

  memcpy(dest, src, nghttp3_callbackslen_version(callbacks_version));
}

const nghttp3_callbacks *
nghttp3_callbacks_convert_to_latest(nghttp3_callbacks *dest,
                                    int callbacks_version,
                                    const nghttp3_callbacks *src) {
  if (callbacks_version == NGHTTP3_CALLBACKS_VERSION) {
    return src;
  }

  memset(dest, 0, sizeof(*dest));

  callbacks_copy(dest, src, callbacks_version);

  return dest;
}

void nghttp3_callbacks_convert_to_old(int callbacks_version,
                                      nghttp3_callbacks *dest,
                                      const nghttp3_callbacks *src) {
  assert(callbacks_version != NGHTTP3_CALLBACKS_VERSION);

  callbacks_copy(dest, src, callbacks_version);
}

size_t nghttp3_callbackslen_version(int callbacks_version) {
  nghttp3_callbacks callbacks;

  switch (callbacks_version) {
  case NGHTTP3_CALLBACKS_VERSION:
    return sizeof(callbacks);
  case NGHTTP3_CALLBACKS_V1:
    return offsetof(nghttp3_callbacks, recv_settings) +
           sizeof(callbacks.recv_settings);
  default:
    nghttp3_unreachable();
  }
}

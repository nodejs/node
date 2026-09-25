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
#ifndef NGHTTP3_CALLBACKS_H
#define NGHTTP3_CALLBACKS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <nghttp3/nghttp3.h>

/*
 * nghttp3_callbacks_convert_to_latest converts |src| of version
 * |callbacks_version| to the latest version
 * NGHTTP3_CALLBACKS_VERSION.
 *
 * |dest| must point to the latest version.  |src| may be the older
 * version, and if so, it may have fewer fields.  Accessing those
 * fields causes undefined behavior.
 *
 * If |callbacks_version| == NGHTTP3_CALLBACKS_VERSION, no conversion
 * is made, and |src| is returned.  Otherwise, first |dest| is
 * zero-initialized, and then all valid fields in |src| are copied
 * into |dest|.  Finally, |dest| is returned.
 */
const nghttp3_callbacks *nghttp3_callbacks_convert_to_latest(
  nghttp3_callbacks *dest, int callbacks_version, const nghttp3_callbacks *src);

/*
 * nghttp3_callbacks_convert_to_old converts |src| of the latest
 * version to |dest| of version |callbacks_version|.
 *
 * |callbacks_version| must not be the latest version
 *  NGHTTP3_CALLBACKS_VERSION.
 *
 * |dest| points to the older version, and it may have fewer fields.
 * Accessing those fields causes undefined behavior.
 *
 * This function copies all valid fields in version
 * |callbacks_version| from |src| to |dest|.
 */
void nghttp3_callbacks_convert_to_old(int callbacks_version,
                                      nghttp3_callbacks *dest,
                                      const nghttp3_callbacks *src);

/*
 * nghttp3_callbackslen_version returns the effective length of
 * nghttp3_callbacks at the version |callbacks_version|.
 */
size_t nghttp3_callbackslen_version(int callbacks_version);

#endif /* !defined(NGHTTP3_CALLBACKS_H) */

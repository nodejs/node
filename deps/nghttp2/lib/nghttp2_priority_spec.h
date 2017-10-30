/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2014 Tatsuhiro Tsujikawa
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
#ifndef NGHTTP2_PRIORITY_SPEC_H
#define NGHTTP2_PRIORITY_SPEC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>

/*
 * This function normalizes pri_spec->weight if it is out of range.
 * If pri_spec->weight is less than NGHTTP2_MIN_WEIGHT, it is set to
 * NGHTTP2_MIN_WEIGHT.  If pri_spec->weight is larger than
 * NGHTTP2_MAX_WEIGHT, it is set to NGHTTP2_MAX_WEIGHT.
 */
void nghttp2_priority_spec_normalize_weight(nghttp2_priority_spec *pri_spec);

#endif /* NGHTTP2_PRIORITY_SPEC_H */

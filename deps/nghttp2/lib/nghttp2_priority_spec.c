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
#include "nghttp2_priority_spec.h"

void nghttp2_priority_spec_init(nghttp2_priority_spec *pri_spec,
                                int32_t stream_id, int32_t weight,
                                int exclusive) {
  pri_spec->stream_id = stream_id;
  pri_spec->weight = weight;
  pri_spec->exclusive = exclusive != 0;
}

void nghttp2_priority_spec_default_init(nghttp2_priority_spec *pri_spec) {
  pri_spec->stream_id = 0;
  pri_spec->weight = NGHTTP2_DEFAULT_WEIGHT;
  pri_spec->exclusive = 0;
}

int nghttp2_priority_spec_check_default(const nghttp2_priority_spec *pri_spec) {
  return pri_spec->stream_id == 0 &&
         pri_spec->weight == NGHTTP2_DEFAULT_WEIGHT && pri_spec->exclusive == 0;
}

void nghttp2_priority_spec_normalize_weight(nghttp2_priority_spec *pri_spec) {
  if (pri_spec->weight < NGHTTP2_MIN_WEIGHT) {
    pri_spec->weight = NGHTTP2_MIN_WEIGHT;
  } else if (pri_spec->weight > NGHTTP2_MAX_WEIGHT) {
    pri_spec->weight = NGHTTP2_MAX_WEIGHT;
  }
}

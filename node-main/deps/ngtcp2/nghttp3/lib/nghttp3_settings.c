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
#include "nghttp3_settings.h"

#include <string.h>
#include <assert.h>

#include "nghttp3_conv.h"
#include "nghttp3_unreachable.h"

/* NGHTTP3_QPACK_ENCODER_MAX_DTABLE_CAPACITY is the upper bound of the
   dynamic table capacity that QPACK encoder is willing to use. */
#define NGHTTP3_QPACK_ENCODER_MAX_DTABLE_CAPACITY 4096

void nghttp3_settings_default_versioned(int settings_version,
                                        nghttp3_settings *settings) {
  size_t len = nghttp3_settingslen_version(settings_version);

  memset(settings, 0, len);

  switch (settings_version) {
  case NGHTTP3_SETTINGS_VERSION:
  case NGHTTP3_SETTINGS_V1:
    settings->max_field_section_size = NGHTTP3_VARINT_MAX;
    settings->qpack_encoder_max_dtable_capacity =
      NGHTTP3_QPACK_ENCODER_MAX_DTABLE_CAPACITY;

    break;
  }
}

static void settings_copy(nghttp3_settings *dest, const nghttp3_settings *src,
                          int settings_version) {
  assert(settings_version != NGHTTP3_SETTINGS_VERSION);

  memcpy(dest, src, nghttp3_settingslen_version(settings_version));
}

const nghttp3_settings *
nghttp3_settings_convert_to_latest(nghttp3_settings *dest, int settings_version,
                                   const nghttp3_settings *src) {
  if (settings_version == NGHTTP3_SETTINGS_VERSION) {
    return src;
  }

  nghttp3_settings_default(dest);

  settings_copy(dest, src, settings_version);

  return dest;
}

void nghttp3_settings_convert_to_old(int settings_version,
                                     nghttp3_settings *dest,
                                     const nghttp3_settings *src) {
  assert(settings_version != NGHTTP3_SETTINGS_VERSION);

  settings_copy(dest, src, settings_version);
}

size_t nghttp3_settingslen_version(int settings_version) {
  nghttp3_settings settings;

  switch (settings_version) {
  case NGHTTP3_SETTINGS_VERSION:
    return sizeof(settings);
  case NGHTTP3_SETTINGS_V1:
    return offsetof(nghttp3_settings, h3_datagram) +
           sizeof(settings.h3_datagram);
  default:
    nghttp3_unreachable();
  }
}

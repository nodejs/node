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
#ifndef NGHTTP3_SETTINGS_H
#define NGHTTP3_SETTINGS_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <nghttp3/nghttp3.h>

/*
 * nghttp3_settings_convert_to_latest converts |src| of version
 * |settings_version| to the latest version NGHTTP3_SETTINGS_VERSION.
 *
 * |dest| must point to the latest version.  |src| may be the older
 * version, and if so, it may have fewer fields.  Accessing those
 * fields causes undefined behavior.
 *
 * If |settings_version| == NGHTTP3_SETTINGS_VERSION, no conversion is
 * made, and |src| is returned.  Otherwise, first |dest| is
 * initialized via nghttp3_settings_default, and then all valid fields
 * in |src| are copied into |dest|.  Finally, |dest| is returned.
 */
const nghttp3_settings *
nghttp3_settings_convert_to_latest(nghttp3_settings *dest, int settings_version,
                                   const nghttp3_settings *src);

/*
 * nghttp3_settings_convert_to_old converts |src| of the latest
 * version to |dest| of version |settings_version|.
 *
 * |settings_version| must not be the latest version
 *  NGHTTP3_SETTINGS_VERSION.
 *
 * |dest| points to the older version, and it may have fewer fields.
 * Accessing those fields causes undefined behavior.
 *
 * This function copies all valid fields in version |settings_version|
 * from |src| to |dest|.
 */
void nghttp3_settings_convert_to_old(int settings_version,
                                     nghttp3_settings *dest,
                                     const nghttp3_settings *src);

/*
 * nghttp3_settingslen_version returns the effective length of
 * nghttp3_settings at the version |settings_version|.
 */
size_t nghttp3_settingslen_version(int settings_version);

#endif /* !defined(NGHTTP3_SETTINGS_H) */

/*
 * urlparse
 *
 * Copyright (c) 2024 urlparse contributors
 * Copyright (c) 2023 sfparse contributors
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2015 nghttp2 contributors
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
#ifndef URLPARSE_H
#define URLPARSE_H

/* Define WIN32 when build target is Win32 API (borrowed from
   libcurl) */
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#  define WIN32
#endif /* (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32) */

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#if defined(_MSC_VER) && (_MSC_VER < 1800)
/* MSVC < 2013 does not have inttypes.h because it is not C99
   compliant.  See compiler macros and version number in
   https://sourceforge.net/p/predef/wiki/Compilers/ */
#  include <stdint.h>
#else /* !(defined(_MSC_VER) && (_MSC_VER < 1800)) */
#  include <inttypes.h>
#endif /* !(defined(_MSC_VER) && (_MSC_VER < 1800)) */
#include <sys/types.h>
#include <stddef.h>

/**
 * @macro
 *
 * :macro:`URLPARSE_ERR_PARSE` indicates that an error occurred while
 * the parser is processing a URL.
 */
#define URLPARSE_ERR_PARSE -1

/**
 * @enum
 *
 * :type:`urlparse_url_fields` defines URL component fields.
 */
typedef enum urlparse_url_fields {
  /**
   * :enum:`URLPARSE_SCHEMA` is a URL scheme.
   */
  URLPARSE_SCHEMA = 0,
  /**
   * :enum:`URLPARSE_HOST` is a host.
   */
  URLPARSE_HOST = 1,
  /**
   * :enum:`URLPARSE_PORT` is a port.
   */
  URLPARSE_PORT = 2,
  /**
   * :enum:`URLPARSE_PATH` is a path.
   */
  URLPARSE_PATH = 3,
  /**
   * :enum:`URLPARSE_QUERY` is a query.
   */
  URLPARSE_QUERY = 4,
  /**
   * :enum:`URLPARSE_FRAGMENT` is a fragment.
   */
  URLPARSE_FRAGMENT = 5,
  /**
   * :enum:`URLPARSE_USERINFO` is a userinfo.
   */
  URLPARSE_USERINFO = 6,
  /**
   * :enum:`URLPARSE_MAX` is the number of fields.
   */
  URLPARSE_MAX = 7
} urlparse_url_fields;

/**
 * @struct
 *
 * :type:`urlparse_url` is a struct to store the result of parsing a
 * URL.
 */
typedef struct urlparse_url {
  /**
   * :member:`field_set` is a bitmask of (1 << :type:`URLPARSE_*
   * <urlparse_url_fields>`) values.
   */
  uint16_t field_set;
  /**
   * :member:`port` is the integer representation of
   * :enum:`URLPARSE_PORT <urlparse_url_fields.URLPARSE_PORT>` string.
   * It is assigned only when (:member:`field_set` & (1 <<
   * :enum:`URLPARSE_PORT <urlparse_url_fields.URLPARSE_PORT>`)) is
   * nonzero.
   */
  uint16_t port;

  /**
   * @anonstruct_start
   *
   * @struct_urlparse_field_data
   *
   * :member:`field_data` stores the position and its length of each
   * URL component if the corresponding bit is set in
   * :member:`field_set`.  For example,
   * field_data[:enum:`URLPARSE_HOST
   * <urlparse_url_fields.URLPARSE_HOST>`] is assigned if
   * (:member:`field_set` & (1 << :enum:`URLPARSE_HOST
   * <urlparse_url_fields.URLPARSE_HOST>`)) is nonzero.
   */
  struct {
    /**
     * :member:`off` is an offset into buffer in which field starts.
     */
    uint16_t off;
    /**
     * :member:`len` is a length of run in buffer.
     */
    uint16_t len;
    /**
     * @anonstruct_end
     */
  } field_data[URLPARSE_MAX];
} urlparse_url;

/**
 * @function
 *
 * `urlparse_parse_url` parses |url| of length |urllen| bytes, and
 * stores the result in |u|.  If |is_connect| is nonzero, it parses
 * the URL as a request target that appears in CONNECT request, that
 * is, consisting of only the host and port number.
 *
 * This function initializes |u| before its use.  If this function
 * returns nonzero, |u| might not be initialized.
 *
 * This function returns 0 if it succeeds, or
 * :macro:`URLPARSE_ERR_PARSE`.
 */
int urlparse_parse_url(const char *url, size_t urllen, int is_connect,
                       urlparse_url *u);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* !defined(URLPARSE_H) */

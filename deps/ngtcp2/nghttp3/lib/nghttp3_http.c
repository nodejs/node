/*
 * nghttp3
 *
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
#include "nghttp3_http.h"

#include <string.h>
#include <assert.h>

#ifdef __AVX2__
#  include <immintrin.h>
#endif /* __AVX2__ */

#include "nghttp3_stream.h"
#include "nghttp3_macro.h"
#include "nghttp3_conv.h"
#include "nghttp3_unreachable.h"
#include "sfparse/sfparse.h"

static uint8_t downcase(uint8_t c) {
  return 'A' <= c && c <= 'Z' ? (uint8_t)(c - 'A' + 'a') : c;
}

/*
 * memieq returns 1 if the data pointed by |a| of length |n| equals to
 * |b| of the same length in case-insensitive manner.  The data
 * pointed by |a| must not include upper cased letters (A-Z).
 */
static int memieq(const void *a, const void *b, size_t n) {
  size_t i;
  const uint8_t *aa = a, *bb = b;

  for (i = 0; i < n; ++i) {
    if (aa[i] != downcase(bb[i])) {
      return 0;
    }
  }
  return 1;
}

#define lstrieq(A, B, N)                                                       \
  (nghttp3_strlen_lit((A)) == (N) && memieq((A), (B), (N)))

static int32_t parse_status_code(const uint8_t *s, size_t len) {
  if (len != 3 || '1' > s[0] || s[0] > '9' || '0' > s[1] || s[1] > '9' ||
      '0' > s[2] || s[2] > '9') {
    return -1;
  }

  return (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0');
}

static int64_t parse_uint(const uint8_t *s, size_t len) {
  uint64_t n = 0;
  uint32_t c;
  size_t i;

  if (len == 0) {
    return -1;
  }

  for (i = 0; i < len; ++i) {
    if ('0' > s[i] || s[i] > '9') {
      return -1;
    }

    c = s[i] - '0';

    if (n > (NGHTTP3_MAX_VARINT - c) / 10) {
      return -1;
    }

    n = n * 10 + c;
  }

  return (int64_t)n;
}

static int check_pseudo_header(nghttp3_http_state *http,
                               const nghttp3_qpack_nv *nv, uint32_t flag) {
  if ((http->flags & flag) || nv->value->len == 0) {
    return 0;
  }
  http->flags |= flag;
  return 1;
}

static int expect_response_body(const nghttp3_http_state *http) {
  return (http->flags & NGHTTP3_HTTP_FLAG_METH_HEAD) == 0 &&
         http->status_code / 100 != 1 && http->status_code != 304 &&
         http->status_code != 204;
}

/* For "http" or "https" URIs, OPTIONS request may have "*" in :path
   header field to represent system-wide OPTIONS request.  Otherwise,
   :path header field value must start with "/".  This function must
   be called after ":method" header field was received.  This function
   returns nonzero if path is valid.*/
static int check_path_flags(const nghttp3_http_state *http) {
  return (http->flags & NGHTTP3_HTTP_FLAG_SCHEME_HTTP) == 0 ||
         ((http->flags & NGHTTP3_HTTP_FLAG_PATH_REGULAR) ||
          ((http->flags & NGHTTP3_HTTP_FLAG_METH_OPTIONS) &&
           (http->flags & NGHTTP3_HTTP_FLAG_PATH_ASTERISK)));
}

static int is_ws(uint8_t c) {
  switch (c) {
  case ' ':
  case '\t':
    return 1;
  default:
    return 0;
  }
}

int nghttp3_http_parse_priority(nghttp3_pri *dest, const uint8_t *value,
                                size_t valuelen) {
  nghttp3_pri pri = *dest;
  sfparse_parser sfp;
  sfparse_vec key;
  sfparse_value val;
  int rv;

  sfparse_parser_init(&sfp, value, valuelen);

  for (;;) {
    rv = sfparse_parser_dict(&sfp, &key, &val);
    if (rv != 0) {
      if (rv == SFPARSE_ERR_EOF) {
        break;
      }

      return NGHTTP3_ERR_INVALID_ARGUMENT;
    }

    if (key.len != 1) {
      continue;
    }

    switch (key.base[0]) {
    case 'i':
      if (val.type != SFPARSE_TYPE_BOOLEAN) {
        return NGHTTP3_ERR_INVALID_ARGUMENT;
      }

      pri.inc = (uint8_t)val.boolean;

      break;
    case 'u':
      if (val.type != SFPARSE_TYPE_INTEGER ||
          val.integer < NGHTTP3_URGENCY_HIGH ||
          NGHTTP3_URGENCY_LOW < val.integer) {
        return NGHTTP3_ERR_INVALID_ARGUMENT;
      }

      pri.urgency = (uint32_t)val.integer;

      break;
    }
  }

  *dest = pri;

  return 0;
}

int nghttp3_pri_parse_priority_versioned(int pri_version, nghttp3_pri *dest,
                                         const uint8_t *value,
                                         size_t valuelen) {
  (void)pri_version;

  return nghttp3_http_parse_priority(dest, value, valuelen);
}

static const int8_t VALID_AUTHORITY_CHARS[256] = {
  ['!'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['('] = 1, [')'] = 1,
  ['*'] = 1, ['+'] = 1, [','] = 1, ['-'] = 1, ['.'] = 1,  ['0'] = 1, ['1'] = 1,
  ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1,  ['7'] = 1, ['8'] = 1,
  ['9'] = 1, [':'] = 1, [';'] = 1, ['='] = 1, ['A'] = 1,  ['B'] = 1, ['C'] = 1,
  ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1, ['H'] = 1,  ['I'] = 1, ['J'] = 1,
  ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, ['O'] = 1,  ['P'] = 1, ['Q'] = 1,
  ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1, ['V'] = 1,  ['W'] = 1, ['X'] = 1,
  ['Y'] = 1, ['Z'] = 1, ['['] = 1, [']'] = 1, ['_'] = 1,  ['a'] = 1, ['b'] = 1,
  ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1,  ['h'] = 1, ['i'] = 1,
  ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1,  ['o'] = 1, ['p'] = 1,
  ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1,  ['v'] = 1, ['w'] = 1,
  ['x'] = 1, ['y'] = 1, ['z'] = 1, ['~'] = 1,
};

static int check_authority(const uint8_t *value, size_t len) {
  const uint8_t *last;
  for (last = value + len; value != last; ++value) {
    if (!VALID_AUTHORITY_CHARS[*value]) {
      return 0;
    }
  }
  return 1;
}

static int check_scheme(const uint8_t *value, size_t len) {
  const uint8_t *last;
  if (len == 0) {
    return 0;
  }

  if (!(('A' <= *value && *value <= 'Z') || ('a' <= *value && *value <= 'z'))) {
    return 0;
  }

  last = value + len;
  ++value;

  for (; value != last; ++value) {
    if (!(('A' <= *value && *value <= 'Z') ||
          ('a' <= *value && *value <= 'z') ||
          ('0' <= *value && *value <= '9') || *value == '+' || *value == '-' ||
          *value == '.')) {
      return 0;
    }
  }
  return 1;
}

static const int8_t VALID_METHOD_CHARS[256] = {
  ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['*'] = 1,
  ['+'] = 1, ['-'] = 1, ['.'] = 1, ['0'] = 1, ['1'] = 1, ['2'] = 1,  ['3'] = 1,
  ['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,  ['A'] = 1,
  ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1,  ['H'] = 1,
  ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1,  ['O'] = 1,
  ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1,  ['V'] = 1,
  ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1, ['^'] = 1, ['_'] = 1,  ['`'] = 1,
  ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1,  ['g'] = 1,
  ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1,  ['n'] = 1,
  ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1,  ['u'] = 1,
  ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1, ['|'] = 1,  ['~'] = 1,
};

static int check_method(const uint8_t *value, size_t len) {
  const uint8_t *last;
  if (len == 0) {
    return 0;
  }
  for (last = value + len; value != last; ++value) {
    if (!VALID_METHOD_CHARS[*value]) {
      return 0;
    }
  }
  return 1;
}

static const int8_t VALID_PATH_CHARS[256] = {
  ['!'] = 1,  ['"'] = 1,  ['#'] = 1,  ['$'] = 1,  ['%'] = 1,  ['&'] = 1,
  ['\''] = 1, ['('] = 1,  [')'] = 1,  ['*'] = 1,  ['+'] = 1,  [','] = 1,
  ['-'] = 1,  ['.'] = 1,  ['/'] = 1,  ['0'] = 1,  ['1'] = 1,  ['2'] = 1,
  ['3'] = 1,  ['4'] = 1,  ['5'] = 1,  ['6'] = 1,  ['7'] = 1,  ['8'] = 1,
  ['9'] = 1,  [':'] = 1,  [';'] = 1,  ['<'] = 1,  ['='] = 1,  ['>'] = 1,
  ['?'] = 1,  ['@'] = 1,  ['A'] = 1,  ['B'] = 1,  ['C'] = 1,  ['D'] = 1,
  ['E'] = 1,  ['F'] = 1,  ['G'] = 1,  ['H'] = 1,  ['I'] = 1,  ['J'] = 1,
  ['K'] = 1,  ['L'] = 1,  ['M'] = 1,  ['N'] = 1,  ['O'] = 1,  ['P'] = 1,
  ['Q'] = 1,  ['R'] = 1,  ['S'] = 1,  ['T'] = 1,  ['U'] = 1,  ['V'] = 1,
  ['W'] = 1,  ['X'] = 1,  ['Y'] = 1,  ['Z'] = 1,  ['['] = 1,  ['\\'] = 1,
  [']'] = 1,  ['^'] = 1,  ['_'] = 1,  ['`'] = 1,  ['a'] = 1,  ['b'] = 1,
  ['c'] = 1,  ['d'] = 1,  ['e'] = 1,  ['f'] = 1,  ['g'] = 1,  ['h'] = 1,
  ['i'] = 1,  ['j'] = 1,  ['k'] = 1,  ['l'] = 1,  ['m'] = 1,  ['n'] = 1,
  ['o'] = 1,  ['p'] = 1,  ['q'] = 1,  ['r'] = 1,  ['s'] = 1,  ['t'] = 1,
  ['u'] = 1,  ['v'] = 1,  ['w'] = 1,  ['x'] = 1,  ['y'] = 1,  ['z'] = 1,
  ['{'] = 1,  ['|'] = 1,  ['}'] = 1,  ['~'] = 1,  [0x80] = 1, [0x81] = 1,
  [0x82] = 1, [0x83] = 1, [0x84] = 1, [0x85] = 1, [0x86] = 1, [0x87] = 1,
  [0x88] = 1, [0x89] = 1, [0x8A] = 1, [0x8B] = 1, [0x8C] = 1, [0x8D] = 1,
  [0x8E] = 1, [0x8F] = 1, [0x90] = 1, [0x91] = 1, [0x92] = 1, [0x93] = 1,
  [0x94] = 1, [0x95] = 1, [0x96] = 1, [0x97] = 1, [0x98] = 1, [0x99] = 1,
  [0x9A] = 1, [0x9B] = 1, [0x9C] = 1, [0x9D] = 1, [0x9E] = 1, [0x9F] = 1,
  [0xA0] = 1, [0xA1] = 1, [0xA2] = 1, [0xA3] = 1, [0xA4] = 1, [0xA5] = 1,
  [0xA6] = 1, [0xA7] = 1, [0xA8] = 1, [0xA9] = 1, [0xAA] = 1, [0xAB] = 1,
  [0xAC] = 1, [0xAD] = 1, [0xAE] = 1, [0xAF] = 1, [0xB0] = 1, [0xB1] = 1,
  [0xB2] = 1, [0xB3] = 1, [0xB4] = 1, [0xB5] = 1, [0xB6] = 1, [0xB7] = 1,
  [0xB8] = 1, [0xB9] = 1, [0xBA] = 1, [0xBB] = 1, [0xBC] = 1, [0xBD] = 1,
  [0xBE] = 1, [0xBF] = 1, [0xC0] = 1, [0xC1] = 1, [0xC2] = 1, [0xC3] = 1,
  [0xC4] = 1, [0xC5] = 1, [0xC6] = 1, [0xC7] = 1, [0xC8] = 1, [0xC9] = 1,
  [0xCA] = 1, [0xCB] = 1, [0xCC] = 1, [0xCD] = 1, [0xCE] = 1, [0xCF] = 1,
  [0xD0] = 1, [0xD1] = 1, [0xD2] = 1, [0xD3] = 1, [0xD4] = 1, [0xD5] = 1,
  [0xD6] = 1, [0xD7] = 1, [0xD8] = 1, [0xD9] = 1, [0xDA] = 1, [0xDB] = 1,
  [0xDC] = 1, [0xDD] = 1, [0xDE] = 1, [0xDF] = 1, [0xE0] = 1, [0xE1] = 1,
  [0xE2] = 1, [0xE3] = 1, [0xE4] = 1, [0xE5] = 1, [0xE6] = 1, [0xE7] = 1,
  [0xE8] = 1, [0xE9] = 1, [0xEA] = 1, [0xEB] = 1, [0xEC] = 1, [0xED] = 1,
  [0xEE] = 1, [0xEF] = 1, [0xF0] = 1, [0xF1] = 1, [0xF2] = 1, [0xF3] = 1,
  [0xF4] = 1, [0xF5] = 1, [0xF6] = 1, [0xF7] = 1, [0xF8] = 1, [0xF9] = 1,
  [0xFA] = 1, [0xFB] = 1, [0xFC] = 1, [0xFD] = 1, [0xFE] = 1, [0xFF] = 1,
};

static int check_path(const uint8_t *value, size_t len) {
  const uint8_t *last;
  for (last = value + len; value != last; ++value) {
    if (!VALID_PATH_CHARS[*value]) {
      return 0;
    }
  }
  return 1;
}

static int http_request_on_header(nghttp3_http_state *http,
                                  const nghttp3_qpack_nv *nv, int trailers,
                                  int connect_protocol) {
  nghttp3_pri pri;

  switch (nv->token) {
  case NGHTTP3_QPACK_TOKEN__AUTHORITY:
    if (!check_authority(nv->value->base, nv->value->len) ||
        !check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__AUTHORITY)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  case NGHTTP3_QPACK_TOKEN__METHOD:
    if (!check_method(nv->value->base, nv->value->len) ||
        !check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__METHOD)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    switch (nv->value->len) {
    case 4:
      if (lstreq("HEAD", nv->value->base, nv->value->len)) {
        http->flags |= NGHTTP3_HTTP_FLAG_METH_HEAD;
      }
      break;
    case 7:
      switch (nv->value->base[6]) {
      case 'T':
        if (lstreq("CONNECT", nv->value->base, nv->value->len)) {
          http->flags |= NGHTTP3_HTTP_FLAG_METH_CONNECT;
        }
        break;
      case 'S':
        if (lstreq("OPTIONS", nv->value->base, nv->value->len)) {
          http->flags |= NGHTTP3_HTTP_FLAG_METH_OPTIONS;
        }
        break;
      }
      break;
    }
    break;
  case NGHTTP3_QPACK_TOKEN__PATH:
    if (!check_path(nv->value->base, nv->value->len) ||
        !check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__PATH)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    if (nv->value->base[0] == '/') {
      http->flags |= NGHTTP3_HTTP_FLAG_PATH_REGULAR;
    } else if (nv->value->len == 1 && nv->value->base[0] == '*') {
      http->flags |= NGHTTP3_HTTP_FLAG_PATH_ASTERISK;
    }
    break;
  case NGHTTP3_QPACK_TOKEN__SCHEME:
    if (!check_scheme(nv->value->base, nv->value->len) ||
        !check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__SCHEME)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    /* scheme is case-insensitive:
       https://datatracker.ietf.org/doc/html/rfc3986#section-3.1 */
    if (lstrieq("http", nv->value->base, nv->value->len) ||
        lstrieq("https", nv->value->base, nv->value->len)) {
      http->flags |= NGHTTP3_HTTP_FLAG_SCHEME_HTTP;
    }
    break;
  case NGHTTP3_QPACK_TOKEN__PROTOCOL:
    if (!connect_protocol ||
        !nghttp3_check_header_value(nv->value->base, nv->value->len) ||
        !check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__PROTOCOL)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  case NGHTTP3_QPACK_TOKEN_HOST:
    if (!check_authority(nv->value->base, nv->value->len)) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
    if (!check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG_HOST)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  case NGHTTP3_QPACK_TOKEN_CONTENT_LENGTH: {
    /* https://tools.ietf.org/html/rfc7230#section-4.1.2: A sender
       MUST NOT generate a trailer that contains a field necessary for
       message framing (e.g., Transfer-Encoding and Content-Length),
       ... */
    if (trailers) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
    if (http->content_length != -1) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    http->content_length = parse_uint(nv->value->base, nv->value->len);
    if (http->content_length == -1) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  }
  /* disallowed header fields */
  case NGHTTP3_QPACK_TOKEN_CONNECTION:
  case NGHTTP3_QPACK_TOKEN_KEEP_ALIVE:
  case NGHTTP3_QPACK_TOKEN_PROXY_CONNECTION:
  case NGHTTP3_QPACK_TOKEN_TRANSFER_ENCODING:
  case NGHTTP3_QPACK_TOKEN_UPGRADE:
    return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
  case NGHTTP3_QPACK_TOKEN_TE:
    if (!lstrieq("trailers", nv->value->base, nv->value->len)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  case NGHTTP3_QPACK_TOKEN_PRIORITY:
    if (!nghttp3_check_header_value(nv->value->base, nv->value->len)) {
      http->flags &= ~NGHTTP3_HTTP_FLAG_PRIORITY;
      http->flags |= NGHTTP3_HTTP_FLAG_BAD_PRIORITY;

      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }

    if (trailers || (http->flags & NGHTTP3_HTTP_FLAG_BAD_PRIORITY)) {
      break;
    }

    pri = http->pri;

    if (nghttp3_http_parse_priority(&pri, nv->value->base, nv->value->len) ==
        0) {
      http->pri = pri;
      http->flags |= NGHTTP3_HTTP_FLAG_PRIORITY;
      break;
    }

    http->flags &= ~NGHTTP3_HTTP_FLAG_PRIORITY;
    http->flags |= NGHTTP3_HTTP_FLAG_BAD_PRIORITY;

    break;
  default:
    if (nv->name->base[0] == ':') {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }

    if (!nghttp3_check_header_value(nv->value->base, nv->value->len)) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
  }

  return 0;
}

static int http_response_on_header(nghttp3_http_state *http,
                                   const nghttp3_qpack_nv *nv, int trailers) {
  switch (nv->token) {
  case NGHTTP3_QPACK_TOKEN__STATUS: {
    if (!check_pseudo_header(http, nv, NGHTTP3_HTTP_FLAG__STATUS)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    http->status_code = parse_status_code(nv->value->base, nv->value->len);
    if (http->status_code == -1 || http->status_code == 101) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  }
  case NGHTTP3_QPACK_TOKEN_CONTENT_LENGTH: {
    /* https://tools.ietf.org/html/rfc7230#section-4.1.2: A sender
       MUST NOT generate a trailer that contains a field necessary for
       message framing (e.g., Transfer-Encoding and Content-Length),
       ... */
    if (trailers) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
    if (http->status_code == 204) {
      /* content-length header field in 204 response is prohibited by
         RFC 7230.  But some widely used servers send content-length:
         0.  Until they get fixed, we ignore it. */
      if (/* Found multiple content-length field */
          http->content_length != -1 ||
          !lstrieq("0", nv->value->base, nv->value->len)) {
        return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
      }
      http->content_length = 0;
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
    if (http->status_code / 100 == 1 ||
        /* https://tools.ietf.org/html/rfc7230#section-3.3.3 */
        (http->status_code / 100 == 2 &&
         (http->flags & NGHTTP3_HTTP_FLAG_METH_CONNECT))) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
    if (http->content_length != -1) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    http->content_length = parse_uint(nv->value->base, nv->value->len);
    if (http->content_length == -1) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  }
  /* disallowed header fields */
  case NGHTTP3_QPACK_TOKEN_CONNECTION:
  case NGHTTP3_QPACK_TOKEN_KEEP_ALIVE:
  case NGHTTP3_QPACK_TOKEN_PROXY_CONNECTION:
  case NGHTTP3_QPACK_TOKEN_TRANSFER_ENCODING:
  case NGHTTP3_QPACK_TOKEN_UPGRADE:
    return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
  case NGHTTP3_QPACK_TOKEN_TE:
    if (!lstrieq("trailers", nv->value->base, nv->value->len)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    break;
  default:
    if (nv->name->base[0] == ':') {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }

    if (!nghttp3_check_header_value(nv->value->base, nv->value->len)) {
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    }
  }

  return 0;
}

static int http_check_nonempty_header_name(const uint8_t *name, size_t len);

int nghttp3_http_on_header(nghttp3_http_state *http, const nghttp3_qpack_nv *nv,
                           int request, int trailers, int connect_protocol) {
  if (nv->name->len == 0) {
    http->flags |= NGHTTP3_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;

    return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
  }

  if (nv->name->base[0] == ':') {
    /* pseudo header must have a valid token. */
    if (nv->token == -1 || trailers ||
        (http->flags & NGHTTP3_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
  } else {
    http->flags |= NGHTTP3_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;

    switch (http_check_nonempty_header_name(nv->name->base, nv->name->len)) {
    case 0:
      return NGHTTP3_ERR_REMOVE_HTTP_HEADER;
    case -1:
      /* header field name must be lower-cased without exception */
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
  }

  assert(nv->name->len > 0);

  if (request) {
    return http_request_on_header(http, nv, trailers, connect_protocol);
  }

  return http_response_on_header(http, nv, trailers);
}

int nghttp3_http_on_request_headers(nghttp3_http_state *http) {
  if (!(http->flags & NGHTTP3_HTTP_FLAG__PROTOCOL) &&
      (http->flags & NGHTTP3_HTTP_FLAG_METH_CONNECT)) {
    if ((http->flags & (NGHTTP3_HTTP_FLAG__SCHEME | NGHTTP3_HTTP_FLAG__PATH)) ||
        (http->flags & NGHTTP3_HTTP_FLAG__AUTHORITY) == 0) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    http->content_length = -1;
  } else {
    if ((http->flags & NGHTTP3_HTTP_FLAG_REQ_HEADERS) !=
          NGHTTP3_HTTP_FLAG_REQ_HEADERS ||
        (http->flags &
         (NGHTTP3_HTTP_FLAG__AUTHORITY | NGHTTP3_HTTP_FLAG_HOST)) == 0) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
    if (http->flags & NGHTTP3_HTTP_FLAG__PROTOCOL) {
      if ((http->flags & NGHTTP3_HTTP_FLAG_METH_CONNECT) == 0 ||
          (http->flags & NGHTTP3_HTTP_FLAG__AUTHORITY) == 0) {
        return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
      }

      http->content_length = -1;
    }
    if (!check_path_flags(http)) {
      return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
    }
  }

  return 0;
}

int nghttp3_http_on_response_headers(nghttp3_http_state *http) {
  if ((http->flags & NGHTTP3_HTTP_FLAG__STATUS) == 0) {
    return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
  }

  if (http->status_code / 100 == 1) {
    /* non-final response */
    http->flags = (http->flags & NGHTTP3_HTTP_FLAG_METH_ALL) |
                  NGHTTP3_HTTP_FLAG_EXPECT_FINAL_RESPONSE;
    http->content_length = -1;
    http->status_code = -1;
    return 0;
  }

  http->flags &= ~NGHTTP3_HTTP_FLAG_EXPECT_FINAL_RESPONSE;

  if (!expect_response_body(http)) {
    http->content_length = 0;
  } else if (http->flags & NGHTTP3_HTTP_FLAG_METH_CONNECT) {
    http->content_length = -1;
  }

  return 0;
}

int nghttp3_http_on_remote_end_stream(const nghttp3_stream *stream) {
  if ((stream->rx.http.flags & NGHTTP3_HTTP_FLAG_EXPECT_FINAL_RESPONSE) ||
      (stream->rx.http.content_length != -1 &&
       stream->rx.http.content_length != stream->rx.http.recv_content_length)) {
    return NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING;
  }

  return 0;
}

int nghttp3_http_on_data_chunk(nghttp3_stream *stream, size_t n) {
  stream->rx.http.recv_content_length += (int64_t)n;

  if ((stream->rx.http.flags & NGHTTP3_HTTP_FLAG_EXPECT_FINAL_RESPONSE) ||
      (stream->rx.http.content_length != -1 &&
       stream->rx.http.recv_content_length > stream->rx.http.content_length)) {
    return NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING;
  }

  return 0;
}

void nghttp3_http_record_request_method(nghttp3_stream *stream,
                                        const nghttp3_nv *nva, size_t nvlen) {
  size_t i;
  const nghttp3_nv *nv;

  /* TODO we should do this strictly. */
  for (i = 0; i < nvlen; ++i) {
    nv = &nva[i];
    if (!(nv->namelen == 7 && nv->name[6] == 'd' &&
          memcmp(":metho", nv->name, nv->namelen - 1) == 0)) {
      continue;
    }
    if (lstreq("CONNECT", nv->value, nv->valuelen)) {
      stream->rx.http.flags |= NGHTTP3_HTTP_FLAG_METH_CONNECT;
      return;
    }
    if (lstreq("HEAD", nv->value, nv->valuelen)) {
      stream->rx.http.flags |= NGHTTP3_HTTP_FLAG_METH_HEAD;
      return;
    }
    return;
  }
}

static const int8_t VALID_HD_NAME_CHARS[256] = {
  ['!'] = 1,  ['#'] = 1,  ['$'] = 1,  ['%'] = 1,  ['&'] = 1,  ['\''] = 1,
  ['*'] = 1,  ['+'] = 1,  ['-'] = 1,  ['.'] = 1,  ['0'] = 1,  ['1'] = 1,
  ['2'] = 1,  ['3'] = 1,  ['4'] = 1,  ['5'] = 1,  ['6'] = 1,  ['7'] = 1,
  ['8'] = 1,  ['9'] = 1,  ['A'] = -1, ['B'] = -1, ['C'] = -1, ['D'] = -1,
  ['E'] = -1, ['F'] = -1, ['G'] = -1, ['H'] = -1, ['I'] = -1, ['J'] = -1,
  ['K'] = -1, ['L'] = -1, ['M'] = -1, ['N'] = -1, ['O'] = -1, ['P'] = -1,
  ['Q'] = -1, ['R'] = -1, ['S'] = -1, ['T'] = -1, ['U'] = -1, ['V'] = -1,
  ['W'] = -1, ['X'] = -1, ['Y'] = -1, ['Z'] = -1, ['^'] = 1,  ['_'] = 1,
  ['`'] = 1,  ['a'] = 1,  ['b'] = 1,  ['c'] = 1,  ['d'] = 1,  ['e'] = 1,
  ['f'] = 1,  ['g'] = 1,  ['h'] = 1,  ['i'] = 1,  ['j'] = 1,  ['k'] = 1,
  ['l'] = 1,  ['m'] = 1,  ['n'] = 1,  ['o'] = 1,  ['p'] = 1,  ['q'] = 1,
  ['r'] = 1,  ['s'] = 1,  ['t'] = 1,  ['u'] = 1,  ['v'] = 1,  ['w'] = 1,
  ['x'] = 1,  ['y'] = 1,  ['z'] = 1,  ['|'] = 1,  ['~'] = 1,
};

int nghttp3_check_header_name(const uint8_t *name, size_t len) {
  const uint8_t *last;
  if (len == 0) {
    return 0;
  }
  if (*name == ':') {
    if (len == 1) {
      return 0;
    }
    ++name;
    --len;
  }
  for (last = name + len; name != last; ++name) {
    if (!VALID_HD_NAME_CHARS[*name]) {
      return 0;
    }
  }
  return 1;
}

/* http_check_nonempty_header_name validates regular header name
   pointed by |name| of length |len|.  |len| must be greater than
   zero.  This function returns 1 if it succeeds, or -1 if the name
   contains a character in [A-Z], otherwise 0. */
static int http_check_nonempty_header_name(const uint8_t *name, size_t len) {
  const uint8_t *last;
  int rv;

  for (last = name + len; name != last; ++name) {
    rv = VALID_HD_NAME_CHARS[*name];
    if (rv != 1) {
      return rv;
    }
  }

  return 1;
}

static const int8_t VALID_HD_VALUE_CHARS[256] = {
  [0x09] = 1, [' '] = 1,  ['!'] = 1,  ['"'] = 1,  ['#'] = 1,  ['$'] = 1,
  ['%'] = 1,  ['&'] = 1,  ['\''] = 1, ['('] = 1,  [')'] = 1,  ['*'] = 1,
  ['+'] = 1,  [','] = 1,  ['-'] = 1,  ['.'] = 1,  ['/'] = 1,  ['0'] = 1,
  ['1'] = 1,  ['2'] = 1,  ['3'] = 1,  ['4'] = 1,  ['5'] = 1,  ['6'] = 1,
  ['7'] = 1,  ['8'] = 1,  ['9'] = 1,  [':'] = 1,  [';'] = 1,  ['<'] = 1,
  ['='] = 1,  ['>'] = 1,  ['?'] = 1,  ['@'] = 1,  ['A'] = 1,  ['B'] = 1,
  ['C'] = 1,  ['D'] = 1,  ['E'] = 1,  ['F'] = 1,  ['G'] = 1,  ['H'] = 1,
  ['I'] = 1,  ['J'] = 1,  ['K'] = 1,  ['L'] = 1,  ['M'] = 1,  ['N'] = 1,
  ['O'] = 1,  ['P'] = 1,  ['Q'] = 1,  ['R'] = 1,  ['S'] = 1,  ['T'] = 1,
  ['U'] = 1,  ['V'] = 1,  ['W'] = 1,  ['X'] = 1,  ['Y'] = 1,  ['Z'] = 1,
  ['['] = 1,  ['\\'] = 1, [']'] = 1,  ['^'] = 1,  ['_'] = 1,  ['`'] = 1,
  ['a'] = 1,  ['b'] = 1,  ['c'] = 1,  ['d'] = 1,  ['e'] = 1,  ['f'] = 1,
  ['g'] = 1,  ['h'] = 1,  ['i'] = 1,  ['j'] = 1,  ['k'] = 1,  ['l'] = 1,
  ['m'] = 1,  ['n'] = 1,  ['o'] = 1,  ['p'] = 1,  ['q'] = 1,  ['r'] = 1,
  ['s'] = 1,  ['t'] = 1,  ['u'] = 1,  ['v'] = 1,  ['w'] = 1,  ['x'] = 1,
  ['y'] = 1,  ['z'] = 1,  ['{'] = 1,  ['|'] = 1,  ['}'] = 1,  ['~'] = 1,
  [0x80] = 1, [0x81] = 1, [0x82] = 1, [0x83] = 1, [0x84] = 1, [0x85] = 1,
  [0x86] = 1, [0x87] = 1, [0x88] = 1, [0x89] = 1, [0x8A] = 1, [0x8B] = 1,
  [0x8C] = 1, [0x8D] = 1, [0x8E] = 1, [0x8F] = 1, [0x90] = 1, [0x91] = 1,
  [0x92] = 1, [0x93] = 1, [0x94] = 1, [0x95] = 1, [0x96] = 1, [0x97] = 1,
  [0x98] = 1, [0x99] = 1, [0x9A] = 1, [0x9B] = 1, [0x9C] = 1, [0x9D] = 1,
  [0x9E] = 1, [0x9F] = 1, [0xA0] = 1, [0xA1] = 1, [0xA2] = 1, [0xA3] = 1,
  [0xA4] = 1, [0xA5] = 1, [0xA6] = 1, [0xA7] = 1, [0xA8] = 1, [0xA9] = 1,
  [0xAA] = 1, [0xAB] = 1, [0xAC] = 1, [0xAD] = 1, [0xAE] = 1, [0xAF] = 1,
  [0xB0] = 1, [0xB1] = 1, [0xB2] = 1, [0xB3] = 1, [0xB4] = 1, [0xB5] = 1,
  [0xB6] = 1, [0xB7] = 1, [0xB8] = 1, [0xB9] = 1, [0xBA] = 1, [0xBB] = 1,
  [0xBC] = 1, [0xBD] = 1, [0xBE] = 1, [0xBF] = 1, [0xC0] = 1, [0xC1] = 1,
  [0xC2] = 1, [0xC3] = 1, [0xC4] = 1, [0xC5] = 1, [0xC6] = 1, [0xC7] = 1,
  [0xC8] = 1, [0xC9] = 1, [0xCA] = 1, [0xCB] = 1, [0xCC] = 1, [0xCD] = 1,
  [0xCE] = 1, [0xCF] = 1, [0xD0] = 1, [0xD1] = 1, [0xD2] = 1, [0xD3] = 1,
  [0xD4] = 1, [0xD5] = 1, [0xD6] = 1, [0xD7] = 1, [0xD8] = 1, [0xD9] = 1,
  [0xDA] = 1, [0xDB] = 1, [0xDC] = 1, [0xDD] = 1, [0xDE] = 1, [0xDF] = 1,
  [0xE0] = 1, [0xE1] = 1, [0xE2] = 1, [0xE3] = 1, [0xE4] = 1, [0xE5] = 1,
  [0xE6] = 1, [0xE7] = 1, [0xE8] = 1, [0xE9] = 1, [0xEA] = 1, [0xEB] = 1,
  [0xEC] = 1, [0xED] = 1, [0xEE] = 1, [0xEF] = 1, [0xF0] = 1, [0xF1] = 1,
  [0xF2] = 1, [0xF3] = 1, [0xF4] = 1, [0xF5] = 1, [0xF6] = 1, [0xF7] = 1,
  [0xF8] = 1, [0xF9] = 1, [0xFA] = 1, [0xFB] = 1, [0xFC] = 1, [0xFD] = 1,
  [0xFE] = 1, [0xFF] = 1,
};

#ifdef __AVX2__
static int contains_bad_header_value_char_avx2(const uint8_t *first,
                                               const uint8_t *last) {
  const __m256i ctll = _mm256_set1_epi8(0x00 - 1);
  const __m256i ctlr = _mm256_set1_epi8(0x1F + 1);
  const __m256i ht = _mm256_set1_epi8('\t');
  const __m256i del = _mm256_set1_epi8(0x7F);
  __m256i s, x;
  uint32_t m;

  for (; first != last; first += 32) {
    s = _mm256_loadu_si256((void *)first);

    x = _mm256_andnot_si256(
      _mm256_cmpeq_epi8(s, ht),
      _mm256_and_si256(_mm256_cmpgt_epi8(s, ctll), _mm256_cmpgt_epi8(ctlr, s)));
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, del), x);

    m = (uint32_t)_mm256_movemask_epi8(x);
    if (m) {
      return 1;
    }
  }

  return 0;
}
#endif /* __AVX2__ */

int nghttp3_check_header_value(const uint8_t *value, size_t len) {
  const uint8_t *last;
#ifdef __AVX2__
  const uint8_t *last32;
#endif /* __AVX2__ */

  switch (len) {
  case 0:
    return 1;
  case 1:
    if (is_ws(*value)) {
      return 0;
    }

    break;
  default:
    if (is_ws(*value) || is_ws(*(value + len - 1))) {
      return 0;
    }
  }

  last = value + len;

#ifdef __AVX2__
  if (len >= 32) {
    last32 = value + (len & ~(size_t)0x1FU);
    if (contains_bad_header_value_char_avx2(value, last32)) {
      return 0;
    }

    value = last32;
  }
#endif /* __AVX2__ */

  for (; value != last; ++value) {
    if (!VALID_HD_VALUE_CHARS[*value]) {
      return 0;
    }
  }
  return 1;
}

int nghttp3_pri_eq(const nghttp3_pri *a, const nghttp3_pri *b) {
  return a->urgency == b->urgency && a->inc == b->inc;
}

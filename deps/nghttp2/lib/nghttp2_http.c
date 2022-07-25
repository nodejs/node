/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
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
#include "nghttp2_http.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "nghttp2_hd.h"
#include "nghttp2_helper.h"

static uint8_t downcase(uint8_t c) {
  return 'A' <= c && c <= 'Z' ? (uint8_t)(c - 'A' + 'a') : c;
}

static int memieq(const void *a, const void *b, size_t n) {
  size_t i;
  const uint8_t *aa = a, *bb = b;

  for (i = 0; i < n; ++i) {
    if (downcase(aa[i]) != downcase(bb[i])) {
      return 0;
    }
  }
  return 1;
}

#define lstrieq(A, B, N) ((sizeof((A)) - 1) == (N) && memieq((A), (B), (N)))

static int64_t parse_uint(const uint8_t *s, size_t len) {
  int64_t n = 0;
  size_t i;
  if (len == 0) {
    return -1;
  }
  for (i = 0; i < len; ++i) {
    if ('0' <= s[i] && s[i] <= '9') {
      if (n > INT64_MAX / 10) {
        return -1;
      }
      n *= 10;
      if (n > INT64_MAX - (s[i] - '0')) {
        return -1;
      }
      n += s[i] - '0';
      continue;
    }
    return -1;
  }
  return n;
}

static int lws(const uint8_t *s, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (s[i] != ' ' && s[i] != '\t') {
      return 0;
    }
  }
  return 1;
}

static int check_pseudo_header(nghttp2_stream *stream, const nghttp2_hd_nv *nv,
                               int flag) {
  if (stream->http_flags & flag) {
    return 0;
  }
  if (lws(nv->value->base, nv->value->len)) {
    return 0;
  }
  stream->http_flags = (uint16_t)(stream->http_flags | flag);
  return 1;
}

static int expect_response_body(nghttp2_stream *stream) {
  return (stream->http_flags & NGHTTP2_HTTP_FLAG_METH_HEAD) == 0 &&
         stream->status_code / 100 != 1 && stream->status_code != 304 &&
         stream->status_code != 204;
}

/* For "http" or "https" URIs, OPTIONS request may have "*" in :path
   header field to represent system-wide OPTIONS request.  Otherwise,
   :path header field value must start with "/".  This function must
   be called after ":method" header field was received.  This function
   returns nonzero if path is valid.*/
static int check_path(nghttp2_stream *stream) {
  return (stream->http_flags & NGHTTP2_HTTP_FLAG_SCHEME_HTTP) == 0 ||
         ((stream->http_flags & NGHTTP2_HTTP_FLAG_PATH_REGULAR) ||
          ((stream->http_flags & NGHTTP2_HTTP_FLAG_METH_OPTIONS) &&
           (stream->http_flags & NGHTTP2_HTTP_FLAG_PATH_ASTERISK)));
}

static int http_request_on_header(nghttp2_stream *stream, nghttp2_hd_nv *nv,
                                  int trailer, int connect_protocol) {
  if (nv->name->base[0] == ':') {
    if (trailer ||
        (stream->http_flags & NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
  }

  switch (nv->token) {
  case NGHTTP2_TOKEN__AUTHORITY:
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__AUTHORITY)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  case NGHTTP2_TOKEN__METHOD:
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__METHOD)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    switch (nv->value->len) {
    case 4:
      if (lstreq("HEAD", nv->value->base, nv->value->len)) {
        stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_HEAD;
      }
      break;
    case 7:
      switch (nv->value->base[6]) {
      case 'T':
        if (lstreq("CONNECT", nv->value->base, nv->value->len)) {
          if (stream->stream_id % 2 == 0) {
            /* we won't allow CONNECT for push */
            return NGHTTP2_ERR_HTTP_HEADER;
          }
          stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_CONNECT;
        }
        break;
      case 'S':
        if (lstreq("OPTIONS", nv->value->base, nv->value->len)) {
          stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_OPTIONS;
        }
        break;
      }
      break;
    }
    break;
  case NGHTTP2_TOKEN__PATH:
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__PATH)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    if (nv->value->base[0] == '/') {
      stream->http_flags |= NGHTTP2_HTTP_FLAG_PATH_REGULAR;
    } else if (nv->value->len == 1 && nv->value->base[0] == '*') {
      stream->http_flags |= NGHTTP2_HTTP_FLAG_PATH_ASTERISK;
    }
    break;
  case NGHTTP2_TOKEN__SCHEME:
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__SCHEME)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    if ((nv->value->len == 4 && memieq("http", nv->value->base, 4)) ||
        (nv->value->len == 5 && memieq("https", nv->value->base, 5))) {
      stream->http_flags |= NGHTTP2_HTTP_FLAG_SCHEME_HTTP;
    }
    break;
  case NGHTTP2_TOKEN__PROTOCOL:
    if (!connect_protocol) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }

    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__PROTOCOL)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  case NGHTTP2_TOKEN_HOST:
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG_HOST)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  case NGHTTP2_TOKEN_CONTENT_LENGTH: {
    if (stream->content_length != -1) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    stream->content_length = parse_uint(nv->value->base, nv->value->len);
    if (stream->content_length == -1) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  }
  /* disallowed header fields */
  case NGHTTP2_TOKEN_CONNECTION:
  case NGHTTP2_TOKEN_KEEP_ALIVE:
  case NGHTTP2_TOKEN_PROXY_CONNECTION:
  case NGHTTP2_TOKEN_TRANSFER_ENCODING:
  case NGHTTP2_TOKEN_UPGRADE:
    return NGHTTP2_ERR_HTTP_HEADER;
  case NGHTTP2_TOKEN_TE:
    if (!lstrieq("trailers", nv->value->base, nv->value->len)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  default:
    if (nv->name->base[0] == ':') {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
  }

  if (nv->name->base[0] != ':') {
    stream->http_flags |= NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;
  }

  return 0;
}

static int http_response_on_header(nghttp2_stream *stream, nghttp2_hd_nv *nv,
                                   int trailer) {
  if (nv->name->base[0] == ':') {
    if (trailer ||
        (stream->http_flags & NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
  }

  switch (nv->token) {
  case NGHTTP2_TOKEN__STATUS: {
    if (!check_pseudo_header(stream, nv, NGHTTP2_HTTP_FLAG__STATUS)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    if (nv->value->len != 3) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    stream->status_code = (int16_t)parse_uint(nv->value->base, nv->value->len);
    if (stream->status_code == -1 || stream->status_code == 101) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  }
  case NGHTTP2_TOKEN_CONTENT_LENGTH: {
    if (stream->status_code == 204) {
      /* content-length header field in 204 response is prohibited by
         RFC 7230.  But some widely used servers send content-length:
         0.  Until they get fixed, we ignore it. */
      if (stream->content_length != -1) {
        /* Found multiple content-length field */
        return NGHTTP2_ERR_HTTP_HEADER;
      }
      if (!lstrieq("0", nv->value->base, nv->value->len)) {
        return NGHTTP2_ERR_HTTP_HEADER;
      }
      stream->content_length = 0;
      return NGHTTP2_ERR_REMOVE_HTTP_HEADER;
    }
    if (stream->status_code / 100 == 1) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    /* https://tools.ietf.org/html/rfc7230#section-3.3.3 */
    if (stream->status_code / 100 == 2 &&
        (stream->http_flags & NGHTTP2_HTTP_FLAG_METH_CONNECT)) {
      return NGHTTP2_ERR_REMOVE_HTTP_HEADER;
    }
    if (stream->content_length != -1) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    stream->content_length = parse_uint(nv->value->base, nv->value->len);
    if (stream->content_length == -1) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  }
  /* disallowed header fields */
  case NGHTTP2_TOKEN_CONNECTION:
  case NGHTTP2_TOKEN_KEEP_ALIVE:
  case NGHTTP2_TOKEN_PROXY_CONNECTION:
  case NGHTTP2_TOKEN_TRANSFER_ENCODING:
  case NGHTTP2_TOKEN_UPGRADE:
    return NGHTTP2_ERR_HTTP_HEADER;
  case NGHTTP2_TOKEN_TE:
    if (!lstrieq("trailers", nv->value->base, nv->value->len)) {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    break;
  default:
    if (nv->name->base[0] == ':') {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
  }

  if (nv->name->base[0] != ':') {
    stream->http_flags |= NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;
  }

  return 0;
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

int nghttp2_http_on_header(nghttp2_session *session, nghttp2_stream *stream,
                           nghttp2_frame *frame, nghttp2_hd_nv *nv,
                           int trailer) {
  int rv;

  /* We are strict for pseudo header field.  One bad character should
     lead to fail.  OTOH, we should be a bit forgiving for regular
     headers, since existing public internet has so much illegal
     headers floating around and if we kill the stream because of
     this, we may disrupt many web sites and/or libraries.  So we
     become conservative here, and just ignore those illegal regular
     headers. */
  if (!nghttp2_check_header_name(nv->name->base, nv->name->len)) {
    size_t i;
    if (nv->name->len > 0 && nv->name->base[0] == ':') {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    /* header field name must be lower-cased without exception */
    for (i = 0; i < nv->name->len; ++i) {
      uint8_t c = nv->name->base[i];
      if ('A' <= c && c <= 'Z') {
        return NGHTTP2_ERR_HTTP_HEADER;
      }
    }
    /* When ignoring regular headers, we set this flag so that we
       still enforce header field ordering rule for pseudo header
       fields. */
    stream->http_flags |= NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;
    return NGHTTP2_ERR_IGN_HTTP_HEADER;
  }

  switch (nv->token) {
  case NGHTTP2_TOKEN__METHOD:
    rv = nghttp2_check_method(nv->value->base, nv->value->len);
    break;
  case NGHTTP2_TOKEN__PATH:
    rv = nghttp2_check_path(nv->value->base, nv->value->len);
    break;
  case NGHTTP2_TOKEN__AUTHORITY:
  case NGHTTP2_TOKEN_HOST:
    rv = nghttp2_check_authority(nv->value->base, nv->value->len);
    break;
  case NGHTTP2_TOKEN__SCHEME:
    rv = check_scheme(nv->value->base, nv->value->len);
    break;
  default:
    rv = nghttp2_check_header_value(nv->value->base, nv->value->len);
  }

  if (rv == 0) {
    assert(nv->name->len > 0);
    if (nv->name->base[0] == ':') {
      return NGHTTP2_ERR_HTTP_HEADER;
    }
    /* When ignoring regular headers, we set this flag so that we
       still enforce header field ordering rule for pseudo header
       fields. */
    stream->http_flags |= NGHTTP2_HTTP_FLAG_PSEUDO_HEADER_DISALLOWED;
    return NGHTTP2_ERR_IGN_HTTP_HEADER;
  }

  if (session->server || frame->hd.type == NGHTTP2_PUSH_PROMISE) {
    return http_request_on_header(stream, nv, trailer,
                                  session->server &&
                                      session->pending_enable_connect_protocol);
  }

  return http_response_on_header(stream, nv, trailer);
}

int nghttp2_http_on_request_headers(nghttp2_stream *stream,
                                    nghttp2_frame *frame) {
  if (!(stream->http_flags & NGHTTP2_HTTP_FLAG__PROTOCOL) &&
      (stream->http_flags & NGHTTP2_HTTP_FLAG_METH_CONNECT)) {
    if ((stream->http_flags &
         (NGHTTP2_HTTP_FLAG__SCHEME | NGHTTP2_HTTP_FLAG__PATH)) ||
        (stream->http_flags & NGHTTP2_HTTP_FLAG__AUTHORITY) == 0) {
      return -1;
    }
    stream->content_length = -1;
  } else {
    if ((stream->http_flags & NGHTTP2_HTTP_FLAG_REQ_HEADERS) !=
            NGHTTP2_HTTP_FLAG_REQ_HEADERS ||
        (stream->http_flags &
         (NGHTTP2_HTTP_FLAG__AUTHORITY | NGHTTP2_HTTP_FLAG_HOST)) == 0) {
      return -1;
    }
    if ((stream->http_flags & NGHTTP2_HTTP_FLAG__PROTOCOL) &&
        ((stream->http_flags & NGHTTP2_HTTP_FLAG_METH_CONNECT) == 0 ||
         (stream->http_flags & NGHTTP2_HTTP_FLAG__AUTHORITY) == 0)) {
      return -1;
    }
    if (!check_path(stream)) {
      return -1;
    }
  }

  if (frame->hd.type == NGHTTP2_PUSH_PROMISE) {
    /* we are going to reuse data fields for upcoming response.  Clear
       them now, except for method flags. */
    stream->http_flags &= NGHTTP2_HTTP_FLAG_METH_ALL;
    stream->content_length = -1;
  }

  return 0;
}

int nghttp2_http_on_response_headers(nghttp2_stream *stream) {
  if ((stream->http_flags & NGHTTP2_HTTP_FLAG__STATUS) == 0) {
    return -1;
  }

  if (stream->status_code / 100 == 1) {
    /* non-final response */
    stream->http_flags =
        (uint16_t)((stream->http_flags & NGHTTP2_HTTP_FLAG_METH_ALL) |
                   NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE);
    stream->content_length = -1;
    stream->status_code = -1;
    return 0;
  }

  stream->http_flags =
      (uint16_t)(stream->http_flags & ~NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE);

  if (!expect_response_body(stream)) {
    stream->content_length = 0;
  } else if (stream->http_flags & (NGHTTP2_HTTP_FLAG_METH_CONNECT |
                                   NGHTTP2_HTTP_FLAG_METH_UPGRADE_WORKAROUND)) {
    stream->content_length = -1;
  }

  return 0;
}

int nghttp2_http_on_trailer_headers(nghttp2_stream *stream,
                                    nghttp2_frame *frame) {
  (void)stream;

  if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == 0) {
    return -1;
  }

  return 0;
}

int nghttp2_http_on_remote_end_stream(nghttp2_stream *stream) {
  if (stream->http_flags & NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE) {
    return -1;
  }

  if (stream->content_length != -1 &&
      stream->content_length != stream->recv_content_length) {
    return -1;
  }

  return 0;
}

int nghttp2_http_on_data_chunk(nghttp2_stream *stream, size_t n) {
  stream->recv_content_length += (int64_t)n;

  if ((stream->http_flags & NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE) ||
      (stream->content_length != -1 &&
       stream->recv_content_length > stream->content_length)) {
    return -1;
  }

  return 0;
}

void nghttp2_http_record_request_method(nghttp2_stream *stream,
                                        nghttp2_frame *frame) {
  const nghttp2_nv *nva;
  size_t nvlen;
  size_t i;

  switch (frame->hd.type) {
  case NGHTTP2_HEADERS:
    nva = frame->headers.nva;
    nvlen = frame->headers.nvlen;
    break;
  case NGHTTP2_PUSH_PROMISE:
    nva = frame->push_promise.nva;
    nvlen = frame->push_promise.nvlen;
    break;
  default:
    return;
  }

  /* TODO we should do this strictly. */
  for (i = 0; i < nvlen; ++i) {
    const nghttp2_nv *nv = &nva[i];
    if (!(nv->namelen == 7 && nv->name[6] == 'd' &&
          memcmp(":metho", nv->name, nv->namelen - 1) == 0)) {
      continue;
    }
    if (lstreq("CONNECT", nv->value, nv->valuelen)) {
      stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_CONNECT;
      return;
    }
    if (lstreq("HEAD", nv->value, nv->valuelen)) {
      stream->http_flags |= NGHTTP2_HTTP_FLAG_METH_HEAD;
      return;
    }
    return;
  }
}

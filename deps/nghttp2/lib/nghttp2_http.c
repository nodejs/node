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
#include "nghttp2_extpri.h"

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

static int check_pseudo_header(nghttp2_stream *stream, const nghttp2_hd_nv *nv,
                               uint32_t flag) {
  if ((stream->http_flags & flag) || nv->value->len == 0) {
    return 0;
  }
  stream->http_flags = stream->http_flags | flag;
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
  nghttp2_extpri extpri;

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
  case NGHTTP2_TOKEN_PRIORITY:
    if (!trailer &&
        /* Do not parse the header field in PUSH_PROMISE. */
        (stream->stream_id & 1) &&
        (stream->flags & NGHTTP2_STREAM_FLAG_NO_RFC7540_PRIORITIES) &&
        !(stream->http_flags & NGHTTP2_HTTP_FLAG_BAD_PRIORITY)) {
      nghttp2_extpri_from_uint8(&extpri, stream->http_extpri);
      if (nghttp2_http_parse_priority(&extpri, nv->value->base,
                                      nv->value->len) == 0) {
        stream->http_extpri = nghttp2_extpri_to_uint8(&extpri);
        stream->http_flags |= NGHTTP2_HTTP_FLAG_PRIORITY;
      } else {
        stream->http_flags &= (uint32_t)~NGHTTP2_HTTP_FLAG_PRIORITY;
        stream->http_flags |= NGHTTP2_HTTP_FLAG_BAD_PRIORITY;
      }
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

static int lws(const uint8_t *s, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    if (s[i] != ' ' && s[i] != '\t') {
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
    if (session->server || frame->hd.type == NGHTTP2_PUSH_PROMISE) {
      rv = nghttp2_check_authority(nv->value->base, nv->value->len);
    } else if (
        stream->flags &
        NGHTTP2_STREAM_FLAG_NO_RFC9113_LEADING_AND_TRAILING_WS_VALIDATION) {
      rv = nghttp2_check_header_value(nv->value->base, nv->value->len);
    } else {
      rv = nghttp2_check_header_value_rfc9113(nv->value->base, nv->value->len);
    }
    break;
  case NGHTTP2_TOKEN__SCHEME:
    rv = check_scheme(nv->value->base, nv->value->len);
    break;
  case NGHTTP2_TOKEN__PROTOCOL:
    /* Check the value consists of just white spaces, which was done
       in check_pseudo_header before
       nghttp2_check_header_value_rfc9113 has been introduced. */
    if ((stream->flags &
         NGHTTP2_STREAM_FLAG_NO_RFC9113_LEADING_AND_TRAILING_WS_VALIDATION) &&
        lws(nv->value->base, nv->value->len)) {
      rv = 0;
      break;
    }
    /* fall through */
  default:
    if (stream->flags &
        NGHTTP2_STREAM_FLAG_NO_RFC9113_LEADING_AND_TRAILING_WS_VALIDATION) {
      rv = nghttp2_check_header_value(nv->value->base, nv->value->len);
    } else {
      rv = nghttp2_check_header_value_rfc9113(nv->value->base, nv->value->len);
    }
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
    stream->http_flags = (stream->http_flags & NGHTTP2_HTTP_FLAG_METH_ALL) |
                         NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE;
    stream->content_length = -1;
    stream->status_code = -1;
    return 0;
  }

  stream->http_flags =
      stream->http_flags & (uint32_t)~NGHTTP2_HTTP_FLAG_EXPECT_FINAL_RESPONSE;

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

/* Generated by genchartbl.py */
static const int SF_KEY_CHARS[] = {
    0 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
    0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 0 /* HT   */,
    0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
    0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
    0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
    0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
    0 /* RS   */, 0 /* US   */, 0 /* SPC  */, 0 /* !    */, 0 /* "    */,
    0 /* #    */, 0 /* $    */, 0 /* %    */, 0 /* &    */, 0 /* '    */,
    0 /* (    */, 0 /* )    */, 1 /* *    */, 0 /* +    */, 0 /* ,    */,
    1 /* -    */, 1 /* .    */, 0 /* /    */, 1 /* 0    */, 1 /* 1    */,
    1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
    1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 0 /* :    */, 0 /* ;    */,
    0 /* <    */, 0 /* =    */, 0 /* >    */, 0 /* ?    */, 0 /* @    */,
    0 /* A    */, 0 /* B    */, 0 /* C    */, 0 /* D    */, 0 /* E    */,
    0 /* F    */, 0 /* G    */, 0 /* H    */, 0 /* I    */, 0 /* J    */,
    0 /* K    */, 0 /* L    */, 0 /* M    */, 0 /* N    */, 0 /* O    */,
    0 /* P    */, 0 /* Q    */, 0 /* R    */, 0 /* S    */, 0 /* T    */,
    0 /* U    */, 0 /* V    */, 0 /* W    */, 0 /* X    */, 0 /* Y    */,
    0 /* Z    */, 0 /* [    */, 0 /* \    */, 0 /* ]    */, 0 /* ^    */,
    1 /* _    */, 0 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
    1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
    1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
    1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
    1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
    1 /* x    */, 1 /* y    */, 1 /* z    */, 0 /* {    */, 0 /* |    */,
    0 /* }    */, 0 /* ~    */, 0 /* DEL  */, 0 /* 0x80 */, 0 /* 0x81 */,
    0 /* 0x82 */, 0 /* 0x83 */, 0 /* 0x84 */, 0 /* 0x85 */, 0 /* 0x86 */,
    0 /* 0x87 */, 0 /* 0x88 */, 0 /* 0x89 */, 0 /* 0x8a */, 0 /* 0x8b */,
    0 /* 0x8c */, 0 /* 0x8d */, 0 /* 0x8e */, 0 /* 0x8f */, 0 /* 0x90 */,
    0 /* 0x91 */, 0 /* 0x92 */, 0 /* 0x93 */, 0 /* 0x94 */, 0 /* 0x95 */,
    0 /* 0x96 */, 0 /* 0x97 */, 0 /* 0x98 */, 0 /* 0x99 */, 0 /* 0x9a */,
    0 /* 0x9b */, 0 /* 0x9c */, 0 /* 0x9d */, 0 /* 0x9e */, 0 /* 0x9f */,
    0 /* 0xa0 */, 0 /* 0xa1 */, 0 /* 0xa2 */, 0 /* 0xa3 */, 0 /* 0xa4 */,
    0 /* 0xa5 */, 0 /* 0xa6 */, 0 /* 0xa7 */, 0 /* 0xa8 */, 0 /* 0xa9 */,
    0 /* 0xaa */, 0 /* 0xab */, 0 /* 0xac */, 0 /* 0xad */, 0 /* 0xae */,
    0 /* 0xaf */, 0 /* 0xb0 */, 0 /* 0xb1 */, 0 /* 0xb2 */, 0 /* 0xb3 */,
    0 /* 0xb4 */, 0 /* 0xb5 */, 0 /* 0xb6 */, 0 /* 0xb7 */, 0 /* 0xb8 */,
    0 /* 0xb9 */, 0 /* 0xba */, 0 /* 0xbb */, 0 /* 0xbc */, 0 /* 0xbd */,
    0 /* 0xbe */, 0 /* 0xbf */, 0 /* 0xc0 */, 0 /* 0xc1 */, 0 /* 0xc2 */,
    0 /* 0xc3 */, 0 /* 0xc4 */, 0 /* 0xc5 */, 0 /* 0xc6 */, 0 /* 0xc7 */,
    0 /* 0xc8 */, 0 /* 0xc9 */, 0 /* 0xca */, 0 /* 0xcb */, 0 /* 0xcc */,
    0 /* 0xcd */, 0 /* 0xce */, 0 /* 0xcf */, 0 /* 0xd0 */, 0 /* 0xd1 */,
    0 /* 0xd2 */, 0 /* 0xd3 */, 0 /* 0xd4 */, 0 /* 0xd5 */, 0 /* 0xd6 */,
    0 /* 0xd7 */, 0 /* 0xd8 */, 0 /* 0xd9 */, 0 /* 0xda */, 0 /* 0xdb */,
    0 /* 0xdc */, 0 /* 0xdd */, 0 /* 0xde */, 0 /* 0xdf */, 0 /* 0xe0 */,
    0 /* 0xe1 */, 0 /* 0xe2 */, 0 /* 0xe3 */, 0 /* 0xe4 */, 0 /* 0xe5 */,
    0 /* 0xe6 */, 0 /* 0xe7 */, 0 /* 0xe8 */, 0 /* 0xe9 */, 0 /* 0xea */,
    0 /* 0xeb */, 0 /* 0xec */, 0 /* 0xed */, 0 /* 0xee */, 0 /* 0xef */,
    0 /* 0xf0 */, 0 /* 0xf1 */, 0 /* 0xf2 */, 0 /* 0xf3 */, 0 /* 0xf4 */,
    0 /* 0xf5 */, 0 /* 0xf6 */, 0 /* 0xf7 */, 0 /* 0xf8 */, 0 /* 0xf9 */,
    0 /* 0xfa */, 0 /* 0xfb */, 0 /* 0xfc */, 0 /* 0xfd */, 0 /* 0xfe */,
    0 /* 0xff */,
};

static ssize_t sf_parse_key(const uint8_t *begin, const uint8_t *end) {
  const uint8_t *p = begin;

  if ((*p < 'a' || 'z' < *p) && *p != '*') {
    return -1;
  }

  for (; p != end && SF_KEY_CHARS[*p]; ++p)
    ;

  return p - begin;
}

static ssize_t sf_parse_integer_or_decimal(nghttp2_sf_value *dest,
                                           const uint8_t *begin,
                                           const uint8_t *end) {
  const uint8_t *p = begin;
  int sign = 1;
  int64_t value = 0;
  int type = NGHTTP2_SF_VALUE_TYPE_INTEGER;
  size_t len = 0;
  size_t fpos = 0;
  size_t i;

  if (*p == '-') {
    if (++p == end) {
      return -1;
    }

    sign = -1;
  }

  if (*p < '0' || '9' < *p) {
    return -1;
  }

  for (; p != end; ++p) {
    switch (*p) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      value *= 10;
      value += *p - '0';

      if (++len > 15) {
        return -1;
      }

      break;
    case '.':
      if (type != NGHTTP2_SF_VALUE_TYPE_INTEGER) {
        goto fin;
      }

      if (len > 12) {
        return -1;
      }
      fpos = len;
      type = NGHTTP2_SF_VALUE_TYPE_DECIMAL;

      break;
    default:
      goto fin;
    };
  }

fin:
  switch (type) {
  case NGHTTP2_SF_VALUE_TYPE_INTEGER:
    if (dest) {
      dest->type = (uint8_t)type;
      dest->i = value * sign;
    }

    return p - begin;
  case NGHTTP2_SF_VALUE_TYPE_DECIMAL:
    if (fpos == len || len - fpos > 3) {
      return -1;
    }

    if (dest) {
      dest->type = (uint8_t)type;
      dest->d = (double)value;
      for (i = len - fpos; i > 0; --i) {
        dest->d /= (double)10;
      }
      dest->d *= sign;
    }

    return p - begin;
  default:
    assert(0);
    abort();
  }
}

/* Generated by genchartbl.py */
static const int SF_DQUOTE_CHARS[] = {
    0 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
    0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 0 /* HT   */,
    0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
    0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
    0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
    0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
    0 /* RS   */, 0 /* US   */, 1 /* SPC  */, 1 /* !    */, 0 /* "    */,
    1 /* #    */, 1 /* $    */, 1 /* %    */, 1 /* &    */, 1 /* '    */,
    1 /* (    */, 1 /* )    */, 1 /* *    */, 1 /* +    */, 1 /* ,    */,
    1 /* -    */, 1 /* .    */, 1 /* /    */, 1 /* 0    */, 1 /* 1    */,
    1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
    1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 1 /* :    */, 1 /* ;    */,
    1 /* <    */, 1 /* =    */, 1 /* >    */, 1 /* ?    */, 1 /* @    */,
    1 /* A    */, 1 /* B    */, 1 /* C    */, 1 /* D    */, 1 /* E    */,
    1 /* F    */, 1 /* G    */, 1 /* H    */, 1 /* I    */, 1 /* J    */,
    1 /* K    */, 1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
    1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */, 1 /* T    */,
    1 /* U    */, 1 /* V    */, 1 /* W    */, 1 /* X    */, 1 /* Y    */,
    1 /* Z    */, 1 /* [    */, 0 /* \    */, 1 /* ]    */, 1 /* ^    */,
    1 /* _    */, 1 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
    1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
    1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
    1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
    1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
    1 /* x    */, 1 /* y    */, 1 /* z    */, 1 /* {    */, 1 /* |    */,
    1 /* }    */, 1 /* ~    */, 0 /* DEL  */, 0 /* 0x80 */, 0 /* 0x81 */,
    0 /* 0x82 */, 0 /* 0x83 */, 0 /* 0x84 */, 0 /* 0x85 */, 0 /* 0x86 */,
    0 /* 0x87 */, 0 /* 0x88 */, 0 /* 0x89 */, 0 /* 0x8a */, 0 /* 0x8b */,
    0 /* 0x8c */, 0 /* 0x8d */, 0 /* 0x8e */, 0 /* 0x8f */, 0 /* 0x90 */,
    0 /* 0x91 */, 0 /* 0x92 */, 0 /* 0x93 */, 0 /* 0x94 */, 0 /* 0x95 */,
    0 /* 0x96 */, 0 /* 0x97 */, 0 /* 0x98 */, 0 /* 0x99 */, 0 /* 0x9a */,
    0 /* 0x9b */, 0 /* 0x9c */, 0 /* 0x9d */, 0 /* 0x9e */, 0 /* 0x9f */,
    0 /* 0xa0 */, 0 /* 0xa1 */, 0 /* 0xa2 */, 0 /* 0xa3 */, 0 /* 0xa4 */,
    0 /* 0xa5 */, 0 /* 0xa6 */, 0 /* 0xa7 */, 0 /* 0xa8 */, 0 /* 0xa9 */,
    0 /* 0xaa */, 0 /* 0xab */, 0 /* 0xac */, 0 /* 0xad */, 0 /* 0xae */,
    0 /* 0xaf */, 0 /* 0xb0 */, 0 /* 0xb1 */, 0 /* 0xb2 */, 0 /* 0xb3 */,
    0 /* 0xb4 */, 0 /* 0xb5 */, 0 /* 0xb6 */, 0 /* 0xb7 */, 0 /* 0xb8 */,
    0 /* 0xb9 */, 0 /* 0xba */, 0 /* 0xbb */, 0 /* 0xbc */, 0 /* 0xbd */,
    0 /* 0xbe */, 0 /* 0xbf */, 0 /* 0xc0 */, 0 /* 0xc1 */, 0 /* 0xc2 */,
    0 /* 0xc3 */, 0 /* 0xc4 */, 0 /* 0xc5 */, 0 /* 0xc6 */, 0 /* 0xc7 */,
    0 /* 0xc8 */, 0 /* 0xc9 */, 0 /* 0xca */, 0 /* 0xcb */, 0 /* 0xcc */,
    0 /* 0xcd */, 0 /* 0xce */, 0 /* 0xcf */, 0 /* 0xd0 */, 0 /* 0xd1 */,
    0 /* 0xd2 */, 0 /* 0xd3 */, 0 /* 0xd4 */, 0 /* 0xd5 */, 0 /* 0xd6 */,
    0 /* 0xd7 */, 0 /* 0xd8 */, 0 /* 0xd9 */, 0 /* 0xda */, 0 /* 0xdb */,
    0 /* 0xdc */, 0 /* 0xdd */, 0 /* 0xde */, 0 /* 0xdf */, 0 /* 0xe0 */,
    0 /* 0xe1 */, 0 /* 0xe2 */, 0 /* 0xe3 */, 0 /* 0xe4 */, 0 /* 0xe5 */,
    0 /* 0xe6 */, 0 /* 0xe7 */, 0 /* 0xe8 */, 0 /* 0xe9 */, 0 /* 0xea */,
    0 /* 0xeb */, 0 /* 0xec */, 0 /* 0xed */, 0 /* 0xee */, 0 /* 0xef */,
    0 /* 0xf0 */, 0 /* 0xf1 */, 0 /* 0xf2 */, 0 /* 0xf3 */, 0 /* 0xf4 */,
    0 /* 0xf5 */, 0 /* 0xf6 */, 0 /* 0xf7 */, 0 /* 0xf8 */, 0 /* 0xf9 */,
    0 /* 0xfa */, 0 /* 0xfb */, 0 /* 0xfc */, 0 /* 0xfd */, 0 /* 0xfe */,
    0 /* 0xff */,
};

static ssize_t sf_parse_string(nghttp2_sf_value *dest, const uint8_t *begin,
                               const uint8_t *end) {
  const uint8_t *p = begin;

  if (*p++ != '"') {
    return -1;
  }

  for (; p != end; ++p) {
    switch (*p) {
    case '\\':
      if (++p == end) {
        return -1;
      }

      switch (*p) {
      case '"':
      case '\\':
        break;
      default:
        return -1;
      }

      break;
    case '"':
      if (dest) {
        dest->type = NGHTTP2_SF_VALUE_TYPE_STRING;
        dest->s.base = begin + 1;
        dest->s.len = (size_t)(p - dest->s.base);
      }

      ++p;

      return p - begin;
    default:
      if (!SF_DQUOTE_CHARS[*p]) {
        return -1;
      }
    }
  }

  return -1;
}

/* Generated by genchartbl.py */
static const int SF_TOKEN_CHARS[] = {
    0 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
    0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 0 /* HT   */,
    0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
    0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
    0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
    0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
    0 /* RS   */, 0 /* US   */, 0 /* SPC  */, 1 /* !    */, 0 /* "    */,
    1 /* #    */, 1 /* $    */, 1 /* %    */, 1 /* &    */, 1 /* '    */,
    0 /* (    */, 0 /* )    */, 1 /* *    */, 1 /* +    */, 0 /* ,    */,
    1 /* -    */, 1 /* .    */, 1 /* /    */, 1 /* 0    */, 1 /* 1    */,
    1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
    1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 1 /* :    */, 0 /* ;    */,
    0 /* <    */, 0 /* =    */, 0 /* >    */, 0 /* ?    */, 0 /* @    */,
    1 /* A    */, 1 /* B    */, 1 /* C    */, 1 /* D    */, 1 /* E    */,
    1 /* F    */, 1 /* G    */, 1 /* H    */, 1 /* I    */, 1 /* J    */,
    1 /* K    */, 1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
    1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */, 1 /* T    */,
    1 /* U    */, 1 /* V    */, 1 /* W    */, 1 /* X    */, 1 /* Y    */,
    1 /* Z    */, 0 /* [    */, 0 /* \    */, 0 /* ]    */, 1 /* ^    */,
    1 /* _    */, 1 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
    1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
    1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
    1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
    1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
    1 /* x    */, 1 /* y    */, 1 /* z    */, 0 /* {    */, 1 /* |    */,
    0 /* }    */, 1 /* ~    */, 0 /* DEL  */, 0 /* 0x80 */, 0 /* 0x81 */,
    0 /* 0x82 */, 0 /* 0x83 */, 0 /* 0x84 */, 0 /* 0x85 */, 0 /* 0x86 */,
    0 /* 0x87 */, 0 /* 0x88 */, 0 /* 0x89 */, 0 /* 0x8a */, 0 /* 0x8b */,
    0 /* 0x8c */, 0 /* 0x8d */, 0 /* 0x8e */, 0 /* 0x8f */, 0 /* 0x90 */,
    0 /* 0x91 */, 0 /* 0x92 */, 0 /* 0x93 */, 0 /* 0x94 */, 0 /* 0x95 */,
    0 /* 0x96 */, 0 /* 0x97 */, 0 /* 0x98 */, 0 /* 0x99 */, 0 /* 0x9a */,
    0 /* 0x9b */, 0 /* 0x9c */, 0 /* 0x9d */, 0 /* 0x9e */, 0 /* 0x9f */,
    0 /* 0xa0 */, 0 /* 0xa1 */, 0 /* 0xa2 */, 0 /* 0xa3 */, 0 /* 0xa4 */,
    0 /* 0xa5 */, 0 /* 0xa6 */, 0 /* 0xa7 */, 0 /* 0xa8 */, 0 /* 0xa9 */,
    0 /* 0xaa */, 0 /* 0xab */, 0 /* 0xac */, 0 /* 0xad */, 0 /* 0xae */,
    0 /* 0xaf */, 0 /* 0xb0 */, 0 /* 0xb1 */, 0 /* 0xb2 */, 0 /* 0xb3 */,
    0 /* 0xb4 */, 0 /* 0xb5 */, 0 /* 0xb6 */, 0 /* 0xb7 */, 0 /* 0xb8 */,
    0 /* 0xb9 */, 0 /* 0xba */, 0 /* 0xbb */, 0 /* 0xbc */, 0 /* 0xbd */,
    0 /* 0xbe */, 0 /* 0xbf */, 0 /* 0xc0 */, 0 /* 0xc1 */, 0 /* 0xc2 */,
    0 /* 0xc3 */, 0 /* 0xc4 */, 0 /* 0xc5 */, 0 /* 0xc6 */, 0 /* 0xc7 */,
    0 /* 0xc8 */, 0 /* 0xc9 */, 0 /* 0xca */, 0 /* 0xcb */, 0 /* 0xcc */,
    0 /* 0xcd */, 0 /* 0xce */, 0 /* 0xcf */, 0 /* 0xd0 */, 0 /* 0xd1 */,
    0 /* 0xd2 */, 0 /* 0xd3 */, 0 /* 0xd4 */, 0 /* 0xd5 */, 0 /* 0xd6 */,
    0 /* 0xd7 */, 0 /* 0xd8 */, 0 /* 0xd9 */, 0 /* 0xda */, 0 /* 0xdb */,
    0 /* 0xdc */, 0 /* 0xdd */, 0 /* 0xde */, 0 /* 0xdf */, 0 /* 0xe0 */,
    0 /* 0xe1 */, 0 /* 0xe2 */, 0 /* 0xe3 */, 0 /* 0xe4 */, 0 /* 0xe5 */,
    0 /* 0xe6 */, 0 /* 0xe7 */, 0 /* 0xe8 */, 0 /* 0xe9 */, 0 /* 0xea */,
    0 /* 0xeb */, 0 /* 0xec */, 0 /* 0xed */, 0 /* 0xee */, 0 /* 0xef */,
    0 /* 0xf0 */, 0 /* 0xf1 */, 0 /* 0xf2 */, 0 /* 0xf3 */, 0 /* 0xf4 */,
    0 /* 0xf5 */, 0 /* 0xf6 */, 0 /* 0xf7 */, 0 /* 0xf8 */, 0 /* 0xf9 */,
    0 /* 0xfa */, 0 /* 0xfb */, 0 /* 0xfc */, 0 /* 0xfd */, 0 /* 0xfe */,
    0 /* 0xff */,
};

static ssize_t sf_parse_token(nghttp2_sf_value *dest, const uint8_t *begin,
                              const uint8_t *end) {
  const uint8_t *p = begin;

  if ((*p < 'A' || 'Z' < *p) && (*p < 'a' || 'z' < *p) && *p != '*') {
    return -1;
  }

  for (; p != end && SF_TOKEN_CHARS[*p]; ++p)
    ;

  if (dest) {
    dest->type = NGHTTP2_SF_VALUE_TYPE_TOKEN;
    dest->s.base = begin;
    dest->s.len = (size_t)(p - begin);
  }

  return p - begin;
}

/* Generated by genchartbl.py */
static const int SF_BYTESEQ_CHARS[] = {
    0 /* NUL  */, 0 /* SOH  */, 0 /* STX  */, 0 /* ETX  */, 0 /* EOT  */,
    0 /* ENQ  */, 0 /* ACK  */, 0 /* BEL  */, 0 /* BS   */, 0 /* HT   */,
    0 /* LF   */, 0 /* VT   */, 0 /* FF   */, 0 /* CR   */, 0 /* SO   */,
    0 /* SI   */, 0 /* DLE  */, 0 /* DC1  */, 0 /* DC2  */, 0 /* DC3  */,
    0 /* DC4  */, 0 /* NAK  */, 0 /* SYN  */, 0 /* ETB  */, 0 /* CAN  */,
    0 /* EM   */, 0 /* SUB  */, 0 /* ESC  */, 0 /* FS   */, 0 /* GS   */,
    0 /* RS   */, 0 /* US   */, 0 /* SPC  */, 0 /* !    */, 0 /* "    */,
    0 /* #    */, 0 /* $    */, 0 /* %    */, 0 /* &    */, 0 /* '    */,
    0 /* (    */, 0 /* )    */, 0 /* *    */, 1 /* +    */, 0 /* ,    */,
    0 /* -    */, 0 /* .    */, 1 /* /    */, 1 /* 0    */, 1 /* 1    */,
    1 /* 2    */, 1 /* 3    */, 1 /* 4    */, 1 /* 5    */, 1 /* 6    */,
    1 /* 7    */, 1 /* 8    */, 1 /* 9    */, 0 /* :    */, 0 /* ;    */,
    0 /* <    */, 1 /* =    */, 0 /* >    */, 0 /* ?    */, 0 /* @    */,
    1 /* A    */, 1 /* B    */, 1 /* C    */, 1 /* D    */, 1 /* E    */,
    1 /* F    */, 1 /* G    */, 1 /* H    */, 1 /* I    */, 1 /* J    */,
    1 /* K    */, 1 /* L    */, 1 /* M    */, 1 /* N    */, 1 /* O    */,
    1 /* P    */, 1 /* Q    */, 1 /* R    */, 1 /* S    */, 1 /* T    */,
    1 /* U    */, 1 /* V    */, 1 /* W    */, 1 /* X    */, 1 /* Y    */,
    1 /* Z    */, 0 /* [    */, 0 /* \    */, 0 /* ]    */, 0 /* ^    */,
    0 /* _    */, 0 /* `    */, 1 /* a    */, 1 /* b    */, 1 /* c    */,
    1 /* d    */, 1 /* e    */, 1 /* f    */, 1 /* g    */, 1 /* h    */,
    1 /* i    */, 1 /* j    */, 1 /* k    */, 1 /* l    */, 1 /* m    */,
    1 /* n    */, 1 /* o    */, 1 /* p    */, 1 /* q    */, 1 /* r    */,
    1 /* s    */, 1 /* t    */, 1 /* u    */, 1 /* v    */, 1 /* w    */,
    1 /* x    */, 1 /* y    */, 1 /* z    */, 0 /* {    */, 0 /* |    */,
    0 /* }    */, 0 /* ~    */, 0 /* DEL  */, 0 /* 0x80 */, 0 /* 0x81 */,
    0 /* 0x82 */, 0 /* 0x83 */, 0 /* 0x84 */, 0 /* 0x85 */, 0 /* 0x86 */,
    0 /* 0x87 */, 0 /* 0x88 */, 0 /* 0x89 */, 0 /* 0x8a */, 0 /* 0x8b */,
    0 /* 0x8c */, 0 /* 0x8d */, 0 /* 0x8e */, 0 /* 0x8f */, 0 /* 0x90 */,
    0 /* 0x91 */, 0 /* 0x92 */, 0 /* 0x93 */, 0 /* 0x94 */, 0 /* 0x95 */,
    0 /* 0x96 */, 0 /* 0x97 */, 0 /* 0x98 */, 0 /* 0x99 */, 0 /* 0x9a */,
    0 /* 0x9b */, 0 /* 0x9c */, 0 /* 0x9d */, 0 /* 0x9e */, 0 /* 0x9f */,
    0 /* 0xa0 */, 0 /* 0xa1 */, 0 /* 0xa2 */, 0 /* 0xa3 */, 0 /* 0xa4 */,
    0 /* 0xa5 */, 0 /* 0xa6 */, 0 /* 0xa7 */, 0 /* 0xa8 */, 0 /* 0xa9 */,
    0 /* 0xaa */, 0 /* 0xab */, 0 /* 0xac */, 0 /* 0xad */, 0 /* 0xae */,
    0 /* 0xaf */, 0 /* 0xb0 */, 0 /* 0xb1 */, 0 /* 0xb2 */, 0 /* 0xb3 */,
    0 /* 0xb4 */, 0 /* 0xb5 */, 0 /* 0xb6 */, 0 /* 0xb7 */, 0 /* 0xb8 */,
    0 /* 0xb9 */, 0 /* 0xba */, 0 /* 0xbb */, 0 /* 0xbc */, 0 /* 0xbd */,
    0 /* 0xbe */, 0 /* 0xbf */, 0 /* 0xc0 */, 0 /* 0xc1 */, 0 /* 0xc2 */,
    0 /* 0xc3 */, 0 /* 0xc4 */, 0 /* 0xc5 */, 0 /* 0xc6 */, 0 /* 0xc7 */,
    0 /* 0xc8 */, 0 /* 0xc9 */, 0 /* 0xca */, 0 /* 0xcb */, 0 /* 0xcc */,
    0 /* 0xcd */, 0 /* 0xce */, 0 /* 0xcf */, 0 /* 0xd0 */, 0 /* 0xd1 */,
    0 /* 0xd2 */, 0 /* 0xd3 */, 0 /* 0xd4 */, 0 /* 0xd5 */, 0 /* 0xd6 */,
    0 /* 0xd7 */, 0 /* 0xd8 */, 0 /* 0xd9 */, 0 /* 0xda */, 0 /* 0xdb */,
    0 /* 0xdc */, 0 /* 0xdd */, 0 /* 0xde */, 0 /* 0xdf */, 0 /* 0xe0 */,
    0 /* 0xe1 */, 0 /* 0xe2 */, 0 /* 0xe3 */, 0 /* 0xe4 */, 0 /* 0xe5 */,
    0 /* 0xe6 */, 0 /* 0xe7 */, 0 /* 0xe8 */, 0 /* 0xe9 */, 0 /* 0xea */,
    0 /* 0xeb */, 0 /* 0xec */, 0 /* 0xed */, 0 /* 0xee */, 0 /* 0xef */,
    0 /* 0xf0 */, 0 /* 0xf1 */, 0 /* 0xf2 */, 0 /* 0xf3 */, 0 /* 0xf4 */,
    0 /* 0xf5 */, 0 /* 0xf6 */, 0 /* 0xf7 */, 0 /* 0xf8 */, 0 /* 0xf9 */,
    0 /* 0xfa */, 0 /* 0xfb */, 0 /* 0xfc */, 0 /* 0xfd */, 0 /* 0xfe */,
    0 /* 0xff */,
};

static ssize_t sf_parse_byteseq(nghttp2_sf_value *dest, const uint8_t *begin,
                                const uint8_t *end) {
  const uint8_t *p = begin;

  if (*p++ != ':') {
    return -1;
  }

  for (; p != end; ++p) {
    switch (*p) {
    case ':':
      if (dest) {
        dest->type = NGHTTP2_SF_VALUE_TYPE_BYTESEQ;
        dest->s.base = begin + 1;
        dest->s.len = (size_t)(p - dest->s.base);
      }

      ++p;

      return p - begin;
    default:
      if (!SF_BYTESEQ_CHARS[*p]) {
        return -1;
      }
    }
  }

  return -1;
}

static ssize_t sf_parse_boolean(nghttp2_sf_value *dest, const uint8_t *begin,
                                const uint8_t *end) {
  const uint8_t *p = begin;
  int b;

  if (*p++ != '?') {
    return -1;
  }

  if (p == end) {
    return -1;
  }

  switch (*p++) {
  case '0':
    b = 0;
    break;
  case '1':
    b = 1;
    break;
  default:
    return -1;
  }

  if (dest) {
    dest->type = NGHTTP2_SF_VALUE_TYPE_BOOLEAN;
    dest->b = b;
  }

  return p - begin;
}

static ssize_t sf_parse_bare_item(nghttp2_sf_value *dest, const uint8_t *begin,
                                  const uint8_t *end) {
  switch (*begin) {
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return sf_parse_integer_or_decimal(dest, begin, end);
  case '"':
    return sf_parse_string(dest, begin, end);
  case '*':
    return sf_parse_token(dest, begin, end);
  case ':':
    return sf_parse_byteseq(dest, begin, end);
  case '?':
    return sf_parse_boolean(dest, begin, end);
  default:
    if (('A' <= *begin && *begin <= 'Z') || ('a' <= *begin && *begin <= 'z')) {
      return sf_parse_token(dest, begin, end);
    }
    return -1;
  }
}

#define sf_discard_sp_end_err(BEGIN, END, ERR)                                 \
  for (;; ++(BEGIN)) {                                                         \
    if ((BEGIN) == (END)) {                                                    \
      return (ERR);                                                            \
    }                                                                          \
    if (*(BEGIN) != ' ') {                                                     \
      break;                                                                   \
    }                                                                          \
  }

static ssize_t sf_parse_params(const uint8_t *begin, const uint8_t *end) {
  const uint8_t *p = begin;
  ssize_t slen;

  for (; p != end && *p == ';';) {
    ++p;

    sf_discard_sp_end_err(p, end, -1);

    slen = sf_parse_key(p, end);
    if (slen < 0) {
      return -1;
    }

    p += slen;

    if (p == end || *p != '=') {
      /* Boolean true */
    } else if (++p == end) {
      return -1;
    } else {
      slen = sf_parse_bare_item(NULL, p, end);
      if (slen < 0) {
        return -1;
      }

      p += slen;
    }
  }

  return p - begin;
}

static ssize_t sf_parse_item(nghttp2_sf_value *dest, const uint8_t *begin,
                             const uint8_t *end) {
  const uint8_t *p = begin;
  ssize_t slen;

  slen = sf_parse_bare_item(dest, p, end);
  if (slen < 0) {
    return -1;
  }

  p += slen;

  slen = sf_parse_params(p, end);
  if (slen < 0) {
    return -1;
  }

  p += slen;

  return p - begin;
}

ssize_t nghttp2_sf_parse_item(nghttp2_sf_value *dest, const uint8_t *begin,
                              const uint8_t *end) {
  return sf_parse_item(dest, begin, end);
}

static ssize_t sf_parse_inner_list(nghttp2_sf_value *dest, const uint8_t *begin,
                                   const uint8_t *end) {
  const uint8_t *p = begin;
  ssize_t slen;

  if (*p++ != '(') {
    return -1;
  }

  for (;;) {
    sf_discard_sp_end_err(p, end, -1);

    if (*p == ')') {
      ++p;

      slen = sf_parse_params(p, end);
      if (slen < 0) {
        return -1;
      }

      p += slen;

      if (dest) {
        dest->type = NGHTTP2_SF_VALUE_TYPE_INNER_LIST;
      }

      return p - begin;
    }

    slen = sf_parse_item(NULL, p, end);
    if (slen < 0) {
      return -1;
    }

    p += slen;

    if (p == end || (*p != ' ' && *p != ')')) {
      return -1;
    }
  }
}

ssize_t nghttp2_sf_parse_inner_list(nghttp2_sf_value *dest,
                                    const uint8_t *begin, const uint8_t *end) {
  return sf_parse_inner_list(dest, begin, end);
}

static ssize_t sf_parse_item_or_inner_list(nghttp2_sf_value *dest,
                                           const uint8_t *begin,
                                           const uint8_t *end) {
  if (*begin == '(') {
    return sf_parse_inner_list(dest, begin, end);
  }

  return sf_parse_item(dest, begin, end);
}

#define sf_discard_ows(BEGIN, END)                                             \
  for (;; ++(BEGIN)) {                                                         \
    if ((BEGIN) == (END)) {                                                    \
      goto fin;                                                                \
    }                                                                          \
    if (*(BEGIN) != ' ' && *(BEGIN) != '\t') {                                 \
      break;                                                                   \
    }                                                                          \
  }

#define sf_discard_ows_end_err(BEGIN, END, ERR)                                \
  for (;; ++(BEGIN)) {                                                         \
    if ((BEGIN) == (END)) {                                                    \
      return (ERR);                                                            \
    }                                                                          \
    if (*(BEGIN) != ' ' && *(BEGIN) != '\t') {                                 \
      break;                                                                   \
    }                                                                          \
  }

int nghttp2_http_parse_priority(nghttp2_extpri *dest, const uint8_t *value,
                                size_t valuelen) {
  const uint8_t *p = value, *end = value + valuelen;
  ssize_t slen;
  nghttp2_sf_value val;
  nghttp2_extpri pri = *dest;
  const uint8_t *key;
  size_t keylen;

  for (; p != end && *p == ' '; ++p)
    ;

  for (; p != end;) {
    slen = sf_parse_key(p, end);
    if (slen < 0) {
      return NGHTTP2_ERR_INVALID_ARGUMENT;
    }

    key = p;
    keylen = (size_t)slen;

    p += slen;

    if (p == end || *p != '=') {
      /* Boolean true */
      val.type = NGHTTP2_SF_VALUE_TYPE_BOOLEAN;
      val.b = 1;

      slen = sf_parse_params(p, end);
      if (slen < 0) {
        return NGHTTP2_ERR_INVALID_ARGUMENT;
      }
    } else if (++p == end) {
      return NGHTTP2_ERR_INVALID_ARGUMENT;
    } else {
      slen = sf_parse_item_or_inner_list(&val, p, end);
      if (slen < 0) {
        return NGHTTP2_ERR_INVALID_ARGUMENT;
      }
    }

    p += slen;

    if (keylen == 1) {
      switch (key[0]) {
      case 'i':
        if (val.type != NGHTTP2_SF_VALUE_TYPE_BOOLEAN) {
          return NGHTTP2_ERR_INVALID_ARGUMENT;
        }

        pri.inc = val.b;

        break;
      case 'u':
        if (val.type != NGHTTP2_SF_VALUE_TYPE_INTEGER ||
            val.i < NGHTTP2_EXTPRI_URGENCY_HIGH ||
            NGHTTP2_EXTPRI_URGENCY_LOW < val.i) {
          return NGHTTP2_ERR_INVALID_ARGUMENT;
        }

        pri.urgency = (uint32_t)val.i;

        break;
      }
    }

    sf_discard_ows(p, end);

    if (*p++ != ',') {
      return NGHTTP2_ERR_INVALID_ARGUMENT;
    }

    sf_discard_ows_end_err(p, end, NGHTTP2_ERR_INVALID_ARGUMENT);
  }

fin:
  *dest = pri;

  return 0;
}

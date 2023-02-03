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
#ifndef NGHTTP2_HTTP_H
#define NGHTTP2_HTTP_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>
#include "nghttp2_session.h"
#include "nghttp2_stream.h"

/*
 * This function is called when HTTP header field |nv| in |frame| is
 * received for |stream|.  This function will validate |nv| against
 * the current state of stream.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_HTTP_HEADER
 *     Invalid HTTP header field was received.
 * NGHTTP2_ERR_IGN_HTTP_HEADER
 *     Invalid HTTP header field was received but it can be treated as
 *     if it was not received because of compatibility reasons.
 */
int nghttp2_http_on_header(nghttp2_session *session, nghttp2_stream *stream,
                           nghttp2_frame *frame, nghttp2_hd_nv *nv,
                           int trailer);

/*
 * This function is called when request header is received.  This
 * function performs validation and returns 0 if it succeeds, or -1.
 */
int nghttp2_http_on_request_headers(nghttp2_stream *stream,
                                    nghttp2_frame *frame);

/*
 * This function is called when response header is received.  This
 * function performs validation and returns 0 if it succeeds, or -1.
 */
int nghttp2_http_on_response_headers(nghttp2_stream *stream);

/*
 * This function is called trailer header (for both request and
 * response) is received.  This function performs validation and
 * returns 0 if it succeeds, or -1.
 */
int nghttp2_http_on_trailer_headers(nghttp2_stream *stream,
                                    nghttp2_frame *frame);

/*
 * This function is called when END_STREAM flag is seen in incoming
 * frame.  This function performs validation and returns 0 if it
 * succeeds, or -1.
 */
int nghttp2_http_on_remote_end_stream(nghttp2_stream *stream);

/*
 * This function is called when chunk of data is received.  This
 * function performs validation and returns 0 if it succeeds, or -1.
 */
int nghttp2_http_on_data_chunk(nghttp2_stream *stream, size_t n);

/*
 * This function inspects header field in |frame| and records its
 * method in stream->http_flags.  If frame->hd.type is neither
 * NGHTTP2_HEADERS nor NGHTTP2_PUSH_PROMISE, this function does
 * nothing.
 */
void nghttp2_http_record_request_method(nghttp2_stream *stream,
                                        nghttp2_frame *frame);

/*
 * RFC 8941 Structured Field Values.
 */
typedef enum nghttp2_sf_value_type {
  NGHTTP2_SF_VALUE_TYPE_BOOLEAN,
  NGHTTP2_SF_VALUE_TYPE_INTEGER,
  NGHTTP2_SF_VALUE_TYPE_DECIMAL,
  NGHTTP2_SF_VALUE_TYPE_STRING,
  NGHTTP2_SF_VALUE_TYPE_TOKEN,
  NGHTTP2_SF_VALUE_TYPE_BYTESEQ,
  NGHTTP2_SF_VALUE_TYPE_INNER_LIST,
} nghttp2_sf_value_type;

/*
 * nghttp2_sf_value stores Structured Field Values item.  For Inner
 * List, only type is set to NGHTTP2_SF_VALUE_TYPE_INNER_LIST.
 */
typedef struct nghttp2_sf_value {
  uint8_t type;
  union {
    int b;
    int64_t i;
    double d;
    struct {
      const uint8_t *base;
      size_t len;
    } s;
  };
} nghttp2_sf_value;

/*
 * nghttp2_sf_parse_item parses the input sequence [|begin|, |end|)
 * and stores the parsed an Item in |dest|.  It returns the number of
 * bytes consumed if it succeeds, or -1.  This function is declared
 * here for unit tests.
 */
ssize_t nghttp2_sf_parse_item(nghttp2_sf_value *dest, const uint8_t *begin,
                              const uint8_t *end);

/*
 * nghttp2_sf_parse_inner_list parses the input sequence [|begin|, |end|)
 * and stores the parsed an Inner List in |dest|.  It returns the number of
 * bytes consumed if it succeeds, or -1.  This function is declared
 * here for unit tests.
 */
ssize_t nghttp2_sf_parse_inner_list(nghttp2_sf_value *dest,
                                    const uint8_t *begin, const uint8_t *end);

int nghttp2_http_parse_priority(nghttp2_extpri *dest, const uint8_t *value,
                                size_t valuelen);

#endif /* NGHTTP2_HTTP_H */

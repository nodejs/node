/* Copyright (c) 2008, 2009 Ryan Dahl (ry@tinyclouds.org)
 * Based on Zed Shaw's Mongrel, copyright (c) Zed A. Shaw
 *
 * All rights reserved.
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
#ifndef http_parser_h
#define http_parser_h
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#  include <stddef.h>
#endif
#include <sys/types.h>

typedef struct http_parser http_parser;

/* Callbacks should return non-zero to indicate an error. The parse will
 * then halt execution.
 *
 * http_data_cb does not return data chunks. It will be call arbitrarally
 * many times for each string. E.G. you might get 10 callbacks for "on_path"
 * each providing just a few characters more data.
 */
typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);
typedef int (*http_cb) (http_parser*);

/* Request Methods */
#define HTTP_COPY       0x0001
#define HTTP_DELETE     0x0002
#define HTTP_GET        0x0004
#define HTTP_HEAD       0x0008
#define HTTP_LOCK       0x0010
#define HTTP_MKCOL      0x0020
#define HTTP_MOVE       0x0040
#define HTTP_OPTIONS    0x0080
#define HTTP_POST       0x0100
#define HTTP_PROPFIND   0x0200
#define HTTP_PROPPATCH  0x0400
#define HTTP_PUT        0x0800
#define HTTP_TRACE      0x1000
#define HTTP_UNLOCK     0x2000

enum http_parser_type { HTTP_REQUEST, HTTP_RESPONSE };

#define HTTP_VERSION_OTHER 0x00
#define HTTP_VERSION_11    0x01
#define HTTP_VERSION_10    0x02
#define HTTP_VERSION_09    0x04

struct http_parser {
  /** PRIVATE **/
  int cs;
  enum http_parser_type type;

  size_t chunk_size;
  char flags;

  size_t body_read;

  const char *header_field_mark;
  size_t      header_field_size;
  const char *header_value_mark;
  size_t      header_value_size;
  const char *query_string_mark;
  size_t      query_string_size;
  const char *path_mark;
  size_t      path_size;
  const char *uri_mark;
  size_t      uri_size;
  const char *fragment_mark;
  size_t      fragment_size;

  /** READ-ONLY **/
  unsigned short status_code; /* responses only */
  unsigned short method;      /* requests only */
  short version;
  short keep_alive;
  ssize_t content_length;

  /** PUBLIC **/
  void *data; /* A pointer to get hook to the "connection" or "socket" object */

  /* an ordered list of callbacks */

  http_cb      on_message_begin;

  /* requests only */
  http_data_cb on_path;
  http_data_cb on_query_string;
  http_data_cb on_uri;
  http_data_cb on_fragment;

  http_data_cb on_header_field;
  http_data_cb on_header_value;
  http_cb      on_headers_complete;
  http_data_cb on_body;
  http_cb      on_message_complete;
};

/* Initializes an http_parser structure.  The second argument specifies if
 * it will be parsing requests or responses.
 */
void http_parser_init (http_parser *parser, enum http_parser_type);

void http_parser_execute (http_parser *parser, const char *data, size_t len);

int http_parser_has_error (http_parser *parser);

static inline int
http_parser_should_keep_alive (http_parser *parser)
{
  if (parser->keep_alive == -1) return (parser->version == HTTP_VERSION_11);
  return parser->keep_alive;
}


#ifdef __cplusplus
}
#endif
#endif

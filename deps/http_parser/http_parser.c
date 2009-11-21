/* Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
 *
 * Some parts of this source file were taken from NGINX
 * (src/http/ngx_http_parser.c) copyright (C) 2002-2009 Igor Sysoev. 
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE. 
 */
#include <http_parser.h>
#include <stdint.h>
#include <assert.h>

#ifndef NULL
# define NULL ((void*)0)
#endif

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAX_FIELD_SIZE (80*1024)


#define MARK(FOR)                                                    \
do {                                                                 \
  parser->FOR##_mark = p;                                            \
  parser->FOR##_size = 0;                                            \
} while (0)

#define CALLBACK(FOR)                                                \
do {                                                                 \
  if (0 != FOR##_callback(parser, p)) return (p - data);             \
  parser->FOR##_mark = NULL;                                         \
} while (0)

#define CALLBACK_NOCLEAR(FOR)                                        \
do {                                                                 \
  if (0 != FOR##_callback(parser, p)) return (p - data);             \
} while (0)

#define CALLBACK2(FOR)                                               \
do {                                                                 \
  if (0 != FOR##_callback(parser)) return (p - data);                \
} while (0)

#define DEFINE_CALLBACK(FOR) \
static inline int FOR##_callback (http_parser *parser, const char *p) \
{ \
  if (!parser->FOR##_mark) return 0; \
  assert(parser->FOR##_mark); \
  const char *mark = parser->FOR##_mark; \
  parser->FOR##_size += p - mark; \
  if (parser->FOR##_size > MAX_FIELD_SIZE) return -1; \
  int r = 0; \
  if (parser->on_##FOR) r = parser->on_##FOR(parser, mark, p - mark); \
  return r; \
}

DEFINE_CALLBACK(url)
DEFINE_CALLBACK(path)
DEFINE_CALLBACK(query_string)
DEFINE_CALLBACK(fragment)
DEFINE_CALLBACK(header_field)
DEFINE_CALLBACK(header_value)

static inline int headers_complete_callback (http_parser *parser)
{
  if (parser->on_headers_complete == NULL) return 0;
  return parser->on_headers_complete(parser);
}

static inline int message_begin_callback (http_parser *parser)
{
  if (parser->on_message_begin == NULL) return 0;
  return parser->on_message_begin(parser);
}

static inline int message_complete_callback (http_parser *parser)
{
  if (parser->on_message_complete == NULL) return 0;
  return parser->on_message_complete(parser);
}

#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"

#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"


static const unsigned char lowcase[] =
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
  "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
  "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static const int unhex[] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };


static const uint32_t  usual[] = {
    0xffffdbfe, /* 1111 1111 1111 1111  1101 1011 1111 1110 */

                /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
    0x7ffffff6, /* 0111 1111 1111 1111  1111 1111 1111 0110 */

                /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

                /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
};

enum state 
  { s_dead = 1 /* important that this is > 0 */

  , s_start_res
  , s_res_H
  , s_res_HT
  , s_res_HTT
  , s_res_HTTP
  , s_res_first_http_major
  , s_res_http_major
  , s_res_first_http_minor
  , s_res_http_minor
  , s_res_first_status_code
  , s_res_status_code
  , s_res_status
  , s_res_line_almost_done

  , s_start_req
  , s_req_method_G
  , s_req_method_GE
  , s_req_method_P
  , s_req_method_PU
  , s_req_method_PO
  , s_req_method_POS
  , s_req_method_H
  , s_req_method_HE
  , s_req_method_HEA
  , s_req_method_D
  , s_req_method_DE
  , s_req_method_DEL
  , s_req_method_DELE
  , s_req_method_DELET
  , s_req_spaces_before_url
  , s_req_schema
  , s_req_schema_slash
  , s_req_schema_slash_slash
  , s_req_host
  , s_req_port
  , s_req_path
  , s_req_query_string_start
  , s_req_query_string
  , s_req_fragment_start
  , s_req_fragment
  , s_req_http_start
  , s_req_http_H
  , s_req_http_HT
  , s_req_http_HTT
  , s_req_http_HTTP
  , s_req_first_http_major
  , s_req_http_major
  , s_req_first_http_minor
  , s_req_http_minor
  , s_req_line_almost_done

  , s_header_field_start
  , s_header_field
  , s_header_value_start
  , s_header_value

  , s_header_almost_done

  , s_headers_almost_done
  , s_headers_done

  , s_chunk_size_start
  , s_chunk_size
  , s_chunk_size_almost_done
  , s_chunk_parameters
  , s_chunk_data
  , s_chunk_data_almost_done
  , s_chunk_data_done

  , s_body_identity
  , s_body_identity_eof
  };

enum header_states 
  { h_general = 0
  , h_C
  , h_CO
  , h_CON

  , h_matching_connection
  , h_matching_content_length
  , h_matching_transfer_encoding

  , h_connection
  , h_content_length
  , h_transfer_encoding

  , h_matching_transfer_encoding_chunked
  , h_matching_connection_keep_alive
  , h_matching_connection_close

  , h_transfer_encoding_chunked
  , h_connection_keep_alive
  , h_connection_close
  };

enum flags
  { F_CHUNKED               = 0x0001
  , F_CONNECTION_KEEP_ALIVE = 0x0002
  , F_CONNECTION_CLOSE      = 0x0004
  , F_TRAILING              = 0x0010
  };

#define ERROR (p - data)
#define CR '\r'
#define LF '\n'
#define LOWER(c) (unsigned char)(c | 0x20)

#if HTTP_PARSER_STRICT
# define STRICT_CHECK(cond) if (cond) return ERROR
# define NEW_MESSAGE() (http_should_keep_alive(parser) ? start_state : s_dead)
#else 
# define STRICT_CHECK(cond) 
# define NEW_MESSAGE() start_state
#endif

static inline
size_t parse (http_parser *parser, const char *data, size_t len, int start_state)
{
  char c, ch; 
  const char *p, *pe;
  ssize_t to_read;

  enum state state = parser->state;
  enum header_states header_state = parser->header_state;
  size_t header_index = parser->header_index;

  if (len == 0) {
    if (state == s_body_identity_eof) {
      CALLBACK2(message_complete);
    }
    return 0;
  }

  if (parser->header_field_mark)   parser->header_field_mark   = data;
  if (parser->header_value_mark)   parser->header_value_mark   = data;
  if (parser->fragment_mark)       parser->fragment_mark       = data;
  if (parser->query_string_mark)   parser->query_string_mark   = data;
  if (parser->path_mark)           parser->path_mark           = data;
  if (parser->url_mark)            parser->url_mark            = data;

  for (p=data, pe=data+len; p != pe; p++) {
    ch = *p;
    switch (state) {

      case s_dead:
        /* this state is used after a 'Connection: close' message
         * the parser will error out if it reads another message
         */
        return ERROR;

      case s_start_res:
      {
        parser->flags = 0;
        parser->content_length = -1;

        CALLBACK2(message_begin);

        switch (ch) {
          case 'H':
            state = s_res_H; 
            break;

          case CR:
          case LF:
            break;

          default:
            return ERROR;
        }
        break;
      }

      case s_res_H:
        STRICT_CHECK(ch != 'T');
        state = s_res_HT;
        break;

      case s_res_HT:
        STRICT_CHECK(ch != 'T');
        state = s_res_HTT;
        break;

      case s_res_HTT: 
        STRICT_CHECK(ch != 'P');
        state = s_res_HTTP;
        break;

      case s_res_HTTP:
        STRICT_CHECK(ch != '/');
        state = s_res_first_http_major;
        break;

      case s_res_first_http_major:
        if (ch < '1' || ch > '9') return ERROR;
        parser->http_major = ch - '0';
        state = s_res_http_major;
        break;

      /* major HTTP version or dot */
      case s_res_http_major:
      {
        if (ch == '.') {
          state = s_res_first_http_minor;
          break;
        }

        if (ch < '0' || ch > '9') return ERROR;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) return ERROR;
        break;
      }
       
      /* first digit of minor HTTP version */
      case s_res_first_http_minor:
        if (ch < '0' || ch > '9') return ERROR;
        parser->http_minor = ch - '0';
        state = s_res_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_res_http_minor:
      {
        if (ch == ' ') {
          state = s_res_first_status_code;
          break;
        }

        if (ch < '0' || ch > '9') return ERROR;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) return ERROR;
        break;
      }

      case s_res_first_status_code:
      {
        if (ch < '0' || ch > '9') {
          if (ch == ' ') {
            break;
          }
          return ERROR;
        }
        parser->status_code = ch - '0';
        state = s_res_status_code;
        break;
      }

      case s_res_status_code:
      {
        if (ch < '0' || ch > '9') {
          switch (ch) {
            case ' ':
              state = s_res_status;
              break;
            case CR:
              state = s_res_line_almost_done;
              break;
            case LF:
              state = s_header_field_start;
              break;
            default:
              return ERROR;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) return ERROR;
        break;
      }

      case s_res_status:
        /* the human readable status. e.g. "NOT FOUND"
         * we are not humans so just ignore this */
        if (ch == CR) {
          state = s_res_line_almost_done;
          break;
        }

        if (ch == LF) {
          state = s_header_field_start;
          break;
        }
        break;

      case s_res_line_almost_done:
        STRICT_CHECK(ch != LF);
        state = s_header_field_start;
        break;

      case s_start_req:
      {
        parser->flags = 0;
        parser->content_length = -1;

        CALLBACK2(message_begin);

        switch (ch) {
          /* GET */
          case 'G':
            state = s_req_method_G;
            break;

          /* POST, PUT */
          case 'P':
            state = s_req_method_P;
            break;

          /* HEAD */
          case 'H':
            state = s_req_method_H;
            break;

          /* DELETE */
          case 'D':
            state = s_req_method_D;
            break;

          case CR:
          case LF:
            break;

          default:
            return ERROR;
        }
        break;
      }

      /* GET */

      case s_req_method_G:
        STRICT_CHECK(ch != 'E');
        state = s_req_method_GE;
        break;

      case s_req_method_GE:
        STRICT_CHECK(ch != 'T');
        parser->method = HTTP_GET;
        state = s_req_spaces_before_url;
        break;

      /* HEAD */

      case s_req_method_H:
        STRICT_CHECK(ch != 'E');
        state = s_req_method_HE;
        break;

      case s_req_method_HE:
        STRICT_CHECK(ch != 'A');
        state = s_req_method_HEA;
        break;

      case s_req_method_HEA:
        STRICT_CHECK(ch != 'D');
        parser->method = HTTP_HEAD;
        state = s_req_spaces_before_url;
        break;

      /* POST, PUT */

      case s_req_method_P:
        switch (ch) {
          case 'O':
            state = s_req_method_PO;
            break;

          case 'U':
            state = s_req_method_PU;
            break;

          default:
            return ERROR;
        }
        break;

      /* PUT */

      case s_req_method_PU:
        STRICT_CHECK(ch != 'T');
        parser->method = HTTP_PUT;
        state = s_req_spaces_before_url;
        break;

      /* POST */

      case s_req_method_PO:
        STRICT_CHECK(ch != 'S');
        state = s_req_method_POS;
        break;

      case s_req_method_POS:
        STRICT_CHECK(ch != 'T');
        parser->method = HTTP_POST;
        state = s_req_spaces_before_url;
        break;

      /* DELETE */

      case s_req_method_D:
        STRICT_CHECK(ch != 'E');
        state = s_req_method_DE;
        break;

      case s_req_method_DE:
        STRICT_CHECK(ch != 'L');
        state = s_req_method_DEL;
        break;

      case s_req_method_DEL:
        STRICT_CHECK(ch != 'E');
        state = s_req_method_DELE;
        break;

      case s_req_method_DELE:
        STRICT_CHECK(ch != 'T');
        state = s_req_method_DELET;
        break;

      case s_req_method_DELET:
        STRICT_CHECK(ch != 'E');
        parser->method = HTTP_DELETE;
        state = s_req_spaces_before_url;
        break;


      case s_req_spaces_before_url:
      {
        if (ch == ' ') break;

        if (ch == '/') {
          MARK(url);
          MARK(path);
          state = s_req_path;
          break;
        }

        c = LOWER(ch);

        if (c >= 'a' && c <= 'z') {
          MARK(url);
          state = s_req_schema;
          break;
        }

        return ERROR;
      }

      case s_req_schema:
      {
        c = LOWER(ch);

        if (c >= 'a' && c <= 'z') break;

        if (ch == ':') {
          state = s_req_schema_slash;
          break;
        }

        return ERROR;
      }

      case s_req_schema_slash:
        STRICT_CHECK(ch != '/');
        state = s_req_schema_slash_slash;
        break;

      case s_req_schema_slash_slash:
        STRICT_CHECK(ch != '/');
        state = s_req_host;
        break;

      case s_req_host:
      {
        c = LOWER(ch);
        if (c >= 'a' && c <= 'z') break;
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') break;
        switch (ch) {
          case ':':
            state = s_req_port;
            break;
          case '/':
            MARK(path);
            state = s_req_path;
            break;
          case ' ':
            /* The request line looks like:
             *   "GET http://foo.bar.com HTTP/1.1"
             * That is, there is no path.
             */
            CALLBACK(url);
            state = s_req_http_start;
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_port:
      {
        if (ch >= '0' && ch <= '9') break;
        switch (ch) {
          case '/':
            MARK(path);
            state = s_req_path;
            break;
          case ' ':
            /* The request line looks like:
             *   "GET http://foo.bar.com:1234 HTTP/1.1"
             * That is, there is no path.
             */
            CALLBACK(url);
            state = s_req_http_start;
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_path:
      {
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) break;

        switch (ch) {
          case ' ':
            CALLBACK(url);
            CALLBACK(path);
            state = s_req_http_start;
            break;
          case CR:
            CALLBACK(url);
            CALLBACK(path);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            CALLBACK(url);
            CALLBACK(path);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
            CALLBACK(path);
            state = s_req_query_string_start;
            break;
          case '#':
            CALLBACK(path);
            state = s_req_fragment_start;
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_query_string_start:
      {
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) {
          MARK(query_string);
          state = s_req_query_string;
          break;
        }

        switch (ch) {
          case '?':
            break; // XXX ignore extra '?' ... is this right? 
          case ' ':
            CALLBACK(url);
            state = s_req_http_start;
            break;
          case CR:
            CALLBACK(url);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            CALLBACK(url);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '#':
            state = s_req_fragment_start;
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_query_string:
      {
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) break;

        switch (ch) {
          case ' ':
            CALLBACK(url);
            CALLBACK(query_string);
            state = s_req_http_start;
            break;
          case CR:
            CALLBACK(url);
            CALLBACK(query_string);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            CALLBACK(url);
            CALLBACK(query_string);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '#':
            CALLBACK(query_string);
            state = s_req_fragment_start;
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_fragment_start:
      {
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) {
          MARK(fragment);
          state = s_req_fragment;
          break;
        }

        switch (ch) {
          case ' ':
            CALLBACK(url);
            state = s_req_http_start;
            break;
          case CR:
            CALLBACK(url);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            CALLBACK(url);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
            MARK(fragment);
            state = s_req_fragment;
            break;
          case '#':
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_fragment:
      {
        if (usual[ch >> 5] & (1 << (ch & 0x1f))) break;

        switch (ch) {
          case ' ':
            CALLBACK(url);
            CALLBACK(fragment);
            state = s_req_http_start;
            break;
          case CR:
            CALLBACK(url);
            CALLBACK(fragment);
            parser->http_minor = 9;
            state = s_req_line_almost_done;
            break;
          case LF:
            CALLBACK(url);
            CALLBACK(fragment);
            parser->http_minor = 9;
            state = s_header_field_start;
            break;
          case '?':
          case '#':
            break;
          default:
            return ERROR;
        }
        break;
      }

      case s_req_http_start:
        switch (ch) {
          case 'H':
            state = s_req_http_H;
            break;
          case ' ':
            break;
          default:
            return ERROR;
        }
        break;

      case s_req_http_H:
        STRICT_CHECK(ch != 'T');
        state = s_req_http_HT;
        break;

      case s_req_http_HT:
        STRICT_CHECK(ch != 'T');
        state = s_req_http_HTT;
        break;

      case s_req_http_HTT:
        STRICT_CHECK(ch != 'P');
        state = s_req_http_HTTP;
        break;

      case s_req_http_HTTP:
        STRICT_CHECK(ch != '/');
        state = s_req_first_http_major;
        break;

      /* first digit of major HTTP version */
      case s_req_first_http_major:
        if (ch < '1' || ch > '9') return ERROR;
        parser->http_major = ch - '0';
        state = s_req_http_major;
        break;

      /* major HTTP version or dot */
      case s_req_http_major:
      {
        if (ch == '.') {
          state = s_req_first_http_minor;
          break;
        }

        if (ch < '0' || ch > '9') return ERROR;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) return ERROR;
        break;
      }

      /* first digit of minor HTTP version */
      case s_req_first_http_minor:
        if (ch < '0' || ch > '9') return ERROR;
        parser->http_minor = ch - '0';
        state = s_req_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_req_http_minor:
      {
        if (ch == CR) {
          state = s_req_line_almost_done;
          break;
        }

        if (ch == LF) {
          state = s_header_field_start;
          break;
        }

        /* XXX allow spaces after digit? */

        if (ch < '0' || ch > '9') return ERROR;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) return ERROR;
        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != LF) return ERROR;
        state = s_header_field_start;
        break;
      }

      case s_header_field_start:
      {
        if (ch == CR) {
          state = s_headers_almost_done;
          break;
        }

        if (ch == LF) {
          state = s_headers_done;
          break;
        }

        c = LOWER(ch);

        if (c < 'a' || 'z' < c) return ERROR;

        MARK(header_field);

        header_index = 0;
        state = s_header_field;

        switch (c) {
          case 'c':
            header_state = h_C;
            break;

          case 't':
            header_state = h_matching_transfer_encoding;
            break;

          default:
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_field:
      {
        c = lowcase[(int)ch];

        if (c) {
          switch (header_state) {
            case h_general:
              break;

            case h_C:
              header_index++;
              header_state = (c == 'o' ? h_CO : h_general);
              break;

            case h_CO:
              header_index++;
              header_state = (c == 'n' ? h_CON : h_general);
              break;

            case h_CON:
              header_index++;
              switch (c) {
                case 'n':
                  header_state = h_matching_connection;
                  break;
                case 't':
                  header_state = h_matching_content_length;
                  break;
                default:
                  header_state = h_general;
                  break;
              }
              break;

            /* connection */

            case h_matching_connection:
              header_index++;
              if (header_index > sizeof(CONNECTION)-1
                  || c != CONNECTION[header_index]) {
                header_state = h_general;
              } else if (header_index == sizeof(CONNECTION)-2) {
                header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              header_index++;
              if (header_index > sizeof(CONTENT_LENGTH)-1
                  || c != CONTENT_LENGTH[header_index]) {
                header_state = h_general;
              } else if (header_index == sizeof(CONTENT_LENGTH)-2) {
                header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              header_index++;
              if (header_index > sizeof(TRANSFER_ENCODING)-1
                  || c != TRANSFER_ENCODING[header_index]) {
                header_state = h_general;
              } else if (header_index == sizeof(TRANSFER_ENCODING)-2) {
                header_state = h_transfer_encoding;
              }
              break;

            case h_connection:
            case h_content_length:
            case h_transfer_encoding:
              if (ch != ' ') header_state = h_general;
              break;

            default:
              assert(0 && "Unknown header_state");
              break;
          }
          break;
        }

        if (ch == ':') {
          CALLBACK(header_field);
          state = s_header_value_start;
          break;
        }

        if (ch == CR) {
          state = s_header_almost_done;
          CALLBACK(header_field);
          break;
        }

        if (ch == LF) {
          CALLBACK(header_field);
          state = s_header_field_start;
          break;
        }

        return ERROR;
      }

      case s_header_value_start:
      {
        if (ch == ' ') break;

        MARK(header_value);

        state = s_header_value;
        header_index = 0;

        c = lowcase[(int)ch];

        if (!c) {
          if (ch == CR) {
            header_state = h_general;
            state = s_header_almost_done;
            break;
          }

          if (ch == LF) {
            state = s_header_field_start;
            break;
          }

          header_state = h_general;
          break;
        } 

        switch (header_state) {
          case h_transfer_encoding:
            /* looking for 'Transfer-Encoding: chunked' */
            if ('c' == c) {
              header_state = h_matching_transfer_encoding_chunked;
            } else {
              header_state = h_general;
            }
            break;

          case h_content_length:
            if (ch < '0' || ch > '9') return ERROR;
            parser->content_length = ch - '0';
            break;

          case h_connection:
            /* looking for 'Connection: keep-alive' */
            if (c == 'k') {
              header_state = h_matching_connection_keep_alive;
            /* looking for 'Connection: close' */
            } else if (c == 'c') {
              header_state = h_matching_connection_close;
            } else {
              header_state = h_general;
            }
            break;

          default:
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_value:
      {
        c = lowcase[(int)ch];

        if (!c) {
          if (ch == CR) {
            CALLBACK(header_value);
            state = s_header_almost_done;
            break;
          }

          if (ch == LF) {
            CALLBACK(header_value);
            state = s_header_field_start;
            break;
          }
          break;
        }

        switch (header_state) {
          case h_general:
            break;

          case h_connection:
          case h_transfer_encoding:
            assert(0 && "Shouldn't get here.");
            break;

          case h_content_length:
            if (ch < '0' || ch > '9') return ERROR;
            parser->content_length *= 10;
            parser->content_length += ch - '0';
            break;

          /* Transfer-Encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            header_index++;
            if (header_index > sizeof(CHUNKED)-1 
                || c != CHUNKED[header_index]) {
              header_state = h_general;
            } else if (header_index == sizeof(CHUNKED)-2) {
              header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'Connection: keep-alive' */
          case h_matching_connection_keep_alive:
            header_index++;
            if (header_index > sizeof(KEEP_ALIVE)-1
                || c != KEEP_ALIVE[header_index]) {
              header_state = h_general;
            } else if (header_index == sizeof(KEEP_ALIVE)-2) {
              header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'Connection: close' */
          case h_matching_connection_close:
            header_index++;
            if (header_index > sizeof(CLOSE)-1 || c != CLOSE[header_index]) {
              header_state = h_general;
            } else if (header_index == sizeof(CLOSE)-2) {
              header_state = h_connection_close;
            }
            break;

          case h_transfer_encoding_chunked:
          case h_connection_keep_alive:
          case h_connection_close:
            if (ch != ' ') header_state = h_general;
            break;

          default:
            state = s_header_value;
            header_state = h_general;
            break;
        }
        break;
      }

      case s_header_almost_done:
      {
        STRICT_CHECK(ch != LF);

        state = s_header_field_start;

        switch (header_state) {
          case h_connection_keep_alive:
            parser->flags |= F_CONNECTION_KEEP_ALIVE;
            break;
          case h_connection_close:
            parser->flags |= F_CONNECTION_CLOSE;
            break;
          case h_transfer_encoding_chunked:
            parser->flags |= F_CHUNKED;
            break;
          default:
            break;
        }
        break;
      }

      case s_headers_almost_done:
      {
        STRICT_CHECK(ch != LF);

        if (parser->flags & F_TRAILING) {
          /* End of a chunked request */
          CALLBACK2(message_complete);
          state = NEW_MESSAGE();
          break;
        }

        parser->body_read = 0;

        CALLBACK2(headers_complete);
        
        if (parser->flags & F_CHUNKED) {
          /* chunked encoding - ignore Content-Length header */
          state = s_chunk_size_start;
        } else {
          if (parser->content_length == 0) {
            /* Content-Length header given but zero: Content-Length: 0\r\n */
            CALLBACK2(message_complete);
            state = NEW_MESSAGE();
          } else if (parser->content_length > 0) {
            /* Content-Length header given and non-zero */
            state = s_body_identity;
          } else {
            if (start_state == s_start_req || http_should_keep_alive(parser)) {
              /* Assume content-length 0 - read the next */
              CALLBACK2(message_complete);
              state = NEW_MESSAGE();
            } else {
              /* Read body until EOF */
              state = s_body_identity_eof;
            }
          }
        }

        break;
      }

      case s_body_identity:
        to_read = MIN(pe - p, (ssize_t)(parser->content_length - parser->body_read));
        if (to_read > 0) {
          if (parser->on_body) parser->on_body(parser, p, to_read);
          p += to_read - 1;
          parser->body_read += to_read;
          if (parser->body_read == parser->content_length) {
            CALLBACK2(message_complete);
            state = NEW_MESSAGE();
          }
        }
        break;

      /* read until EOF */
      case s_body_identity_eof:
        to_read = pe - p;
        if (to_read > 0) {
          if (parser->on_body) parser->on_body(parser, p, to_read);
          p += to_read - 1;
          parser->body_read += to_read;
        }
        break;

      case s_chunk_size_start:
      {
        assert(parser->flags & F_CHUNKED);

        c = unhex[(int)ch];
        if (c == -1) return ERROR;
        parser->content_length = c;
        state = s_chunk_size;
        break;
      }

      case s_chunk_size:
      {
        assert(parser->flags & F_CHUNKED);

        if (ch == CR) {
          state = s_chunk_size_almost_done;
          break;
        }

        c = unhex[(int)ch];

        if (c == -1) {
          if (ch == ';' || ch == ' ') {
            state = s_chunk_parameters;
            break;
          }
          return ERROR;
        }

        parser->content_length *= 16;
        parser->content_length += c;
        break;
      }

      case s_chunk_parameters:
      {
        assert(parser->flags & F_CHUNKED);
        /* just ignore this shit. TODO check for overflow */
        if (ch == CR) {
          state = s_chunk_size_almost_done;
          break;
        }
        break;
      }

      case s_chunk_size_almost_done:
      {
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);

        if (parser->content_length == 0) {
          parser->flags |= F_TRAILING;
          state = s_header_field_start;
        } else {
          state = s_chunk_data;
        }
        break;
      }

      case s_chunk_data:
      {
        assert(parser->flags & F_CHUNKED);

        to_read = MIN(pe - p, (ssize_t)(parser->content_length));

        if (to_read > 0) {
          if (parser->on_body) parser->on_body(parser, p, to_read);
          p += to_read - 1;
        }

        if (to_read == parser->content_length) {
          state = s_chunk_data_almost_done;
        }

        parser->content_length -= to_read;
        break;
      }

      case s_chunk_data_almost_done:
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != CR);
        state = s_chunk_data_done;
        break;

      case s_chunk_data_done:
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);
        state = s_chunk_size_start;
        break;

      default:
        assert(0 && "unhandled state");
        return ERROR;
    }
  }

  CALLBACK_NOCLEAR(header_field);
  CALLBACK_NOCLEAR(header_value);
  CALLBACK_NOCLEAR(fragment);
  CALLBACK_NOCLEAR(query_string);
  CALLBACK_NOCLEAR(path);
  CALLBACK_NOCLEAR(url);

  parser->state = state;
  parser->header_state = header_state;
  parser->header_index = header_index;

  return len;
}


size_t
http_parse_requests (http_parser *parser, const char *data, size_t len)
{
  if (!parser->state) parser->state = s_start_req;
  return parse(parser, data, len, s_start_req);
}


size_t
http_parse_responses (http_parser *parser, const char *data, size_t len)
{
  if (!parser->state) parser->state = s_start_res;
  return parse(parser, data, len, s_start_res);
}


int
http_should_keep_alive (http_parser *parser)
{
  if (parser->http_major > 0 && parser->http_minor > 0) {
    /* HTTP/1.1 */
    if (parser->flags & F_CONNECTION_CLOSE) {
      return 0;
    } else {
      return 1;
    }
  } else {
    /* HTTP/1.0 or earlier */
    if (parser->flags & F_CONNECTION_KEEP_ALIVE) {
      return 1;
    } else {
      return 0;
    }
  }
}


void
http_parser_init (http_parser *parser)
{
  parser->state = 0;
  parser->on_message_begin = NULL;
  parser->on_path = NULL;
  parser->on_query_string = NULL;
  parser->on_url = NULL;
  parser->on_fragment = NULL;
  parser->on_header_field = NULL;
  parser->on_header_value = NULL;
  parser->on_headers_complete = NULL;
  parser->on_body = NULL;
  parser->on_message_complete = NULL;

  parser->header_field_mark = NULL;
  parser->header_value_mark = NULL;
  parser->query_string_mark = NULL;
  parser->path_mark = NULL;
  parser->url_mark = NULL;
  parser->fragment_mark = NULL;
}


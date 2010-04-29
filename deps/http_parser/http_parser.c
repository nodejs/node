/* Copyright 2009,2010 Ryan Dahl <ry@tinyclouds.org>
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
#include <stddef.h>


#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif


#define MAX_FIELD_SIZE (80*1024)


#define CALLBACK2(FOR)                                               \
do {                                                                 \
  if (settings.on_##FOR) {                                           \
    if (0 != settings.on_##FOR(parser)) return (p - data);           \
  }                                                                  \
} while (0)


#define MARK(FOR)                                                    \
do {                                                                 \
  parser->FOR##_mark = p;                                            \
  parser->FOR##_size = 0;                                            \
} while (0)


#define CALLBACK_NOCLEAR(FOR)                                        \
do {                                                                 \
  if (parser->FOR##_mark) {                                          \
    parser->FOR##_size += p - parser->FOR##_mark;                    \
    if (parser->FOR##_size > MAX_FIELD_SIZE) return (p - data);      \
    if (settings.on_##FOR) {                                         \
      if (0 != settings.on_##FOR(parser,                             \
                                 parser->FOR##_mark,                 \
                                 p - parser->FOR##_mark))            \
      {                                                              \
        return (p - data);                                           \
      }                                                              \
    }                                                                \
  }                                                                  \
} while (0)


#define CALLBACK(FOR)                                                \
do {                                                                 \
  CALLBACK_NOCLEAR(FOR);                                             \
  parser->FOR##_mark = NULL;                                         \
} while (0)


#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"


static const unsigned char lowcase[] =
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
  "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0_"
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

#define USUAL(c) (usual[c >> 5] & (1 << (c & 0x1f)))


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

  , s_req_method
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
  /* Important: 's_headers_almost_done' must be the last 'header' state. All
   * states beyond this must be 'body' states. It is used for overflow
   * checking. See the PARSING_HEADER() macro.
   */
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


#define PARSING_HEADER(state) (state <= s_headers_almost_done && 0 == (parser->flags & F_TRAILING))


enum header_states
  { h_general = 0
  , h_C
  , h_CO
  , h_CON

  , h_matching_connection
  , h_matching_proxy_connection
  , h_matching_content_length
  , h_matching_transfer_encoding
  , h_matching_upgrade

  , h_connection
  , h_content_length
  , h_transfer_encoding
  , h_upgrade

  , h_matching_transfer_encoding_chunked
  , h_matching_connection_keep_alive
  , h_matching_connection_close

  , h_transfer_encoding_chunked
  , h_connection_keep_alive
  , h_connection_close
  };


enum flags
  { F_CHUNKED               = 1 << 0
  , F_CONNECTION_KEEP_ALIVE = 1 << 1
  , F_CONNECTION_CLOSE      = 1 << 2
  , F_TRAILING              = 1 << 3
  , F_UPGRADE               = 1 << 4
  };


#define CR '\r'
#define LF '\n'
#define LOWER(c) (unsigned char)(c | 0x20)


#define start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)


#if HTTP_PARSER_STRICT
# define STRICT_CHECK(cond) if (cond) goto error
# define NEW_MESSAGE() (http_should_keep_alive(parser) ? start_state : s_dead)
#else
# define STRICT_CHECK(cond)
# define NEW_MESSAGE() start_state
#endif


#define ngx_str3_cmp(m, c0, c1, c2)                                           \
    m[0] == c0 && m[1] == c1 && m[2] == c2

#define ngx_str3Ocmp(m, c0, c1, c2, c3)                                       \
    m[0] == c0 && m[2] == c2 && m[3] == c3

#define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3

#define ngx_str5cmp(m, c0, c1, c2, c3, c4)                                    \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3 && m[4] == c4

#define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5

#define ngx_str7_cmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                       \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5 && m[6] == c6

#define ngx_str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                        \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5 && m[6] == c6 && m[7] == c7

#define ngx_str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                    \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5 && m[6] == c6 && m[7] == c7 && m[8] == c8


size_t http_parser_execute (http_parser *parser,
                            http_parser_settings settings,
                            const char *data,
                            size_t len)
{
  char c, ch;
  const char *p = data, *pe;
  ssize_t to_read;

  enum state state = (enum state) parser->state;
  enum header_states header_state = (enum header_states) parser->header_state;
  size_t index = parser->index;
  size_t nread = parser->nread;

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

    if (++nread > HTTP_MAX_HEADER_SIZE && PARSING_HEADER(state)) {
      /* Buffer overflow attack */
      goto error;
    }

    switch (state) {

      case s_dead:
        /* this state is used after a 'Connection: close' message
         * the parser will error out if it reads another message
         */
        goto error;

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
            goto error;
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
        if (ch < '1' || ch > '9') goto error;
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

        if (ch < '0' || ch > '9') goto error;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) goto error;
        break;
      }

      /* first digit of minor HTTP version */
      case s_res_first_http_minor:
        if (ch < '0' || ch > '9') goto error;
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

        if (ch < '0' || ch > '9') goto error;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) goto error;
        break;
      }

      case s_res_first_status_code:
      {
        if (ch < '0' || ch > '9') {
          if (ch == ' ') {
            break;
          }
          goto error;
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
              goto error;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) goto error;
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
        if (ch == CR || ch == LF)
          break;
        parser->flags = 0;
        parser->content_length = -1;

        CALLBACK2(message_begin);

        if (ch < 'A' || 'Z' < ch) goto error;

        parser->method = (enum http_method) 0;
        index = 0;
        parser->buffer[0] = ch;
        state = s_req_method;
        break;
      }

      case s_req_method:
        if (ch == ' ') {
          assert(index+1 < HTTP_PARSER_MAX_METHOD_LEN);
          parser->buffer[index+1] = '\0';

          switch (index+1) {
            case 3:
              if (ngx_str3_cmp(parser->buffer, 'G', 'E', 'T')) {
                parser->method = HTTP_GET;
                break;
              }

              if (ngx_str3_cmp(parser->buffer, 'P', 'U', 'T')) {
                parser->method = HTTP_PUT;
                break;
              }

              break;

            case 4:
              if (ngx_str4cmp(parser->buffer, 'P', 'O', 'S', 'T')) {
                parser->method = HTTP_POST;
                break;
              }

              if (ngx_str4cmp(parser->buffer, 'H', 'E', 'A', 'D')) {
                parser->method = HTTP_HEAD;
                break;
              }

              if (ngx_str4cmp(parser->buffer, 'C', 'O', 'P', 'Y')) {
                parser->method = HTTP_COPY;
                break;
              }

              if (ngx_str4cmp(parser->buffer, 'M', 'O', 'V', 'E')) {
                parser->method = HTTP_MOVE;
                break;
              }

              break;

            case 5:
              if (ngx_str5cmp(parser->buffer, 'M', 'K', 'C', 'O', 'L')) {
                parser->method = HTTP_MKCOL;
                break;
              }

              if (ngx_str5cmp(parser->buffer, 'T', 'R', 'A', 'C', 'E')) {
                parser->method = HTTP_TRACE;
                break;
              }

              break;

            case 6:
              if (ngx_str6cmp(parser->buffer, 'D', 'E', 'L', 'E', 'T', 'E')) {
                parser->method = HTTP_DELETE;
                break;
              }

              if (ngx_str6cmp(parser->buffer, 'U', 'N', 'L', 'O', 'C', 'K')) {
                parser->method = HTTP_UNLOCK;
                break;
              }

              break;

            case 7:
              if (ngx_str7_cmp(parser->buffer,
                    'O', 'P', 'T', 'I', 'O', 'N', 'S', '\0')) {
                parser->method = HTTP_OPTIONS;
                break;
              }

              if (ngx_str7_cmp(parser->buffer,
                    'C', 'O', 'N', 'N', 'E', 'C', 'T', '\0')) {
                parser->method = HTTP_CONNECT;
                break;
              }

              break;

            case 8:
              if (ngx_str8cmp(parser->buffer,
                    'P', 'R', 'O', 'P', 'F', 'I', 'N', 'D')) {
                parser->method = HTTP_PROPFIND;
                break;
              }

              break;

            case 9:
              if (ngx_str9cmp(parser->buffer, 
                    'P', 'R', 'O', 'P', 'P', 'A', 'T', 'C', 'H')) {
                parser->method = HTTP_PROPPATCH;
                break;
              }

              break;
          }
          state = s_req_spaces_before_url;
          break;
        }

        if (ch < 'A' || 'Z' < ch) goto error;

        if (++index >= HTTP_PARSER_MAX_METHOD_LEN - 1) {
          goto error;
        }

        parser->buffer[index] = ch;

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

        goto error;
      }

      case s_req_schema:
      {
        c = LOWER(ch);

        if (c >= 'a' && c <= 'z') break;

        if (ch == ':') {
          state = s_req_schema_slash;
          break;
        }

        goto error;
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
            goto error;
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
            goto error;
        }
        break;
      }

      case s_req_path:
      {
        if (USUAL(ch)) break;

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
            goto error;
        }
        break;
      }

      case s_req_query_string_start:
      {
        if (USUAL(ch)) {
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
            goto error;
        }
        break;
      }

      case s_req_query_string:
      {
        if (USUAL(ch)) break;

        switch (ch) {
          case '?':
            // allow extra '?' in query string
            break;
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
            goto error;
        }
        break;
      }

      case s_req_fragment_start:
      {
        if (USUAL(ch)) {
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
            goto error;
        }
        break;
      }

      case s_req_fragment:
      {
        if (USUAL(ch)) break;

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
            goto error;
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
            goto error;
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
        if (ch < '1' || ch > '9') goto error;
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

        if (ch < '0' || ch > '9') goto error;

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) goto error;
        break;
      }

      /* first digit of minor HTTP version */
      case s_req_first_http_minor:
        if (ch < '0' || ch > '9') goto error;
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

        if (ch < '0' || ch > '9') goto error;

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) goto error;
        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != LF) goto error;
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
          /* they might be just sending \n instead of \r\n so this would be
           * the second \n to denote the end of headers*/
          state = s_headers_almost_done;
          goto headers_almost_done;
        }

        c = LOWER(ch);

        if (c < 'a' || 'z' < c) goto error;

        MARK(header_field);

        index = 0;
        state = s_header_field;

        switch (c) {
          case 'c':
            header_state = h_C;
            break;

          case 'p':
            header_state = h_matching_proxy_connection;
            break;

          case 't':
            header_state = h_matching_transfer_encoding;
            break;

          case 'u':
            header_state = h_matching_upgrade;
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
              index++;
              header_state = (c == 'o' ? h_CO : h_general);
              break;

            case h_CO:
              index++;
              header_state = (c == 'n' ? h_CON : h_general);
              break;

            case h_CON:
              index++;
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
              index++;
              if (index > sizeof(CONNECTION)-1
                  || c != CONNECTION[index]) {
                header_state = h_general;
              } else if (index == sizeof(CONNECTION)-2) {
                header_state = h_connection;
              }
              break;

            /* proxy-connection */

            case h_matching_proxy_connection:
              index++;
              if (index > sizeof(PROXY_CONNECTION)-1
                  || c != PROXY_CONNECTION[index]) {
                header_state = h_general;
              } else if (index == sizeof(PROXY_CONNECTION)-2) {
                header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              index++;
              if (index > sizeof(CONTENT_LENGTH)-1
                  || c != CONTENT_LENGTH[index]) {
                header_state = h_general;
              } else if (index == sizeof(CONTENT_LENGTH)-2) {
                header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              index++;
              if (index > sizeof(TRANSFER_ENCODING)-1
                  || c != TRANSFER_ENCODING[index]) {
                header_state = h_general;
              } else if (index == sizeof(TRANSFER_ENCODING)-2) {
                header_state = h_transfer_encoding;
              }
              break;

            /* upgrade */

            case h_matching_upgrade:
              index++;
              if (index > sizeof(UPGRADE)-1
                  || c != UPGRADE[index]) {
                header_state = h_general;
              } else if (index == sizeof(UPGRADE)-2) {
                header_state = h_upgrade;
              }
              break;

            case h_connection:
            case h_content_length:
            case h_transfer_encoding:
            case h_upgrade:
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

        goto error;
      }

      case s_header_value_start:
      {
        if (ch == ' ') break;

        MARK(header_value);

        state = s_header_value;
        index = 0;

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
          case h_upgrade:
            parser->flags |= F_UPGRADE;
            header_state = h_general;
            break;

          case h_transfer_encoding:
            /* looking for 'Transfer-Encoding: chunked' */
            if ('c' == c) {
              header_state = h_matching_transfer_encoding_chunked;
            } else {
              header_state = h_general;
            }
            break;

          case h_content_length:
            if (ch < '0' || ch > '9') goto error;
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
            goto header_almost_done;
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
            if (ch < '0' || ch > '9') goto error;
            parser->content_length *= 10;
            parser->content_length += ch - '0';
            break;

          /* Transfer-Encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            index++;
            if (index > sizeof(CHUNKED)-1
                || c != CHUNKED[index]) {
              header_state = h_general;
            } else if (index == sizeof(CHUNKED)-2) {
              header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'Connection: keep-alive' */
          case h_matching_connection_keep_alive:
            index++;
            if (index > sizeof(KEEP_ALIVE)-1
                || c != KEEP_ALIVE[index]) {
              header_state = h_general;
            } else if (index == sizeof(KEEP_ALIVE)-2) {
              header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'Connection: close' */
          case h_matching_connection_close:
            index++;
            if (index > sizeof(CLOSE)-1 || c != CLOSE[index]) {
              header_state = h_general;
            } else if (index == sizeof(CLOSE)-2) {
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
      header_almost_done:
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
      headers_almost_done:
      {
        STRICT_CHECK(ch != LF);

        if (parser->flags & F_TRAILING) {
          /* End of a chunked request */
          CALLBACK2(message_complete);
          state = NEW_MESSAGE();
          break;
        }

        parser->body_read = 0;
        nread = 0;

        if (parser->flags & F_UPGRADE) parser->upgrade = 1;

        CALLBACK2(headers_complete);

        // Exit, the rest of the connect is in a different protocol.
        if (parser->flags & F_UPGRADE) {
          CALLBACK2(message_complete);
          return (p - data);
        }

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
            if (parser->type == HTTP_REQUEST || http_should_keep_alive(parser)) {
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
          if (settings.on_body) settings.on_body(parser, p, to_read);
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
          if (settings.on_body) settings.on_body(parser, p, to_read);
          p += to_read - 1;
          parser->body_read += to_read;
        }
        break;

      case s_chunk_size_start:
      {
        assert(parser->flags & F_CHUNKED);

        c = unhex[(int)ch];
        if (c == -1) goto error;
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
          goto error;
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
          if (settings.on_body) settings.on_body(parser, p, to_read);
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
        goto error;
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
  parser->index = index;
  parser->nread = nread;

  return len;

error:
  return (p - data);
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
http_parser_init (http_parser *parser, enum http_parser_type t)
{
  parser->type = t;
  parser->state = (t == HTTP_REQUEST ? s_start_req : s_start_res);
  parser->nread = 0;
  parser->upgrade = 0;

  parser->header_field_mark = NULL;
  parser->header_value_mark = NULL;
  parser->query_string_mark = NULL;
  parser->path_mark = NULL;
  parser->url_mark = NULL;
  parser->fragment_mark = NULL;
}


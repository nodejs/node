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
#include "http_parser.h"
#include <limits.h>
#include <assert.h>

/* parser->flags */
#define EATING      0x01
#define ERROR       0x02
#define CHUNKED     0x04
#define EAT_FOREVER 0x10

static int unhex[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     };

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#undef NULL
#define NULL ((void*)(0))

#define MAX_FIELD_SIZE (80*1024)

#define REMAINING (unsigned long)(pe - p)
#define CALLBACK(FOR)                                                \
do {                                                                 \
  if (parser->FOR##_mark) {                                          \
    parser->FOR##_size += p - parser->FOR##_mark;                    \
    if (parser->FOR##_size > MAX_FIELD_SIZE) {                       \
      parser->flags |= ERROR;                                        \
      return;                                                        \
    }                                                                \
    if (parser->on_##FOR) {                                          \
      callback_return_value = parser->on_##FOR(parser,               \
        parser->FOR##_mark,                                          \
        p - parser->FOR##_mark);                                     \
    }                                                                \
    if (callback_return_value != 0) {                                \
      parser->flags |= ERROR;                                        \
      return;                                                        \
    }                                                                \
  }                                                                  \
} while(0)

#define RESET_PARSER(parser)                                         \
  parser->chunk_size = 0;                                            \
  parser->flags = 0;                                                 \
  parser->header_field_mark = NULL;                                  \
  parser->header_value_mark = NULL;                                  \
  parser->query_string_mark = NULL;                                  \
  parser->path_mark = NULL;                                          \
  parser->uri_mark = NULL;                                           \
  parser->fragment_mark = NULL;                                      \
  parser->status_code = 0;                                           \
  parser->method = 0;                                                \
  parser->version = HTTP_VERSION_OTHER;                              \
  parser->keep_alive = -1;                                           \
  parser->content_length = -1;                                       \
  parser->body_read = 0

#define END_REQUEST                                                  \
do {                                                                 \
    if (parser->on_message_complete) {                               \
      callback_return_value =                                        \
        parser->on_message_complete(parser);                         \
    }                                                                \
    RESET_PARSER(parser);                                            \
} while (0)

#define SKIP_BODY(nskip)                                             \
do {                                                                 \
  tmp = (nskip);                                                     \
  if (parser->on_body && tmp > 0) {                                  \
    callback_return_value = parser->on_body(parser, p, tmp);         \
  }                                                                  \
  if (callback_return_value == 0) {                                  \
    p += tmp;                                                        \
    parser->body_read += tmp;                                        \
    parser->chunk_size -= tmp;                                       \
    if (0 == parser->chunk_size) {                                   \
      parser->flags &= ~EATING;                                      \
      if (!(parser->flags & CHUNKED)) {                              \
        END_REQUEST;                                                 \
      }                                                              \
    } else {                                                         \
      parser->flags |= EATING;                                       \
    }                                                                \
  }                                                                  \
} while (0)

%%{
  machine http_parser;

  action mark_header_field {
    parser->header_field_mark = p;
    parser->header_field_size = 0;
  }

  action mark_header_value {
    parser->header_value_mark = p;
    parser->header_value_size = 0;
  }

  action mark_fragment {
    parser->fragment_mark = p;
    parser->fragment_size = 0;
  }

  action mark_query_string {
    parser->query_string_mark = p;
    parser->query_string_size = 0;
  }

  action mark_request_path {
    parser->path_mark = p;
    parser->path_size = 0;
  }

  action mark_request_uri {
    parser->uri_mark = p;
    parser->uri_size = 0;
  }

  action header_field {
    CALLBACK(header_field);
    parser->header_field_mark = NULL;
    parser->header_field_size = 0;
  }

  action header_value {
    CALLBACK(header_value);
    parser->header_value_mark = NULL;
    parser->header_value_size = 0;
  }

  action request_uri {
    CALLBACK(uri);
    parser->uri_mark = NULL;
    parser->uri_size = 0;
  }

  action fragment {
    CALLBACK(fragment);
    parser->fragment_mark = NULL;
    parser->fragment_size = 0;
  }

  action query_string {
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
    parser->query_string_size = 0;
  }

  action request_path {
    CALLBACK(path);
    parser->path_mark = NULL;
    parser->path_size = 0;
  }

  action headers_complete {
    if(parser->on_headers_complete) {
      callback_return_value = parser->on_headers_complete(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }

  action begin_message {
    if(parser->on_message_begin) {
      callback_return_value = parser->on_message_begin(parser);
      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }
    }
  }

  action content_length {
    if (parser->content_length == -1) parser->content_length = 0;
    if (parser->content_length > INT_MAX) {
      parser->flags |= ERROR;
      return;
    }
    parser->content_length *= 10;
    parser->content_length += *p - '0';
  }

  action status_code {
    parser->status_code *= 10;
    parser->status_code += *p - '0';
  }

  action use_chunked_encoding { parser->flags |= CHUNKED;  }

  action set_keep_alive { parser->keep_alive = 1; }
  action set_not_keep_alive { parser->keep_alive = 0; }

  action version_11 { parser->version = HTTP_VERSION_11; }
  action version_10 { parser->version = HTTP_VERSION_10; }
  action version_09 { parser->version = HTTP_VERSION_09; }

  action add_to_chunk_size {
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }

  action skip_chunk_data {
    SKIP_BODY(MIN(parser->chunk_size, REMAINING));
    if (callback_return_value != 0) {
      parser->flags |= ERROR;
      return;
    }

    fhold;
    if (parser->chunk_size > REMAINING) {
      fbreak;
    } else {
      fgoto chunk_end;
    }
  }

  action end_chunked_body {
    END_REQUEST;
    if (parser->type == HTTP_REQUEST) {
      fnext Requests;
    } else {
      fnext Responses;
    }
  }

  action body_logic {
    if (parser->flags & CHUNKED) {
      fnext ChunkedBody;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with
       * skip_chunk_data */
      if (parser->content_length < 0) {
        /* If we didn't get a content length; if not keep-alive
         * just read body until EOF */
        if (!http_parser_should_keep_alive(parser)) {
          parser->flags |= EAT_FOREVER;
          parser->chunk_size = REMAINING;
        } else {
          /* Otherwise, if keep-alive, then assume the message
           * has no body. */
          parser->chunk_size = parser->content_length = 0;
        }
      } else {
        parser->chunk_size = parser->content_length;
      }
      p += 1;

      SKIP_BODY(MIN(REMAINING, parser->chunk_size));

      if (callback_return_value != 0) {
        parser->flags |= ERROR;
        return;
      }

      fhold;
      if(parser->chunk_size > REMAINING) {
        fbreak;
      }
    }
  }

  CRLF = "\r\n";

# character types
  CTL = (cntrl | 127);
  safe = ("$" | "-" | "_" | ".");
  extra = ("!" | "*" | "'" | "(" | ")" | ",");
  reserved = (";" | "/" | "?" | ":" | "@" | "&" | "=" | "+");
  unsafe = (CTL | " " | "\"" | "#" | "%" | "<" | ">");
  national = any -- (alpha | digit | reserved | extra | safe | unsafe);
  unreserved = (alpha | digit | safe | extra | national);
  escape = ("%" xdigit xdigit);
  uchar = (unreserved | escape | "\"");
  pchar = (uchar | ":" | "@" | "&" | "=" | "+");
  tspecials = ("(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\""
              | "/" | "[" | "]" | "?" | "=" | "{" | "}" | " " | "\t");

# elements
  token = (ascii -- (CTL | tspecials));
  quote = "\"";
#  qdtext = token -- "\"";
#  quoted_pair = "\" ascii;
#  quoted_string = "\"" (qdtext | quoted_pair )* "\"";

#  headers

  Method = ( "COPY"      %{ parser->method = HTTP_COPY;      }
           | "DELETE"    %{ parser->method = HTTP_DELETE;    }
           | "GET"       %{ parser->method = HTTP_GET;       }
           | "HEAD"      %{ parser->method = HTTP_HEAD;      }
           | "LOCK"      %{ parser->method = HTTP_LOCK;      }
           | "MKCOL"     %{ parser->method = HTTP_MKCOL;     }
           | "MOVE"      %{ parser->method = HTTP_MOVE;      }
           | "OPTIONS"   %{ parser->method = HTTP_OPTIONS;   }
           | "POST"      %{ parser->method = HTTP_POST;      }
           | "PROPFIND"  %{ parser->method = HTTP_PROPFIND;  }
           | "PROPPATCH" %{ parser->method = HTTP_PROPPATCH; }
           | "PUT"       %{ parser->method = HTTP_PUT;       }
           | "TRACE"     %{ parser->method = HTTP_TRACE;     }
           | "UNLOCK"    %{ parser->method = HTTP_UNLOCK;    }
           ); # Not allowing extension methods

  HTTP_Version = "HTTP/" ( "1.1" %version_11
                         | "1.0" %version_10
                         | "0.9" %version_09
                         | (digit "." digit)
                         );

  scheme = ( alpha | digit | "+" | "-" | "." )* ;
  absolute_uri = (scheme ":" (uchar | reserved )*);
  path = ( pchar+ ( "/" pchar* )* ) ;
  query = ( uchar | reserved )* >mark_query_string %query_string ;
  param = ( pchar | "/" )* ;
  params = ( param ( ";" param )* ) ;
  rel_path = ( path? (";" params)? ) ;
  absolute_path = ( "/"+ rel_path ) >mark_request_path %request_path ("?" query)?;
  Request_URI = ( "*" | absolute_uri | absolute_path ) >mark_request_uri %request_uri;
  Fragment = ( uchar | reserved )* >mark_fragment %fragment;

  field_name = ( token -- ":" )+;
  Field_Name = field_name >mark_header_field %header_field;

  field_value = ((any - " ") any*)?;
  Field_Value = field_value >mark_header_value %header_value;

  hsep = ":" " "*;
  header = (field_name hsep field_value) :> CRLF;
  Header = ( ("Content-Length"i hsep digit+ $content_length)
           | ("Connection"i hsep
               ( "Keep-Alive"i %set_keep_alive
               | "close"i %set_not_keep_alive
               )
             )
           | ("Transfer-Encoding"i hsep "chunked"i %use_chunked_encoding)
           | (Field_Name hsep Field_Value)
           ) :> CRLF;

  Headers = (Header)* :> CRLF @headers_complete;

  Request_Line = ( Method " " Request_URI ("#" Fragment)? " " HTTP_Version CRLF ) ;

  StatusCode = (digit digit digit) $status_code;
  ReasonPhrase =  ascii* -- ("\r" | "\n");
  StatusLine = HTTP_Version  " " StatusCode (" " ReasonPhrase)? CRLF;

# chunked message
  trailing_headers = header*;
  #chunk_ext_val   = token | quoted_string;
  chunk_ext_val = token*;
  chunk_ext_name = token*;
  chunk_extension = ( ";" " "* chunk_ext_name ("=" chunk_ext_val)? )*;
  last_chunk = "0"+ ( chunk_extension | " "+) CRLF;
  chunk_size = (xdigit* [1-9a-fA-F] xdigit* ) $add_to_chunk_size;
  chunk_end  = CRLF;
  chunk_body = any >skip_chunk_data;
  chunk_begin = chunk_size ( chunk_extension | " "+ ) CRLF;
  chunk = chunk_begin chunk_body chunk_end;
  ChunkedBody := chunk* last_chunk trailing_headers CRLF @end_chunked_body;

  Request = (Request_Line Headers) >begin_message @body_logic;
  Response = (StatusLine Headers) >begin_message @body_logic;

  Requests := Request*;
  Responses := Response*;

  main := any >{
    fhold;
    if (parser->type == HTTP_REQUEST) {
      fgoto Requests;
    } else {
      fgoto Responses;
    }
  };

}%%

%% write data;

void
http_parser_init (http_parser *parser, enum http_parser_type type)
{
  int cs = 0;
  %% write init;
  parser->cs = cs;
  parser->type = type;

  parser->on_message_begin = NULL;
  parser->on_path = NULL;
  parser->on_query_string = NULL;
  parser->on_uri = NULL;
  parser->on_fragment = NULL;
  parser->on_header_field = NULL;
  parser->on_header_value = NULL;
  parser->on_headers_complete = NULL;
  parser->on_body = NULL;
  parser->on_message_complete = NULL;

  RESET_PARSER(parser);
}

/** exec **/
void
http_parser_execute (http_parser *parser, const char *buffer, size_t len)
{
  size_t tmp; // REMOVE ME this is extremely hacky
  int callback_return_value = 0;
  const char *p, *pe, *eof;
  int cs = parser->cs;

  p = buffer;
  pe = buffer+len;
  eof = len ? NULL : pe;

  if (parser->flags & EAT_FOREVER) {
    if (len == 0) {
      if (parser->on_message_complete) {
        callback_return_value = parser->on_message_complete(parser);
        if (callback_return_value != 0) parser->flags |= ERROR;
      }
    } else {
      if (parser->on_body) {
        callback_return_value = parser->on_body(parser, p, len);
        if (callback_return_value != 0) parser->flags |= ERROR;
      }
    }
    return;
  }

  if (0 < parser->chunk_size && (parser->flags & EATING)) {
    /* eat body */
    SKIP_BODY(MIN(len, parser->chunk_size));
    if (callback_return_value != 0) {
      parser->flags |= ERROR;
      return;
    }
  }

  if (parser->header_field_mark)   parser->header_field_mark   = buffer;
  if (parser->header_value_mark)   parser->header_value_mark   = buffer;
  if (parser->fragment_mark)       parser->fragment_mark       = buffer;
  if (parser->query_string_mark)   parser->query_string_mark   = buffer;
  if (parser->path_mark)           parser->path_mark           = buffer;
  if (parser->uri_mark)            parser->uri_mark            = buffer;

  %% write exec;

  parser->cs = cs;

  CALLBACK(header_field);
  CALLBACK(header_value);
  CALLBACK(fragment);
  CALLBACK(query_string);
  CALLBACK(path);
  CALLBACK(uri);

  assert(p <= pe && "buffer overflow after parsing execute");
}

int
http_parser_has_error (http_parser *parser)
{
  if (parser->flags & ERROR) return 1;
  return parser->cs == http_parser_error;
}

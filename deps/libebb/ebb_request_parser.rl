/* This file is part of the libebb web server library
 *
 * Copyright (c) 2008 Ryan Dahl (ry@ndahl.us)
 * All rights reserved.
 *
 * This parser is based on code from Zed Shaw's Mongrel.
 * Copyright (c) 2005 Zed A. Shaw
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
#include "ebb_request_parser.h"

#include <stdio.h>
#include <assert.h>

static int unhex[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
                     };
#define TRUE 1
#define FALSE 0
#define MIN(a,b) (a < b ? a : b)

#define REMAINING (pe - p)
#define CURRENT (parser->current_request)
#define CONTENT_LENGTH (parser->current_request->content_length)
#define CALLBACK(FOR)                               \
  if(parser->FOR##_mark && CURRENT->on_##FOR) {     \
    CURRENT->on_##FOR( CURRENT                      \
                , parser->FOR##_mark                \
                , p - parser->FOR##_mark            \
                );                                  \
 }
#define HEADER_CALLBACK(FOR)                        \
  if(parser->FOR##_mark && CURRENT->on_##FOR) {     \
    CURRENT->on_##FOR( CURRENT                      \
                , parser->FOR##_mark                \
                , p - parser->FOR##_mark            \
                , CURRENT->number_of_headers        \
                );                                  \
 }
#define END_REQUEST                        \
    if(CURRENT->on_complete)               \
      CURRENT->on_complete(CURRENT);       \
    CURRENT = NULL;


%%{
  machine ebb_request_parser;

  action mark_header_field   { parser->header_field_mark   = p; }
  action mark_header_value   { parser->header_value_mark   = p; }
  action mark_fragment       { parser->fragment_mark       = p; }
  action mark_query_string   { parser->query_string_mark   = p; }
  action mark_request_path   { parser->path_mark           = p; }
  action mark_request_uri    { parser->uri_mark            = p; }

  action write_field { 
    HEADER_CALLBACK(header_field);
    parser->header_field_mark = NULL;
  }

  action write_value {
    HEADER_CALLBACK(header_value);
    parser->header_value_mark = NULL;
  }

  action request_uri { 
    CALLBACK(uri);
    parser->uri_mark = NULL;
  }

  action fragment { 
    CALLBACK(fragment);
    parser->fragment_mark = NULL;
  }

  action query_string { 
    CALLBACK(query_string);
    parser->query_string_mark = NULL;
  }

  action request_path {
    CALLBACK(path);
    parser->path_mark = NULL;
  }

  action content_length {
    CURRENT->content_length *= 10;
    CURRENT->content_length += *p - '0';
  }

  action use_identity_encoding { CURRENT->transfer_encoding = EBB_IDENTITY; }
  action use_chunked_encoding { CURRENT->transfer_encoding = EBB_CHUNKED; }

  action set_keep_alive { CURRENT->keep_alive = TRUE; }
  action set_not_keep_alive { CURRENT->keep_alive = FALSE; }

  action expect_continue {
    CURRENT->expect_continue = TRUE;
  }

  action trailer {
    /* not implemenetd yet. (do requests even have trailing headers?) */
  }

  action version_major {
    CURRENT->version_major *= 10;
    CURRENT->version_major += *p - '0';
  }

  action version_minor {
    CURRENT->version_minor *= 10;
    CURRENT->version_minor += *p - '0';
  }

  action end_header_line {
    CURRENT->number_of_headers++;
  }

  action end_headers {
    if(CURRENT->on_headers_complete)
      CURRENT->on_headers_complete(CURRENT);
  }

  action add_to_chunk_size {
    parser->chunk_size *= 16;
    parser->chunk_size += unhex[(int)*p];
  }

  action skip_chunk_data {
    skip_body(&p, parser, MIN(parser->chunk_size, REMAINING));
    fhold; 
    if(parser->chunk_size > REMAINING) {
      fbreak;
    } else {
      fgoto chunk_end; 
    }
  }

  action end_chunked_body {
    END_REQUEST;
    fnext main;
  }

  action start_req {
    assert(CURRENT == NULL);
    CURRENT = parser->new_request(parser->data);
  }

  action body_logic {
    if(CURRENT->transfer_encoding == EBB_CHUNKED) {
      fnext ChunkedBody;
    } else {
      /* this is pretty stupid. i'd prefer to combine this with skip_chunk_data */
      parser->chunk_size = CURRENT->content_length;
      p += 1;  
      skip_body(&p, parser, MIN(REMAINING, CURRENT->content_length));
      fhold;
      if(parser->chunk_size > REMAINING) {
        fbreak;
      } 
    }
  }

#
##
###
#### HTTP/1.1 STATE MACHINE
###
##   RequestHeaders and character types are from
#    Zed Shaw's beautiful Mongrel parser.

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
  uchar = (unreserved | escape);
  pchar = (uchar | ":" | "@" | "&" | "=" | "+");
  tspecials = ("(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\"" | "/" | "[" | "]" | "?" | "=" | "{" | "}" | " " | "\t");

# elements
  token = (ascii -- (CTL | tspecials));
  quote = "\"";
#  qdtext = token -- "\""; 
#  quoted_pair = "\" ascii;
#  quoted_string = "\"" (qdtext | quoted_pair )* "\"";

#  headers

  Method = ( "COPY"      %{ CURRENT->method = EBB_COPY;      }
           | "DELETE"    %{ CURRENT->method = EBB_DELETE;    }
           | "GET"       %{ CURRENT->method = EBB_GET;       }
           | "HEAD"      %{ CURRENT->method = EBB_HEAD;      }
           | "LOCK"      %{ CURRENT->method = EBB_LOCK;      }
           | "MKCOL"     %{ CURRENT->method = EBB_MKCOL;     }
           | "MOVE"      %{ CURRENT->method = EBB_MOVE;      }
           | "OPTIONS"   %{ CURRENT->method = EBB_OPTIONS;   }
           | "POST"      %{ CURRENT->method = EBB_POST;      }
           | "PROPFIND"  %{ CURRENT->method = EBB_PROPFIND;  }
           | "PROPPATCH" %{ CURRENT->method = EBB_PROPPATCH; }
           | "PUT"       %{ CURRENT->method = EBB_PUT;       }
           | "TRACE"     %{ CURRENT->method = EBB_TRACE;     }
           | "UNLOCK"    %{ CURRENT->method = EBB_UNLOCK;    }
           ); # Not allowing extension methods

  HTTP_Version = "HTTP/" digit+ $version_major "." digit+ $version_minor;

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
  Field_Name = field_name >mark_header_field %write_field;

  field_value = ((any - " ") any*)?;
  Field_Value = field_value >mark_header_value %write_value;

  hsep = ":" " "*;
  header = (field_name hsep field_value) :> CRLF;
  Header = ( ("Content-Length"i hsep digit+ $content_length)
           | ("Connection"i hsep 
               ( "Keep-Alive"i %set_keep_alive
               | "close"i %set_not_keep_alive
               )
             )
           | ("Transfer-Encoding"i %use_chunked_encoding hsep "identity" %use_identity_encoding)
         # | ("Expect"i hsep "100-continue"i %expect_continue)
         # | ("Trailer"i hsep field_value %trailer)
           | (Field_Name hsep Field_Value)
           ) :> CRLF;

  Request_Line = ( Method " " Request_URI ("#" Fragment)? " " HTTP_Version CRLF ) ;
  RequestHeader = Request_Line (Header %end_header_line)* :> CRLF @end_headers;

# chunked message
  trailing_headers = header*;
  #chunk_ext_val   = token | quoted_string;
  chunk_ext_val = token*;
  chunk_ext_name = token*;
  chunk_extension = ( ";" " "* chunk_ext_name ("=" chunk_ext_val)? )*;
  last_chunk = "0"+ chunk_extension CRLF;
  chunk_size = (xdigit* [1-9a-fA-F] xdigit*) $add_to_chunk_size;
  chunk_end  = CRLF;
  chunk_body = any >skip_chunk_data;
  chunk_begin = chunk_size chunk_extension CRLF;
  chunk = chunk_begin chunk_body chunk_end;
  ChunkedBody := chunk* last_chunk trailing_headers CRLF @end_chunked_body;

  Request = RequestHeader >start_req @body_logic;

  main := Request*; # sequence of requests (for keep-alive)
}%%

%% write data;

static void
skip_body(const char **p, ebb_request_parser *parser, size_t nskip) {
  if(CURRENT->on_body && nskip > 0) {
    CURRENT->on_body(CURRENT, *p, nskip);
  }
  CURRENT->body_read += nskip;
  parser->chunk_size -= nskip;
  *p += nskip;
  if(0 == parser->chunk_size) {
    parser->eating = FALSE;
    if(CURRENT->transfer_encoding == EBB_IDENTITY) {
      END_REQUEST;
    }
  } else {
    parser->eating = TRUE;
  }
}

void ebb_request_parser_init(ebb_request_parser *parser) 
{
  int cs = 0;
  %% write init;
  parser->cs = cs;

  parser->chunk_size = 0;
  parser->eating = 0;
  
  parser->current_request = NULL;

  parser->header_field_mark = parser->header_value_mark   = 
  parser->query_string_mark = parser->path_mark           = 
  parser->uri_mark          = parser->fragment_mark       = NULL;

  parser->new_request = NULL;
}


/** exec **/
size_t ebb_request_parser_execute(ebb_request_parser *parser, const char *buffer, size_t len)
{
  const char *p, *pe;
  int cs = parser->cs;

  assert(parser->new_request && "undefined callback");

  p = buffer;
  pe = buffer+len;

  if(0 < parser->chunk_size && parser->eating) {
    /* eat body */
    size_t eat = MIN(len, parser->chunk_size);
    skip_body(&p, parser, eat);
  } 

  if(parser->header_field_mark)   parser->header_field_mark   = buffer;
  if(parser->header_value_mark)   parser->header_value_mark   = buffer;
  if(parser->fragment_mark)       parser->fragment_mark       = buffer;
  if(parser->query_string_mark)   parser->query_string_mark   = buffer;
  if(parser->path_mark)           parser->path_mark           = buffer;
  if(parser->uri_mark)            parser->uri_mark            = buffer;

  %% write exec;

  parser->cs = cs;

  HEADER_CALLBACK(header_field);
  HEADER_CALLBACK(header_value);
  CALLBACK(fragment);
  CALLBACK(query_string);
  CALLBACK(path);
  CALLBACK(uri);

  assert(p <= pe && "buffer overflow after parsing execute");

  return(p - buffer);
}

int ebb_request_parser_has_error(ebb_request_parser *parser) 
{
  return parser->cs == ebb_request_parser_error;
}

int ebb_request_parser_is_finished(ebb_request_parser *parser) 
{
  return parser->cs == ebb_request_parser_first_final;
}

void ebb_request_init(ebb_request *request)
{
  request->expect_continue = FALSE;
  request->body_read = 0;
  request->content_length = 0;
  request->version_major = 0;
  request->version_minor = 0;
  request->number_of_headers = 0;
  request->transfer_encoding = EBB_IDENTITY;
  request->keep_alive = -1;

  request->on_complete = NULL;
  request->on_headers_complete = NULL;
  request->on_body = NULL;
  request->on_header_field = NULL;
  request->on_header_value = NULL;
  request->on_uri = NULL;
  request->on_fragment = NULL;
  request->on_path = NULL;
  request->on_query_string = NULL;
}

int ebb_request_should_keep_alive(ebb_request *request)
{
  if(request->keep_alive == -1)
    if(request->version_major == 1)
      return (request->version_minor != 0);
    else if(request->version_major == 0)
      return FALSE;
    else
      return TRUE;
  else
    return request->keep_alive;
}

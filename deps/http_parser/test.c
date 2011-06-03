/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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
#include "http_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* rand */
#include <string.h>
#include <stdarg.h>

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#define MAX_HEADERS 13
#define MAX_ELEMENT_SIZE 500

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static http_parser *parser;

struct message {
  const char *name; // for debugging purposes
  const char *raw;
  enum http_parser_type type;
  enum http_method method;
  int status_code;
  char request_path[MAX_ELEMENT_SIZE];
  char request_url[MAX_ELEMENT_SIZE];
  char fragment[MAX_ELEMENT_SIZE];
  char query_string[MAX_ELEMENT_SIZE];
  char body[MAX_ELEMENT_SIZE];
  size_t body_size;
  int num_headers;
  enum { NONE=0, FIELD, VALUE } last_header_element;
  char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
  int should_keep_alive;

  int upgrade;

  unsigned short http_major;
  unsigned short http_minor;

  int message_begin_cb_called;
  int headers_complete_cb_called;
  int message_complete_cb_called;
  int message_complete_on_eof;
};

static int currently_parsing_eof;

static struct message messages[5];
static int num_messages;

/* * R E Q U E S T S * */
const struct message requests[] =
#define CURL_GET 0
{ {.name= "curl get"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /test HTTP/1.1\r\n"
         "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
         "Host: 0.0.0.0=5000\r\n"
         "Accept: */*\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 3
  ,.headers=
    { { "User-Agent", "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1" }
    , { "Host", "0.0.0.0=5000" }
    , { "Accept", "*/*" }
    }
  ,.body= ""
  }

#define FIREFOX_GET 1
, {.name= "firefox get"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /favicon.ico HTTP/1.1\r\n"
         "Host: 0.0.0.0=5000\r\n"
         "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
         "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
         "Accept-Language: en-us,en;q=0.5\r\n"
         "Accept-Encoding: gzip,deflate\r\n"
         "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
         "Keep-Alive: 300\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/favicon.ico"
  ,.request_url= "/favicon.ico"
  ,.num_headers= 8
  ,.headers=
    { { "Host", "0.0.0.0=5000" }
    , { "User-Agent", "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0" }
    , { "Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" }
    , { "Accept-Language", "en-us,en;q=0.5" }
    , { "Accept-Encoding", "gzip,deflate" }
    , { "Accept-Charset", "ISO-8859-1,utf-8;q=0.7,*;q=0.7" }
    , { "Keep-Alive", "300" }
    , { "Connection", "keep-alive" }
    }
  ,.body= ""
  }

#define DUMBFUCK 2
, {.name= "dumbfuck"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /dumbfuck HTTP/1.1\r\n"
         "aaaaaaaaaaaaa:++++++++++\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/dumbfuck"
  ,.request_url= "/dumbfuck"
  ,.num_headers= 1
  ,.headers=
    { { "aaaaaaaaaaaaa",  "++++++++++" }
    }
  ,.body= ""
  }

#define FRAGMENT_IN_URI 3
, {.name= "fragment in url"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "page=1"
  ,.fragment= "posts-17408"
  ,.request_path= "/forums/1/topics/2375"
  /* XXX request url does include fragment? */
  ,.request_url= "/forums/1/topics/2375?page=1#posts-17408"
  ,.num_headers= 0
  ,.body= ""
  }

#define GET_NO_HEADERS_NO_BODY 4
, {.name= "get no headers no body"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /get_no_headers_no_body/world HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE /* would need Connection: close */
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_no_headers_no_body/world"
  ,.request_url= "/get_no_headers_no_body/world"
  ,.num_headers= 0
  ,.body= ""
  }

#define GET_ONE_HEADER_NO_BODY 5
, {.name= "get one header no body"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /get_one_header_no_body HTTP/1.1\r\n"
         "Accept: */*\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE /* would need Connection: close */
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_one_header_no_body"
  ,.request_url= "/get_one_header_no_body"
  ,.num_headers= 1
  ,.headers=
    { { "Accept" , "*/*" }
    }
  ,.body= ""
  }

#define GET_FUNKY_CONTENT_LENGTH 6
, {.name= "get funky content length body hello"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /get_funky_content_length_body_hello HTTP/1.0\r\n"
         "conTENT-Length: 5\r\n"
         "\r\n"
         "HELLO"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_funky_content_length_body_hello"
  ,.request_url= "/get_funky_content_length_body_hello"
  ,.num_headers= 1
  ,.headers=
    { { "conTENT-Length" , "5" }
    }
  ,.body= "HELLO"
  }

#define POST_IDENTITY_BODY_WORLD 7
, {.name= "post identity body world"
  ,.type= HTTP_REQUEST
  ,.raw= "POST /post_identity_body_world?q=search#hey HTTP/1.1\r\n"
         "Accept: */*\r\n"
         "Transfer-Encoding: identity\r\n"
         "Content-Length: 5\r\n"
         "\r\n"
         "World"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_POST
  ,.query_string= "q=search"
  ,.fragment= "hey"
  ,.request_path= "/post_identity_body_world"
  ,.request_url= "/post_identity_body_world?q=search#hey"
  ,.num_headers= 3
  ,.headers=
    { { "Accept", "*/*" }
    , { "Transfer-Encoding", "identity" }
    , { "Content-Length", "5" }
    }
  ,.body= "World"
  }

#define POST_CHUNKED_ALL_YOUR_BASE 8
, {.name= "post - chunked body: all your base are belong to us"
  ,.type= HTTP_REQUEST
  ,.raw= "POST /post_chunked_all_your_base HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "1e\r\nall your base are belong to us\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/post_chunked_all_your_base"
  ,.request_url= "/post_chunked_all_your_base"
  ,.num_headers= 1
  ,.headers=
    { { "Transfer-Encoding" , "chunked" }
    }
  ,.body= "all your base are belong to us"
  }

#define TWO_CHUNKS_MULT_ZERO_END 9
, {.name= "two chunks ; triple zero ending"
  ,.type= HTTP_REQUEST
  ,.raw= "POST /two_chunks_mult_zero_end HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "000\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/two_chunks_mult_zero_end"
  ,.request_url= "/two_chunks_mult_zero_end"
  ,.num_headers= 1
  ,.headers=
    { { "Transfer-Encoding", "chunked" }
    }
  ,.body= "hello world"
  }

#define CHUNKED_W_TRAILING_HEADERS 10
, {.name= "chunked with trailing headers. blech."
  ,.type= HTTP_REQUEST
  ,.raw= "POST /chunked_w_trailing_headers HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "0\r\n"
         "Vary: *\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_trailing_headers"
  ,.request_url= "/chunked_w_trailing_headers"
  ,.num_headers= 3
  ,.headers=
    { { "Transfer-Encoding",  "chunked" }
    , { "Vary", "*" }
    , { "Content-Type", "text/plain" }
    }
  ,.body= "hello world"
  }

#define CHUNKED_W_BULLSHIT_AFTER_LENGTH 11
, {.name= "with bullshit after the length"
  ,.type= HTTP_REQUEST
  ,.raw= "POST /chunked_w_bullshit_after_length HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5; ihatew3;whatthefuck=aretheseparametersfor\r\nhello\r\n"
         "6; blahblah; blah\r\n world\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_bullshit_after_length"
  ,.request_url= "/chunked_w_bullshit_after_length"
  ,.num_headers= 1
  ,.headers=
    { { "Transfer-Encoding", "chunked" }
    }
  ,.body= "hello world"
  }

#define WITH_QUOTES 12
, {.name= "with quotes"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /with_\"stupid\"_quotes?foo=\"bar\" HTTP/1.1\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "foo=\"bar\""
  ,.fragment= ""
  ,.request_path= "/with_\"stupid\"_quotes"
  ,.request_url= "/with_\"stupid\"_quotes?foo=\"bar\""
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define APACHEBENCH_GET 13
/* The server receiving this request SHOULD NOT wait for EOF
 * to know that content-length == 0.
 * How to represent this in a unit test? message_complete_on_eof
 * Compare with NO_CONTENT_LENGTH_RESPONSE.
 */
, {.name = "apachebench get"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /test HTTP/1.0\r\n"
         "Host: 0.0.0.0:5000\r\n"
         "User-Agent: ApacheBench/2.3\r\n"
         "Accept: */*\r\n\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 3
  ,.headers= { { "Host", "0.0.0.0:5000" }
             , { "User-Agent", "ApacheBench/2.3" }
             , { "Accept", "*/*" }
             }
  ,.body= ""
  }

#define QUERY_URL_WITH_QUESTION_MARK_GET 14
/* Some clients include '?' characters in query strings.
 */
, {.name = "query url with question mark"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /test.cgi?foo=bar?baz HTTP/1.1\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "foo=bar?baz"
  ,.fragment= ""
  ,.request_path= "/test.cgi"
  ,.request_url= "/test.cgi?foo=bar?baz"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define PREFIX_NEWLINE_GET 15
/* Some clients, especially after a POST in a keep-alive connection,
 * will send an extra CRLF before the next request
 */
, {.name = "newline prefix get"
  ,.type= HTTP_REQUEST
  ,.raw= "\r\nGET /test HTTP/1.1\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define UPGRADE_REQUEST 16
, {.name = "upgrade request"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /demo HTTP/1.1\r\n"
         "Host: example.com\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
         "Sec-WebSocket-Protocol: sample\r\n"
         "Upgrade: WebSocket\r\n"
         "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
         "Origin: http://example.com\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/demo"
  ,.request_url= "/demo"
  ,.num_headers= 7
  ,.upgrade=1
  ,.headers= { { "Host", "example.com" }
             , { "Connection", "Upgrade" }
             , { "Sec-WebSocket-Key2", "12998 5 Y3 1  .P00" }
             , { "Sec-WebSocket-Protocol", "sample" }
             , { "Upgrade", "WebSocket" }
             , { "Sec-WebSocket-Key1", "4 @1  46546xW%0l 1 5" }
             , { "Origin", "http://example.com" }
             }
  ,.body= ""
  }

#define CONNECT_REQUEST 17
, {.name = "connect request"
  ,.type= HTTP_REQUEST
  ,.raw= "CONNECT 0-home0.netscape.com:443 HTTP/1.0\r\n"
         "User-agent: Mozilla/1.1N\r\n"
         "Proxy-authorization: basic aGVsbG86d29ybGQ=\r\n"
         "\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= HTTP_CONNECT
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "0-home0.netscape.com:443"
  ,.num_headers= 2
  ,.upgrade=1
  ,.headers= { { "User-agent", "Mozilla/1.1N" }
             , { "Proxy-authorization", "basic aGVsbG86d29ybGQ=" }
             }
  ,.body= ""
  }

#define REPORT_REQ 18
, {.name= "report request"
  ,.type= HTTP_REQUEST
  ,.raw= "REPORT /test HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_REPORT
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define NO_HTTP_VERSION 19
, {.name= "request with no http version"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /\r\n"
         "\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 0
  ,.http_minor= 9
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define MSEARCH_REQ 20
, {.name= "m-search request"
  ,.type= HTTP_REQUEST
  ,.raw= "M-SEARCH * HTTP/1.1\r\n"
         "HOST: 239.255.255.250:1900\r\n"
         "MAN: \"ssdp:discover\"\r\n"
         "ST: \"ssdp:all\"\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_MSEARCH
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "*"
  ,.request_url= "*"
  ,.num_headers= 3
  ,.headers= { { "HOST", "239.255.255.250:1900" }
             , { "MAN", "\"ssdp:discover\"" }
             , { "ST", "\"ssdp:all\"" }
             }
  ,.body= ""
  }

#define QUERY_TERMINATED_HOST 21
, {.name= "host terminated by a query string"
  ,.type= HTTP_REQUEST
  ,.raw= "GET http://hypnotoad.org?hail=all HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "hail=all"
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org?hail=all"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define QUERY_TERMINATED_HOSTPORT 22
, {.name= "host:port terminated by a query string"
  ,.type= HTTP_REQUEST
  ,.raw= "GET http://hypnotoad.org:1234?hail=all HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "hail=all"
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org:1234?hail=all"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define SPACE_TERMINATED_HOSTPORT 23
, {.name= "host:port terminated by a space"
  ,.type= HTTP_REQUEST
  ,.raw= "GET http://hypnotoad.org:1234 HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org:1234"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#if !HTTP_PARSER_STRICT
#define UTF8_PATH_REQ 24
, {.name= "utf-8 path request"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /δ¶/δt/pope?q=1#narf HTTP/1.1\r\n"
         "Host: github.com\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_GET
  ,.query_string= "q=1"
  ,.fragment= "narf"
  ,.request_path= "/δ¶/δt/pope"
  ,.request_url= "/δ¶/δt/pope?q=1#narf"
  ,.num_headers= 1
  ,.headers= { {"Host", "github.com" }
             }
  ,.body= ""
  }

#define HOSTNAME_UNDERSCORE 25
, {.name = "hostname underscore"
  ,.type= HTTP_REQUEST
  ,.raw= "CONNECT home_0.netscape.com:443 HTTP/1.0\r\n"
         "User-agent: Mozilla/1.1N\r\n"
         "Proxy-authorization: basic aGVsbG86d29ybGQ=\r\n"
         "\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= HTTP_CONNECT
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "home_0.netscape.com:443"
  ,.num_headers= 2
  ,.upgrade=1
  ,.headers= { { "User-agent", "Mozilla/1.1N" }
             , { "Proxy-authorization", "basic aGVsbG86d29ybGQ=" }
             }
  ,.body= ""
  }
#endif  /* !HTTP_PARSER_STRICT */

#define PATCH_REQ 26
, {.name = "PATCH request"
  ,.type= HTTP_REQUEST
  ,.raw= "PATCH /file.txt HTTP/1.1\r\n"
         "Host: www.example.com\r\n"
         "Content-Type: application/example\r\n"
         "If-Match: \"e0023aa4e\"\r\n"
         "Content-Length: 10\r\n"
         "\r\n"
         "cccccccccc"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= HTTP_PATCH
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/file.txt"
  ,.request_url= "/file.txt"
  ,.num_headers= 4
  ,.upgrade=0
  ,.headers= { { "Host", "www.example.com" }
             , { "Content-Type", "application/example" }
             , { "If-Match", "\"e0023aa4e\"" }
             , { "Content-Length", "10" }
             }
  ,.body= "cccccccccc"
  }

, {.name= NULL } /* sentinel */
};

/* * R E S P O N S E S * */
const struct message responses[] =
#define GOOGLE_301 0
{ {.name= "google 301"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 301 Moved Permanently\r\n"
         "Location: http://www.google.com/\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
         "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
         "X-$PrototypeBI-Version: 1.6.0.3\r\n" /* $ char in header field */
         "Cache-Control: public, max-age=2592000\r\n"
         "Server: gws\r\n"
         "Content-Length:  219  \r\n"
         "\r\n"
         "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
         "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
         "<H1>301 Moved</H1>\n"
         "The document has moved\n"
         "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
         "</BODY></HTML>\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 301
  ,.num_headers= 8
  ,.headers=
    { { "Location", "http://www.google.com/" }
    , { "Content-Type", "text/html; charset=UTF-8" }
    , { "Date", "Sun, 26 Apr 2009 11:11:49 GMT" }
    , { "Expires", "Tue, 26 May 2009 11:11:49 GMT" }
    , { "X-$PrototypeBI-Version", "1.6.0.3" }
    , { "Cache-Control", "public, max-age=2592000" }
    , { "Server", "gws" }
    , { "Content-Length", "219  " }
    }
  ,.body= "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
          "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
          "<H1>301 Moved</H1>\n"
          "The document has moved\n"
          "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
          "</BODY></HTML>\r\n"
  }

#define NO_CONTENT_LENGTH_RESPONSE 1
/* The client should wait for the server's EOF. That is, when content-length
 * is not specified, and "Connection: close", the end of body is specified
 * by the EOF.
 * Compare with APACHEBENCH_GET
 */
, {.name= "no content-length response"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Date: Tue, 04 Aug 2009 07:59:32 GMT\r\n"
         "Server: Apache\r\n"
         "X-Powered-By: Servlet/2.5 JSP/2.1\r\n"
         "Content-Type: text/xml; charset=utf-8\r\n"
         "Connection: close\r\n"
         "\r\n"
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
         "  <SOAP-ENV:Body>\n"
         "    <SOAP-ENV:Fault>\n"
         "       <faultcode>SOAP-ENV:Client</faultcode>\n"
         "       <faultstring>Client Error</faultstring>\n"
         "    </SOAP-ENV:Fault>\n"
         "  </SOAP-ENV:Body>\n"
         "</SOAP-ENV:Envelope>"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= TRUE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 5
  ,.headers=
    { { "Date", "Tue, 04 Aug 2009 07:59:32 GMT" }
    , { "Server", "Apache" }
    , { "X-Powered-By", "Servlet/2.5 JSP/2.1" }
    , { "Content-Type", "text/xml; charset=utf-8" }
    , { "Connection", "close" }
    }
  ,.body= "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
          "  <SOAP-ENV:Body>\n"
          "    <SOAP-ENV:Fault>\n"
          "       <faultcode>SOAP-ENV:Client</faultcode>\n"
          "       <faultstring>Client Error</faultstring>\n"
          "    </SOAP-ENV:Fault>\n"
          "  </SOAP-ENV:Body>\n"
          "</SOAP-ENV:Envelope>"
  }

#define NO_HEADERS_NO_BODY_404 2
, {.name= "404 no headers no body"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 404 Not Found\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 404
  ,.num_headers= 0
  ,.headers= {}
  ,.body_size= 0
  ,.body= ""
  }

#define NO_REASON_PHRASE 3
, {.name= "301 no response phrase"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 301\r\n\r\n"
  ,.should_keep_alive = TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 301
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define TRAILING_SPACE_ON_CHUNKED_BODY 4
, {.name="200 trailing space on chunked body"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "25  \r\n"
         "This is the data in the first chunk\r\n"
         "\r\n"
         "1C\r\n"
         "and this is the second one\r\n"
         "\r\n"
         "0  \r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 2
  ,.headers=
    { {"Content-Type", "text/plain" }
    , {"Transfer-Encoding", "chunked" }
    }
  ,.body_size = 37+28
  ,.body =
         "This is the data in the first chunk\r\n"
         "and this is the second one\r\n"

  }

#define NO_CARRIAGE_RET 5
, {.name="no carriage ret"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\n"
         "Content-Type: text/html; charset=utf-8\n"
         "Connection: close\n"
         "\n"
         "these headers are from http://news.ycombinator.com/"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= TRUE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 2
  ,.headers=
    { {"Content-Type", "text/html; charset=utf-8" }
    , {"Connection", "close" }
    }
  ,.body= "these headers are from http://news.ycombinator.com/"
  }

#define PROXY_CONNECTION 6
, {.name="proxy connection"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Content-Length: 11\r\n"
         "Proxy-Connection: close\r\n"
         "Date: Thu, 31 Dec 2009 20:55:48 +0000\r\n"
         "\r\n"
         "hello world"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 4
  ,.headers=
    { {"Content-Type", "text/html; charset=UTF-8" }
    , {"Content-Length", "11" }
    , {"Proxy-Connection", "close" }
    , {"Date", "Thu, 31 Dec 2009 20:55:48 +0000"}
    }
  ,.body= "hello world"
  }

#define UNDERSTORE_HEADER_KEY 7
  // shown by
  // curl -o /dev/null -v "http://ad.doubleclick.net/pfadx/DARTSHELLCONFIGXML;dcmt=text/xml;"
, {.name="underscore header key"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Server: DCLK-AdSvr\r\n"
         "Content-Type: text/xml\r\n"
         "Content-Length: 0\r\n"
         "DCLK_imp: v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 4
  ,.headers=
    { {"Server", "DCLK-AdSvr" }
    , {"Content-Type", "text/xml" }
    , {"Content-Length", "0" }
    , {"DCLK_imp", "v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o" }
    }
  ,.body= ""
  }

#define BONJOUR_MADAME_FR 8
/* The client should not merge two headers fields when the first one doesn't
 * have a value.
 */
, {.name= "bonjourmadame.fr"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.0 301 Moved Permanently\r\n"
         "Date: Thu, 03 Jun 2010 09:56:32 GMT\r\n"
         "Server: Apache/2.2.3 (Red Hat)\r\n"
         "Cache-Control: public\r\n"
         "Pragma: \r\n"
         "Location: http://www.bonjourmadame.fr/\r\n"
         "Vary: Accept-Encoding\r\n"
         "Content-Length: 0\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 0
  ,.status_code= 301
  ,.num_headers= 9
  ,.headers=
    { { "Date", "Thu, 03 Jun 2010 09:56:32 GMT" }
    , { "Server", "Apache/2.2.3 (Red Hat)" }
    , { "Cache-Control", "public" }
    , { "Pragma", "" }
    , { "Location", "http://www.bonjourmadame.fr/" }
    , { "Vary",  "Accept-Encoding" }
    , { "Content-Length", "0" }
    , { "Content-Type", "text/html; charset=UTF-8" }
    , { "Connection", "keep-alive" }
    }
  ,.body= ""
  }

#define SPACE_IN_FIELD_RES 9
/* Should handle spaces in header fields */
, {.name= "field space"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Server: Microsoft-IIS/6.0\r\n"
         "X-Powered-By: ASP.NET\r\n"
         "en-US Content-Type: text/xml\r\n" /* this is the problem */
         "Content-Type: text/xml\r\n"
         "Content-Length: 16\r\n"
         "Date: Fri, 23 Jul 2010 18:45:38 GMT\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
         "<xml>hello</xml>" /* fake body */
  ,.should_keep_alive= TRUE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 7
  ,.headers=
    { { "Server",  "Microsoft-IIS/6.0" }
    , { "X-Powered-By", "ASP.NET" }
    , { "en-US Content-Type", "text/xml" }
    , { "Content-Type", "text/xml" }
    , { "Content-Length", "16" }
    , { "Date", "Fri, 23 Jul 2010 18:45:38 GMT" }
    , { "Connection", "keep-alive" }
    }
  ,.body= "<xml>hello</xml>"
  }


#define RES_FIELD_UNDERSCORE 10
/* Should handle spaces in header fields */
, {.name= "field underscore"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 200 OK\r\n"
         "Date: Tue, 28 Sep 2010 01:14:13 GMT\r\n"
         "Server: Apache\r\n"
         "Cache-Control: no-cache, must-revalidate\r\n"
         "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
         ".et-Cookie: PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com\r\n"
         "Vary: Accept-Encoding\r\n"
         "_eep-Alive: timeout=45\r\n" /* semantic value ignored */
         "_onnection: Keep-Alive\r\n" /* semantic value ignored */
         "Transfer-Encoding: chunked\r\n"
         "Content-Type: text/html\r\n"
         "Connection: close\r\n"
         "\r\n"
         "0\r\n\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.num_headers= 11
  ,.headers=
    { { "Date", "Tue, 28 Sep 2010 01:14:13 GMT" }
    , { "Server", "Apache" }
    , { "Cache-Control", "no-cache, must-revalidate" }
    , { "Expires", "Mon, 26 Jul 1997 05:00:00 GMT" }
    , { ".et-Cookie", "PlaxoCS=1274804622353690521; path=/; domain=.plaxo.com" }
    , { "Vary", "Accept-Encoding" }
    , { "_eep-Alive", "timeout=45" }
    , { "_onnection", "Keep-Alive" }
    , { "Transfer-Encoding", "chunked" }
    , { "Content-Type", "text/html" }
    , { "Connection", "close" }
    }
  ,.body= ""
  }

#define NON_ASCII_IN_STATUS_LINE 11
/* Should handle non-ASCII in status line */
, {.name= "non-ASCII in status line"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 500 Oriëntatieprobleem\r\n"
         "Date: Fri, 5 Nov 2010 23:07:12 GMT+2\r\n"
         "Content-Length: 0\r\n"
         "Connection: close\r\n"
         "\r\n"
  ,.should_keep_alive= FALSE
  ,.message_complete_on_eof= FALSE
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 500
  ,.num_headers= 3
  ,.headers=
    { { "Date", "Fri, 5 Nov 2010 23:07:12 GMT+2" }
    , { "Content-Length", "0" }
    , { "Connection", "close" }
    }
  ,.body= ""
  }


, {.name= NULL } /* sentinel */
};

int
request_path_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strncat(messages[num_messages].request_path, buf, len);
  return 0;
}

int
request_url_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strncat(messages[num_messages].request_url, buf, len);
  return 0;
}

int
query_string_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strncat(messages[num_messages].query_string, buf, len);
  return 0;
}

int
fragment_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strncat(messages[num_messages].fragment, buf, len);
  return 0;
}

int
header_field_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  struct message *m = &messages[num_messages];

  if (m->last_header_element != FIELD)
    m->num_headers++;

  strncat(m->headers[m->num_headers-1][0], buf, len);

  m->last_header_element = FIELD;

  return 0;
}

int
header_value_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  struct message *m = &messages[num_messages];

  strncat(m->headers[m->num_headers-1][1], buf, len);

  m->last_header_element = VALUE;

  return 0;
}

int
body_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strncat(messages[num_messages].body, buf, len);
  messages[num_messages].body_size += len;
 // printf("body_cb: '%s'\n", requests[num_messages].body);
  return 0;
}

int
count_body_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  assert(buf);
  messages[num_messages].body_size += len;
  return 0;
}

int
message_begin_cb (http_parser *p)
{
  assert(p == parser);
  messages[num_messages].message_begin_cb_called = TRUE;
  return 0;
}

int
headers_complete_cb (http_parser *p)
{
  assert(p == parser);
  messages[num_messages].method = parser->method;
  messages[num_messages].status_code = parser->status_code;
  messages[num_messages].http_major = parser->http_major;
  messages[num_messages].http_minor = parser->http_minor;
  messages[num_messages].headers_complete_cb_called = TRUE;
  messages[num_messages].should_keep_alive = http_should_keep_alive(parser);
  return 0;
}

int
message_complete_cb (http_parser *p)
{
  assert(p == parser);
  if (messages[num_messages].should_keep_alive != http_should_keep_alive(parser))
  {
    fprintf(stderr, "\n\n *** Error http_should_keep_alive() should have same "
                    "value in both on_message_complete and on_headers_complete "
                    "but it doesn't! ***\n\n");
    assert(0);
    exit(1);
  }
  messages[num_messages].message_complete_cb_called = TRUE;

  messages[num_messages].message_complete_on_eof = currently_parsing_eof;

  num_messages++;
  return 0;
}

static http_parser_settings settings =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_path = request_path_cb
  ,.on_url = request_url_cb
  ,.on_fragment = fragment_cb
  ,.on_query_string = query_string_cb
  ,.on_body = body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };

static http_parser_settings settings_count_body =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_path = request_path_cb
  ,.on_url = request_url_cb
  ,.on_fragment = fragment_cb
  ,.on_query_string = query_string_cb
  ,.on_body = count_body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };

static http_parser_settings settings_null =
  {.on_message_begin = 0
  ,.on_header_field = 0
  ,.on_header_value = 0
  ,.on_path = 0
  ,.on_url = 0
  ,.on_fragment = 0
  ,.on_query_string = 0
  ,.on_body = 0
  ,.on_headers_complete = 0
  ,.on_message_complete = 0
  };

void
parser_init (enum http_parser_type type)
{
  num_messages = 0;

  assert(parser == NULL);

  parser = malloc(sizeof(http_parser));

  http_parser_init(parser, type);

  memset(&messages, 0, sizeof messages);

}

void
parser_free ()
{
  assert(parser);
  free(parser);
  parser = NULL;
}

size_t parse (const char *buf, size_t len)
{
  size_t nparsed;
  currently_parsing_eof = (len == 0);
  nparsed = http_parser_execute(parser, &settings, buf, len);
  return nparsed;
}

size_t parse_count_body (const char *buf, size_t len)
{
  size_t nparsed;
  currently_parsing_eof = (len == 0);
  nparsed = http_parser_execute(parser, &settings_count_body, buf, len);
  return nparsed;
}

static inline int
check_str_eq (const struct message *m,
              const char *prop,
              const char *expected,
              const char *found) {
  if (0 != strcmp(expected, found)) {
    printf("\n*** Error: %s in '%s' ***\n\n", prop, m->name);
    printf("expected '%s'\n", expected);
    printf("   found '%s'\n", found);
    return 0;
  }
  return 1;
}

static inline int
check_num_eq (const struct message *m,
              const char *prop,
              int expected,
              int found) {
  if (expected != found) {
    printf("\n*** Error: %s in '%s' ***\n\n", prop, m->name);
    printf("expected %d\n", expected);
    printf("   found %d\n", found);
    return 0;
  }
  return 1;
}

#define MESSAGE_CHECK_STR_EQ(expected, found, prop) \
  if (!check_str_eq(expected, #prop, expected->prop, found->prop)) return 0

#define MESSAGE_CHECK_NUM_EQ(expected, found, prop) \
  if (!check_num_eq(expected, #prop, expected->prop, found->prop)) return 0


int
message_eq (int index, const struct message *expected)
{
  int i;
  struct message *m = &messages[index];

  MESSAGE_CHECK_NUM_EQ(expected, m, http_major);
  MESSAGE_CHECK_NUM_EQ(expected, m, http_minor);

  if (expected->type == HTTP_REQUEST) {
    MESSAGE_CHECK_NUM_EQ(expected, m, method);
  } else {
    MESSAGE_CHECK_NUM_EQ(expected, m, status_code);
  }

  MESSAGE_CHECK_NUM_EQ(expected, m, should_keep_alive);
  MESSAGE_CHECK_NUM_EQ(expected, m, message_complete_on_eof);

  assert(m->message_begin_cb_called);
  assert(m->headers_complete_cb_called);
  assert(m->message_complete_cb_called);


  MESSAGE_CHECK_STR_EQ(expected, m, request_path);
  MESSAGE_CHECK_STR_EQ(expected, m, query_string);
  MESSAGE_CHECK_STR_EQ(expected, m, fragment);
  MESSAGE_CHECK_STR_EQ(expected, m, request_url);
  if (expected->body_size) {
    MESSAGE_CHECK_NUM_EQ(expected, m, body_size);
  } else {
    MESSAGE_CHECK_STR_EQ(expected, m, body);
  }

  MESSAGE_CHECK_NUM_EQ(expected, m, num_headers);

  int r;
  for (i = 0; i < m->num_headers; i++) {
    r = check_str_eq(expected, "header field", expected->headers[i][0], m->headers[i][0]);
    if (!r) return 0;
    r = check_str_eq(expected, "header value", expected->headers[i][1], m->headers[i][1]);
    if (!r) return 0;
  }

  return 1;
}

static void
print_error (const char *raw, size_t error_location)
{
  fprintf(stderr, "\n*** parse error ***\n\n");

  int this_line = 0, char_len = 0;
  size_t i, j, len = strlen(raw), error_location_line = 0;
  for (i = 0; i < len; i++) {
    if (i == error_location) this_line = 1;
    switch (raw[i]) {
      case '\r':
        char_len = 2;
        fprintf(stderr, "\\r");
        break;

      case '\n':
        char_len = 2;
        fprintf(stderr, "\\n\n");

        if (this_line) goto print;

        error_location_line = 0;
        continue;

      default:
        char_len = 1;
        fputc(raw[i], stderr);
        break;
    }
    if (!this_line) error_location_line += char_len;
  }

  fprintf(stderr, "[eof]\n");

 print:
  for (j = 0; j < error_location_line; j++) {
    fputc(' ', stderr);
  }
  fprintf(stderr, "^\n\nerror location: %u\n", (unsigned int)error_location);
}


void
test_message (const struct message *message)
{
  size_t raw_len = strlen(message->raw);
  size_t msg1len;
  for (msg1len = 0; msg1len < raw_len; msg1len++) {
    parser_init(message->type);

    size_t read;
    const char *msg1 = message->raw;
    const char *msg2 = msg1 + msg1len;
    size_t msg2len = raw_len - msg1len;

    if (msg1len) {
      read = parse(msg1, msg1len);

      if (message->upgrade && parser->upgrade) goto test;

      if (read != msg1len) {
        print_error(msg1, read);
        exit(1);
      }
    }


    read = parse(msg2, msg2len);

    if (message->upgrade && parser->upgrade) goto test;

    if (read != msg2len) {
      print_error(msg2, read);
      exit(1);
    }

    read = parse(NULL, 0);

    if (message->upgrade && parser->upgrade) goto test;

    if (read != 0) {
      print_error(message->raw, read);
      exit(1);
    }

  test:

    if (num_messages != 1) {
      printf("\n*** num_messages != 1 after testing '%s' ***\n\n", message->name);
      exit(1);
    }

    if(!message_eq(0, message)) exit(1);

    parser_free();
  }
}

void
test_message_count_body (const struct message *message)
{
  parser_init(message->type);

  size_t read;
  size_t l = strlen(message->raw);
  size_t i, toread;
  size_t chunk = 4024;

  for (i = 0; i < l; i+= chunk) {
    toread = MIN(l-i, chunk);
    read = parse_count_body(message->raw + i, toread);
    if (read != toread) {
      print_error(message->raw, read);
      exit(1);
    }
  }


  read = parse_count_body(NULL, 0);
  if (read != 0) {
    print_error(message->raw, read);
    exit(1);
  }

  if (num_messages != 1) {
    printf("\n*** num_messages != 1 after testing '%s' ***\n\n", message->name);
    exit(1);
  }

  if(!message_eq(0, message)) exit(1);

  parser_free();
}

void
test_simple (const char *buf, int should_pass)
{
  parser_init(HTTP_REQUEST);

  size_t parsed;
  int pass;
  parsed = parse(buf, strlen(buf));
  pass = (parsed == strlen(buf));
  parsed = parse(NULL, 0);
  pass &= (parsed == 0);

  parser_free();

  if (pass != should_pass) {
    fprintf(stderr, "\n*** test_simple expected %s ***\n\n%s", should_pass ? "success" : "error", buf);
    exit(1);
  }
}

void
test_header_overflow_error (int req)
{
  http_parser parser;
  http_parser_init(&parser, req ? HTTP_REQUEST : HTTP_RESPONSE);
  size_t parsed;
  const char *buf;
  buf = req ? "GET / HTTP/1.1\r\n" : "HTTP/1.0 200 OK\r\n";
  parsed = http_parser_execute(&parser, &settings_null, buf, strlen(buf));
  assert(parsed == strlen(buf));

  buf = "header-key: header-value\r\n";
  int i;
  for (i = 0; i < 10000; i++) {
    if (http_parser_execute(&parser, &settings_null, buf, strlen(buf)) != strlen(buf)) {
      //fprintf(stderr, "error found on iter %d\n", i);
      return;
    }
  }

  fprintf(stderr, "\n*** Error expected but none in header overflow test ***\n");
  exit(1);
}

void
test_no_overflow_long_body (int req, size_t length)
{
  http_parser parser;
  http_parser_init(&parser, req ? HTTP_REQUEST : HTTP_RESPONSE);
  size_t parsed;
  size_t i;
  char buf1[3000];
  size_t buf1len = sprintf(buf1, "%s\r\nConnection: Keep-Alive\r\nContent-Length: %zu\r\n\r\n",
      req ? "POST / HTTP/1.0" : "HTTP/1.0 200 OK", length);
  parsed = http_parser_execute(&parser, &settings_null, buf1, buf1len);
  if (parsed != buf1len)
    goto err;

  for (i = 0; i < length; i++) {
    char foo = 'a';
    parsed = http_parser_execute(&parser, &settings_null, &foo, 1);
    if (parsed != 1)
      goto err;
  }

  parsed = http_parser_execute(&parser, &settings_null, buf1, buf1len);
  if (parsed != buf1len) goto err;
  return;

 err:
  fprintf(stderr,
          "\n*** error in test_no_overflow_long_body %s of length %zu ***\n",
          req ? "REQUEST" : "RESPONSE",
          length);
  exit(1);
}

void
test_multiple3 (const struct message *r1, const struct message *r2, const struct message *r3)
{
  int message_count = 1;
  if (!r1->upgrade) {
    message_count++;
    if (!r2->upgrade) message_count++;
  }
  int has_upgrade = (message_count < 3 || r3->upgrade);

  char total[ strlen(r1->raw)
            + strlen(r2->raw)
            + strlen(r3->raw)
            + 1
            ];
  total[0] = '\0';

  strcat(total, r1->raw);
  strcat(total, r2->raw);
  strcat(total, r3->raw);

  parser_init(r1->type);

  size_t read;

  read = parse(total, strlen(total));

  if (has_upgrade && parser->upgrade) goto test;

  if (read != strlen(total)) {
    print_error(total, read);
    exit(1);
  }

  read = parse(NULL, 0);

  if (has_upgrade && parser->upgrade) goto test;

  if (read != 0) {
    print_error(total, read);
    exit(1);
  }

test:

  if (message_count != num_messages) {
    fprintf(stderr, "\n\n*** Parser didn't see 3 messages only %d *** \n", num_messages);
    exit(1);
  }

  if (!message_eq(0, r1)) exit(1);
  if (message_count > 1) {
    if (!message_eq(1, r2)) exit(1);
    if (message_count > 2) {
      if (!message_eq(2, r3)) exit(1);
    }
  }

  parser_free();
}

/* SCAN through every possible breaking to make sure the
 * parser can handle getting the content in any chunks that
 * might come from the socket
 */
void
test_scan (const struct message *r1, const struct message *r2, const struct message *r3)
{
  char total[80*1024] = "\0";
  char buf1[80*1024] = "\0";
  char buf2[80*1024] = "\0";
  char buf3[80*1024] = "\0";

  strcat(total, r1->raw);
  strcat(total, r2->raw);
  strcat(total, r3->raw);

  size_t read;

  int total_len = strlen(total);

  int total_ops = 2 * (total_len - 1) * (total_len - 2) / 2;
  int ops = 0 ;

  size_t buf1_len, buf2_len, buf3_len;

  int i,j,type_both;
  for (type_both = 0; type_both < 2; type_both ++ ) {
    for (j = 2; j < total_len; j ++ ) {
      for (i = 1; i < j; i ++ ) {

        if (ops % 1000 == 0)  {
          printf("\b\b\b\b%3.0f%%", 100 * (float)ops /(float)total_ops);
          fflush(stdout);
        }
        ops += 1;

        parser_init(type_both ? HTTP_BOTH : r1->type);

        buf1_len = i;
        strncpy(buf1, total, buf1_len);
        buf1[buf1_len] = 0;

        buf2_len = j - i;
        strncpy(buf2, total+i, buf2_len);
        buf2[buf2_len] = 0;

        buf3_len = total_len - j;
        strncpy(buf3, total+j, buf3_len);
        buf3[buf3_len] = 0;

        read = parse(buf1, buf1_len);

        if (r3->upgrade && parser->upgrade) goto test;

        if (read != buf1_len) {
          print_error(buf1, read);
          goto error;
        }

        read = parse(buf2, buf2_len);

        if (r3->upgrade && parser->upgrade) goto test;

        if (read != buf2_len) {
          print_error(buf2, read);
          goto error;
        }

        read = parse(buf3, buf3_len);

        if (r3->upgrade && parser->upgrade) goto test;

        if (read != buf3_len) {
          print_error(buf3, read);
          goto error;
        }

        parse(NULL, 0);

test:

        if (3 != num_messages) {
          fprintf(stderr, "\n\nParser didn't see 3 messages only %d\n", num_messages);
          goto error;
        }

        if (!message_eq(0, r1)) {
          fprintf(stderr, "\n\nError matching messages[0] in test_scan.\n");
          goto error;
        }

        if (!message_eq(1, r2)) {
          fprintf(stderr, "\n\nError matching messages[1] in test_scan.\n");
          goto error;
        }

        if (!message_eq(2, r3)) {
          fprintf(stderr, "\n\nError matching messages[2] in test_scan.\n");
          goto error;
        }

        parser_free();
      }
    }
  }
  puts("\b\b\b\b100%");
  return;

 error:
  fprintf(stderr, "i=%d  j=%d\n", i, j);
  fprintf(stderr, "buf1 (%u) %s\n\n", (unsigned int)buf1_len, buf1);
  fprintf(stderr, "buf2 (%u) %s\n\n", (unsigned int)buf2_len , buf2);
  fprintf(stderr, "buf3 (%u) %s\n", (unsigned int)buf3_len, buf3);
  exit(1);
}

// user required to free the result
// string terminated by \0
char *
create_large_chunked_message (int body_size_in_kb, const char* headers)
{
  int i;
  size_t wrote = 0;
  size_t headers_len = strlen(headers);
  size_t bufsize = headers_len + (5+1024+2)*body_size_in_kb + 6;
  char * buf = malloc(bufsize);

  memcpy(buf, headers, headers_len);
  wrote += headers_len;

  for (i = 0; i < body_size_in_kb; i++) {
    // write 1kb chunk into the body.
    memcpy(buf + wrote, "400\r\n", 5);
    wrote += 5;
    memset(buf + wrote, 'C', 1024);
    wrote += 1024;
    strcpy(buf + wrote, "\r\n");
    wrote += 2;
  }

  memcpy(buf + wrote, "0\r\n\r\n", 6);
  wrote += 6;
  assert(wrote == bufsize);

  return buf;
}


int
main (void)
{
  parser = NULL;
  int i, j, k;
  int request_count;
  int response_count;

  printf("sizeof(http_parser) = %u\n", (unsigned int)sizeof(http_parser));

  for (request_count = 0; requests[request_count].name; request_count++);
  for (response_count = 0; responses[response_count].name; response_count++);

  //// OVERFLOW CONDITIONS

  test_header_overflow_error(HTTP_REQUEST);
  test_no_overflow_long_body(HTTP_REQUEST, 1000);
  test_no_overflow_long_body(HTTP_REQUEST, 100000);

  test_header_overflow_error(HTTP_RESPONSE);
  test_no_overflow_long_body(HTTP_RESPONSE, 1000);
  test_no_overflow_long_body(HTTP_RESPONSE, 100000);

  //// RESPONSES

  for (i = 0; i < response_count; i++) {
    test_message(&responses[i]);
  }

  for (i = 0; i < response_count; i++) {
    if (!responses[i].should_keep_alive) continue;
    for (j = 0; j < response_count; j++) {
      if (!responses[j].should_keep_alive) continue;
      for (k = 0; k < response_count; k++) {
        test_multiple3(&responses[i], &responses[j], &responses[k]);
      }
    }
  }

  test_message_count_body(&responses[NO_HEADERS_NO_BODY_404]);
  test_message_count_body(&responses[TRAILING_SPACE_ON_CHUNKED_BODY]);

  // test very large chunked response
  {
    char * msg = create_large_chunked_message(31337,
      "HTTP/1.0 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n");
    struct message large_chunked =
      {.name= "large chunked"
      ,.type= HTTP_RESPONSE
      ,.raw= msg
      ,.should_keep_alive= FALSE
      ,.message_complete_on_eof= FALSE
      ,.http_major= 1
      ,.http_minor= 0
      ,.status_code= 200
      ,.num_headers= 2
      ,.headers=
        { { "Transfer-Encoding", "chunked" }
        , { "Content-Type", "text/plain" }
        }
      ,.body_size= 31337*1024
      };
    test_message_count_body(&large_chunked);
    free(msg);
  }



  printf("response scan 1/2      ");
  test_scan( &responses[TRAILING_SPACE_ON_CHUNKED_BODY]
           , &responses[NO_HEADERS_NO_BODY_404]
           , &responses[NO_REASON_PHRASE]
           );

  printf("response scan 2/2      ");
  test_scan( &responses[BONJOUR_MADAME_FR]
           , &responses[UNDERSTORE_HEADER_KEY]
           , &responses[NO_CARRIAGE_RET]
           );

  puts("responses okay");


  /// REQUESTS

  test_simple("hello world", 0);
  test_simple("GET / HTP/1.1\r\n\r\n", 0);


  test_simple("ASDF / HTTP/1.1\r\n\r\n", 0);
  test_simple("PROPPATCHA / HTTP/1.1\r\n\r\n", 0);
  test_simple("GETA / HTTP/1.1\r\n\r\n", 0);

  // Well-formed but incomplete
  test_simple("GET / HTTP/1.1\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 6\r\n"
              "\r\n"
              "fooba",
              0);

  static const char *all_methods[] = {
    "DELETE",
    "GET",
    "HEAD",
    "POST",
    "PUT",
    //"CONNECT", //CONNECT can't be tested like other methods, it's a tunnel
    "OPTIONS",
    "TRACE",
    "COPY",
    "LOCK",
    "MKCOL",
    "MOVE",
    "PROPFIND",
    "PROPPATCH",
    "UNLOCK",
    0 };
  const char **this_method;
  for (this_method = all_methods; *this_method; this_method++) {
    char buf[200];
    sprintf(buf, "%s / HTTP/1.1\r\n\r\n", *this_method);
    test_simple(buf, 1);
  }

  const char *dumbfuck2 =
    "GET / HTTP/1.1\r\n"
    "X-SSL-Bullshit:   -----BEGIN CERTIFICATE-----\r\n"
    "\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx\r\n"
    "\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT\r\n"
    "\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu\r\n"
    "\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV\r\n"
    "\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV\r\n"
    "\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB\r\n"
    "\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF\r\n"
    "\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR\r\n"
    "\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL\r\n"
    "\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP\r\n"
    "\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR\r\n"
    "\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG\r\n"
    "\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgHTTPAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs\r\n"
    "\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD\r\n"
    "\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj\r\n"
    "\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj\r\n"
    "\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG\r\n"
    "\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE\r\n"
    "\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO\r\n"
    "\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1\r\n"
    "\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBDAWLmh0\r\n"
    "\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD\r\n"
    "\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv\r\n"
    "\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3\r\n"
    "\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8\r\n"
    "\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk\r\n"
    "\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK\r\n"
    "\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu\r\n"
    "\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3\r\n"
    "\tRA==\r\n"
    "\t-----END CERTIFICATE-----\r\n"
    "\r\n";
  test_simple(dumbfuck2, 0);

#if 0
  // NOTE(Wed Nov 18 11:57:27 CET 2009) this seems okay. we just read body
  // until EOF.
  //
  // no content-length
  // error if there is a body without content length
  const char *bad_get_no_headers_no_body = "GET /bad_get_no_headers_no_body/world HTTP/1.1\r\n"
                                           "Accept: */*\r\n"
                                           "\r\n"
                                           "HELLO";
  test_simple(bad_get_no_headers_no_body, 0);
#endif
  /* TODO sending junk and large headers gets rejected */


  /* check to make sure our predefined requests are okay */
  for (i = 0; requests[i].name; i++) {
    test_message(&requests[i]);
  }



  for (i = 0; i < request_count; i++) {
    if (!requests[i].should_keep_alive) continue;
    for (j = 0; j < request_count; j++) {
      if (!requests[j].should_keep_alive) continue;
      for (k = 0; k < request_count; k++) {
        test_multiple3(&requests[i], &requests[j], &requests[k]);
      }
    }
  }

  printf("request scan 1/4      ");
  test_scan( &requests[GET_NO_HEADERS_NO_BODY]
           , &requests[GET_ONE_HEADER_NO_BODY]
           , &requests[GET_NO_HEADERS_NO_BODY]
           );

  printf("request scan 2/4      ");
  test_scan( &requests[POST_CHUNKED_ALL_YOUR_BASE]
           , &requests[POST_IDENTITY_BODY_WORLD]
           , &requests[GET_FUNKY_CONTENT_LENGTH]
           );

  printf("request scan 3/4      ");
  test_scan( &requests[TWO_CHUNKS_MULT_ZERO_END]
           , &requests[CHUNKED_W_TRAILING_HEADERS]
           , &requests[CHUNKED_W_BULLSHIT_AFTER_LENGTH]
           );

  printf("request scan 4/4      ");
  test_scan( &requests[QUERY_URL_WITH_QUESTION_MARK_GET]
           , &requests[PREFIX_NEWLINE_GET ]
           , &requests[CONNECT_REQUEST]
           );

  puts("requests okay");

  return 0;
}

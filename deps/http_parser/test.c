#include "http_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#define MAX_HEADERS 10
#define MAX_ELEMENT_SIZE 500

static http_parser parser;
struct message {
  const char *name; // for debugging purposes
  const char *raw;
  enum http_parser_type type;
  int method;
  int status_code;
  char request_path[MAX_ELEMENT_SIZE];
  char request_uri[MAX_ELEMENT_SIZE];
  char fragment[MAX_ELEMENT_SIZE];
  char query_string[MAX_ELEMENT_SIZE];
  char body[MAX_ELEMENT_SIZE];
  int num_headers;
  enum { NONE=0, FIELD, VALUE } last_header_element;
  char headers [MAX_HEADERS][2][MAX_ELEMENT_SIZE];
  int should_keep_alive;
};

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
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_uri= "/test"
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
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/favicon.ico"
  ,.request_uri= "/favicon.ico"
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
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/dumbfuck"
  ,.request_uri= "/dumbfuck"
  ,.num_headers= 1
  ,.headers= 
    { { "aaaaaaaaaaaaa",  "++++++++++" }
    }
  ,.body= ""
  }

#define FRAGMENT_IN_URI 3
, {.name= "fragment in uri"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.method= HTTP_GET
  ,.query_string= "page=1"
  ,.fragment= "posts-17408"
  ,.request_path= "/forums/1/topics/2375"
  /* XXX request uri does not include fragment? */
  ,.request_uri= "/forums/1/topics/2375?page=1" 
  ,.num_headers= 0
  ,.body= ""
  }

#define GET_NO_HEADERS_NO_BODY 4
, {.name= "get no headers no body"
  ,.type= HTTP_REQUEST
  ,.raw= "GET /get_no_headers_no_body/world HTTP/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= TRUE
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_no_headers_no_body/world"
  ,.request_uri= "/get_no_headers_no_body/world"
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
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_one_header_no_body"
  ,.request_uri= "/get_one_header_no_body"
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
  ,.method= HTTP_GET
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_funky_content_length_body_hello"
  ,.request_uri= "/get_funky_content_length_body_hello"
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
  ,.method= HTTP_POST
  ,.query_string= "q=search"
  ,.fragment= "hey"
  ,.request_path= "/post_identity_body_world"
  ,.request_uri= "/post_identity_body_world?q=search"
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
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/post_chunked_all_your_base"
  ,.request_uri= "/post_chunked_all_your_base"
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
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/two_chunks_mult_zero_end"
  ,.request_uri= "/two_chunks_mult_zero_end"
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
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_trailing_headers"
  ,.request_uri= "/chunked_w_trailing_headers"
  ,.num_headers= 1
  ,.headers= 
    { { "Transfer-Encoding",  "chunked" }
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
  ,.method= HTTP_POST
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_bullshit_after_length"
  ,.request_uri= "/chunked_w_bullshit_after_length"
  ,.num_headers= 1
  ,.headers= 
    { { "Transfer-Encoding", "chunked" }
    }
  ,.body= "hello world"
  }

, {.name= NULL } /* sentinel */
};

/* * R E S P O N S E S * */ 
const struct message responses[] = 
{ {.name= "google 301"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 301 Moved Permanently\r\n"
         "Location: http://www.google.com/\r\n"
         "Content-Type: text/html; charset=UTF-8\r\n"
         "Date: Sun, 26 Apr 2009 11:11:49 GMT\r\n"
         "Expires: Tue, 26 May 2009 11:11:49 GMT\r\n"
         "Cache-Control: public, max-age=2592000\r\n"
         "Server: gws\r\n"
         "Content-Length: 219\r\n"
         "\r\n"
         "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
         "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
         "<H1>301 Moved</H1>\n"
         "The document has moved\n"
         "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
         "</BODY></HTML>\r\n"
  ,.should_keep_alive= TRUE
  ,.status_code= 301
  ,.num_headers= 7
  ,.headers= 
    { { "Location", "http://www.google.com/" }
    , { "Content-Type", "text/html; charset=UTF-8" }
    , { "Date", "Sun, 26 Apr 2009 11:11:49 GMT" }
    , { "Expires", "Tue, 26 May 2009 11:11:49 GMT" }
    , { "Cache-Control", "public, max-age=2592000" }
    , { "Server", "gws" }
    , { "Content-Length", "219" }
    }
  ,.body= "<HTML><HEAD><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
          "<TITLE>301 Moved</TITLE></HEAD><BODY>\n"
          "<H1>301 Moved</H1>\n"
          "The document has moved\n"
          "<A HREF=\"http://www.google.com/\">here</A>.\r\n"
          "</BODY></HTML>\r\n"
  }

, {.name= "404 no headers no body"
  ,.type= HTTP_RESPONSE
  ,.raw= "HTTP/1.1 404 Not Found\r\n\r\n"
  ,.should_keep_alive= TRUE
  ,.status_code= 404
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

, {.name= NULL } /* sentinel */
};

int
request_path_cb (http_parser *_, const char *p, size_t len)
{
  strncat(messages[num_messages].request_path, p, len);
  return 0;
}

int
request_uri_cb (http_parser *_, const char *p, size_t len)
{
  strncat(messages[num_messages].request_uri, p, len);
  return 0;
}

int
query_string_cb (http_parser *_, const char *p, size_t len)
{
  strncat(messages[num_messages].query_string, p, len);
  return 0;
}

int
fragment_cb (http_parser *_, const char *p, size_t len)
{
  strncat(messages[num_messages].fragment, p, len);
  return 0;
}

int
header_field_cb (http_parser *_, const char *p, size_t len)
{
  struct message *m = &messages[num_messages];

  if (m->last_header_element != FIELD)
    m->num_headers++;

  strncat(m->headers[m->num_headers-1][0], p, len);

  m->last_header_element = FIELD;

  return 0;
}

int
header_value_cb (http_parser *_, const char *p, size_t len)
{
  struct message *m = &messages[num_messages];

  strncat(m->headers[m->num_headers-1][1], p, len);

  m->last_header_element = VALUE;

  return 0;
}

int
body_cb (http_parser *_, const char *p, size_t len)
{
  strncat(messages[num_messages].body, p, len);
 // printf("body_cb: '%s'\n", requests[num_messages].body);
  return 0;
}

int
message_complete_cb (http_parser *parser)
{
  messages[num_messages].method = parser->method;
  messages[num_messages].status_code = parser->status_code;

  num_messages++;
  return 0;
}

int
message_begin_cb (http_parser *_)
{
  return 0;
}

void
parser_init (enum http_parser_type type)
{
  num_messages = 0;

  http_parser_init(&parser, type);

  memset(&messages, 0, sizeof messages);

  parser.on_message_begin     = message_begin_cb;
  parser.on_header_field      = header_field_cb;
  parser.on_header_value      = header_value_cb;
  parser.on_path              = request_path_cb;
  parser.on_uri               = request_uri_cb;
  parser.on_fragment          = fragment_cb;
  parser.on_query_string      = query_string_cb;
  parser.on_body              = body_cb;
  parser.on_headers_complete  = NULL;
  parser.on_message_complete  = message_complete_cb;
}

void
message_eq (int index, const struct message *expected)
{
  int i;
  struct message *m = &messages[index];

  assert(m->method == expected->method);
  assert(m->status_code == expected->status_code);

  assert(0 == strcmp(m->body, expected->body));
  assert(0 == strcmp(m->fragment, expected->fragment));
  assert(0 == strcmp(m->query_string, expected->query_string));
  assert(0 == strcmp(m->request_path, expected->request_path));
  assert(0 == strcmp(m->request_uri, expected->request_uri));
  assert(m->num_headers == expected->num_headers);
  for (i = 0; i < m->num_headers; i++) {
    assert(0 == strcmp(m->headers[i][0], expected->headers[i][0]));
    assert(0 == strcmp(m->headers[i][1], expected->headers[i][1]));
  }
}

void
parse_messages (int message_count, const struct message *input_messages[])
{
  // Concat the input messages
  size_t length = 0;
  int i;
  for (i = 0; i < message_count; i++) {
    length += strlen(input_messages[i]->raw);
  }
  char total[length + 1];
  total[0] = '\0';

  for (i = 0; i < message_count; i++) {
    strcat(total, input_messages[i]->raw);
  }

  // Parse the stream
  size_t traversed = 0;
  parser_init(HTTP_REQUEST);

  traversed = http_parser_execute(&parser, total, length);

  assert(!http_parser_has_error(&parser));
  assert(num_messages == message_count);

  for (i = 0; i < message_count; i++) {
    message_eq(i, input_messages[i]);
  }
}


void
test_message (const struct message *message)
{
  size_t traversed = 0;
  parser_init(message->type);

  traversed = http_parser_execute(&parser, message->raw, strlen(message->raw));
  assert(!http_parser_has_error(&parser));
  assert(num_messages == 1);

  message_eq(0, message);
}

void
test_error (const char *buf)
{
  size_t traversed = 0;
  parser_init(HTTP_REQUEST);

  traversed = http_parser_execute(&parser, buf, strlen(buf));

  assert(http_parser_has_error(&parser));
}

void
test_multiple3 (const struct message *r1, const struct message *r2, const struct message *r3)
{
  char total[ strlen(r1->raw) 
            + strlen(r2->raw) 
            + strlen(r3->raw) 
            + 1
            ];
  total[0] = '\0';

  strcat(total, r1->raw); 
  strcat(total, r2->raw); 
  strcat(total, r3->raw); 

  size_t traversed = 0;
  parser_init(HTTP_REQUEST);

  traversed = http_parser_execute(&parser, total, strlen(total));

  assert(! http_parser_has_error(&parser) );
  assert(num_messages == 3);
  message_eq(0, r1);
  message_eq(1, r2);
  message_eq(2, r3);
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

  int total_len = strlen(total);

  int total_ops = (total_len - 1) * (total_len - 2) / 2; 
  int ops = 0 ;

  int i,j;
  for (j = 2; j < total_len; j ++ ) {
    for (i = 1; i < j; i ++ ) {

      if (ops % 1000 == 0)  {
        printf("\b\b\b\b%3.0f%%", 100 * (float)ops /(float)total_ops);
        fflush(stdout);
      }
      ops += 1;

      parser_init(HTTP_REQUEST);

      int buf1_len = i;
      strncpy(buf1, total, buf1_len);
      buf1[buf1_len] = 0;

      int buf2_len = j - i;
      strncpy(buf2, total+i, buf2_len);
      buf2[buf2_len] = 0;

      int buf3_len = total_len - j;
      strncpy(buf3, total+j, buf3_len);
      buf3[buf3_len] = 0;

      /*
      printf("buf1: %s - %d\n", buf1, buf1_len);
      printf("buf2: %s - %d \n", buf2, buf2_len );
      printf("buf3: %s - %d\n\n", buf3, buf3_len);
      */

      http_parser_execute(&parser, buf1, buf1_len);

      assert(!http_parser_has_error(&parser));

      http_parser_execute(&parser, buf2, buf2_len);

      assert(!http_parser_has_error(&parser));

      http_parser_execute(&parser, buf3, buf3_len);

      assert(! http_parser_has_error(&parser));

      assert(3 == num_messages);

      message_eq(0, r1);
      message_eq(1, r2);
      message_eq(2, r3);
    }
  }
  printf("\b\b\b\b100%\n");
}

int
main (void)
{
  int i, j, k;

  printf("sizeof(http_parser) = %d\n", sizeof(http_parser));

  int request_count; 
  for (request_count = 0; requests[request_count].name; request_count++);

  int response_count; 
  for (response_count = 0; responses[response_count].name; response_count++);


  //// RESPONSES  

  for (i = 0; i < response_count; i++) {
    test_message(&responses[i]);
  }



  puts("responses okay");



  /// REQUESTS


  test_error("hello world");
  test_error("GET / HTP/1.1\r\n\r\n");

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
  test_error(dumbfuck2);

  // no content-length
  // error if there is a body without content length
  const char *bad_get_no_headers_no_body = "GET /bad_get_no_headers_no_body/world HTTP/1.1\r\n"
                                           "Accept: */*\r\n"
                                           "\r\n"
                                           "HELLO";
  test_error(bad_get_no_headers_no_body); 


  /* TODO sending junk and large headers gets rejected */


  /* check to make sure our predefined requests are okay */
  for (i = 0; requests[i].name; i++) {
    test_message(&requests[i]);
  }

  for (i = 0; i < request_count; i++) {
    for (j = 0; j < request_count; j++) {
      for (k = 0; k < request_count; k++) {
        //printf("%d %d %d\n", i, j, k);
        test_multiple3(&requests[i], &requests[j], &requests[k]);
      }
    }
  }

  printf("request scan 1/3      ");
  test_scan( &requests[GET_NO_HEADERS_NO_BODY]
           , &requests[GET_ONE_HEADER_NO_BODY]
           , &requests[GET_NO_HEADERS_NO_BODY]
           );

  printf("request scan 2/3      ");
  test_scan( &requests[GET_FUNKY_CONTENT_LENGTH]
           , &requests[POST_IDENTITY_BODY_WORLD]
           , &requests[POST_CHUNKED_ALL_YOUR_BASE]
           );

  printf("request scan 3/3      ");
  test_scan( &requests[TWO_CHUNKS_MULT_ZERO_END]
           , &requests[CHUNKED_W_TRAILING_HEADERS]
           , &requests[CHUNKED_W_BULLSHIT_AFTER_LENGTH]
           );

  puts("requests okay");


  return 0;
}

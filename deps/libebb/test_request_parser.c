/* unit tests for request parser
 * Copyright 2008 ryah dahl, ry@ndahl.us
 *
 * This software may be distributed under the "MIT" license included in the
 * README
 */
#include "ebb_request_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define MAX_HEADERS 500
#define MAX_ELEMENT_SIZE 500

static ebb_request_parser parser;
struct request_data {
  const char *raw;
  int request_method;
  char request_path[MAX_ELEMENT_SIZE];
  char request_uri[MAX_ELEMENT_SIZE];
  char fragment[MAX_ELEMENT_SIZE];
  char query_string[MAX_ELEMENT_SIZE];
  char body[MAX_ELEMENT_SIZE];
  int num_headers;
  char header_fields[MAX_HEADERS][MAX_ELEMENT_SIZE];
  char header_values[MAX_HEADERS][MAX_ELEMENT_SIZE];
  int should_keep_alive;
  ebb_request request;
};
static struct request_data requests[5];
static int num_requests;

const struct request_data curl_get = 
  { raw: "GET /test HTTP/1.1\r\n"
         "User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
         "Host: 0.0.0.0:5000\r\n"
         "Accept: */*\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/test"
  , request_uri: "/test"
  , num_headers: 3
  , header_fields: { "User-Agent", "Host", "Accept" }
  , header_values: { "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1", "0.0.0.0:5000", "*/*" }
  , body: ""
  };

const struct request_data firefox_get = 
  { raw: "GET /favicon.ico HTTP/1.1\r\n"
         "Host: 0.0.0.0:5000\r\n"
         "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
         "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
         "Accept-Language: en-us,en;q=0.5\r\n"
         "Accept-Encoding: gzip,deflate\r\n"
         "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
         "Keep-Alive: 300\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/favicon.ico"
  , request_uri: "/favicon.ico"
  , num_headers: 8
  , header_fields: 
    { "Host"
    , "User-Agent"
    , "Accept"
    , "Accept-Language"
    , "Accept-Encoding"
    , "Accept-Charset"
    , "Keep-Alive"
    , "Connection" 
    }
  , header_values: 
    { "0.0.0.0:5000"
    , "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0"
    , "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"
    , "en-us,en;q=0.5"
    , "gzip,deflate"
    , "ISO-8859-1,utf-8;q=0.7,*;q=0.7"
    , "300"
    , "keep-alive"
    }
  , body: ""
  };

const struct request_data dumbfuck =
  { raw: "GET /dumbfuck HTTP/1.1\r\n"
         "aaaaaaaaaaaaa:++++++++++\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/dumbfuck"
  , request_uri: "/dumbfuck"
  , num_headers: 1
  , header_fields: { "aaaaaaaaaaaaa" }
  , header_values: {  "++++++++++" }
  , body: ""
  };

const struct request_data fragment_in_uri = 
  { raw: "GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: "page=1"
  , fragment: "posts-17408"
  , request_path: "/forums/1/topics/2375"
  /* XXX request uri does not include fragment? */
  , request_uri: "/forums/1/topics/2375?page=1" 
  , num_headers: 0
  , body: ""
  };


// get - no headers - no body
const struct request_data get_no_headers_no_body =  
  { raw: "GET /get_no_headers_no_body/world HTTP/1.1\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/get_no_headers_no_body/world"
  , request_uri: "/get_no_headers_no_body/world"
  , num_headers: 0
  , body: ""
  };

// get - one header - no body
const struct request_data get_one_header_no_body =  
  { raw: "GET /get_one_header_no_body HTTP/1.1\r\n"
         "Accept: */*\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/get_one_header_no_body"
  , request_uri: "/get_one_header_no_body"
  , num_headers: 1
  , header_fields: { "Accept" }
  , header_values: { "*/*" }
  , body: ""
  };

// get - no headers - body "HELLO"
const struct request_data get_funky_content_length_body_hello =  
  { raw: "GET /get_funky_content_length_body_hello HTTP/1.0\r\n"
         "conTENT-Length: 5\r\n"
         "\r\n"
         "HELLO"
  , should_keep_alive: FALSE
  , request_method: EBB_GET
  , query_string: ""
  , fragment: ""
  , request_path: "/get_funky_content_length_body_hello"
  , request_uri: "/get_funky_content_length_body_hello"
  , num_headers: 1
  , header_fields: { "conTENT-Length" }
  , header_values: { "5" }
  , body: "HELLO"
  };

// post - one header - body "World"
const struct request_data post_identity_body_world =  
  { raw: "POST /post_identity_body_world?q=search#hey HTTP/1.1\r\n"
         "Accept: */*\r\n"
         "Transfer-Encoding: identity\r\n"
         "Content-Length: 5\r\n"
         "\r\n"
         "World"
  , should_keep_alive: TRUE
  , request_method: EBB_POST
  , query_string: "q=search"
  , fragment: "hey"
  , request_path: "/post_identity_body_world"
  , request_uri: "/post_identity_body_world?q=search"
  , num_headers: 3
  , header_fields: { "Accept", "Transfer-Encoding", "Content-Length" }
  , header_values: { "*/*", "identity", "5" }
  , body: "World"
  };

// post - no headers - chunked body "all your base are belong to us"
const struct request_data post_chunked_all_your_base =  
  { raw: "POST /post_chunked_all_your_base HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "1e\r\nall your base are belong to us\r\n"
         "0\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_POST
  , query_string: ""
  , fragment: ""
  , request_path: "/post_chunked_all_your_base"
  , request_uri: "/post_chunked_all_your_base"
  , num_headers: 1
  , header_fields: { "Transfer-Encoding" }
  , header_values: { "chunked" }
  , body: "all your base are belong to us"
  };

// two chunks ; triple zero ending
const struct request_data two_chunks_mult_zero_end =  
  { raw: "POST /two_chunks_mult_zero_end HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "000\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_POST
  , query_string: ""
  , fragment: ""
  , request_path: "/two_chunks_mult_zero_end"
  , request_uri: "/two_chunks_mult_zero_end"
  , num_headers: 1
  , header_fields: { "Transfer-Encoding" }
  , header_values: { "chunked" }
  , body: "hello world"
  };


// chunked with trailing headers. blech.
const struct request_data chunked_w_trailing_headers =  
  { raw: "POST /chunked_w_trailing_headers HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "0\r\n"
         "Vary: *\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_POST
  , query_string: ""
  , fragment: ""
  , request_path: "/chunked_w_trailing_headers"
  , request_uri: "/chunked_w_trailing_headers"
  , num_headers: 1
  , header_fields: { "Transfer-Encoding" }
  , header_values: { "chunked" }
  , body: "hello world"
  };

// with bullshit after the length
const struct request_data chunked_w_bullshit_after_length =  
  { raw: "POST /chunked_w_bullshit_after_length HTTP/1.1\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5; ihatew3;whatthefuck=aretheseparametersfor\r\nhello\r\n"
         "6; blahblah; blah\r\n world\r\n"
         "0\r\n"
         "\r\n"
  , should_keep_alive: TRUE
  , request_method: EBB_POST
  , query_string: ""
  , fragment: ""
  , request_path: "/chunked_w_bullshit_after_length"
  , request_uri: "/chunked_w_bullshit_after_length"
  , num_headers: 1
  , header_fields: { "Transfer-Encoding" }
  , header_values: { "chunked" }
  , body: "hello world"
  };

const struct request_data *fixtures[] =
  { &curl_get 
  , &firefox_get 
  , &dumbfuck
  , &fragment_in_uri 
  , &get_no_headers_no_body  
  , &get_one_header_no_body  
  , &get_funky_content_length_body_hello  
  , &post_identity_body_world  
  , &post_chunked_all_your_base  
  , &two_chunks_mult_zero_end  
  , &chunked_w_trailing_headers  
  , &chunked_w_bullshit_after_length  
  , NULL
  };


int request_data_eq
  ( struct request_data *r1
  , const struct request_data *r2
  )
{ 
  if(ebb_request_should_keep_alive(&r1->request) != r2->should_keep_alive) {
    printf("requests disagree on keep-alive");
    return FALSE;
  }

  if(0 != strcmp(r1->body, r2->body)) {
    printf("body '%s' != '%s'\n", r1->body, r2->body);
    return FALSE;
  }
  if(0 != strcmp(r1->fragment, r2->fragment)) {
    printf("fragment '%s' != '%s'\n", r1->fragment, r2->fragment);
    return FALSE;
  }
  if(0 != strcmp(r1->query_string, r2->query_string)) {
    printf("query_string '%s' != '%s'\n", r1->query_string, r2->query_string);
    return FALSE;
  }
  if(r1->request.method != r2->request_method) {
    printf("request_method '%d' != '%d'\n", r1->request.method, r2->request_method);
    return FALSE;
  }
  if(0 != strcmp(r1->request_path, r2->request_path)) {
    printf("request_path '%s' != '%s'\n", r1->request_path, r2->request_path);
    return FALSE;
  }
  if(0 != strcmp(r1->request_uri, r2->request_uri)) {
    printf("request_uri '%s' != '%s'\n", r1->request_uri, r2->request_uri);
    return FALSE;
  }
  if(r1->num_headers != r2->num_headers) {
    printf("num_headers '%d' != '%d'\n", r1->num_headers, r2->num_headers);
    return FALSE;
  }
  int i;
  for(i = 0; i < r1->num_headers; i++) {
    if(0 != strcmp(r1->header_fields[i], r2->header_fields[i])) {
      printf("header field '%s' != '%s'\n", r1->header_fields[i], r2->header_fields[i]);
      return FALSE;
    }
    if(0 != strcmp(r1->header_values[i], r2->header_values[i])) {
      printf("header field '%s' != '%s'\n", r1->header_values[i], r2->header_values[i]);
      return FALSE;
    }
  }
  return TRUE;
}

int request_eq
  ( int index
  , const struct request_data *expected
  )
{
  return request_data_eq(&requests[index], expected);
}

void request_complete(ebb_request *info)
{
  num_requests++;
}

void request_path_cb(ebb_request *request, const char *p, size_t len)
{
  strncat(requests[num_requests].request_path, p, len);
}

void request_uri_cb(ebb_request *request, const char *p, size_t len)
{
  strncat(requests[num_requests].request_uri, p, len);
}

void query_string_cb(ebb_request *request, const char *p, size_t len)
{
  strncat(requests[num_requests].query_string, p, len);
}

void fragment_cb(ebb_request *request, const char *p, size_t len)
{
  strncat(requests[num_requests].fragment, p, len);
}

void header_field_cb(ebb_request *request, const char *p, size_t len, int header_index)
{
  strncat(requests[num_requests].header_fields[header_index], p, len);
}

void header_value_cb(ebb_request *request, const char *p, size_t len, int header_index)
{
  strncat(requests[num_requests].header_values[header_index], p, len);
  requests[num_requests].num_headers = header_index + 1;
}

void body_handler(ebb_request *request, const char *p, size_t len)
{
  strncat(requests[num_requests].body, p, len);
 // printf("body_handler: '%s'\n", requests[num_requests].body);
}

ebb_request* new_request ()
{
  requests[num_requests].num_headers = 0;
  requests[num_requests].request_method = -1;
  requests[num_requests].request_path[0] = 0;
  requests[num_requests].request_uri[0] = 0;
  requests[num_requests].fragment[0] = 0;
  requests[num_requests].query_string[0] = 0;
  requests[num_requests].body[0] = 0;
  int i; 
  for(i = 0; i < MAX_HEADERS; i++) {
    requests[num_requests].header_fields[i][0] = 0;
    requests[num_requests].header_values[i][0] = 0;
  }
    
  ebb_request *r = &requests[num_requests].request;
  ebb_request_init(r);

  r->on_complete = request_complete;
  r->on_header_field = header_field_cb;
  r->on_header_value = header_value_cb;
  r->on_path = request_path_cb;
  r->on_uri = request_uri_cb;
  r->on_fragment = fragment_cb;
  r->on_query_string = query_string_cb;
  r->on_body = body_handler;
  r->on_headers_complete = NULL;

  r->data = &requests[num_requests];
 // printf("new request %d\n", num_requests);
  return r;
}

void parser_init()
{
  num_requests = 0;

  ebb_request_parser_init(&parser);

  parser.new_request = new_request;
}

int test_request
  ( const struct request_data *request_data
  )
{
  size_t traversed = 0;
  parser_init();

  traversed = ebb_request_parser_execute( &parser
                                        , request_data->raw 
                                        , strlen(request_data->raw)
                                        );
  if( ebb_request_parser_has_error(&parser) )
    return FALSE;
  if(! ebb_request_parser_is_finished(&parser) )
    return FALSE;
  if(num_requests != 1)
    return FALSE;

  return request_eq(0, request_data);
}

int test_error
  ( const char *buf
  )
{
  size_t traversed = 0;
  parser_init();

  traversed = ebb_request_parser_execute(&parser, buf, strlen(buf));

  return ebb_request_parser_has_error(&parser);
}


int test_multiple3
  ( const struct request_data *r1
  , const struct request_data *r2
  , const struct request_data *r3
  )
{
  char total[80*1024] = "\0";

  strcat(total, r1->raw); 
  strcat(total, r2->raw); 
  strcat(total, r3->raw); 

  size_t traversed = 0;
  parser_init();

  traversed = ebb_request_parser_execute(&parser, total, strlen(total));


  if( ebb_request_parser_has_error(&parser) )
    return FALSE;
  if(! ebb_request_parser_is_finished(&parser) )
    return FALSE;
  if(num_requests != 3)
    return FALSE;

  if(!request_eq(0, r1)){
    printf("request 1 error.\n");
    return FALSE;
  }
  if(!request_eq(1, r2)){
    printf("request 2 error.\n");
    return FALSE;
  }
  if(!request_eq(2, r3)){
    printf("request 3 error.\n");
    return FALSE;
  }

  return TRUE;
}

/**
 * SCAN through every possible breaking to make sure the 
 * parser can handle getting the content in any chunks that
 * might come from the socket
 */
int test_scan2
  ( const struct request_data *r1
  , const struct request_data *r2
  , const struct request_data *r3
  )
{
  char total[80*1024] = "\0";
  char buf1[80*1024] = "\0";
  char buf2[80*1024] = "\0";

  strcat(total, r1->raw); 
  strcat(total, r2->raw); 
  strcat(total, r3->raw); 

  int total_len = strlen(total);

  //printf("total_len = %d\n", total_len);
  int i;
  for(i = 1; i < total_len - 1; i ++ ) {

    parser_init();

    int buf1_len = i;
    strncpy(buf1, total, buf1_len);
    buf1[buf1_len] = 0;

    int buf2_len = total_len - i;
    strncpy(buf2, total+i, buf2_len);
    buf2[buf2_len] = 0;

    ebb_request_parser_execute(&parser, buf1, buf1_len);

    if( ebb_request_parser_has_error(&parser) ) {
      return FALSE;
    }
    /*
    if(ebb_request_parser_is_finished(&parser)) 
      return FALSE;
    */

    ebb_request_parser_execute(&parser, buf2, buf2_len);

    if( ebb_request_parser_has_error(&parser))
      return FALSE;
    if(!ebb_request_parser_is_finished(&parser)) 
      return FALSE;

    if(3 != num_requests) {
      printf("scan error: got %d requests in iteration %d\n", num_requests, i);
      return FALSE;
    }

    if(!request_eq(0, r1)) {
      printf("not maching r1\n");
      return FALSE;
    }
    if(!request_eq(1, r2)) {
      printf("not maching r2\n");
      return FALSE;
    }
    if(!request_eq(2, r3)) {
      printf("not maching r3\n");
      return FALSE;
    }
  }
  return TRUE;
}

int test_scan3
  ( const struct request_data *r1
  , const struct request_data *r2
  , const struct request_data *r3
  )
{
  char total[80*1024] = "\0";
  char buf1[80*1024] = "\0";
  char buf2[80*1024] = "\0";
  char buf3[80*1024] = "\0";

  strcat(total, r1->raw); 
  strcat(total, r2->raw); 
  strcat(total, r3->raw); 

  int total_len = strlen(total);

  //printf("total_len = %d\n", total_len);
  int i,j;
  for(j = 2; j < total_len - 1; j ++ ) {
    for(i = 1; i < j; i ++ ) {

      parser_init();




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

      ebb_request_parser_execute(&parser, buf1, buf1_len);

      if( ebb_request_parser_has_error(&parser) ) {
        return FALSE;
      }

      ebb_request_parser_execute(&parser, buf2, buf2_len);

      if( ebb_request_parser_has_error(&parser) ) {
        return FALSE;
      }

      ebb_request_parser_execute(&parser, buf3, buf3_len);

      if( ebb_request_parser_has_error(&parser))
        return FALSE;
      if(!ebb_request_parser_is_finished(&parser)) 
        return FALSE;

      if(3 != num_requests) {
        printf("scan error: only got %d requests in iteration %d\n", num_requests, i);
        return FALSE;
      }

      if(!request_eq(0, r1)) {
        printf("not maching r1\n");
        return FALSE;
      }
      if(!request_eq(1, r2)) {
        printf("not maching r2\n");
        return FALSE;
      }
      if(!request_eq(2, r3)) {
        printf("not maching r3\n");
        return FALSE;
      }
    }
  }
  return TRUE;
}

int main() 
{

  assert(test_error("hello world"));
  assert(test_error("GET / HTP/1.1\r\n\r\n"));

  assert(test_request(&curl_get));
  assert(test_request(&firefox_get));

  // Zed's header tests

  assert(test_request(&dumbfuck));

  const char *dumbfuck2 = "GET / HTTP/1.1\r\nX-SSL-Bullshit:   -----BEGIN CERTIFICATE-----\r\n\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx\r\n\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT\r\n\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu\r\n\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV\r\n\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV\r\n\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB\r\n\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF\r\n\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR\r\n\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL\r\n\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP\r\n\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR\r\n\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG\r\n\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgEBBAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs\r\n\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD\r\n\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj\r\n\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj\r\n\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG\r\n\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE\r\n\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO\r\n\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1\r\n\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBDAWLmh0\r\n\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD\r\n\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv\r\n\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3\r\n\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8\r\n\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk\r\n\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK\r\n\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu\r\n\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3\r\n\tRA==\r\n\t-----END CERTIFICATE-----\r\n\r\n";
  assert(test_error(dumbfuck2));

  assert(test_request(&fragment_in_uri));

  /* TODO sending junk and large headers gets rejected */


  /* check to make sure our predefined requests are okay */

  assert(test_request(&get_no_headers_no_body));
  assert(test_request(&get_one_header_no_body));
  assert(test_request(&get_no_headers_no_body));

  // no content-length
  const char *bad_get_no_headers_no_body = "GET /bad_get_no_headers_no_body/world HTTP/1.1\r\nAccept: */*\r\nHELLO\r\n";
  assert(test_error(bad_get_no_headers_no_body)); // error if there is a body without content length

  assert(test_request(&get_funky_content_length_body_hello));
  assert(test_request(&post_identity_body_world));
  assert(test_request(&post_chunked_all_your_base));
  assert(test_request(&two_chunks_mult_zero_end));
  assert(test_request(&chunked_w_trailing_headers));

  assert(test_request(&chunked_w_bullshit_after_length));
  assert(1 == requests[0].request.version_major); 
  assert(1 == requests[0].request.version_minor);

  // three requests - no bodies
  assert( test_multiple3( &get_no_headers_no_body
                        , &get_one_header_no_body
                        , &get_no_headers_no_body
                        ));

  // three requests - one body
  assert( test_multiple3(&get_no_headers_no_body, &get_funky_content_length_body_hello, &get_no_headers_no_body));

  // three requests with bodies -- last is chunked
  assert( test_multiple3(&get_funky_content_length_body_hello, &post_identity_body_world, &post_chunked_all_your_base));

  // three chunked requests
  assert( test_multiple3(&two_chunks_mult_zero_end, &post_chunked_all_your_base, &chunked_w_trailing_headers));


  assert(test_scan2(&get_no_headers_no_body, &get_one_header_no_body, &get_no_headers_no_body));
  assert(test_scan2(&get_funky_content_length_body_hello, &post_identity_body_world, &post_chunked_all_your_base));
  assert(test_scan2(&two_chunks_mult_zero_end, &chunked_w_trailing_headers, &chunked_w_bullshit_after_length));

  assert(test_scan3(&get_no_headers_no_body, &get_one_header_no_body, &get_no_headers_no_body));
  assert(test_scan3(&get_funky_content_length_body_hello, &post_identity_body_world, &post_chunked_all_your_base));
  assert(test_scan3(&two_chunks_mult_zero_end, &chunked_w_trailing_headers, &chunked_w_bullshit_after_length));


  printf("okay\n");
  return 0;
}


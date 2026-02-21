/*
 * urlparse
 *
 * Copyright (c) 2024 urlparse contributors
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
#include "urlparse_test.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "urlparse.h"
#include "http-parser/http_parser.h"

static const MunitTest tests[] = {
  munit_void_test(test_urlparse_parse_url),
  munit_test_end(),
};

const MunitSuite urlparse_suite = {
  "/urlparse", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

#define init_buffer(BUF, BUFLEN, S)                                            \
  do {                                                                         \
    (BUF) = malloc(sizeof(S) - 1);                                             \
    memcpy((BUF), (S), sizeof(S) - 1);                                         \
    (BUFLEN) = sizeof(S) - 1;                                                  \
  } while (0)

#define assert_field_equal(URL, FIELD, BUF, EXPECTED)                          \
  do {                                                                         \
    assert_true((URL).field_set & (1 << (FIELD)));                             \
    assert_memn_equal((EXPECTED), sizeof(EXPECTED) - 1,                        \
                      (BUF) + (URL).field_data[(FIELD)].off,                   \
                      (URL).field_data[(FIELD)].len);                          \
  } while (0)

#define assert_field_unset(URL, FIELD)                                         \
  assert_false((URL).field_set & (1 << (FIELD)))

/* Copied from https://github.com/nodejs/http-parser */
static void dump_url(const char *url, const urlparse_url *u) {
  size_t i;

  fprintf(stderr, "\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < URLPARSE_MAX; i++) {
    if ((u->field_set & (1 << i)) == 0) {
      fprintf(stderr, "\tfield_data[%zu]: unset\n", i);
      continue;
    }

    fprintf(stderr, "\tfield_data[%zu]: off: %u len: %u part: \"%.*s\"\n", i,
            u->field_data[i].off, u->field_data[i].len, u->field_data[i].len,
            url + u->field_data[i].off);
  }
}

#define assert_url(BUF, U1, U2)                                                \
  do {                                                                         \
    if (memcmp((U1), (U2), sizeof(urlparse_url)) == 0) {                       \
      break;                                                                   \
    }                                                                          \
                                                                               \
    dump_url((BUF), (urlparse_url *)(U1));                                     \
    dump_url((BUF), (U2));                                                     \
    assert_memory_equal(sizeof(urlparse_url), (U1), (U2));                     \
  } while (0)

void test_urlparse_parse_url(void) {
  char *b;
  size_t blen;
  urlparse_url u;
  int rv;
  struct http_parser_url hu;

  {
    /* absempty */
    init_buffer(b, blen, "https://example.com/");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* absempty + port */
    init_buffer(b, blen, "https://example.com:8080/foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PORT, b, "8080");
    assert_uint16(8080, ==, u.port);
    assert_field_equal(u, URLPARSE_PATH, b, "/foo");

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* port too large */
    init_buffer(b, blen, "https://example.com:65536/foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* leading zeros in port */
    init_buffer(b, blen, "https://example.com:0000443/foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PORT, b, "0000443");
    assert_uint16(443, ==, u.port);
    assert_field_equal(u, URLPARSE_PATH, b, "/foo");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* empty path */
    init_buffer(b, blen, "https://example.com");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* userinfo */
    init_buffer(b, blen, "https://foo:bar@example.com/");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_USERINFO, b, "foo:bar");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* host:port like userinfo */
    init_buffer(b, blen, "https://foo:111@example.com/");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_USERINFO, b, "foo:111");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* IPv6 host */
    init_buffer(b, blen, "https://[::1]/");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "::1");
    assert_field_equal(u, URLPARSE_PATH, b, "/");

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* query */
    init_buffer(b, blen, "https://example.com/?foo/??");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_equal(u, URLPARSE_QUERY, b, "foo/??");

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* fragment */
    init_buffer(b, blen, "https://example.com/#?foo/??");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_equal(u, URLPARSE_FRAGMENT, b, "?foo/??");

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* path + fragment */
    init_buffer(b, blen, "https://example.com/?32423?fdaff#?foo/??");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_equal(u, URLPARSE_QUERY, b, "32423?fdaff");
    assert_field_equal(u, URLPARSE_FRAGMENT, b, "?foo/??");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* path_rootless */
    init_buffer(b, blen, "foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* path_absolute */
    init_buffer(b, blen, "/foo/bar");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "/foo/bar");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* is_connect */
    init_buffer(b, blen, "example.com:443");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 1, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PORT, b, "443");
    assert_uint16(443, ==, u.port);
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 1, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* is_connect without port */
    init_buffer(b, blen, "example.com");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 1, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 1, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* is_connect with empty port */
    init_buffer(b, blen, "example.com:");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 1, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 1, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* is_connect with scheme */
    init_buffer(b, blen, "https://example.com:443");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 1, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 1, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* empty scheme */
    init_buffer(b, blen, "://example.com:443");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* starts with "//" */
    init_buffer(b, blen, "//example.com:443");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "//example.com:443");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* no colon in first component */
    init_buffer(b, blen, "https/foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* query starts with ?? */
    init_buffer(b, blen, "/foo??bar");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "/foo");
    assert_field_equal(u, URLPARSE_QUERY, b, "?bar");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* fragment starts with ## */
    init_buffer(b, blen, "/foo##bar");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "/foo");
    assert_field_equal(u, URLPARSE_FRAGMENT, b, "bar");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* query after host */
    init_buffer(b, blen, "https://example.com?foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_QUERY, b, "foo");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* query after port */
    init_buffer(b, blen, "https://example.com:8443?foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PORT, b, "8443");
    assert_uint16(8443, ==, u.port);
    assert_field_equal(u, URLPARSE_QUERY, b, "foo");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* fragment after host */
    init_buffer(b, blen, "https://example.com#foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* fragment after port */
    init_buffer(b, blen, "https://example.com:8443#foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* query only */
    init_buffer(b, blen, "?foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* fragment only */
    init_buffer(b, blen, "#foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* host all chars */
    init_buffer(
      b, blen,
      "https://"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(
      u, URLPARSE_HOST, b,
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* IPv6 all chars */
    init_buffer(
      b, blen,
      "https://"
      "[abcdefABCDEF0123456789:.%"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%.-_~]");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(
      u, URLPARSE_HOST, b,
      "abcdefABCDEF0123456789:.%"
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%.-_~");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* userinfo all chars */
    init_buffer(b, blen,
                "https://"
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "-._~%!$&'()*+,;=:@example.com");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_USERINFO, b,
                       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012"
                       "3456789-._~%!$&'()*+,;=:");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* path all chars */
    init_buffer(b, blen,
                "https://example.com/"
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "-._~%!$&'()*+,;=:@/\"<>[\\]^`{|}");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(
      u, URLPARSE_PATH, b,
      "/abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~%!$&'()*+,;=:@/\"<>[\\]^`{|}");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* query all chars */
    init_buffer(b, blen,
                "https://"
                "example.com?"
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "-._~%!$&'()*+,;=:@/?\"<>[\\]^`{|}");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(
      u, URLPARSE_QUERY, b,
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~%!$&'()*+,;=:@/?\"<>[\\]^`{|}");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_PATH);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* fragment all chars */
    init_buffer(b, blen,
                "https://"
                "example.com/#"
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                "-._~%!$&'()*+,;=:@/?\"<>[\\]^`{|}");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_SCHEMA, b, "https");
    assert_field_equal(u, URLPARSE_HOST, b, "example.com");
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_equal(
      u, URLPARSE_FRAGMENT, b,
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "-._~%!$&'()*+,;=:@/?\"<>[\\]^`{|}");
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* just * */
    init_buffer(b, blen, "*");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "*");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* just ** */
    init_buffer(b, blen, "**");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "**");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* ** followed by ? */
    init_buffer(b, blen, "**?foo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "**");
    assert_field_equal(u, URLPARSE_QUERY, b, "foo");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* just / */
    init_buffer(b, blen, "/");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "/");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);
    assert_field_unset(u, URLPARSE_FRAGMENT);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* * flollowed by # */
    init_buffer(b, blen, "*#w#");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);
    assert_field_equal(u, URLPARSE_PATH, b, "*");
    assert_field_equal(u, URLPARSE_FRAGMENT, b, "w#");
    assert_field_unset(u, URLPARSE_SCHEMA);
    assert_field_unset(u, URLPARSE_USERINFO);
    assert_field_unset(u, URLPARSE_HOST);
    assert_field_unset(u, URLPARSE_PORT);
    assert_field_unset(u, URLPARSE_QUERY);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* bad IPv6 */
    init_buffer(b, blen, "mmm://[[[[[[[[)iomm://)mo");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* non-alpha scheme */
    init_buffer(b, blen, "c6://c:@N");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* unterminated IPv6 */
    init_buffer(b, blen, "Cc://[8");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* IPv6 followed by garbage */
    init_buffer(b, blen, "s://[F]*iiiiffy");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* empty IPv6 */
    init_buffer(b, blen, "h://@[]//2h:");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(URLPARSE_ERR_PARSE, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, !=, rv);

    free(b);
  }

  {
    /* is_connect with query */
    init_buffer(b, blen, "A:1?");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 1, &u);

    assert_int(0, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 1, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }

  {
    /* long URL */
    init_buffer(
      b, blen,
      "https://"
      "abcdefghijklmno2qrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~%!$&'("
      ")*+,;=:@[abcdefABCDEF0123456789:.%"
      "abcdefghijklmnopqrDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.~_%]:12345/"
      "abcdefghijklmnopqrstuxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~%!$&'()*"
      "+,;=:@[abcdefABCDEF0123456789:.%"
      "abcdefghijklmnopqrDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.~_%]:12345/"
      "abcdefghijklmnopqrstuvwx$&'()*+,;=:@/"
      "?\"<>[\\]^`{|}#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP<>[\\]^`{|}");

    rv = urlparse_parse_url(b, blen, /* is_connect = */ 0, &u);

    assert_int(0, ==, rv);

    memset(&hu, 0, sizeof(hu));
    rv = http_parser_parse_url(b, blen, /* is_connect = */ 0, &hu);

    assert_int(0, ==, rv);
    assert_url(b, &hu, &u);

    free(b);
  }
}

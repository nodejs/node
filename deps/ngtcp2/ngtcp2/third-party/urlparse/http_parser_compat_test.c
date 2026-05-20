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
#include "http_parser_compat_test.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "urlparse.h"

static const MunitTest tests[] = {
  munit_void_test(test_http_parser_compat),
  munit_test_end(),
};

const MunitSuite http_parser_compat_suite = {
  "/http_parser_compat", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

/* Copied from
   https://github.com/nodejs/http-parser/blob/ec8b5ee63f0e51191ea43bb0c6eac7bfbff3141d/test.c
 */

struct url_test {
  const char *name;
  const char *url;
  int is_connect;
  urlparse_url u;
  int rv;
};

static const struct url_test url_tests[] =
  {
    {
      .name = "proxy request",
      .url = "http://hostname/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {7, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {15, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "proxy request with port",
      .url = "http://hostname:444/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PORT) | (1 << URLPARSE_PATH),
          .port = 444,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {7, 8} /* URLPARSE_HOST */,
              {16, 3} /* URLPARSE_PORT */,
              {19, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "CONNECT request",
      .url = "hostname:443",
      .is_connect = 1,
      .u =
        {
          .field_set = (1 << URLPARSE_HOST) | (1 << URLPARSE_PORT),
          .port = 443,
          .field_data =
            {
              {0, 0} /* URLPARSE_SCHEMA */,
              {0, 8} /* URLPARSE_HOST */,
              {9, 3} /* URLPARSE_PORT */,
              {0, 0} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "CONNECT request but not connect",
      .url = "hostname:443",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "proxy ipv6 request",
      .url = "http://[1:2::3:4]/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {17, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "proxy ipv6 request with port",
      .url = "http://[1:2::3:4]:67/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PORT) | (1 << URLPARSE_PATH),
          .port = 67,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 8} /* URLPARSE_HOST */,
              {18, 2} /* URLPARSE_PORT */,
              {20, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "CONNECT ipv6 address",
      .url = "[1:2::3:4]:443",
      .is_connect = 1,
      .u =
        {
          .field_set = (1 << URLPARSE_HOST) | (1 << URLPARSE_PORT),
          .port = 443,
          .field_data =
            {
              {0, 0} /* URLPARSE_SCHEMA */,
              {1, 8} /* URLPARSE_HOST */,
              {11, 3} /* URLPARSE_PORT */,
              {0, 0} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "ipv4 in ipv6 address",
      .url = "http://[2001:0000:0000:0000:0000:0000:1.9.1.1]/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 37} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {46, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "extra ? in query string",
      .url = "http://a.tbcdn.cn/p/fp/2010c/??fp-header-min.css,fp-base-min.css,"
             "fp-channel-min.css,fp-product-min.css,fp-mall-min.css,fp-"
             "category-min.css,"
             "fp-sub-min.css,fp-gdp4p-min.css,fp-css3-min.css,fp-misc-min.css?"
             "t=20101022.css",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH) | (1 << URLPARSE_QUERY),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {7, 10} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {17, 12} /* URLPARSE_PATH */,
              {30, 187} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "space URL encoded",
      .url = "/toto.html?toto=a%20b",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_PATH) | (1 << URLPARSE_QUERY),
          .port = 0,
          .field_data =
            {
              {0, 0} /* URLPARSE_SCHEMA */,
              {0, 0} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {0, 10} /* URLPARSE_PATH */,
              {11, 10} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "URL fragment",
      .url = "/toto.html#titi",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_PATH) | (1 << URLPARSE_FRAGMENT),
          .port = 0,
          .field_data =
            {
              {0, 0} /* URLPARSE_SCHEMA */,
              {0, 0} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {0, 10} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {11, 4} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "complex URL fragment",
      .url = "http://www.webmasterworld.com/r.cgi?f=21&d=8405&url="
             "http://www.example.com/index.html?foo=bar&hello=world#midpage",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH) | (1 << URLPARSE_QUERY) |
                       (1 << URLPARSE_FRAGMENT),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {7, 22} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {29, 6} /* URLPARSE_PATH */,
              {36, 69} /* URLPARSE_QUERY */,
              {106, 7} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "complex URL from node js url parser doc",
      .url = "http://host.com:8080/p/a/t/h?query=string#hash",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PORT) | (1 << URLPARSE_PATH) |
                       (1 << URLPARSE_QUERY) | (1 << URLPARSE_FRAGMENT),
          .port = 8080,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {7, 8} /* URLPARSE_HOST */,
              {16, 4} /* URLPARSE_PORT */,
              {20, 8} /* URLPARSE_PATH */,
              {29, 12} /* URLPARSE_QUERY */,
              {42, 4} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "complex URL with basic auth from node js url parser doc",
      .url = "http://a:b@host.com:8080/p/a/t/h?query=string#hash",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PORT) | (1 << URLPARSE_PATH) |
                       (1 << URLPARSE_QUERY) | (1 << URLPARSE_FRAGMENT) |
                       (1 << URLPARSE_USERINFO),
          .port = 8080,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {11, 8} /* URLPARSE_HOST */,
              {20, 4} /* URLPARSE_PORT */,
              {24, 8} /* URLPARSE_PATH */,
              {33, 12} /* URLPARSE_QUERY */,
              {46, 4} /* URLPARSE_FRAGMENT */,
              {7, 3} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "double @",
      .url = "http://a:b@@hostname:443/",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "proxy empty host",
      .url = "http://:443/",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "proxy empty port",
      .url = "http://hostname:/",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "CONNECT with basic auth",
      .url = "a:b@hostname:443",
      .is_connect = 1,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "CONNECT empty host",
      .url = ":443",
      .is_connect = 1,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "CONNECT empty port",
      .url = "hostname:",
      .is_connect = 1,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "CONNECT with extra bits",
      .url = "hostname:443/",
      .is_connect = 1,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "space in URL",
      .url = "/foo bar/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy basic auth with space url encoded",
      .url = "http://a%20:b@host.com/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH) | (1 << URLPARSE_USERINFO),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {14, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {22, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {7, 6} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "carriage return in URL",
      .url = "/foo\rbar/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy double : in URL",
      .url = "http://hostname::443/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy basic auth with double :",
      .url = "http://a::b@host.com/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH) | (1 << URLPARSE_USERINFO),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {12, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {20, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {7, 4} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "line feed in URL",
      .url = "/foo\nbar/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy empty basic auth",
      .url = "http://@hostname/fo",
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {16, 3} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "proxy line feed in hostname",
      .url = "http://host\name/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy % in hostname",
      .url = "http://host%name/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy ; in hostname",
      .url = "http://host;ame/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy basic auth with unreservedchars",
      .url = "http://a!;-_!=+$@host.com/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH) | (1 << URLPARSE_USERINFO),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {17, 8} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {25, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {7, 9} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "proxy only empty basic auth",
      .url = "http://@/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy only basic auth",
      .url = "http://toto@/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy emtpy hostname",
      .url = "http:///fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "proxy = in URL",
      .url = "http://host=ame/fo",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "ipv6 address with Zone ID",
      .url = "http://[fe80::a%25eth0]/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 14} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {23, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "ipv6 address with Zone ID, but '%' is not percent-encoded",
      .url = "http://[fe80::a%eth0]/",
      .is_connect = 0,
      .u =
        {
          .field_set = (1 << URLPARSE_SCHEMA) | (1 << URLPARSE_HOST) |
                       (1 << URLPARSE_PATH),
          .port = 0,
          .field_data =
            {
              {0, 4} /* URLPARSE_SCHEMA */,
              {8, 12} /* URLPARSE_HOST */,
              {0, 0} /* URLPARSE_PORT */,
              {21, 1} /* URLPARSE_PATH */,
              {0, 0} /* URLPARSE_QUERY */,
              {0, 0} /* URLPARSE_FRAGMENT */,
              {0, 0} /* URLPARSE_USERINFO */,
            },
        },
      .rv = 0,
    },
    {
      .name = "ipv6 address ending with '%'",
      .url = "http://[fe80::a%]/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "ipv6 address with Zone ID including bad character",
      .url = "http://[fe80::a%$HOME]/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "just ipv6 Zone ID",
      .url = "http://[%eth0]/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "empty url",
      .url = "",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "NULL url",
      .url = NULL,
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "full of spaces url",
      .url = "  ",
      .is_connect = 0,
      .rv = URLPARSE_ERR_PARSE,
    },
    {
      .name = "tab in URL",
      .url = "/foo\tbar/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
    {
      .name = "form feed in URL",
      .url = "/foo\fbar/",
      .rv = URLPARSE_ERR_PARSE /* s_dead */,
    },
};

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

void test_http_parser_compat(void) {
  urlparse_url u;
  const struct url_test *test;
  size_t i;
  int rv;

  for (i = 0; i < (sizeof(url_tests) / sizeof(url_tests[0])); ++i) {
    test = &url_tests[i];
    memset(&u, 0, sizeof(u));

    rv = urlparse_parse_url(test->url, test->url ? strlen(test->url) : 0,
                            test->is_connect, &u);

    if (test->rv == 0) {
      if (rv != 0) {
        fprintf(stderr,
                "\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
                "unexpected rv %d ***\n\n",
                test->url, test->name, rv);

        assert_int(0, ==, rv);
      }

      if (memcmp(&u, &test->u, sizeof(u)) != 0) {
        fprintf(stderr,
                "\n*** http_parser_parse_url(\"%s\") \"%s\" failed ***\n",
                test->url, test->name);

        fprintf(stderr, "target http_parser_url:\n");
        dump_url(test->url, &test->u);
        fprintf(stderr, "result http_parser_url:\n");
        dump_url(test->url, &u);

        assert_memory_equal(sizeof(u), &test->u, &u);
      }
    } else {
      /* test->rv != 0 */
      if (rv == 0) {
        fprintf(stderr,
                "\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
                "unexpected rv %d ***\n\n",
                test->url, test->name, rv);

        assert_int(test->rv, ==, rv);
      }
    }
  }
}

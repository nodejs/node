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
#include "urlparse.h"

#include <string.h>
#include <assert.h>

#define ALPHAS                                                                 \
  ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1, \
  ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, \
  ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1, \
  ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1, ['a'] = 1, ['b'] = 1, \
  ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1, ['h'] = 1, ['i'] = 1, \
  ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1, ['o'] = 1, ['p'] = 1, \
  ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1, ['v'] = 1, ['w'] = 1, \
  ['x'] = 1, ['y'] = 1, ['z'] = 1

#define DIGITS                                                                 \
  ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1, \
  ['7'] = 1, ['8'] = 1, ['9'] = 1

#define HEXDIGITS                                                              \
  DIGITS, ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1,    \
          ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1

typedef struct urlparse_parser {
  const char *begin;
  const char *pos;
  const char *end;
} urlparse_parser;

static int urlparse_parser_eof(urlparse_parser *up) {
  return up->pos == up->end;
}

static void urlparse_url_set_field_data(urlparse_url *dest, int field,
                                        const char *data, const char *start,
                                        const char *end) {
  dest->field_set |= (uint16_t)(1 << field);
  dest->field_data[field].off = (uint16_t)(start - data);
  dest->field_data[field].len = (uint16_t)(end - start);
}

static const uint8_t scheme_chars[256] = {
  ALPHAS,
  [':'] = 2,
  ['/'] = 3,
  ['*'] = 3,
};

static int parse_scheme(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  if (urlparse_parser_eof(up)) {
    return URLPARSE_ERR_PARSE;
  }

  switch (scheme_chars[(uint8_t)*up->pos]) {
  case 0:
  case 2:
    return URLPARSE_ERR_PARSE;
  case 1:
    break;
  case 3:
    return 0;
  }

  start = up->pos;
  ++up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (scheme_chars[(uint8_t)*up->pos]) {
    case 0:
    case 3:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      goto fin;
    }
  }

  return URLPARSE_ERR_PARSE;

fin:
  urlparse_url_set_field_data(dest, URLPARSE_SCHEMA, up->begin, start, up->pos);

  return 0;
}

static const uint8_t path_chars[256] = {
  /* unreserved */
  DIGITS,
  ALPHAS,
  ['-'] = 1,
  ['.'] = 1,
  ['_'] = 1,
  ['~'] = 1,
  /* pct-encoded */
  ['%'] = 1,
  /* sub-delims */
  ['!'] = 1,
  ['$'] = 1,
  ['&'] = 1,
  ['\''] = 1,
  ['('] = 1,
  [')'] = 1,
  ['*'] = 1,
  ['+'] = 1,
  [','] = 1,
  [';'] = 1,
  ['='] = 1,
  /* extra */
  [':'] = 1,
  ['@'] = 1,
  ['/'] = 1,
  /* http-parser allows the following characters as well. */
  ['"'] = 1,
  ['<'] = 1,
  ['>'] = 1,
  ['['] = 1,
  ['\\'] = 1,
  [']'] = 1,
  ['^'] = 1,
  ['`'] = 1,
  ['{'] = 1,
  ['|'] = 1,
  ['}'] = 1,
  /* query found */
  ['?'] = 2,
  /* fragment found */
  ['#'] = 2,
};

static int parse_path_abempty(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  if (urlparse_parser_eof(up)) {
    return 0;
  }

  switch (*up->pos) {
  case '?':
    return 0;
  case '/':
    break;
  case '*':
    if (dest->field_set & (1 << URLPARSE_HOST)) {
      return URLPARSE_ERR_PARSE;
    }

    break;
  default:
    return URLPARSE_ERR_PARSE;
  }

  start = up->pos++;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (path_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      goto fin;
    }
  }

fin:
  urlparse_url_set_field_data(dest, URLPARSE_PATH, up->begin, start, up->pos);

  return 0;
}

static const uint8_t userinfo_chars[256] = {
  /* unreserved */
  DIGITS,
  ALPHAS,
  ['-'] = 1,
  ['.'] = 1,
  ['_'] = 1,
  ['~'] = 1,
  /* pct-encoded */
  ['%'] = 1,
  /* sub-delims */
  ['!'] = 1,
  ['$'] = 1,
  ['&'] = 1,
  ['\''] = 1,
  ['('] = 1,
  [')'] = 1,
  ['*'] = 1,
  ['+'] = 1,
  [','] = 1,
  [';'] = 1,
  ['='] = 1,
  [':'] = 1,
  ['@'] = 2,
};

static int parse_userinfo(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  assert(!urlparse_parser_eof(up));

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (userinfo_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      goto fin;
    }
  }

  return URLPARSE_ERR_PARSE;

fin:
  if (start != up->pos) {
    urlparse_url_set_field_data(dest, URLPARSE_USERINFO, up->begin, start,
                                up->pos);
  }

  ++up->pos;

  return 0;
}

static const uint8_t host_chars[256] = {
  DIGITS, ALPHAS, ['.'] = 1, ['-'] = 1, [':'] = 2, ['/'] = 2, ['?'] = 2,
};

static int parse_host(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  assert(!urlparse_parser_eof(up));

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (host_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      break;
    }

    break;
  }

  if (start == up->pos) {
    return URLPARSE_ERR_PARSE;
  }

  urlparse_url_set_field_data(dest, URLPARSE_HOST, up->begin, start, up->pos);

  return 0;
}

static const uint8_t ipv6_host_chars[256] = {
  HEXDIGITS, [':'] = 1, ['.'] = 1, ['%'] = 2, [']'] = 3,
};

static const uint8_t ipv6_zone_chars[256] = {
  DIGITS,    ALPHAS,    ['%'] = 1, ['.'] = 1,
  ['-'] = 1, ['_'] = 1, ['~'] = 1, [']'] = 2,
};

static int parse_ipv6_host(urlparse_parser *up, urlparse_url *dest) {
  const char *start, *zone_start;

  if (urlparse_parser_eof(up)) {
    return URLPARSE_ERR_PARSE;
  }

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (ipv6_host_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      if (start == up->pos) {
        return URLPARSE_ERR_PARSE;
      }

      zone_start = up->pos;

      ++up->pos;
      if (urlparse_parser_eof(up)) {
        return URLPARSE_ERR_PARSE;
      }

      for (; !urlparse_parser_eof(up); ++up->pos) {
        switch (ipv6_zone_chars[(uint8_t)*up->pos]) {
        case 0:
          return URLPARSE_ERR_PARSE;
        case 1:
          continue;
        case 2:
          if (zone_start + 1 == up->pos) {
            return URLPARSE_ERR_PARSE;
          }

          goto fin;
        }
      }

      return URLPARSE_ERR_PARSE;
    case 3:
      goto fin;
    }
  }

  return URLPARSE_ERR_PARSE;

fin:
  if (start == up->pos) {
    return URLPARSE_ERR_PARSE;
  }

  urlparse_url_set_field_data(dest, URLPARSE_HOST, up->begin, start, up->pos);

  ++up->pos;

  return 0;
}

static const uint8_t port_chars[256] = {
  DIGITS,
  ['/'] = 2,
  ['?'] = 2,
};

static int parse_port(urlparse_parser *up, urlparse_url *dest) {
  const char *start;
  uint16_t port, d;

  if (urlparse_parser_eof(up)) {
    /* http_parser disallows empty port. */
    return URLPARSE_ERR_PARSE;
  }

  start = up->pos;
  port = 0;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (port_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      if (port > UINT16_MAX / 10) {
        return URLPARSE_ERR_PARSE;
      }

      port *= 10;
      d = (uint16_t)(*up->pos - '0');

      if (port > UINT16_MAX - d) {
        return URLPARSE_ERR_PARSE;
      }

      port += d;

      break;
    case 2:
      /* http_parser disallows empty port. */
      if (start == up->pos) {
        return URLPARSE_ERR_PARSE;
      }

      goto fin;
    }
  }

fin:
  urlparse_url_set_field_data(dest, URLPARSE_PORT, up->begin, start, up->pos);
  dest->port = port;

  return 0;
}

static int parse_authority(urlparse_parser *up, urlparse_url *dest) {
  const char *start;
  int rv;

  if (urlparse_parser_eof(up)) {
    return URLPARSE_ERR_PARSE;
  }

  if (*up->pos == '[') {
    ++up->pos;

    rv = parse_ipv6_host(up, dest);
    if (rv != 0) {
      return rv;
    }
  } else {
    start = up->pos;

    rv = parse_host(up, dest);
    if (rv == 0) {
      if (urlparse_parser_eof(up) || *up->pos != ':') {
        return 0;
      }

      ++up->pos;

      rv = parse_port(up, dest);
      if (rv == 0) {
        return 0;
      }
    }

    /* Rewind, and try parsing userinfo, and then host. */
    up->pos = start;

    rv = parse_userinfo(up, dest);
    if (rv != 0) {
      return rv;
    }

    if (urlparse_parser_eof(up)) {
      return URLPARSE_ERR_PARSE;
    }

    if (*up->pos == '[') {
      ++up->pos;

      rv = parse_ipv6_host(up, dest);
      if (rv != 0) {
        return rv;
      }
    } else {
      rv = parse_host(up, dest);
      if (rv != 0) {
        return rv;
      }
    }
  }

  if (urlparse_parser_eof(up) || *up->pos != ':') {
    return 0;
  }

  ++up->pos;

  return parse_port(up, dest);
}

static const uint8_t extra_chars[256] = {
  /* unreserved */
  DIGITS,
  ALPHAS,
  ['-'] = 1,
  ['.'] = 1,
  ['_'] = 1,
  ['~'] = 1,
  /* pct-encoded */
  ['%'] = 1,
  /* sub-delims */
  ['!'] = 1,
  ['$'] = 1,
  ['&'] = 1,
  ['\''] = 1,
  ['('] = 1,
  [')'] = 1,
  ['*'] = 1,
  ['+'] = 1,
  [','] = 1,
  [';'] = 1,
  ['='] = 1,
  /* extra */
  [':'] = 1,
  ['@'] = 1,
  /* query/fragment specific */
  ['/'] = 1,
  ['?'] = 1,
  /* http-parser allows the following characters as well. */
  ['"'] = 1,
  ['<'] = 1,
  ['>'] = 1,
  ['['] = 1,
  ['\\'] = 1,
  [']'] = 1,
  ['^'] = 1,
  ['`'] = 1,
  ['{'] = 1,
  ['|'] = 1,
  ['}'] = 1,
  ['#'] = 2,
};

static int parse_extra(urlparse_parser *up, urlparse_url *dest, int field) {
  const char *start;

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (extra_chars[(uint8_t)*up->pos]) {
    case 0:
      return URLPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      if (field == URLPARSE_QUERY) {
        goto fin;
      }

      continue;
    }
  }

fin:
  if (start != up->pos) {
    urlparse_url_set_field_data(dest, field, up->begin, start, up->pos);
  }

  return 0;
}

int urlparse_parse_url(const char *url, size_t urllen, int is_connect,
                       urlparse_url *dest) {
  urlparse_parser up;
  int rv;

  if (urllen == 0 || urllen > UINT16_MAX) {
    return URLPARSE_ERR_PARSE;
  }

  up.begin = url;
  up.pos = url;
  up.end = url + urllen;

  memset(dest, 0, sizeof(*dest));

  if (is_connect) {
    /* http_parser requires host:port when is_connect is nonzero. */
    rv = parse_authority(&up, dest);
    if (rv != 0) {
      return rv;
    }
  } else {
    rv = parse_scheme(&up, dest);
    if (rv != 0) {
      return rv;
    }

    if (dest->field_set & (1 << URLPARSE_SCHEMA)) {
      if (up.end - up.pos < 3 || *up.pos != ':' || *(up.pos + 1) != '/' ||
          *(up.pos + 2) != '/') {
        return URLPARSE_ERR_PARSE;
      }

      up.pos += 3;

      rv = parse_authority(&up, dest);
      if (rv != 0) {
        return rv;
      }
    }
  }

  rv = parse_path_abempty(&up, dest);
  if (rv != 0) {
    return rv;
  }

  if (urlparse_parser_eof(&up)) {
    goto fin;
  }

  if (*up.pos == '?') {
    ++up.pos;

    rv = parse_extra(&up, dest, URLPARSE_QUERY);
    if (rv != 0) {
      return rv;
    }
  }

  if (urlparse_parser_eof(&up)) {
    goto fin;
  }

  assert(*up.pos == '#');

  ++up.pos;

  for (; !urlparse_parser_eof(&up) && *up.pos == '#'; ++up.pos)
    ;

  rv = parse_extra(&up, dest, URLPARSE_FRAGMENT);
  if (rv != 0) {
    return rv;
  }

fin:
  if (is_connect &&
      dest->field_set != ((1 << URLPARSE_HOST) | (1 << URLPARSE_PORT))) {
    return URLPARSE_ERR_PARSE;
  }

  return 0;
}

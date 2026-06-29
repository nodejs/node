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
#include <stdio.h>

#define DIGIT_CASES                                                            \
  case '0':                                                                    \
  case '1':                                                                    \
  case '2':                                                                    \
  case '3':                                                                    \
  case '4':                                                                    \
  case '5':                                                                    \
  case '6':                                                                    \
  case '7':                                                                    \
  case '8':                                                                    \
  case '9'

#define LCALPHA_CASES                                                          \
  case 'a':                                                                    \
  case 'b':                                                                    \
  case 'c':                                                                    \
  case 'd':                                                                    \
  case 'e':                                                                    \
  case 'f':                                                                    \
  case 'g':                                                                    \
  case 'h':                                                                    \
  case 'i':                                                                    \
  case 'j':                                                                    \
  case 'k':                                                                    \
  case 'l':                                                                    \
  case 'm':                                                                    \
  case 'n':                                                                    \
  case 'o':                                                                    \
  case 'p':                                                                    \
  case 'q':                                                                    \
  case 'r':                                                                    \
  case 's':                                                                    \
  case 't':                                                                    \
  case 'u':                                                                    \
  case 'v':                                                                    \
  case 'w':                                                                    \
  case 'x':                                                                    \
  case 'y':                                                                    \
  case 'z'

#define UCALPHA_CASES                                                          \
  case 'A':                                                                    \
  case 'B':                                                                    \
  case 'C':                                                                    \
  case 'D':                                                                    \
  case 'E':                                                                    \
  case 'F':                                                                    \
  case 'G':                                                                    \
  case 'H':                                                                    \
  case 'I':                                                                    \
  case 'J':                                                                    \
  case 'K':                                                                    \
  case 'L':                                                                    \
  case 'M':                                                                    \
  case 'N':                                                                    \
  case 'O':                                                                    \
  case 'P':                                                                    \
  case 'Q':                                                                    \
  case 'R':                                                                    \
  case 'S':                                                                    \
  case 'T':                                                                    \
  case 'U':                                                                    \
  case 'V':                                                                    \
  case 'W':                                                                    \
  case 'X':                                                                    \
  case 'Y':                                                                    \
  case 'Z'

#define ALPHA_CASES                                                            \
  UCALPHA_CASES:                                                               \
  LCALPHA_CASES

#define HEX_CASES                                                              \
  DIGIT_CASES:                                                                 \
  case 'A':                                                                    \
  case 'B':                                                                    \
  case 'C':                                                                    \
  case 'D':                                                                    \
  case 'E':                                                                    \
  case 'F':                                                                    \
  case 'a':                                                                    \
  case 'b':                                                                    \
  case 'c':                                                                    \
  case 'd':                                                                    \
  case 'e':                                                                    \
  case 'f'

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

static int parse_scheme(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  if (urlparse_parser_eof(up)) {
    return URLPARSE_ERR_PARSE;
  }

  switch (*up->pos) {
  ALPHA_CASES:
    break;
  case '/':
  case '*':
    return 0;
  default:
    return URLPARSE_ERR_PARSE;
  }

  start = up->pos;
  ++up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (*up->pos) {
    ALPHA_CASES:
      continue;
    case ':':
      goto fin;
    default:
      return URLPARSE_ERR_PARSE;
    }
  }

  return URLPARSE_ERR_PARSE;

fin:
  urlparse_url_set_field_data(dest, URLPARSE_SCHEMA, up->begin, start, up->pos);

  return 0;
}

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
    switch (*up->pos) {
      /* unreserved */
    ALPHA_CASES:
    DIGIT_CASES:
    case '-':
    case '.':
    case '_':
    case '~':
      /* pct-encoded */
    case '%':
      /* sub-delims */
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
      /* extra */
    case ':':
    case '@':
    case '/':
      /* http-parser allows the following characters as well. */
    case '"':
    case '<':
    case '>':
    case '[':
    case '\\':
    case ']':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      continue;
    case '?':
    case '#':
      goto fin;
    default:
      return URLPARSE_ERR_PARSE;
    }
  }

fin:
  urlparse_url_set_field_data(dest, URLPARSE_PATH, up->begin, start, up->pos);

  return 0;
}

static int parse_userinfo(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  assert(!urlparse_parser_eof(up));

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (*up->pos) {
      /* unreserved */
    ALPHA_CASES:
    DIGIT_CASES:
    case '-':
    case '.':
    case '_':
    case '~':
      /* pct-encoded */
    case '%':
      /* sub-delims */
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
    case ':':
      continue;
    case '@':
      goto fin;
    default:
      return URLPARSE_ERR_PARSE;
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

static int parse_host(urlparse_parser *up, urlparse_url *dest) {
  const char *start;

  assert(!urlparse_parser_eof(up));

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (*up->pos) {
    ALPHA_CASES:
    DIGIT_CASES:
    case '.':
    case '-':
      continue;
    case ':':
    case '/':
    case '?':
      break;
    default:
      return URLPARSE_ERR_PARSE;
    }

    break;
  }

  if (start == up->pos) {
    return URLPARSE_ERR_PARSE;
  }

  urlparse_url_set_field_data(dest, URLPARSE_HOST, up->begin, start, up->pos);

  return 0;
}

static int parse_ipv6_host(urlparse_parser *up, urlparse_url *dest) {
  const char *start, *zone_start;

  if (urlparse_parser_eof(up)) {
    return URLPARSE_ERR_PARSE;
  }

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (*up->pos) {
    HEX_CASES:
    case ':':
    case '.':
      continue;
    case '%':
      if (start == up->pos) {
        return URLPARSE_ERR_PARSE;
      }

      zone_start = up->pos;

      ++up->pos;
      if (urlparse_parser_eof(up)) {
        return URLPARSE_ERR_PARSE;
      }

      for (; !urlparse_parser_eof(up); ++up->pos) {
        switch (*up->pos) {
        ALPHA_CASES:
        DIGIT_CASES:
        case '%':
        case '.':
        case '-':
        case '_':
        case '~':
          continue;
        case ']':
          if (zone_start + 1 == up->pos) {
            return URLPARSE_ERR_PARSE;
          }

          goto fin;
        default:
          return URLPARSE_ERR_PARSE;
        }
      }

      return URLPARSE_ERR_PARSE;
    case ']':
      goto fin;
    default:
      return URLPARSE_ERR_PARSE;
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
    switch (*up->pos) {
    DIGIT_CASES:
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
    case '/':
    case '?':
      /* http_parser disallows empty port. */
      if (start == up->pos) {
        return URLPARSE_ERR_PARSE;
      }

      goto fin;
    default:
      return URLPARSE_ERR_PARSE;
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

static int parse_extra(urlparse_parser *up, urlparse_url *dest, int field) {
  const char *start;

  start = up->pos;

  for (; !urlparse_parser_eof(up); ++up->pos) {
    switch (*up->pos) {
      /* unreserved */
    ALPHA_CASES:
    DIGIT_CASES:
    case '-':
    case '.':
    case '_':
    case '~':
      /* pct-encoded */
    case '%':
      /* sub-delims */
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
      /* extra */
    case ':':
    case '@':
      /* query/fragment specific */
    case '/':
    case '?':
      /* http-parser allows the following characters as well. */
    case '"':
    case '<':
    case '>':
    case '[':
    case '\\':
    case ']':
    case '^':
    case '`':
    case '{':
    case '|':
    case '}':
      continue;
    case '#':
      if (field == URLPARSE_QUERY) {
        goto fin;
      }

      continue;
    default:
      return URLPARSE_ERR_PARSE;
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

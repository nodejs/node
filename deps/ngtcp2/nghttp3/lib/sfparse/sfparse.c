/*
 * sfparse
 *
 * Copyright (c) 2023 sfparse contributors
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2015 nghttp2 contributors
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
#include "sfparse.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __AVX2__
#  include <immintrin.h>
#endif /* __AVX2__ */

#define SFPARSE_STATE_DICT 0x08u
#define SFPARSE_STATE_LIST 0x10u
#define SFPARSE_STATE_ITEM 0x18u

#define SFPARSE_STATE_INNER_LIST 0x04u

#define SFPARSE_STATE_BEFORE 0x00u
#define SFPARSE_STATE_BEFORE_PARAMS 0x01u
#define SFPARSE_STATE_PARAMS 0x02u
#define SFPARSE_STATE_AFTER 0x03u

#define SFPARSE_STATE_OP_MASK 0x03u

#define SFPARSE_SET_STATE_AFTER(NAME)                                          \
  (SFPARSE_STATE_##NAME | SFPARSE_STATE_AFTER)
#define SFPARSE_SET_STATE_BEFORE_PARAMS(NAME)                                  \
  (SFPARSE_STATE_##NAME | SFPARSE_STATE_BEFORE_PARAMS)
#define SFPARSE_SET_STATE_INNER_LIST_BEFORE(NAME)                              \
  (SFPARSE_STATE_##NAME | SFPARSE_STATE_INNER_LIST | SFPARSE_STATE_BEFORE)

#define SFPARSE_STATE_DICT_AFTER SFPARSE_SET_STATE_AFTER(DICT)
#define SFPARSE_STATE_DICT_BEFORE_PARAMS SFPARSE_SET_STATE_BEFORE_PARAMS(DICT)
#define SFPARSE_STATE_DICT_INNER_LIST_BEFORE                                   \
  SFPARSE_SET_STATE_INNER_LIST_BEFORE(DICT)

#define SFPARSE_STATE_LIST_AFTER SFPARSE_SET_STATE_AFTER(LIST)
#define SFPARSE_STATE_LIST_BEFORE_PARAMS SFPARSE_SET_STATE_BEFORE_PARAMS(LIST)
#define SFPARSE_STATE_LIST_INNER_LIST_BEFORE                                   \
  SFPARSE_SET_STATE_INNER_LIST_BEFORE(LIST)

#define SFPARSE_STATE_ITEM_AFTER SFPARSE_SET_STATE_AFTER(ITEM)
#define SFPARSE_STATE_ITEM_BEFORE_PARAMS SFPARSE_SET_STATE_BEFORE_PARAMS(ITEM)
#define SFPARSE_STATE_ITEM_INNER_LIST_BEFORE                                   \
  SFPARSE_SET_STATE_INNER_LIST_BEFORE(ITEM)

#define SFPARSE_STATE_INITIAL 0x00u

#define LCALPHAS                                                               \
  ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1, \
  ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1, \
  ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1, \
  ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1

#define ALPHAS(N)                                                              \
  ['A'] = (N), ['B'] = (N), ['C'] = (N), ['D'] = (N), ['E'] = (N),             \
  ['F'] = (N), ['G'] = (N), ['H'] = (N), ['I'] = (N), ['J'] = (N),             \
  ['K'] = (N), ['L'] = (N), ['M'] = (N), ['N'] = (N), ['O'] = (N),             \
  ['P'] = (N), ['Q'] = (N), ['R'] = (N), ['S'] = (N), ['T'] = (N),             \
  ['U'] = (N), ['V'] = (N), ['W'] = (N), ['X'] = (N), ['Y'] = (N),             \
  ['Z'] = (N), ['a'] = (N), ['b'] = (N), ['c'] = (N), ['d'] = (N),             \
  ['e'] = (N), ['f'] = (N), ['g'] = (N), ['h'] = (N), ['i'] = (N),             \
  ['j'] = (N), ['k'] = (N), ['l'] = (N), ['m'] = (N), ['n'] = (N),             \
  ['o'] = (N), ['p'] = (N), ['q'] = (N), ['r'] = (N), ['s'] = (N),             \
  ['t'] = (N), ['u'] = (N), ['v'] = (N), ['w'] = (N), ['x'] = (N),             \
  ['y'] = (N), ['z'] = (N)

#define DIGITS(N)                                                              \
  ['0'] = (N), ['1'] = (N), ['2'] = (N), ['3'] = (N), ['4'] = (N),             \
  ['5'] = (N), ['6'] = (N), ['7'] = (N), ['8'] = (N), ['9'] = (N)

#define HEXALPHAS(N)                                                           \
  ['a'] = (N), ['b'] = (N), ['c'] = (N), ['d'] = (N), ['e'] = (N), ['f'] = (N)

static int is_ws(uint8_t c) {
  switch (c) {
  case ' ':
  case '\t':
    return 1;
  default:
    return 0;
  }
}

#ifdef __AVX2__
#  ifdef _MSC_VER
#    include <intrin.h>

static int ctz(unsigned int v) {
  unsigned long n;

  /* Assume that v is not 0. */
  _BitScanForward(&n, v);

  return (int)n;
}
#  else /* !_MSC_VER */
#    define ctz __builtin_ctz
#  endif /* !_MSC_VER */
#endif   /* __AVX2__ */

static int parser_eof(sfparse_parser *sfp) { return sfp->pos == sfp->end; }

static void parser_discard_ows(sfparse_parser *sfp) {
  for (; !parser_eof(sfp) && is_ws(*sfp->pos); ++sfp->pos)
    ;
}

static void parser_discard_sp(sfparse_parser *sfp) {
  for (; !parser_eof(sfp) && *sfp->pos == ' '; ++sfp->pos)
    ;
}

static void parser_set_op_state(sfparse_parser *sfp, uint32_t op) {
  sfp->state &= ~SFPARSE_STATE_OP_MASK;
  sfp->state |= op;
}

static void parser_unset_inner_list_state(sfparse_parser *sfp) {
  sfp->state &= ~SFPARSE_STATE_INNER_LIST;
}

#ifdef __AVX2__
static const uint8_t *find_char_key(const uint8_t *first, const uint8_t *last) {
  const __m256i us = _mm256_set1_epi8('_');
  const __m256i ds = _mm256_set1_epi8('-');
  const __m256i dot = _mm256_set1_epi8('.');
  const __m256i ast = _mm256_set1_epi8('*');
  const __m256i r0l = _mm256_set1_epi8('0' - 1);
  const __m256i r0r = _mm256_set1_epi8('9' + 1);
  const __m256i r1l = _mm256_set1_epi8('a' - 1);
  const __m256i r1r = _mm256_set1_epi8('z' + 1);
  __m256i s, x;
  uint32_t m;

  for (; first != last; first += 32) {
    s = _mm256_loadu_si256((void *)first);

    x = _mm256_cmpeq_epi8(s, us);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, ds), x);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, dot), x);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, ast), x);
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r0l), _mm256_cmpgt_epi8(r0r, s)),
      x);
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r1l), _mm256_cmpgt_epi8(r1r, s)),
      x);

    m = ~(uint32_t)_mm256_movemask_epi8(x);
    if (m) {
      return first + ctz(m);
    }
  }

  return last;
}
#endif /* __AVX2__ */

static const uint8_t key_tbl[256] = {
  ['*'] = 1, LCALPHAS, ['_'] = 2, ['-'] = 2, ['.'] = 2, DIGITS(2),
};

static int parser_key(sfparse_parser *sfp, sfparse_vec *dest) {
  const uint8_t *base;
#ifdef __AVX2__
  const uint8_t *last;
#endif /* __AVX2__ */

  if (key_tbl[*sfp->pos] != 1) {
    return SFPARSE_ERR_PARSE;
  }

  base = sfp->pos++;

#ifdef __AVX2__
  if (sfp->end - sfp->pos >= 32) {
    last = sfp->pos + ((sfp->end - sfp->pos) & ~0x1fu);

    sfp->pos = find_char_key(sfp->pos, last);
    if (sfp->pos != last) {
      goto fin;
    }
  }
#endif /* __AVX2__ */

  for (; !parser_eof(sfp) && key_tbl[*sfp->pos]; ++sfp->pos)
    ;

#ifdef __AVX2__
fin:
#endif /* __AVX2__ */
  if (dest) {
    dest->base = (uint8_t *)base;
    dest->len = (size_t)(sfp->pos - dest->base);
  }

  return 0;
}

static const uint8_t number_tbl[256] = {
  DIGITS(1),
};

static int parser_number(sfparse_parser *sfp, sfparse_value *dest) {
  int sign = 1;
  int64_t value = 0;
  size_t len = 0;
  size_t fpos = 0;

  if (*sfp->pos == '-') {
    ++sfp->pos;
    if (parser_eof(sfp)) {
      return SFPARSE_ERR_PARSE;
    }

    sign = -1;
  }

  assert(!parser_eof(sfp));

  for (; !parser_eof(sfp) && number_tbl[*sfp->pos]; ++sfp->pos) {
    if (++len > 15) {
      return SFPARSE_ERR_PARSE;
    }

    value *= 10;
    value += *sfp->pos - '0';
  }

  if (len == 0) {
    return SFPARSE_ERR_PARSE;
  }

  if (parser_eof(sfp) || *sfp->pos != '.') {
    if (dest) {
      dest->type = SFPARSE_TYPE_INTEGER;
      dest->flags = SFPARSE_VALUE_FLAG_NONE;
      dest->integer = value * sign;
    }

    return 0;
  }

  /* decimal */

  if (len > 12) {
    return SFPARSE_ERR_PARSE;
  }

  fpos = len;

  ++sfp->pos;

  for (; !parser_eof(sfp) && number_tbl[*sfp->pos]; ++sfp->pos) {
    if (++len > 15) {
      return SFPARSE_ERR_PARSE;
    }

    value *= 10;
    value += *sfp->pos - '0';
  }

  if (fpos == len || len - fpos > 3) {
    return SFPARSE_ERR_PARSE;
  }

  if (dest) {
    dest->type = SFPARSE_TYPE_DECIMAL;
    dest->flags = SFPARSE_VALUE_FLAG_NONE;
    dest->decimal.numer = value * sign;

    switch (len - fpos) {
    case 1:
      dest->decimal.denom = 10;

      break;
    case 2:
      dest->decimal.denom = 100;

      break;
    case 3:
      dest->decimal.denom = 1000;

      break;
    }
  }

  return 0;
}

static int parser_date(sfparse_parser *sfp, sfparse_value *dest) {
  int rv;
  sfparse_value val;

  /* The first byte has already been validated by the caller. */
  assert('@' == *sfp->pos);

  ++sfp->pos;

  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  rv = parser_number(sfp, &val);
  if (rv != 0) {
    return rv;
  }

  if (val.type != SFPARSE_TYPE_INTEGER) {
    return SFPARSE_ERR_PARSE;
  }

  if (dest) {
    *dest = val;
    dest->type = SFPARSE_TYPE_DATE;
  }

  return 0;
}

#ifdef __AVX2__
static const uint8_t *find_char_string(const uint8_t *first,
                                       const uint8_t *last) {
  const __m256i bs = _mm256_set1_epi8('\\');
  const __m256i dq = _mm256_set1_epi8('"');
  const __m256i del = _mm256_set1_epi8(0x7f);
  const __m256i sp = _mm256_set1_epi8(' ');
  __m256i s, x;
  uint32_t m;

  for (; first != last; first += 32) {
    s = _mm256_loadu_si256((void *)first);

    x = _mm256_cmpgt_epi8(sp, s);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, bs), x);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, dq), x);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, del), x);

    m = (uint32_t)_mm256_movemask_epi8(x);
    if (m) {
      return first + ctz(m);
    }
  }

  return last;
}
#endif /* __AVX2__ */

static const uint8_t string_tbl[256] = {
  [' '] = 1, ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1,  ['\''] = 1,
  ['('] = 1, [')'] = 1, ['*'] = 1, ['+'] = 1, [','] = 1, ['-'] = 1,  ['.'] = 1,
  ['/'] = 1, DIGITS(1), [':'] = 1, [';'] = 1, ['<'] = 1, ['='] = 1,  ['>'] = 1,
  ['?'] = 1, ['@'] = 1, ALPHAS(1), ['['] = 1, [']'] = 1, ['^'] = 1,  ['_'] = 1,
  ['`'] = 1, ['{'] = 1, ['|'] = 1, ['}'] = 1, ['~'] = 1, ['\\'] = 2, ['"'] = 3,
};

static int parser_string(sfparse_parser *sfp, sfparse_value *dest) {
  const uint8_t *base;
#ifdef __AVX2__
  const uint8_t *last;
#endif /* __AVX2__ */
  uint32_t flags = SFPARSE_VALUE_FLAG_NONE;

  /* The first byte has already been validated by the caller. */
  assert('"' == *sfp->pos);

  base = ++sfp->pos;

#ifdef __AVX2__
  for (; sfp->end - sfp->pos >= 32; ++sfp->pos) {
    last = sfp->pos + ((sfp->end - sfp->pos) & ~0x1fu);

    sfp->pos = find_char_string(sfp->pos, last);
    if (sfp->pos == last) {
      break;
    }

    switch (*sfp->pos) {
    case '\\':
      ++sfp->pos;
      if (parser_eof(sfp)) {
        return SFPARSE_ERR_PARSE;
      }

      switch (*sfp->pos) {
      case '"':
      case '\\':
        flags = SFPARSE_VALUE_FLAG_ESCAPED_STRING;

        break;
      default:
        return SFPARSE_ERR_PARSE;
      }

      break;
    case '"':
      goto fin;
    default:
      return SFPARSE_ERR_PARSE;
    }
  }
#endif /* __AVX2__ */

  for (; !parser_eof(sfp); ++sfp->pos) {
    switch (string_tbl[*sfp->pos]) {
    case 0:
      return SFPARSE_ERR_PARSE;
    case 1:
      break;
    case 2:
      ++sfp->pos;
      if (parser_eof(sfp)) {
        return SFPARSE_ERR_PARSE;
      }

      switch (*sfp->pos) {
      case '"':
      case '\\':
        flags = SFPARSE_VALUE_FLAG_ESCAPED_STRING;

        break;
      default:
        return SFPARSE_ERR_PARSE;
      }

      break;
    case 3:
      goto fin;
    }
  }

  return SFPARSE_ERR_PARSE;

fin:
  if (dest) {
    dest->type = SFPARSE_TYPE_STRING;
    dest->flags = flags;
    dest->vec.len = (size_t)(sfp->pos - base);
    dest->vec.base = dest->vec.len == 0 ? NULL : (uint8_t *)base;
  }

  ++sfp->pos;

  return 0;
}

#ifdef __AVX2__
static const uint8_t *find_char_token(const uint8_t *first,
                                      const uint8_t *last) {
  /* r0: !..:, excluding "(),
     r1: A..Z
     r2: ^..~, excluding {} */
  const __m256i r0l = _mm256_set1_epi8('!' - 1);
  const __m256i r0r = _mm256_set1_epi8(':' + 1);
  const __m256i dq = _mm256_set1_epi8('"');
  const __m256i prl = _mm256_set1_epi8('(');
  const __m256i prr = _mm256_set1_epi8(')');
  const __m256i comma = _mm256_set1_epi8(',');
  const __m256i r1l = _mm256_set1_epi8('A' - 1);
  const __m256i r1r = _mm256_set1_epi8('Z' + 1);
  const __m256i r2l = _mm256_set1_epi8('^' - 1);
  const __m256i r2r = _mm256_set1_epi8('~' + 1);
  const __m256i cbl = _mm256_set1_epi8('{');
  const __m256i cbr = _mm256_set1_epi8('}');
  __m256i s, x;
  uint32_t m;

  for (; first != last; first += 32) {
    s = _mm256_loadu_si256((void *)first);

    x = _mm256_andnot_si256(
      _mm256_cmpeq_epi8(s, comma),
      _mm256_andnot_si256(
        _mm256_cmpeq_epi8(s, prr),
        _mm256_andnot_si256(
          _mm256_cmpeq_epi8(s, prl),
          _mm256_andnot_si256(_mm256_cmpeq_epi8(s, dq),
                              _mm256_and_si256(_mm256_cmpgt_epi8(s, r0l),
                                               _mm256_cmpgt_epi8(r0r, s))))));
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r1l), _mm256_cmpgt_epi8(r1r, s)),
      x);
    x = _mm256_or_si256(
      _mm256_andnot_si256(
        _mm256_cmpeq_epi8(s, cbr),
        _mm256_andnot_si256(_mm256_cmpeq_epi8(s, cbl),
                            _mm256_and_si256(_mm256_cmpgt_epi8(s, r2l),
                                             _mm256_cmpgt_epi8(r2r, s)))),
      x);

    m = ~(uint32_t)_mm256_movemask_epi8(x);
    if (m) {
      return first + ctz(m);
    }
  }

  return last;
}
#endif /* __AVX2__ */

static const uint8_t token_tbl[256] = {
  ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['*'] = 1,
  ['+'] = 1, ['-'] = 1, ['.'] = 1, ['/'] = 1, DIGITS(1), [':'] = 1,  ALPHAS(1),
  ['^'] = 1, ['_'] = 1, ['`'] = 1, ['|'] = 1, ['~'] = 1,
};

static int parser_token(sfparse_parser *sfp, sfparse_value *dest) {
  const uint8_t *base;
#ifdef __AVX2__
  const uint8_t *last;
#endif /* __AVX2__ */

  /* The first byte has already been validated by the caller. */
  base = sfp->pos++;

#ifdef __AVX2__
  if (sfp->end - sfp->pos >= 32) {
    last = sfp->pos + ((sfp->end - sfp->pos) & ~0x1fu);

    sfp->pos = find_char_token(sfp->pos, last);
    if (sfp->pos != last) {
      goto fin;
    }
  }
#endif /* __AVX2__ */

  for (; !parser_eof(sfp) && token_tbl[*sfp->pos]; ++sfp->pos)
    ;

#ifdef __AVX2__
fin:
#endif /* __AVX2__ */
  if (dest) {
    dest->type = SFPARSE_TYPE_TOKEN;
    dest->flags = SFPARSE_VALUE_FLAG_NONE;
    dest->vec.base = (uint8_t *)base;
    dest->vec.len = (size_t)(sfp->pos - base);
  }

  return 0;
}

#ifdef __AVX2__
static const uint8_t *find_char_byteseq(const uint8_t *first,
                                        const uint8_t *last) {
  const __m256i pls = _mm256_set1_epi8('+');
  const __m256i fs = _mm256_set1_epi8('/');
  const __m256i r0l = _mm256_set1_epi8('0' - 1);
  const __m256i r0r = _mm256_set1_epi8('9' + 1);
  const __m256i r1l = _mm256_set1_epi8('A' - 1);
  const __m256i r1r = _mm256_set1_epi8('Z' + 1);
  const __m256i r2l = _mm256_set1_epi8('a' - 1);
  const __m256i r2r = _mm256_set1_epi8('z' + 1);
  __m256i s, x;
  uint32_t m;

  for (; first != last; first += 32) {
    s = _mm256_loadu_si256((void *)first);

    x = _mm256_cmpeq_epi8(s, pls);
    x = _mm256_or_si256(_mm256_cmpeq_epi8(s, fs), x);
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r0l), _mm256_cmpgt_epi8(r0r, s)),
      x);
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r1l), _mm256_cmpgt_epi8(r1r, s)),
      x);
    x = _mm256_or_si256(
      _mm256_and_si256(_mm256_cmpgt_epi8(s, r2l), _mm256_cmpgt_epi8(r2r, s)),
      x);

    m = ~(uint32_t)_mm256_movemask_epi8(x);
    if (m) {
      return first + ctz(m);
    }
  }

  return last;
}
#endif /* __AVX2__ */

static const uint8_t byteseq_tbl[256] = {
  ['+'] = 1, ['/'] = 1, DIGITS(1), ALPHAS(1), ['='] = 2, [':'] = 3,
};

static int parser_byteseq(sfparse_parser *sfp, sfparse_value *dest) {
  const uint8_t *base;
#ifdef __AVX2__
  const uint8_t *last;
#endif /* __AVX2__ */

  /* The first byte has already been validated by the caller. */
  assert(':' == *sfp->pos);

  base = ++sfp->pos;

#ifdef __AVX2__
  if (sfp->end - sfp->pos >= 32) {
    last = sfp->pos + ((sfp->end - sfp->pos) & ~0x1fu);
    sfp->pos = find_char_byteseq(sfp->pos, last);
  }
#endif /* __AVX2__ */

  for (; !parser_eof(sfp); ++sfp->pos) {
    switch (byteseq_tbl[*sfp->pos]) {
    case 0:
      return SFPARSE_ERR_PARSE;
    case 1:
      continue;
    case 2:
      switch ((sfp->pos - base) & 0x3) {
      case 0:
      case 1:
        return SFPARSE_ERR_PARSE;
      case 2:
        ++sfp->pos;

        if (parser_eof(sfp)) {
          return SFPARSE_ERR_PARSE;
        }

        if (*sfp->pos == '=') {
          ++sfp->pos;
        }

        break;
      case 3:
        ++sfp->pos;

        break;
      }

      if (parser_eof(sfp) || *sfp->pos != ':') {
        return SFPARSE_ERR_PARSE;
      }

      goto fin;
    case 3:
      if (((sfp->pos - base) & 0x3) == 1) {
        return SFPARSE_ERR_PARSE;
      }

      goto fin;
    }
  }

  return SFPARSE_ERR_PARSE;

fin:
  if (dest) {
    dest->type = SFPARSE_TYPE_BYTESEQ;
    dest->flags = SFPARSE_VALUE_FLAG_NONE;
    dest->vec.len = (size_t)(sfp->pos - base);
    dest->vec.base = dest->vec.len == 0 ? NULL : (uint8_t *)base;
  }

  ++sfp->pos;

  return 0;
}

static int parser_boolean(sfparse_parser *sfp, sfparse_value *dest) {
  int b;

  /* The first byte has already been validated by the caller. */
  assert('?' == *sfp->pos);

  ++sfp->pos;

  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  switch (*sfp->pos) {
  case '0':
    b = 0;

    break;
  case '1':
    b = 1;

    break;
  default:
    return SFPARSE_ERR_PARSE;
  }

  ++sfp->pos;

  if (dest) {
    dest->type = SFPARSE_TYPE_BOOLEAN;
    dest->flags = SFPARSE_VALUE_FLAG_NONE;
    dest->boolean = b;
  }

  return 0;
}

static const uint8_t pct_tbl[256] = {
  DIGITS(1),
  HEXALPHAS(2),
};

static int pctdecode(uint8_t *pc, const uint8_t **ppos) {
  uint8_t c, b = **ppos;

  switch (pct_tbl[b]) {
  case 0:
    return -1;
  case 1:
    c = (uint8_t)((b - '0') << 4);

    break;
  case 2:
    c = (uint8_t)((b - 'a' + 10) << 4);

    break;
  default:
    assert(0);
    abort();
  }

  b = *++*ppos;

  switch (pct_tbl[b]) {
  case 0:
    return -1;
  case 1:
    c |= (uint8_t)(b - '0');

    break;
  case 2:
    c |= (uint8_t)(b - 'a' + 10);

    break;
  }

  *pc = c;
  ++*ppos;

  return 0;
}

/* Start of utf8 dfa */
/* Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 *
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

/* clang-format off */
static const uint8_t utf8d[] = {
  /*
   * The first part of the table maps bytes to character classes that
   * to reduce the size of the transition table and create bitmasks.
   */
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
   8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

   /*
    * The second part is a transition table that maps a combination
    * of a state of the automaton and a character class to a state.
    */
   0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
  12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
  12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
  12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
  12,36,12,12,12,12,12,12,12,12,12,12,
};
/* clang-format on */

static void utf8_decode(uint32_t *state, uint8_t byte) {
  *state = utf8d[256 + *state + utf8d[byte]];
}

/* End of utf8 dfa */

static const uint8_t dispstring_tbl[256] = {
  [' '] = 1, ['!'] = 1, ['#'] = 1, ['$'] = 1,  ['&'] = 1, ['\''] = 1, ['('] = 1,
  [')'] = 1, ['*'] = 1, ['+'] = 1, [','] = 1,  ['-'] = 1, ['.'] = 1,  ['/'] = 1,
  DIGITS(1), [':'] = 1, [';'] = 1, ['<'] = 1,  ['='] = 1, ['>'] = 1,  ['?'] = 1,
  ['@'] = 1, ALPHAS(1), ['['] = 1, ['\\'] = 1, [']'] = 1, ['^'] = 1,  ['_'] = 1,
  ['`'] = 1, ['{'] = 1, ['|'] = 1, ['}'] = 1,  ['~'] = 1, ['%'] = 2,  ['"'] = 3,
};

static int parser_dispstring(sfparse_parser *sfp, sfparse_value *dest) {
  const uint8_t *base;
  uint8_t c;
  uint32_t utf8state = UTF8_ACCEPT;

  assert('%' == *sfp->pos);

  ++sfp->pos;

  if (parser_eof(sfp) || *sfp->pos != '"') {
    return SFPARSE_ERR_PARSE;
  }

  base = ++sfp->pos;

  for (; !parser_eof(sfp);) {
    switch (dispstring_tbl[*sfp->pos]) {
    case 0:
      return SFPARSE_ERR_PARSE;
    case 1:
      ++sfp->pos;

      break;
    case 2:
      for (;;) {
        ++sfp->pos;

        if (sfp->pos + 2 > sfp->end || pctdecode(&c, &sfp->pos) != 0) {
          return SFPARSE_ERR_PARSE;
        }

        utf8_decode(&utf8state, c);
        if (utf8state == UTF8_ACCEPT) {
          if (sfp->pos != sfp->end && *sfp->pos == '%') {
            continue;
          }

          break;
        }

        if (utf8state == UTF8_REJECT || sfp->pos + 1 > sfp->end ||
            *sfp->pos != '%') {
          return SFPARSE_ERR_PARSE;
        }
      }

      break;
    case 3:
      assert(utf8state == UTF8_ACCEPT);

      if (dest) {
        dest->type = SFPARSE_TYPE_DISPSTRING;
        dest->flags = SFPARSE_VALUE_FLAG_NONE;
        dest->vec.len = (size_t)(sfp->pos - base);
        dest->vec.base = dest->vec.len == 0 ? NULL : (uint8_t *)base;
      }

      ++sfp->pos;

      return 0;
    }
  }

  return SFPARSE_ERR_PARSE;
}

static const uint8_t bare_item_tbl[256] = {
  ['"'] = 1, ['-'] = 2, DIGITS(2), ['@'] = 3, [':'] = 4,
  ['?'] = 5, ['*'] = 6, ALPHAS(6), ['%'] = 7,
};

static int parser_bare_item(sfparse_parser *sfp, sfparse_value *dest) {
  switch (bare_item_tbl[*sfp->pos]) {
  case 0:
    return SFPARSE_ERR_PARSE;
  case 1:
    return parser_string(sfp, dest);
  case 2:
    return parser_number(sfp, dest);
  case 3:
    return parser_date(sfp, dest);
  case 4:
    return parser_byteseq(sfp, dest);
  case 5:
    return parser_boolean(sfp, dest);
  case 6:
    return parser_token(sfp, dest);
  case 7:
    return parser_dispstring(sfp, dest);
  default:
    assert(0);
    abort();
  }
}

static int parser_skip_inner_list(sfparse_parser *sfp);

int sfparse_parser_param(sfparse_parser *sfp, sfparse_vec *dest_key,
                         sfparse_value *dest_value) {
  int rv;

  switch (sfp->state & SFPARSE_STATE_OP_MASK) {
  case SFPARSE_STATE_BEFORE:
    rv = parser_skip_inner_list(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_BEFORE_PARAMS:
    parser_set_op_state(sfp, SFPARSE_STATE_PARAMS);

    break;
  case SFPARSE_STATE_PARAMS:
    break;
  default:
    assert(0);
    abort();
  }

  if (parser_eof(sfp) || *sfp->pos != ';') {
    parser_set_op_state(sfp, SFPARSE_STATE_AFTER);

    return SFPARSE_ERR_EOF;
  }

  ++sfp->pos;

  parser_discard_sp(sfp);
  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  rv = parser_key(sfp, dest_key);
  if (rv != 0) {
    return rv;
  }

  if (parser_eof(sfp) || *sfp->pos != '=') {
    if (dest_value) {
      dest_value->type = SFPARSE_TYPE_BOOLEAN;
      dest_value->flags = SFPARSE_VALUE_FLAG_NONE;
      dest_value->boolean = 1;
    }

    return 0;
  }

  ++sfp->pos;

  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  return parser_bare_item(sfp, dest_value);
}

static int parser_skip_params(sfparse_parser *sfp) {
  int rv;

  for (;;) {
    rv = sfparse_parser_param(sfp, NULL, NULL);
    switch (rv) {
    case 0:
      break;
    case SFPARSE_ERR_EOF:
      return 0;
    case SFPARSE_ERR_PARSE:
      return rv;
    default:
      assert(0);
      abort();
    }
  }
}

int sfparse_parser_inner_list(sfparse_parser *sfp, sfparse_value *dest) {
  int rv;

  switch (sfp->state & SFPARSE_STATE_OP_MASK) {
  case SFPARSE_STATE_BEFORE:
    parser_discard_sp(sfp);
    if (parser_eof(sfp)) {
      return SFPARSE_ERR_PARSE;
    }

    break;
  case SFPARSE_STATE_BEFORE_PARAMS:
    rv = parser_skip_params(sfp);
    if (rv != 0) {
      return rv;
    }

    /* Technically, we are entering SFPARSE_STATE_AFTER, but we will set
       another state without reading the state. */
    /* parser_set_op_state(sfp, SFPARSE_STATE_AFTER); */

    /* fall through */
  case SFPARSE_STATE_AFTER:
    if (parser_eof(sfp)) {
      return SFPARSE_ERR_PARSE;
    }

    switch (*sfp->pos) {
    case ' ':
      parser_discard_sp(sfp);
      if (parser_eof(sfp)) {
        return SFPARSE_ERR_PARSE;
      }

      break;
    case ')':
      break;
    default:
      return SFPARSE_ERR_PARSE;
    }

    break;
  default:
    assert(0);
    abort();
  }

  if (*sfp->pos == ')') {
    ++sfp->pos;

    parser_unset_inner_list_state(sfp);
    parser_set_op_state(sfp, SFPARSE_STATE_BEFORE_PARAMS);

    return SFPARSE_ERR_EOF;
  }

  rv = parser_bare_item(sfp, dest);
  if (rv != 0) {
    return rv;
  }

  parser_set_op_state(sfp, SFPARSE_STATE_BEFORE_PARAMS);

  return 0;
}

static int parser_skip_inner_list(sfparse_parser *sfp) {
  int rv;

  for (;;) {
    rv = sfparse_parser_inner_list(sfp, NULL);
    switch (rv) {
    case 0:
      break;
    case SFPARSE_ERR_EOF:
      return 0;
    case SFPARSE_ERR_PARSE:
      return rv;
    default:
      assert(0);
      abort();
    }
  }
}

static int parser_next_key_or_item(sfparse_parser *sfp) {
  parser_discard_ows(sfp);

  if (parser_eof(sfp)) {
    return SFPARSE_ERR_EOF;
  }

  if (*sfp->pos != ',') {
    return SFPARSE_ERR_PARSE;
  }

  ++sfp->pos;

  parser_discard_ows(sfp);
  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  return 0;
}

static int parser_dict_value(sfparse_parser *sfp, sfparse_value *dest) {
  int rv;

  if (parser_eof(sfp) || *(sfp->pos) != '=') {
    /* Boolean true */
    if (dest) {
      dest->type = SFPARSE_TYPE_BOOLEAN;
      dest->flags = SFPARSE_VALUE_FLAG_NONE;
      dest->boolean = 1;
    }

    sfp->state = SFPARSE_STATE_DICT_BEFORE_PARAMS;

    return 0;
  }

  ++sfp->pos;

  if (parser_eof(sfp)) {
    return SFPARSE_ERR_PARSE;
  }

  if (*sfp->pos == '(') {
    if (dest) {
      dest->type = SFPARSE_TYPE_INNER_LIST;
      dest->flags = SFPARSE_VALUE_FLAG_NONE;
    }

    ++sfp->pos;

    sfp->state = SFPARSE_STATE_DICT_INNER_LIST_BEFORE;

    return 0;
  }

  rv = parser_bare_item(sfp, dest);
  if (rv != 0) {
    return rv;
  }

  sfp->state = SFPARSE_STATE_DICT_BEFORE_PARAMS;

  return 0;
}

int sfparse_parser_dict(sfparse_parser *sfp, sfparse_vec *dest_key,
                        sfparse_value *dest_value) {
  int rv;

  switch (sfp->state) {
  case SFPARSE_STATE_DICT_INNER_LIST_BEFORE:
    rv = parser_skip_inner_list(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_DICT_BEFORE_PARAMS:
    rv = parser_skip_params(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_DICT_AFTER:
    rv = parser_next_key_or_item(sfp);
    if (rv != 0) {
      return rv;
    }

    break;
  case SFPARSE_STATE_INITIAL:
    parser_discard_sp(sfp);

    if (parser_eof(sfp)) {
      return SFPARSE_ERR_EOF;
    }

    break;
  default:
    assert(0);
    abort();
  }

  rv = parser_key(sfp, dest_key);
  if (rv != 0) {
    return rv;
  }

  return parser_dict_value(sfp, dest_value);
}

int sfparse_parser_list(sfparse_parser *sfp, sfparse_value *dest) {
  int rv;

  switch (sfp->state) {
  case SFPARSE_STATE_LIST_INNER_LIST_BEFORE:
    rv = parser_skip_inner_list(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_LIST_BEFORE_PARAMS:
    rv = parser_skip_params(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_LIST_AFTER:
    rv = parser_next_key_or_item(sfp);
    if (rv != 0) {
      return rv;
    }

    break;
  case SFPARSE_STATE_INITIAL:
    parser_discard_sp(sfp);

    if (parser_eof(sfp)) {
      return SFPARSE_ERR_EOF;
    }

    break;
  default:
    assert(0);
    abort();
  }

  if (*sfp->pos == '(') {
    if (dest) {
      dest->type = SFPARSE_TYPE_INNER_LIST;
      dest->flags = SFPARSE_VALUE_FLAG_NONE;
    }

    ++sfp->pos;

    sfp->state = SFPARSE_STATE_LIST_INNER_LIST_BEFORE;

    return 0;
  }

  rv = parser_bare_item(sfp, dest);
  if (rv != 0) {
    return rv;
  }

  sfp->state = SFPARSE_STATE_LIST_BEFORE_PARAMS;

  return 0;
}

int sfparse_parser_item(sfparse_parser *sfp, sfparse_value *dest) {
  int rv;

  switch (sfp->state) {
  case SFPARSE_STATE_INITIAL:
    parser_discard_sp(sfp);

    if (parser_eof(sfp)) {
      return SFPARSE_ERR_PARSE;
    }

    break;
  case SFPARSE_STATE_ITEM_INNER_LIST_BEFORE:
    rv = parser_skip_inner_list(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_ITEM_BEFORE_PARAMS:
    rv = parser_skip_params(sfp);
    if (rv != 0) {
      return rv;
    }

    /* fall through */
  case SFPARSE_STATE_ITEM_AFTER:
    parser_discard_sp(sfp);

    if (!parser_eof(sfp)) {
      return SFPARSE_ERR_PARSE;
    }

    return SFPARSE_ERR_EOF;
  default:
    assert(0);
    abort();
  }

  if (*sfp->pos == '(') {
    if (dest) {
      dest->type = SFPARSE_TYPE_INNER_LIST;
      dest->flags = SFPARSE_VALUE_FLAG_NONE;
    }

    ++sfp->pos;

    sfp->state = SFPARSE_STATE_ITEM_INNER_LIST_BEFORE;

    return 0;
  }

  rv = parser_bare_item(sfp, dest);
  if (rv != 0) {
    return rv;
  }

  sfp->state = SFPARSE_STATE_ITEM_BEFORE_PARAMS;

  return 0;
}

void sfparse_parser_init(sfparse_parser *sfp, const uint8_t *data,
                         size_t datalen) {
  if (datalen == 0) {
    sfp->pos = sfp->end = NULL;
  } else {
    sfp->pos = data;
    sfp->end = data + datalen;
  }

  sfp->state = SFPARSE_STATE_INITIAL;
}

void sfparse_unescape(sfparse_vec *dest, const sfparse_vec *src) {
  const uint8_t *p, *q;
  uint8_t *o;
  size_t len, slen;

  if (src->len == 0) {
    dest->len = 0;

    return;
  }

  o = dest->base;
  p = src->base;
  len = src->len;

  for (;;) {
    q = memchr(p, '\\', len);
    if (q == NULL) {
      memcpy(o, p, len);
      o += len;

      dest->len = (size_t)(o - dest->base);

      return;
    }

    slen = (size_t)(q - p);
    memcpy(o, p, slen);
    o += slen;

    p = q + 1;
    *o++ = *p++;
    len -= slen + 2;
  }
}

void sfparse_base64decode(sfparse_vec *dest, const sfparse_vec *src) {
  static const int index_tbl[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};
  uint8_t *o;
  const uint8_t *p, *end;
  uint32_t n;
  size_t i, left;
  int idx;

  if (src->len == 0) {
    dest->len = 0;

    return;
  }

  o = dest->base;
  p = src->base;
  left = src->len & 0x3;
  if (left == 0 && src->base[src->len - 1] == '=') {
    left = 4;
  }
  end = src->base + src->len - left;

  for (; p != end;) {
    n = 0;

    for (i = 1; i <= 4; ++i, ++p) {
      idx = index_tbl[*p];

      assert(idx != -1);

      n += (uint32_t)(idx << (24 - i * 6));
    }

    *o++ = (uint8_t)(n >> 16);
    *o++ = (n >> 8) & 0xffu;
    *o++ = n & 0xffu;
  }

  switch (left) {
  case 0:
    goto fin;
  case 1:
    assert(0);
    abort();
  case 3:
    if (src->base[src->len - 1] == '=') {
      left = 2;
    }

    break;
  case 4:
    assert('=' == src->base[src->len - 1]);

    if (src->base[src->len - 2] == '=') {
      left = 2;
    } else {
      left = 3;
    }

    break;
  }

  switch (left) {
  case 2:
    *o = (uint8_t)(index_tbl[*p++] << 2);
    *o++ |= (uint8_t)(index_tbl[*p++] >> 4);

    break;
  case 3:
    n = (uint32_t)(index_tbl[*p++] << 10);
    n += (uint32_t)(index_tbl[*p++] << 4);
    n += (uint32_t)(index_tbl[*p++] >> 2);
    *o++ = (n >> 8) & 0xffu;
    *o++ = n & 0xffu;

    break;
  }

fin:
  dest->len = (size_t)(o - dest->base);
}

void sfparse_pctdecode(sfparse_vec *dest, const sfparse_vec *src) {
  const uint8_t *p, *q;
  uint8_t *o;
  size_t len, slen;

  if (src->len == 0) {
    dest->len = 0;

    return;
  }

  o = dest->base;
  p = src->base;
  len = src->len;

  for (;;) {
    q = memchr(p, '%', len);
    if (q == NULL) {
      memcpy(o, p, len);
      o += len;

      dest->len = (size_t)(o - dest->base);

      return;
    }

    slen = (size_t)(q - p);
    memcpy(o, p, slen);
    o += slen;

    p = q + 1;

    pctdecode(o++, &p);

    len -= slen + 3;
  }
}

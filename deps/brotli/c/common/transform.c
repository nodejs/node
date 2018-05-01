/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "./transform.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* RFC 7932 transforms string data */
static const char kPrefixSuffix[217] =
      "\1 \2, \10 of the \4 of \2s \1.\5 and \4 "
/* 0x  _0 _2  __5        _E    _3  _6 _8     _E */
      "in \1\"\4 to \2\">\1\n\2. \1]\5 for \3 a \6 "
/* 2x     _3_ _5    _A_  _D_ _F  _2 _4     _A   _E */
      "that \1\'\6 with \6 from \4 by \1(\6. T"
/* 4x       _5_ _7      _E      _5    _A _C */
      "he \4 on \4 as \4 is \4ing \2\n\t\1:\3ed "
/* 6x     _3    _8    _D    _2    _7_ _ _A _C */
      "\2=\"\4 at \3ly \1,\2=\'\5.com/\7. This \5"
/* 8x  _0 _ _3    _8   _C _E _ _1     _7       _F */
      " not \3er \3al \4ful \4ive \5less \4es"
/* Ax       _5   _9   _D    _2    _7     _D */
      "t \4ize \2\xc2\xa0\4ous \5 the \2e \0";
/* Cx    _2    _7___ ___ _A    _F     _5  _8 */

static const uint16_t kPrefixSuffixMap[50] = {
  0x00, 0x02, 0x05, 0x0E, 0x13, 0x16, 0x18, 0x1E, 0x23, 0x25,
  0x2A, 0x2D, 0x2F, 0x32, 0x34, 0x3A, 0x3E, 0x45, 0x47, 0x4E,
  0x55, 0x5A, 0x5C, 0x63, 0x68, 0x6D, 0x72, 0x77, 0x7A, 0x7C,
  0x80, 0x83, 0x88, 0x8C, 0x8E, 0x91, 0x97, 0x9F, 0xA5, 0xA9,
  0xAD, 0xB2, 0xB7, 0xBD, 0xC2, 0xC7, 0xCA, 0xCF, 0xD5, 0xD8
};

/* RFC 7932 transforms */
static const uint8_t kTransformsData[] = {
  49, BROTLI_TRANSFORM_IDENTITY, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 0,
   0, BROTLI_TRANSFORM_IDENTITY, 0,
  49, BROTLI_TRANSFORM_OMIT_FIRST_1, 49,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 47,
   0, BROTLI_TRANSFORM_IDENTITY, 49,
   4, BROTLI_TRANSFORM_IDENTITY, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 3,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 6,
  49, BROTLI_TRANSFORM_OMIT_FIRST_2, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_1, 49,
   1, BROTLI_TRANSFORM_IDENTITY, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 1,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 7,
  49, BROTLI_TRANSFORM_IDENTITY, 9,
  48, BROTLI_TRANSFORM_IDENTITY, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 8,
  49, BROTLI_TRANSFORM_IDENTITY, 5,
  49, BROTLI_TRANSFORM_IDENTITY, 10,
  49, BROTLI_TRANSFORM_IDENTITY, 11,
  49, BROTLI_TRANSFORM_OMIT_LAST_3, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 13,
  49, BROTLI_TRANSFORM_IDENTITY, 14,
  49, BROTLI_TRANSFORM_OMIT_FIRST_3, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_2, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 15,
  49, BROTLI_TRANSFORM_IDENTITY, 16,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 12,
   5, BROTLI_TRANSFORM_IDENTITY, 49,
   0, BROTLI_TRANSFORM_IDENTITY, 1,
  49, BROTLI_TRANSFORM_OMIT_FIRST_4, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 18,
  49, BROTLI_TRANSFORM_IDENTITY, 17,
  49, BROTLI_TRANSFORM_IDENTITY, 19,
  49, BROTLI_TRANSFORM_IDENTITY, 20,
  49, BROTLI_TRANSFORM_OMIT_FIRST_5, 49,
  49, BROTLI_TRANSFORM_OMIT_FIRST_6, 49,
  47, BROTLI_TRANSFORM_IDENTITY, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_4, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 22,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 23,
  49, BROTLI_TRANSFORM_IDENTITY, 24,
  49, BROTLI_TRANSFORM_IDENTITY, 25,
  49, BROTLI_TRANSFORM_OMIT_LAST_7, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_1, 26,
  49, BROTLI_TRANSFORM_IDENTITY, 27,
  49, BROTLI_TRANSFORM_IDENTITY, 28,
   0, BROTLI_TRANSFORM_IDENTITY, 12,
  49, BROTLI_TRANSFORM_IDENTITY, 29,
  49, BROTLI_TRANSFORM_OMIT_FIRST_9, 49,
  49, BROTLI_TRANSFORM_OMIT_FIRST_7, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_6, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 21,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 1,
  49, BROTLI_TRANSFORM_OMIT_LAST_8, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 31,
  49, BROTLI_TRANSFORM_IDENTITY, 32,
  47, BROTLI_TRANSFORM_IDENTITY, 3,
  49, BROTLI_TRANSFORM_OMIT_LAST_5, 49,
  49, BROTLI_TRANSFORM_OMIT_LAST_9, 49,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 1,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 8,
   5, BROTLI_TRANSFORM_IDENTITY, 21,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 0,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 10,
  49, BROTLI_TRANSFORM_IDENTITY, 30,
   0, BROTLI_TRANSFORM_IDENTITY, 5,
  35, BROTLI_TRANSFORM_IDENTITY, 49,
  47, BROTLI_TRANSFORM_IDENTITY, 2,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 17,
  49, BROTLI_TRANSFORM_IDENTITY, 36,
  49, BROTLI_TRANSFORM_IDENTITY, 33,
   5, BROTLI_TRANSFORM_IDENTITY, 0,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 21,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 5,
  49, BROTLI_TRANSFORM_IDENTITY, 37,
   0, BROTLI_TRANSFORM_IDENTITY, 30,
  49, BROTLI_TRANSFORM_IDENTITY, 38,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 0,
  49, BROTLI_TRANSFORM_IDENTITY, 39,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 49,
  49, BROTLI_TRANSFORM_IDENTITY, 34,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 8,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 12,
   0, BROTLI_TRANSFORM_IDENTITY, 21,
  49, BROTLI_TRANSFORM_IDENTITY, 40,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 12,
  49, BROTLI_TRANSFORM_IDENTITY, 41,
  49, BROTLI_TRANSFORM_IDENTITY, 42,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 17,
  49, BROTLI_TRANSFORM_IDENTITY, 43,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 5,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 10,
   0, BROTLI_TRANSFORM_IDENTITY, 34,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 33,
  49, BROTLI_TRANSFORM_IDENTITY, 44,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 5,
  45, BROTLI_TRANSFORM_IDENTITY, 49,
   0, BROTLI_TRANSFORM_IDENTITY, 33,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 30,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 30,
  49, BROTLI_TRANSFORM_IDENTITY, 46,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 1,
  49, BROTLI_TRANSFORM_UPPERCASE_FIRST, 34,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 33,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 30,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 1,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 33,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 21,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 12,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 5,
  49, BROTLI_TRANSFORM_UPPERCASE_ALL, 34,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 12,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 30,
   0, BROTLI_TRANSFORM_UPPERCASE_ALL, 34,
   0, BROTLI_TRANSFORM_UPPERCASE_FIRST, 34,
};

static BrotliTransforms kBrotliTransforms = {
  sizeof(kPrefixSuffix),
  (const uint8_t*)kPrefixSuffix,
  kPrefixSuffixMap,
  sizeof(kTransformsData) / (3 * sizeof(kTransformsData[0])),
  kTransformsData,
  {0, 12, 27, 23, 42, 63, 56, 48, 59, 64}
};

const BrotliTransforms* BrotliGetTransforms(void) {
  return &kBrotliTransforms;
}

static int ToUpperCase(uint8_t* p) {
  if (p[0] < 0xC0) {
    if (p[0] >= 'a' && p[0] <= 'z') {
      p[0] ^= 32;
    }
    return 1;
  }
  /* An overly simplified uppercasing model for UTF-8. */
  if (p[0] < 0xE0) {
    p[1] ^= 32;
    return 2;
  }
  /* An arbitrary transform for three byte characters. */
  p[2] ^= 5;
  return 3;
}

int BrotliTransformDictionaryWord(uint8_t* dst, const uint8_t* word, int len,
    const BrotliTransforms* transforms, int transform_idx) {
  int idx = 0;
  const uint8_t* prefix = BROTLI_TRANSFORM_PREFIX(transforms, transform_idx);
  uint8_t type = BROTLI_TRANSFORM_TYPE(transforms, transform_idx);
  const uint8_t* suffix = BROTLI_TRANSFORM_SUFFIX(transforms, transform_idx);
  {
    int prefix_len = *prefix++;
    while (prefix_len--) { dst[idx++] = *prefix++; }
  }
  {
    const int t = type;
    int i = 0;
    if (t <= BROTLI_TRANSFORM_OMIT_LAST_9) {
      len -= t;
    } else if (t >= BROTLI_TRANSFORM_OMIT_FIRST_1
        && t <= BROTLI_TRANSFORM_OMIT_FIRST_9) {
      int skip = t - (BROTLI_TRANSFORM_OMIT_FIRST_1 - 1);
      word += skip;
      len -= skip;
    }
    while (i < len) { dst[idx++] = word[i++]; }
    if (t == BROTLI_TRANSFORM_UPPERCASE_FIRST) {
      ToUpperCase(&dst[idx - len]);
    } else if (t == BROTLI_TRANSFORM_UPPERCASE_ALL) {
      uint8_t* uppercase = &dst[idx - len];
      while (len > 0) {
        int step = ToUpperCase(uppercase);
        uppercase += step;
        len -= step;
      }
    }
  }
  {
    int suffix_len = *suffix++;
    while (suffix_len--) { dst[idx++] = *suffix++; }
    return idx;
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

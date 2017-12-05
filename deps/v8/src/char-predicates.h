// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHAR_PREDICATES_H_
#define V8_CHAR_PREDICATES_H_

#include "src/globals.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {

// Unicode character predicates as defined by ECMA-262, 3rd,
// used for lexical analysis.

inline int AsciiAlphaToLower(uc32 c);
inline bool IsCarriageReturn(uc32 c);
inline bool IsLineFeed(uc32 c);
inline bool IsAsciiIdentifier(uc32 c);
inline bool IsAlphaNumeric(uc32 c);
inline bool IsDecimalDigit(uc32 c);
inline bool IsHexDigit(uc32 c);
inline bool IsOctalDigit(uc32 c);
inline bool IsBinaryDigit(uc32 c);
inline bool IsRegExpWord(uc32 c);
inline bool IsRegExpNewline(uc32 c);

// ES#sec-names-and-keywords
// This includes '_', '$' and '\', and ID_Start according to
// http://www.unicode.org/reports/tr31/, which consists of categories
// 'Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
#ifdef V8_INTL_SUPPORT
struct V8_EXPORT_PRIVATE IdentifierStart {
  static bool Is(uc32 c);
#else
struct IdentifierStart {
  // Non-BMP characters are not supported without I18N.
  static inline bool Is(uc32 c) {
    return (c <= 0xFFFF) ? unibrow::ID_Start::Is(c) : false;
  }
#endif
};

// ES#sec-names-and-keywords
// This includes \u200c and \u200d, and ID_Continue according to
// http://www.unicode.org/reports/tr31/, which consists of ID_Start,
// the categories 'Mn', 'Mc', 'Nd', 'Pc', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
#ifdef V8_INTL_SUPPORT
struct V8_EXPORT_PRIVATE IdentifierPart {
  static bool Is(uc32 c);
#else
struct IdentifierPart {
  static inline bool Is(uc32 c) {
    // Non-BMP charaacters are not supported without I18N.
    if (c <= 0xFFFF) {
      return unibrow::ID_Start::Is(c) || unibrow::ID_Continue::Is(c);
    }
    return false;
  }
#endif
};

// ES6 draft section 11.2
// This includes all code points of Unicode category 'Zs'.
// Further included are \u0009, \u000b, \u000c, and \ufeff.
#ifdef V8_INTL_SUPPORT
struct V8_EXPORT_PRIVATE WhiteSpace {
  static bool Is(uc32 c);
#else
struct WhiteSpace {
  static inline bool Is(uc32 c) { return unibrow::WhiteSpace::Is(c); }
#endif
};

// WhiteSpace and LineTerminator according to ES6 draft section 11.2 and 11.3
// This includes all the characters with Unicode category 'Z' (= Zs+Zl+Zp)
// as well as \u0009 - \u000d and \ufeff.
struct WhiteSpaceOrLineTerminator {
  static inline bool Is(uc32 c) {
    return WhiteSpace::Is(c) || unibrow::IsLineTerminator(c);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CHAR_PREDICATES_H_

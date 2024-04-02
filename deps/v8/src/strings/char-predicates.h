// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_CHAR_PREDICATES_H_
#define V8_STRINGS_CHAR_PREDICATES_H_

#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/strings/unicode.h"

namespace v8 {
namespace internal {

// Unicode character predicates as defined by ECMA-262, 3rd,
// used for lexical analysis.

inline constexpr int AsciiAlphaToLower(base::uc32 c);
inline constexpr bool IsCarriageReturn(base::uc32 c);
inline constexpr bool IsLineFeed(base::uc32 c);
inline constexpr bool IsAsciiIdentifier(base::uc32 c);
inline constexpr bool IsAlphaNumeric(base::uc32 c);
inline constexpr bool IsDecimalDigit(base::uc32 c);
inline constexpr bool IsHexDigit(base::uc32 c);
inline constexpr bool IsOctalDigit(base::uc32 c);
inline constexpr bool IsBinaryDigit(base::uc32 c);
inline constexpr bool IsRegExpWord(base::uc32 c);

inline constexpr bool IsAsciiLower(base::uc32 ch);
inline constexpr bool IsAsciiUpper(base::uc32 ch);

inline constexpr base::uc32 ToAsciiUpper(base::uc32 ch);
inline constexpr base::uc32 ToAsciiLower(base::uc32 ch);

// ES#sec-names-and-keywords
// This includes '_', '$' and '\', and ID_Start according to
// http://www.unicode.org/reports/tr31/, which consists of categories
// 'Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
inline bool IsIdentifierStart(base::uc32 c);
#ifdef V8_INTL_SUPPORT
V8_EXPORT_PRIVATE bool IsIdentifierStartSlow(base::uc32 c);
#else
inline bool IsIdentifierStartSlow(base::uc32 c) {
  // Non-BMP characters are not supported without I18N.
  return (c <= 0xFFFF) ? unibrow::ID_Start::Is(c) : false;
}
#endif

// ES#sec-names-and-keywords
// This includes \u200c and \u200d, and ID_Continue according to
// http://www.unicode.org/reports/tr31/, which consists of ID_Start,
// the categories 'Mn', 'Mc', 'Nd', 'Pc', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
inline bool IsIdentifierPart(base::uc32 c);
#ifdef V8_INTL_SUPPORT
V8_EXPORT_PRIVATE bool IsIdentifierPartSlow(base::uc32 c);
#else
inline bool IsIdentifierPartSlow(base::uc32 c) {
  // Non-BMP charaacters are not supported without I18N.
  if (c <= 0xFFFF) {
    return unibrow::ID_Start::Is(c) || unibrow::ID_Continue::Is(c);
  }
  return false;
}
#endif

// ES6 draft section 11.2
// This includes all code points of Unicode category 'Zs'.
// Further included are \u0009, \u000b, \u000c, and \ufeff.
inline bool IsWhiteSpace(base::uc32 c);
#ifdef V8_INTL_SUPPORT
V8_EXPORT_PRIVATE bool IsWhiteSpaceSlow(base::uc32 c);
#else
inline bool IsWhiteSpaceSlow(base::uc32 c) {
  return unibrow::WhiteSpace::Is(c);
}
#endif

// WhiteSpace and LineTerminator according to ES6 draft section 11.2 and 11.3
// This includes all the characters with Unicode category 'Z' (= Zs+Zl+Zp)
// as well as \u0009 - \u000d and \ufeff.
inline bool IsWhiteSpaceOrLineTerminator(base::uc32 c);
inline bool IsWhiteSpaceOrLineTerminatorSlow(base::uc32 c) {
  return IsWhiteSpaceSlow(c) || unibrow::IsLineTerminator(c);
}

inline bool IsLineTerminatorSequence(base::uc32 c, base::uc32 next);

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_CHAR_PREDICATES_H_

// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHAR_PREDICATES_H_
#define V8_CHAR_PREDICATES_H_

#include "src/unicode.h"

namespace v8 {
namespace internal {

// Unicode character predicates as defined by ECMA-262, 3rd,
// used for lexical analysis.

inline bool IsCarriageReturn(uc32 c);
inline bool IsLineFeed(uc32 c);
inline bool IsDecimalDigit(uc32 c);
inline bool IsHexDigit(uc32 c);
inline bool IsOctalDigit(uc32 c);
inline bool IsBinaryDigit(uc32 c);
inline bool IsRegExpWord(uc32 c);
inline bool IsRegExpNewline(uc32 c);


struct SupplementaryPlanes {
  static bool IsIDStart(uc32 c);
  static bool IsIDPart(uc32 c);
};


// ES6 draft section 11.6
// This includes '_', '$' and '\', and ID_Start according to
// http://www.unicode.org/reports/tr31/, which consists of categories
// 'Lu', 'Ll', 'Lt', 'Lm', 'Lo', 'Nl', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
// For code points in the SMPs, we can resort to ICU (if available).
struct IdentifierStart {
  static inline bool Is(uc32 c) {
    if (c > 0xFFFF) return SupplementaryPlanes::IsIDStart(c);
    return unibrow::ID_Start::Is(c);
  }
};


// ES6 draft section 11.6
// This includes \u200c and \u200d, and ID_Continue according to
// http://www.unicode.org/reports/tr31/, which consists of ID_Start,
// the categories 'Mn', 'Mc', 'Nd', 'Pc', but excluding properties
// 'Pattern_Syntax' or 'Pattern_White_Space'.
// For code points in the SMPs, we can resort to ICU (if available).
struct IdentifierPart {
  static inline bool Is(uc32 c) {
    if (c > 0xFFFF) return SupplementaryPlanes::IsIDPart(c);
    return unibrow::ID_Start::Is(c) || unibrow::ID_Continue::Is(c);
  }
};


// ES6 draft section 11.2
// This includes all code points of Unicode category 'Zs'.
// \u180e stops being one as of Unicode 6.3.0, but ES6 adheres to Unicode 5.1,
// so it is also included.
// Further included are \u0009, \u000b, \u0020, \u00a0, \u000c, and \ufeff.
// There are no category 'Zs' code points in the SMPs.
struct WhiteSpace {
  static inline bool Is(uc32 c) { return unibrow::WhiteSpace::Is(c); }
};


// WhiteSpace and LineTerminator according to ES6 draft section 11.2 and 11.3
// This consists of \000a, \000d, \u2028, and \u2029.
struct WhiteSpaceOrLineTerminator {
  static inline bool Is(uc32 c) {
    return WhiteSpace::Is(c) || unibrow::LineTerminator::Is(c);
  }
};

} }  // namespace v8::internal

#endif  // V8_CHAR_PREDICATES_H_

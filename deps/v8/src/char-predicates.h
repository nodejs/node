// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CHAR_PREDICATES_H_
#define V8_CHAR_PREDICATES_H_

#include "unicode.h"

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

struct IdentifierStart {
  static inline bool Is(uc32 c) {
    switch (c) {
      case '$': case '_': case '\\': return true;
      default: return unibrow::Letter::Is(c);
    }
  }
};


struct IdentifierPart {
  static inline bool Is(uc32 c) {
    return IdentifierStart::Is(c)
        || unibrow::Number::Is(c)
        || c == 0x200C  // U+200C is Zero-Width Non-Joiner.
        || c == 0x200D  // U+200D is Zero-Width Joiner.
        || unibrow::CombiningMark::Is(c)
        || unibrow::ConnectorPunctuation::Is(c);
  }
};


// WhiteSpace according to ECMA-262 5.1, 7.2.
struct WhiteSpace {
  static inline bool Is(uc32 c) {
    return c == 0x0009 ||  // <TAB>
           c == 0x000B ||  // <VT>
           c == 0x000C ||  // <FF>
           c == 0xFEFF ||  // <BOM>
           // \u0020 and \u00A0 are included in unibrow::WhiteSpace.
           unibrow::WhiteSpace::Is(c);
  }
};


// WhiteSpace and LineTerminator according to ECMA-262 5.1, 7.2 and 7.3.
struct WhiteSpaceOrLineTerminator {
  static inline bool Is(uc32 c) {
    return WhiteSpace::Is(c) || unibrow::LineTerminator::Is(c);
  }
};

} }  // namespace v8::internal

#endif  // V8_CHAR_PREDICATES_H_

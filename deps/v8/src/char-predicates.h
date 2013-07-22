// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

} }  // namespace v8::internal

#endif  // V8_CHAR_PREDICATES_H_

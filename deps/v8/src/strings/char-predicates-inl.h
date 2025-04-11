// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_CHAR_PREDICATES_INL_H_
#define V8_STRINGS_CHAR_PREDICATES_INL_H_

#include "src/strings/char-predicates.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/bounds.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// If c is in 'A'-'Z' or 'a'-'z', return its lower-case.
// Else, return something outside of 'A'-'Z' and 'a'-'z'.
// Note: it ignores LOCALE.
inline constexpr int AsciiAlphaToLower(base::uc32 c) { return c | 0x20; }

inline constexpr bool IsCarriageReturn(base::uc32 c) { return c == 0x000D; }

inline constexpr bool IsLineFeed(base::uc32 c) { return c == 0x000A; }

inline constexpr bool IsAsciiIdentifier(base::uc32 c) {
  return IsAlphaNumeric(c) || c == '$' || c == '_';
}

inline constexpr bool IsAlphaNumeric(base::uc32 c) {
  return base::IsInRange(AsciiAlphaToLower(c), 'a', 'z') || IsDecimalDigit(c);
}

inline constexpr bool IsDecimalDigit(base::uc32 c) {
  // ECMA-262, 3rd, 7.8.3 (p 16)
  return base::IsInRange(c, '0', '9');
}

inline constexpr bool IsHexDigit(base::uc32 c) {
  // ECMA-262, 3rd, 7.6 (p 15)
  return IsDecimalDigit(c) || base::IsInRange(AsciiAlphaToLower(c), 'a', 'f');
}

inline constexpr bool IsOctalDigit(base::uc32 c) {
  // ECMA-262, 6th, 7.8.3
  return base::IsInRange(c, '0', '7');
}

inline constexpr bool IsNonOctalDecimalDigit(base::uc32 c) {
  return base::IsInRange(c, '8', '9');
}

inline constexpr bool IsBinaryDigit(base::uc32 c) {
  // ECMA-262, 6th, 7.8.3
  return c == '0' || c == '1';
}

inline constexpr bool IsAscii(base::uc32 c) { return !(c & ~0x7F); }

inline constexpr bool IsAsciiLower(base::uc32 c) {
  return base::IsInRange(c, 'a', 'z');
}

inline constexpr bool IsAsciiUpper(base::uc32 c) {
  return base::IsInRange(c, 'A', 'Z');
}

inline constexpr base::uc32 ToAsciiUpper(base::uc32 c) {
  return c & ~(IsAsciiLower(c) << 5);
}

inline constexpr base::uc32 ToAsciiLower(base::uc32 c) {
  return c | (IsAsciiUpper(c) << 5);
}

inline constexpr bool IsRegExpWord(base::uc32 c) {
  return IsAlphaNumeric(c) || c == '_';
}

// Constexpr cache table for character flags.
enum OneByteCharFlags {
  kIsIdentifierStart = 1 << 0,
  kIsIdentifierPart = 1 << 1,
  kIsWhiteSpace = 1 << 2,
  kIsWhiteSpaceOrLineTerminator = 1 << 3,
  kMaybeLineEnd = 1 << 4
};

// See http://www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt
// ID_Start. Additionally includes '_' and '$'.
constexpr bool IsOneByteIDStart(base::uc32 c) {
  return c == 0x0024 || (c >= 0x0041 && c <= 0x005A) || c == 0x005F ||
         (c >= 0x0061 && c <= 0x007A) || c == 0x00AA || c == 0x00B5 ||
         c == 0x00BA || (c >= 0x00C0 && c <= 0x00D6) ||
         (c >= 0x00D8 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x00FF);
}

// See http://www.unicode.org/Public/UCD/latest/ucd/DerivedCoreProperties.txt
// ID_Continue. Additionally includes '_' and '$'.
constexpr bool IsOneByteIDContinue(base::uc32 c) {
  return c == 0x0024 || (c >= 0x0030 && c <= 0x0039) || c == 0x005F ||
         (c >= 0x0041 && c <= 0x005A) || (c >= 0x0061 && c <= 0x007A) ||
         c == 0x00AA || c == 0x00B5 || c == 0x00B7 || c == 0x00BA ||
         (c >= 0x00C0 && c <= 0x00D6) || (c >= 0x00D8 && c <= 0x00F6) ||
         (c >= 0x00F8 && c <= 0x00FF);
}

constexpr bool IsOneByteWhitespace(base::uc32 c) {
  return c == '\t' || c == '\v' || c == '\f' || c == ' ' || c == u'\xa0';
}

constexpr uint8_t BuildOneByteCharFlags(base::uc32 c) {
  uint8_t result = 0;
  if (IsOneByteIDStart(c) || c == '\\') result |= kIsIdentifierStart;
  if (IsOneByteIDContinue(c) || c == '\\') result |= kIsIdentifierPart;
  if (IsOneByteWhitespace(c)) {
    result |= kIsWhiteSpace | kIsWhiteSpaceOrLineTerminator;
  }
  if (c == '\r' || c == '\n') {
    result |= kIsWhiteSpaceOrLineTerminator | kMaybeLineEnd;
  }
  // Add markers to identify 0x2028 and 0x2029.
  if (c == static_cast<uint8_t>(0x2028) || c == static_cast<uint8_t>(0x2029)) {
    result |= kMaybeLineEnd;
  }
  return result;
}
const constexpr uint8_t kOneByteCharFlags[256] = {
#define BUILD_CHAR_FLAGS(N) BuildOneByteCharFlags(N),
    INT_0_TO_127_LIST(BUILD_CHAR_FLAGS)
#undef BUILD_CHAR_FLAGS
#define BUILD_CHAR_FLAGS(N) BuildOneByteCharFlags(N + 128),
        INT_0_TO_127_LIST(BUILD_CHAR_FLAGS)
#undef BUILD_CHAR_FLAGS
};

bool IsIdentifierStart(base::uc32 c) {
  if (!base::IsInRange(c, 0, 255)) return IsIdentifierStartSlow(c);
  DCHECK_EQ(IsIdentifierStartSlow(c),
            static_cast<bool>(kOneByteCharFlags[c] & kIsIdentifierStart));
  return kOneByteCharFlags[c] & kIsIdentifierStart;
}

bool IsIdentifierPart(base::uc32 c) {
  if (!base::IsInRange(c, 0, 255)) return IsIdentifierPartSlow(c);
  DCHECK_EQ(IsIdentifierPartSlow(c),
            static_cast<bool>(kOneByteCharFlags[c] & kIsIdentifierPart));
  return kOneByteCharFlags[c] & kIsIdentifierPart;
}

bool IsWhiteSpace(base::uc32 c) {
  if (!base::IsInRange(c, 0, 255)) return IsWhiteSpaceSlow(c);
  DCHECK_EQ(IsWhiteSpaceSlow(c),
            static_cast<bool>(kOneByteCharFlags[c] & kIsWhiteSpace));
  return kOneByteCharFlags[c] & kIsWhiteSpace;
}

bool IsWhiteSpaceOrLineTerminator(base::uc32 c) {
  if (!base::IsInRange(c, 0, 255)) return IsWhiteSpaceOrLineTerminatorSlow(c);
  DCHECK_EQ(
      IsWhiteSpaceOrLineTerminatorSlow(c),
      static_cast<bool>(kOneByteCharFlags[c] & kIsWhiteSpaceOrLineTerminator));
  return kOneByteCharFlags[c] & kIsWhiteSpaceOrLineTerminator;
}

bool IsLineTerminatorSequence(base::uc32 c, base::uc32 next) {
  if (kOneByteCharFlags[static_cast<uint8_t>(c)] & kMaybeLineEnd) {
    if (c == '\n') return true;
    if (c == '\r') return next != '\n';
    return base::IsInRange(static_cast<unsigned int>(c), 0x2028u, 0x2029u);
  }
  return false;
}

}  // namespace internal

}  // namespace v8

#endif  // V8_STRINGS_CHAR_PREDICATES_INL_H_

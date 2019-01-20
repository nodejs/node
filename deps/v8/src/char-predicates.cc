// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/char-predicates.h"

#include "unicode/uchar.h"
#include "unicode/urename.h"

namespace v8 {
namespace internal {

// ES#sec-names-and-keywords Names and Keywords
// UnicodeIDStart, '$', '_' and '\'
bool IsIdentifierStartSlow(uc32 c) {
  // cannot use u_isIDStart because it does not work for
  // Other_ID_Start characters.
  return u_hasBinaryProperty(c, UCHAR_ID_START) ||
         (c < 0x60 && (c == '$' || c == '\\' || c == '_'));
}

// ES#sec-names-and-keywords Names and Keywords
// UnicodeIDContinue, '$', '_', '\', ZWJ, and ZWNJ
bool IsIdentifierPartSlow(uc32 c) {
  // Can't use u_isIDPart because it does not work for
  // Other_ID_Continue characters.
  return u_hasBinaryProperty(c, UCHAR_ID_CONTINUE) ||
         (c < 0x60 && (c == '$' || c == '\\' || c == '_')) || c == 0x200C ||
         c == 0x200D;
}

// ES#sec-white-space White Space
// gC=Zs, U+0009, U+000B, U+000C, U+FEFF
bool IsWhiteSpaceSlow(uc32 c) {
  return (u_charType(c) == U_SPACE_SEPARATOR) ||
         (c < 0x0D && (c == 0x09 || c == 0x0B || c == 0x0C)) || c == 0xFEFF;
}

}  // namespace internal
}  // namespace v8

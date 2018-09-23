// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_SCANNER_INL_H_
#define V8_PARSING_SCANNER_INL_H_

#include "src/parsing/scanner.h"
#include "src/unicode-cache-inl.h"

namespace v8 {
namespace internal {

V8_INLINE Token::Value Scanner::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    // We won't skip behind the end of input.
    DCHECK(!unicode_cache_->IsWhiteSpace(kEndOfInput));

    // Advance as long as character is a WhiteSpace or LineTerminator.
    // Remember if the latter is the case.
    if (unibrow::IsLineTerminator(c0_)) {
      next().after_line_terminator = true;
    } else if (!unicode_cache_->IsWhiteSpace(c0_)) {
      break;
    }
    Advance();
  }

  // Return whether or not we skipped any characters.
  if (source_pos() == start_position) {
    DCHECK_NE('0', c0_);
    return Token::ILLEGAL;
  }

  return Token::WHITESPACE;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_SCANNER_INL_H_

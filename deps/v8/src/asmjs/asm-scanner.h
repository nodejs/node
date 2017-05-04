// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASMJS_ASM_SCANNER_H_
#define V8_ASMJS_ASM_SCANNER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "src/asmjs/asm-names.h"
#include "src/base/logging.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Utf16CharacterStream;

// A custom scanner to extract the token stream needed to parse valid
// asm.js: http://asmjs.org/spec/latest/
// This scanner intentionally avoids the portion of JavaScript lexing
// that are not required to determine if code is valid asm.js code.
// * Strings are disallowed except for 'use asm'.
// * Only the subset of keywords needed to check asm.js invariants are
//   included.
// * Identifiers are accumulated into local + global string tables
//   (for performance).
class V8_EXPORT_PRIVATE AsmJsScanner {
 public:
  typedef int32_t token_t;

  AsmJsScanner();
  // Pick the stream to parse (must be called before anything else).
  void SetStream(std::unique_ptr<Utf16CharacterStream> stream);

  // Get current token.
  token_t Token() const { return token_; }
  // Get position of current token.
  size_t Position() const { return position_; }
  // Advance to the next token.
  void Next();
  // Back up by one token.
  void Rewind();

  // Get raw string for current identifier. Note that the returned string will
  // become invalid when the scanner advances, create a copy to preserve it.
  const std::string& GetIdentifierString() const {
    // Identifier strings don't work after a rewind.
    DCHECK(!rewind_);
    return identifier_string_;
  }

  // Check if we just passed a newline.
  bool IsPrecededByNewline() const {
    // Newline tracking doesn't work if you back up.
    DCHECK(!rewind_);
    return preceded_by_newline_;
  }

#if DEBUG
  // Debug only method to go from a token back to its name.
  // Slow, only use for debugging.
  std::string Name(token_t token) const;
#endif

  // Restores old position (token after that position). Note that it is not
  // allowed to rewind right after a seek, because previous tokens are unknown.
  void Seek(size_t pos);

  // Select whether identifiers are resolved in global or local scope,
  // and which scope new identifiers are added to.
  void EnterLocalScope() { in_local_scope_ = true; }
  void EnterGlobalScope() { in_local_scope_ = false; }
  // Drop all current local identifiers.
  void ResetLocals();

  // Methods to check if a token is an identifier and which scope.
  bool IsLocal() const { return IsLocal(Token()); }
  bool IsGlobal() const { return IsGlobal(Token()); }
  static bool IsLocal(token_t token) { return token <= kLocalsStart; }
  static bool IsGlobal(token_t token) { return token >= kGlobalsStart; }
  // Methods to find the index position of an identifier (count starting from
  // 0 for each scope separately).
  static size_t LocalIndex(token_t token) {
    DCHECK(IsLocal(token));
    return -(token - kLocalsStart);
  }
  static size_t GlobalIndex(token_t token) {
    DCHECK(IsGlobal(token));
    return token - kGlobalsStart;
  }

  // Methods to check if the current token is a numeric literal considered an
  // asm.js "double" (contains a dot) or an "unsigned" (without a dot). Note
  // that numbers without a dot outside the [0 .. 2^32) range are errors.
  bool IsUnsigned() const { return Token() == kUnsigned; }
  uint32_t AsUnsigned() const {
    DCHECK(IsUnsigned());
    return unsigned_value_;
  }
  bool IsDouble() const { return Token() == kDouble; }
  double AsDouble() const {
    DCHECK(IsDouble());
    return double_value_;
  }

  // clang-format off
  enum {
    // [-10000-kMaxIdentifierCount, -10000)    :: Local identifiers (counting
    //                                            backwards)
    // [-10000 .. -1)                          :: Builtin tokens like keywords
    //                                            (also includes some special
    //                                             ones like end of input)
    // 0        .. 255                         :: Single char tokens
    // 256      .. 256+kMaxIdentifierCount     :: Global identifiers
    kLocalsStart = -10000,
#define V(name, _junk1, _junk2, _junk3) kToken_##name,
    STDLIB_MATH_FUNCTION_LIST(V)
    STDLIB_ARRAY_TYPE_LIST(V)
#undef V
#define V(name, _junk1) kToken_##name,
    STDLIB_MATH_VALUE_LIST(V)
#undef V
#define V(name) kToken_##name,
    STDLIB_OTHER_LIST(V)
    KEYWORD_NAME_LIST(V)
#undef V
#define V(rawname, name) kToken_##name,
    LONG_SYMBOL_NAME_LIST(V)
#undef V
#define V(name, value, string_name) name = value,
    SPECIAL_TOKEN_LIST(V)
#undef V
    kGlobalsStart = 256,
  };
  // clang-format on

 private:
  std::unique_ptr<Utf16CharacterStream> stream_;
  token_t token_;
  token_t preceding_token_;
  token_t next_token_;         // Only set when in {rewind} state.
  size_t position_;            // Corresponds to {token} position.
  size_t preceding_position_;  // Corresponds to {preceding_token} position.
  size_t next_position_;       // Only set when in {rewind} state.
  bool rewind_;
  std::string identifier_string_;
  bool in_local_scope_;
  std::unordered_map<std::string, token_t> local_names_;
  std::unordered_map<std::string, token_t> global_names_;
  std::unordered_map<std::string, token_t> property_names_;
  int global_count_;
  double double_value_;
  uint32_t unsigned_value_;
  bool preceded_by_newline_;

  // Consume multiple characters.
  void ConsumeIdentifier(uc32 ch);
  void ConsumeNumber(uc32 ch);
  bool ConsumeCComment();
  void ConsumeCPPComment();
  void ConsumeString(uc32 quote);
  void ConsumeCompareOrShift(uc32 ch);

  // Classify character categories.
  bool IsIdentifierStart(uc32 ch);
  bool IsIdentifierPart(uc32 ch);
  bool IsNumberStart(uc32 ch);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ASMJS_ASM_SCANNER_H_

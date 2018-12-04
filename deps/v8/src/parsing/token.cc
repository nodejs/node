// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "src/parsing/token.h"

namespace v8 {
namespace internal {

#define T(name, string, precedence) #name,
const char* const Token::name_[NUM_TOKENS] = {TOKEN_LIST(T, T, T)};
#undef T


#define T(name, string, precedence) string,
const char* const Token::string_[NUM_TOKENS] = {TOKEN_LIST(T, T, T)};
#undef T

constexpr uint8_t length(const char* str) {
  return str ? static_cast<uint8_t>(strlen(str)) : 0;
}
#define T(name, string, precedence) length(string),
const uint8_t Token::string_length_[NUM_TOKENS] = {TOKEN_LIST(T, T, T)};
#undef T

#define T(name, string, precedence) precedence,
const int8_t Token::precedence_[NUM_TOKENS] = {TOKEN_LIST(T, T, T)};
#undef T

#define KT(a, b, c) 'T',
#define KK(a, b, c) 'K',
#define KC(a, b, c) 'C',
const char Token::token_type[] = {TOKEN_LIST(KT, KK, KC)};
#undef KT
#undef KK
#undef KC

}  // namespace internal
}  // namespace v8

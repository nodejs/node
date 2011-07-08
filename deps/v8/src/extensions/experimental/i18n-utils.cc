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

#include "src/extensions/experimental/i18n-utils.h"

#include <string.h>

#include "unicode/unistr.h"

namespace v8 {
namespace internal {

// static
void I18NUtils::StrNCopy(char* dest, int length, const char* src) {
  if (!dest || !src) return;

  strncpy(dest, src, length);
  dest[length - 1] = '\0';
}

// static
bool I18NUtils::ExtractStringSetting(const v8::Handle<v8::Object>& settings,
                                     const char* setting,
                                     icu::UnicodeString* result) {
  if (!setting || !result) return false;

  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Value> value = settings->Get(v8::String::New(setting));
  if (try_catch.HasCaught()) {
    return false;
  }
  // No need to check if |value| is empty because it's taken care of
  // by TryCatch above.
  if (!value->IsUndefined() && !value->IsNull() && value->IsString()) {
    v8::String::Utf8Value utf8_value(value);
    if (*utf8_value == NULL) return false;
    result->setTo(icu::UnicodeString::fromUTF8(*utf8_value));
    return true;
  }
  return false;
}

// static
void I18NUtils::AsciiToUChar(const char* source,
                             int32_t source_length,
                             UChar* target,
                             int32_t target_length) {
  int32_t length =
      source_length < target_length ? source_length : target_length;

  if (length <= 0) {
    return;
  }

  for (int32_t i = 0; i < length - 1; ++i) {
    target[i] = static_cast<UChar>(source[i]);
  }

  target[length - 1] = 0x0u;
}

} }  // namespace v8::internal

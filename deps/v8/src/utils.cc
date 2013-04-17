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

#include <stdarg.h>
#include "../include/v8stdint.h"
#include "checks.h"
#include "platform.h"
#include "utils.h"

namespace v8 {
namespace internal {


SimpleStringBuilder::SimpleStringBuilder(int size) {
  buffer_ = Vector<char>::New(size);
  position_ = 0;
}


void SimpleStringBuilder::AddString(const char* s) {
  AddSubstring(s, StrLength(s));
}


void SimpleStringBuilder::AddSubstring(const char* s, int n) {
  ASSERT(!is_finalized() && position_ + n <= buffer_.length());
  ASSERT(static_cast<size_t>(n) <= strlen(s));
  OS::MemCopy(&buffer_[position_], s, n * kCharSize);
  position_ += n;
}


void SimpleStringBuilder::AddPadding(char c, int count) {
  for (int i = 0; i < count; i++) {
    AddCharacter(c);
  }
}


void SimpleStringBuilder::AddDecimalInteger(int32_t value) {
  uint32_t number = static_cast<uint32_t>(value);
  if (value < 0) {
    AddCharacter('-');
    number = static_cast<uint32_t>(-value);
  }
  int digits = 1;
  for (uint32_t factor = 10; digits < 10; digits++, factor *= 10) {
    if (factor > number) break;
  }
  position_ += digits;
  for (int i = 1; i <= digits; i++) {
    buffer_[position_ - i] = '0' + static_cast<char>(number % 10);
    number /= 10;
  }
}


char* SimpleStringBuilder::Finalize() {
  ASSERT(!is_finalized() && position_ <= buffer_.length());
  // If there is no space for null termination, overwrite last character.
  if (position_ == buffer_.length()) {
    position_--;
    // Print ellipsis.
    for (int i = 3; i > 0 && position_ > i; --i) buffer_[position_ - i] = '.';
  }
  buffer_[position_] = '\0';
  // Make sure nobody managed to add a 0-character to the
  // buffer while building the string.
  ASSERT(strlen(buffer_.start()) == static_cast<size_t>(position_));
  position_ = -1;
  ASSERT(is_finalized());
  return buffer_.start();
}


const DivMagicNumbers DivMagicNumberFor(int32_t divisor) {
  switch (divisor) {
    case 3:    return DivMagicNumberFor3;
    case 5:    return DivMagicNumberFor5;
    case 7:    return DivMagicNumberFor7;
    case 9:    return DivMagicNumberFor9;
    case 11:   return DivMagicNumberFor11;
    case 25:   return DivMagicNumberFor25;
    case 125:  return DivMagicNumberFor125;
    case 625:  return DivMagicNumberFor625;
    default:   return InvalidDivMagicNumber;
  }
}

} }  // namespace v8::internal

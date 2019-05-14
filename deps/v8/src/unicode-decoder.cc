// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/unicode-inl.h"
#include "src/unicode-decoder.h"
#include <stdio.h>
#include <stdlib.h>

namespace unibrow {

uint16_t Utf8Iterator::operator*() {
  if (V8_UNLIKELY(char_ > Utf16::kMaxNonSurrogateCharCode)) {
    return trailing_ ? Utf16::TrailSurrogate(char_)
                     : Utf16::LeadSurrogate(char_);
  }

  DCHECK_EQ(trailing_, false);
  return char_;
}

Utf8Iterator& Utf8Iterator::operator++() {
  if (V8_UNLIKELY(this->Done())) {
    char_ = Utf8::kBufferEmpty;
    return *this;
  }

  if (V8_UNLIKELY(char_ > Utf16::kMaxNonSurrogateCharCode && !trailing_)) {
    trailing_ = true;
    return *this;
  }

  trailing_ = false;
  offset_ = cursor_;

  char_ =
      Utf8::ValueOf(reinterpret_cast<const uint8_t*>(stream_.begin()) + cursor_,
                    stream_.length() - cursor_, &cursor_);
  return *this;
}

Utf8Iterator Utf8Iterator::operator++(int) {
  Utf8Iterator old(*this);
  ++*this;
  return old;
}

bool Utf8Iterator::Done() {
  return offset_ == static_cast<size_t>(stream_.length());
}

void Utf8DecoderBase::Reset(uint16_t* buffer, size_t buffer_length,
                            const v8::internal::Vector<const char>& stream) {
  size_t utf16_length = 0;

  Utf8Iterator it = Utf8Iterator(stream);
  // Loop until stream is read, writing to buffer as long as buffer has space.
  while (utf16_length < buffer_length && !it.Done()) {
    *buffer++ = *it;
    ++it;
    utf16_length++;
  }
  bytes_read_ = it.Offset();
  trailing_ = it.Trailing();
  chars_written_ = utf16_length;

  // Now that writing to buffer is done, we just need to calculate utf16_length
  while (!it.Done()) {
    ++it;
    utf16_length++;
  }
  utf16_length_ = utf16_length;
}

void Utf8DecoderBase::WriteUtf16Slow(
    uint16_t* data, size_t length,
    const v8::internal::Vector<const char>& stream, size_t offset,
    bool trailing) {
  Utf8Iterator it = Utf8Iterator(stream, offset, trailing);
  while (!it.Done()) {
    DCHECK_GT(length--, 0);
    *data++ = *it;
    ++it;
  }
}

}  // namespace unibrow

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/strings/unicode-decoder.h"

#include "src/strings/unicode-inl.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

Utf8Decoder::Utf8Decoder(const base::Vector<const uint8_t>& chars)
    : encoding_(Encoding::kAscii),
      non_ascii_start_(NonAsciiStart(chars.begin(), chars.length())),
      utf16_length_(non_ascii_start_) {
  if (non_ascii_start_ == chars.length()) return;

  const uint8_t* cursor = chars.begin() + non_ascii_start_;
  const uint8_t* end = chars.begin() + chars.length();

  bool is_one_byte = true;
  uint32_t incomplete_char = 0;
  unibrow::Utf8::State state = unibrow::Utf8::State::kAccept;

  while (cursor < end) {
    unibrow::uchar t =
        unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
    if (t != unibrow::Utf8::kIncomplete) {
      is_one_byte = is_one_byte && t <= unibrow::Latin1::kMaxChar;
      utf16_length_++;
      if (t > unibrow::Utf16::kMaxNonSurrogateCharCode) utf16_length_++;
    }
  }

  unibrow::uchar t = unibrow::Utf8::ValueOfIncrementalFinish(&state);
  if (t != unibrow::Utf8::kBufferEmpty) {
    is_one_byte = false;
    utf16_length_++;
  }

  encoding_ = is_one_byte ? Encoding::kLatin1 : Encoding::kUtf16;
}

template <typename Char>
void Utf8Decoder::Decode(Char* out, const base::Vector<const uint8_t>& data) {
  CopyChars(out, data.begin(), non_ascii_start_);

  out += non_ascii_start_;

  uint32_t incomplete_char = 0;
  unibrow::Utf8::State state = unibrow::Utf8::State::kAccept;

  const uint8_t* cursor = data.begin() + non_ascii_start_;
  const uint8_t* end = data.begin() + data.length();

  while (cursor < end) {
    unibrow::uchar t =
        unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
    if (t != unibrow::Utf8::kIncomplete) {
      if (sizeof(Char) == 1 || t <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
        *(out++) = static_cast<Char>(t);
      } else {
        *(out++) = unibrow::Utf16::LeadSurrogate(t);
        *(out++) = unibrow::Utf16::TrailSurrogate(t);
      }
    }
  }

  unibrow::uchar t = unibrow::Utf8::ValueOfIncrementalFinish(&state);
  if (t != unibrow::Utf8::kBufferEmpty) *out = static_cast<Char>(t);
}

template V8_EXPORT_PRIVATE void Utf8Decoder::Decode(
    uint8_t* out, const base::Vector<const uint8_t>& data);

template V8_EXPORT_PRIVATE void Utf8Decoder::Decode(
    uint16_t* out, const base::Vector<const uint8_t>& data);

}  // namespace internal
}  // namespace v8

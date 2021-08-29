// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_UNICODE_DECODER_H_
#define V8_STRINGS_UNICODE_DECODER_H_

#include "src/base/vector.h"
#include "src/strings/unicode.h"

namespace v8 {
namespace internal {

// The return value may point to the first aligned word containing the first
// non-one-byte character, rather than directly to the non-one-byte character.
// If the return value is >= the passed length, the entire string was
// one-byte.
inline int NonAsciiStart(const uint8_t* chars, int length) {
  const uint8_t* start = chars;
  const uint8_t* limit = chars + length;

  if (static_cast<size_t>(length) >= kIntptrSize) {
    // Check unaligned bytes.
    while (!IsAligned(reinterpret_cast<intptr_t>(chars), kIntptrSize)) {
      if (*chars > unibrow::Utf8::kMaxOneByteChar) {
        return static_cast<int>(chars - start);
      }
      ++chars;
    }
    // Check aligned words.
    DCHECK_EQ(unibrow::Utf8::kMaxOneByteChar, 0x7F);
    const uintptr_t non_one_byte_mask = kUintptrAllBitsSet / 0xFF * 0x80;
    while (chars + sizeof(uintptr_t) <= limit) {
      if (*reinterpret_cast<const uintptr_t*>(chars) & non_one_byte_mask) {
        return static_cast<int>(chars - start);
      }
      chars += sizeof(uintptr_t);
    }
  }
  // Check remaining unaligned bytes.
  while (chars < limit) {
    if (*chars > unibrow::Utf8::kMaxOneByteChar) {
      return static_cast<int>(chars - start);
    }
    ++chars;
  }

  return static_cast<int>(chars - start);
}

class V8_EXPORT_PRIVATE Utf8Decoder final {
 public:
  enum class Encoding : uint8_t { kAscii, kLatin1, kUtf16 };

  explicit Utf8Decoder(const base::Vector<const uint8_t>& chars);

  bool is_ascii() const { return encoding_ == Encoding::kAscii; }
  bool is_one_byte() const { return encoding_ <= Encoding::kLatin1; }
  int utf16_length() const { return utf16_length_; }
  int non_ascii_start() const { return non_ascii_start_; }

  template <typename Char>
  V8_EXPORT_PRIVATE void Decode(Char* out,
                                const base::Vector<const uint8_t>& data);

 private:
  Encoding encoding_;
  int non_ascii_start_;
  int utf16_length_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_UNICODE_DECODER_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_DECODER_H_
#define V8_UNICODE_DECODER_H_

#include <sys/types.h>
#include <algorithm>
#include "src/globals.h"
#include "src/unicode.h"
#include "src/utils.h"
#include "src/vector.h"

namespace unibrow {

class Utf8Iterator {
 public:
  explicit Utf8Iterator(const v8::internal::Vector<const char>& stream)
      : Utf8Iterator(stream, 0, false) {}
  Utf8Iterator(const v8::internal::Vector<const char>& stream, size_t offset,
               bool trailing)
      : stream_(stream),
        cursor_(offset),
        offset_(0),
        char_(0),
        trailing_(false) {
    DCHECK_LE(offset, stream.length());
    // Read the first char, setting offset_ to offset in the process.
    ++*this;

    // This must be set after reading the first char, since the offset marks
    // the start of the octet sequence that the trailing char is part of.
    trailing_ = trailing;
    if (trailing) {
      DCHECK_GT(char_, Utf16::kMaxNonSurrogateCharCode);
    }
  }

  uint16_t operator*();
  Utf8Iterator& operator++();
  Utf8Iterator operator++(int);
  bool Done();
  bool Trailing() { return trailing_; }
  size_t Offset() { return offset_; }

 private:
  const v8::internal::Vector<const char>& stream_;
  size_t cursor_;
  size_t offset_;
  uint32_t char_;
  bool trailing_;
};

class V8_EXPORT_PRIVATE Utf8DecoderBase {
 public:
  // Initialization done in subclass.
  inline Utf8DecoderBase();
  inline Utf8DecoderBase(uint16_t* buffer, size_t buffer_length,
                         const v8::internal::Vector<const char>& stream);
  inline size_t Utf16Length() const { return utf16_length_; }

 protected:
  // This reads all characters and sets the utf16_length_.
  // The first buffer_length utf16 chars are cached in the buffer.
  void Reset(uint16_t* buffer, size_t buffer_length,
             const v8::internal::Vector<const char>& vector);
  static void WriteUtf16Slow(uint16_t* data, size_t length,
                             const v8::internal::Vector<const char>& stream,
                             size_t offset, bool trailing);

  size_t bytes_read_;
  size_t chars_written_;
  size_t utf16_length_;
  bool trailing_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Utf8DecoderBase);
};

template <size_t kBufferSize>
class Utf8Decoder : public Utf8DecoderBase {
 public:
  inline Utf8Decoder() = default;
  explicit inline Utf8Decoder(const v8::internal::Vector<const char>& stream);
  inline void Reset(const v8::internal::Vector<const char>& stream);
  inline size_t WriteUtf16(
      uint16_t* data, size_t length,
      const v8::internal::Vector<const char>& stream) const;

 private:
  uint16_t buffer_[kBufferSize];
};

Utf8DecoderBase::Utf8DecoderBase()
    : bytes_read_(0), chars_written_(0), utf16_length_(0), trailing_(false) {}

Utf8DecoderBase::Utf8DecoderBase(
    uint16_t* buffer, size_t buffer_length,
    const v8::internal::Vector<const char>& stream) {
  Reset(buffer, buffer_length, stream);
}

template <size_t kBufferSize>
Utf8Decoder<kBufferSize>::Utf8Decoder(
    const v8::internal::Vector<const char>& stream)
    : Utf8DecoderBase(buffer_, kBufferSize, stream) {}

template <size_t kBufferSize>
void Utf8Decoder<kBufferSize>::Reset(
    const v8::internal::Vector<const char>& stream) {
  Utf8DecoderBase::Reset(buffer_, kBufferSize, stream);
}

template <size_t kBufferSize>
size_t Utf8Decoder<kBufferSize>::WriteUtf16(
    uint16_t* data, size_t data_length,
    const v8::internal::Vector<const char>& stream) const {
  DCHECK_GT(data_length, 0);
  data_length = std::min(data_length, utf16_length_);

  // memcpy everything in buffer.
  size_t memcpy_length = std::min(data_length, chars_written_);
  v8::internal::MemCopy(data, buffer_, memcpy_length * sizeof(uint16_t));

  if (data_length <= chars_written_) return data_length;

  // Copy the rest the slow way.
  WriteUtf16Slow(data + chars_written_, data_length - chars_written_, stream,
                 bytes_read_, trailing_);
  return data_length;
}

class Latin1 {
 public:
  static const unsigned kMaxChar = 0xff;
  // Convert the character to Latin-1 case equivalent if possible.
  static inline uint16_t TryConvertToLatin1(uint16_t);
};

uint16_t Latin1::TryConvertToLatin1(uint16_t c) {
  switch (c) {
    // This are equivalent characters in unicode.
    case 0x39c:
    case 0x3bc:
      return 0xb5;
    // This is an uppercase of a Latin-1 character
    // outside of Latin-1.
    case 0x178:
      return 0xff;
  }
  return c;
}


}  // namespace unibrow

#endif  // V8_UNICODE_DECODER_H_

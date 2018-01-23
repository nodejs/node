// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_DECODER_H_
#define V8_UNICODE_DECODER_H_

#include <sys/types.h>
#include "src/globals.h"
#include "src/utils.h"

namespace unibrow {

class V8_EXPORT_PRIVATE Utf8DecoderBase {
 public:
  // Initialization done in subclass.
  inline Utf8DecoderBase();
  inline Utf8DecoderBase(uint16_t* buffer, size_t buffer_length,
                         const uint8_t* stream, size_t stream_length);
  inline size_t Utf16Length() const { return utf16_length_; }

 protected:
  // This reads all characters and sets the utf16_length_.
  // The first buffer_length utf16 chars are cached in the buffer.
  void Reset(uint16_t* buffer, size_t buffer_length, const uint8_t* stream,
             size_t stream_length);
  static void WriteUtf16Slow(const uint8_t* stream, size_t stream_length,
                             uint16_t* data, size_t length);
  const uint8_t* unbuffered_start_;
  size_t unbuffered_length_;
  size_t utf16_length_;
  bool last_byte_of_buffer_unused_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Utf8DecoderBase);
};

template <size_t kBufferSize>
class Utf8Decoder : public Utf8DecoderBase {
 public:
  inline Utf8Decoder() {}
  inline Utf8Decoder(const char* stream, size_t length);
  inline void Reset(const char* stream, size_t length);
  inline size_t WriteUtf16(uint16_t* data, size_t length) const;

 private:
  uint16_t buffer_[kBufferSize];
};

Utf8DecoderBase::Utf8DecoderBase()
    : unbuffered_start_(nullptr),
      unbuffered_length_(0),
      utf16_length_(0),
      last_byte_of_buffer_unused_(false) {}

Utf8DecoderBase::Utf8DecoderBase(uint16_t* buffer, size_t buffer_length,
                                 const uint8_t* stream, size_t stream_length) {
  Reset(buffer, buffer_length, stream, stream_length);
}


template <size_t kBufferSize>
Utf8Decoder<kBufferSize>::Utf8Decoder(const char* stream, size_t length)
    : Utf8DecoderBase(buffer_, kBufferSize,
                      reinterpret_cast<const uint8_t*>(stream), length) {}


template <size_t kBufferSize>
void Utf8Decoder<kBufferSize>::Reset(const char* stream, size_t length) {
  Utf8DecoderBase::Reset(buffer_, kBufferSize,
                         reinterpret_cast<const uint8_t*>(stream), length);
}


template <size_t kBufferSize>
size_t Utf8Decoder<kBufferSize>::WriteUtf16(uint16_t* data,
                                            size_t length) const {
  DCHECK_GT(length, 0);
  if (length > utf16_length_) length = utf16_length_;
  // memcpy everything in buffer.
  size_t buffer_length =
      last_byte_of_buffer_unused_ ? kBufferSize - 1 : kBufferSize;
  size_t memcpy_length = length <= buffer_length ? length : buffer_length;
  v8::internal::MemCopy(data, buffer_, memcpy_length * sizeof(uint16_t));
  if (length <= buffer_length) return length;
  DCHECK_NOT_NULL(unbuffered_start_);
  // Copy the rest the slow way.
  WriteUtf16Slow(unbuffered_start_, unbuffered_length_, data + buffer_length,
                 length - buffer_length);
  return length;
}

class Latin1 {
 public:
  static const unsigned kMaxChar = 0xff;
  // Returns 0 if character does not convert to single latin-1 character
  // or if the character doesn't not convert back to latin-1 via inverse
  // operation (upper to lower, etc).
  static inline uint16_t ConvertNonLatin1ToLatin1(uint16_t);
};


uint16_t Latin1::ConvertNonLatin1ToLatin1(uint16_t c) {
  DCHECK_GT(c, Latin1::kMaxChar);
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
  return 0;
}


}  // namespace unibrow

#endif  // V8_UNICODE_DECODER_H_

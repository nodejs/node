// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_UNICODE_INL_H_
#define V8_UNICODE_INL_H_

#include "unicode.h"

namespace unibrow {

template <class T, int s> bool Predicate<T, s>::get(uchar code_point) {
  CacheEntry entry = entries_[code_point & kMask];
  if (entry.code_point_ == code_point) return entry.value_;
  return CalculateValue(code_point);
}

template <class T, int s> bool Predicate<T, s>::CalculateValue(
    uchar code_point) {
  bool result = T::Is(code_point);
  entries_[code_point & kMask] = CacheEntry(code_point, result);
  return result;
}

template <class T, int s> int Mapping<T, s>::get(uchar c, uchar n,
    uchar* result) {
  CacheEntry entry = entries_[c & kMask];
  if (entry.code_point_ == c) {
    if (entry.offset_ == 0) {
      return 0;
    } else {
      result[0] = c + entry.offset_;
      return 1;
    }
  } else {
    return CalculateValue(c, n, result);
  }
}

template <class T, int s> int Mapping<T, s>::CalculateValue(uchar c, uchar n,
    uchar* result) {
  bool allow_caching = true;
  int length = T::Convert(c, n, result, &allow_caching);
  if (allow_caching) {
    if (length == 1) {
      entries_[c & kMask] = CacheEntry(c, result[0] - c);
      return 1;
    } else {
      entries_[c & kMask] = CacheEntry(c, 0);
      return 0;
    }
  } else {
    return length;
  }
}


unsigned Utf8::Encode(char* str, uchar c) {
  static const int kMask = ~(1 << 6);
  if (c <= kMaxOneByteChar) {
    str[0] = c;
    return 1;
  } else if (c <= kMaxTwoByteChar) {
    str[0] = 0xC0 | (c >> 6);
    str[1] = 0x80 | (c & kMask);
    return 2;
  } else if (c <= kMaxThreeByteChar) {
    str[0] = 0xE0 | (c >> 12);
    str[1] = 0x80 | ((c >> 6) & kMask);
    str[2] = 0x80 | (c & kMask);
    return 3;
  } else {
    str[0] = 0xF0 | (c >> 18);
    str[1] = 0x80 | ((c >> 12) & kMask);
    str[2] = 0x80 | ((c >> 6) & kMask);
    str[3] = 0x80 | (c & kMask);
    return 4;
  }
}


uchar Utf8::ValueOf(const byte* bytes, unsigned length, unsigned* cursor) {
  if (length <= 0) return kBadChar;
  byte first = bytes[0];
  // Characters between 0000 and 0007F are encoded as a single character
  if (first <= kMaxOneByteChar) {
    *cursor += 1;
    return first;
  }
  return CalculateValue(bytes, length, cursor);
}

unsigned Utf8::Length(uchar c) {
  if (c <= kMaxOneByteChar) {
    return 1;
  } else if (c <= kMaxTwoByteChar) {
    return 2;
  } else if (c <= kMaxThreeByteChar) {
    return 3;
  } else {
    return 4;
  }
}

uchar CharacterStream::GetNext() {
  uchar result = DecodeCharacter(buffer_, &cursor_);
  if (remaining_ == 1) {
    cursor_ = 0;
    FillBuffer();
  } else {
    remaining_--;
  }
  return result;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define IF_LITTLE(expr) expr
#define IF_BIG(expr)    ((void) 0)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define IF_LITTLE(expr) ((void) 0)
#define IF_BIG(expr)    expr
#else
#warning Unknown byte ordering
#endif

bool CharacterStream::EncodeAsciiCharacter(uchar c, byte* buffer,
    unsigned capacity, unsigned& offset) {
  if (offset >= capacity) return false;
  buffer[offset] = c;
  offset += 1;
  return true;
}

bool CharacterStream::EncodeNonAsciiCharacter(uchar c, byte* buffer,
    unsigned capacity, unsigned& offset) {
  unsigned aligned = (offset + 0x3) & ~0x3;
  if ((aligned + sizeof(uchar)) > capacity)
    return false;
  if (offset == aligned) {
    IF_LITTLE(*reinterpret_cast<uchar*>(buffer + aligned) = (c << 8) | 0x80);
    IF_BIG(*reinterpret_cast<uchar*>(buffer + aligned) = c | (1 << 31));
  } else {
    buffer[offset] = 0x80;
    IF_LITTLE(*reinterpret_cast<uchar*>(buffer + aligned) = c << 8);
    IF_BIG(*reinterpret_cast<uchar*>(buffer + aligned) = c);
  }
  offset = aligned + sizeof(uchar);
  return true;
}

bool CharacterStream::EncodeCharacter(uchar c, byte* buffer, unsigned capacity,
    unsigned& offset) {
  if (c <= Utf8::kMaxOneByteChar) {
    return EncodeAsciiCharacter(c, buffer, capacity, offset);
  } else {
    return EncodeNonAsciiCharacter(c, buffer, capacity, offset);
  }
}

uchar CharacterStream::DecodeCharacter(const byte* buffer, unsigned* offset) {
  byte b = buffer[*offset];
  if (b <= Utf8::kMaxOneByteChar) {
    (*offset)++;
    return b;
  } else {
    unsigned aligned = (*offset + 0x3) & ~0x3;
    *offset = aligned + sizeof(uchar);
    IF_LITTLE(return *reinterpret_cast<const uchar*>(buffer + aligned) >> 8);
    IF_BIG(return *reinterpret_cast<const uchar*>(buffer + aligned) &
                    ~(1 << 31));
  }
}

#undef IF_LITTLE
#undef IF_BIG

template <class R, class I, unsigned s>
void InputBuffer<R, I, s>::FillBuffer() {
  buffer_ = R::ReadBlock(input_, util_buffer_, s, &remaining_, &offset_);
}

template <class R, class I, unsigned s>
void InputBuffer<R, I, s>::Rewind() {
  Reset(input_);
}

template <class R, class I, unsigned s>
void InputBuffer<R, I, s>::Reset(unsigned position, I input) {
  input_ = input;
  remaining_ = 0;
  cursor_ = 0;
  offset_ = position;
  buffer_ = R::ReadBlock(input_, util_buffer_, s, &remaining_, &offset_);
}

template <class R, class I, unsigned s>
void InputBuffer<R, I, s>::Reset(I input) {
  Reset(0, input);
}

template <class R, class I, unsigned s>
void InputBuffer<R, I, s>::Seek(unsigned position) {
  offset_ = position;
  buffer_ = R::ReadBlock(input_, util_buffer_, s, &remaining_, &offset_);
}

template <unsigned s>
Utf8InputBuffer<s>::Utf8InputBuffer(const char* data, unsigned length)
    : InputBuffer<Utf8, Buffer<const char*>, s>(Buffer<const char*>(data,
                                                                    length)) {
}

}  // namespace unibrow

#endif  // V8_UNICODE_INL_H_

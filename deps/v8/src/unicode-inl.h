// Copyright 2007-2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNICODE_INL_H_
#define V8_UNICODE_INL_H_

#include "src/unicode.h"
#include "src/base/logging.h"
#include "src/utils.h"

namespace unibrow {

template <class T, int s> bool Predicate<T, s>::get(uchar code_point) {
  CacheEntry entry = entries_[code_point & kMask];
  if (entry.code_point() == code_point) return entry.value();
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


unsigned Utf8::EncodeOneByte(char* str, uint8_t c) {
  static const int kMask = ~(1 << 6);
  if (c <= kMaxOneByteChar) {
    str[0] = c;
    return 1;
  }
  str[0] = 0xC0 | (c >> 6);
  str[1] = 0x80 | (c & kMask);
  return 2;
}

// Encode encodes the UTF-16 code units c and previous into the given str
// buffer, and combines surrogate code units into single code points. If
// replace_invalid is set to true, orphan surrogate code units will be replaced
// with kBadChar.
unsigned Utf8::Encode(char* str,
                      uchar c,
                      int previous,
                      bool replace_invalid) {
  static const int kMask = ~(1 << 6);
  if (c <= kMaxOneByteChar) {
    str[0] = c;
    return 1;
  } else if (c <= kMaxTwoByteChar) {
    str[0] = 0xC0 | (c >> 6);
    str[1] = 0x80 | (c & kMask);
    return 2;
  } else if (c <= kMaxThreeByteChar) {
    DCHECK(!Utf16::IsLeadSurrogate(Utf16::kNoPreviousCharacter));
    if (Utf16::IsSurrogatePair(previous, c)) {
      const int kUnmatchedSize = kSizeOfUnmatchedSurrogate;
      return Encode(str - kUnmatchedSize,
                    Utf16::CombineSurrogatePair(previous, c),
                    Utf16::kNoPreviousCharacter,
                    replace_invalid) - kUnmatchedSize;
    } else if (replace_invalid &&
               (Utf16::IsLeadSurrogate(c) ||
               Utf16::IsTrailSurrogate(c))) {
      c = kBadChar;
    }
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


uchar Utf8::ValueOf(const byte* bytes, size_t length, size_t* cursor) {
  if (length <= 0) return kBadChar;
  byte first = bytes[0];
  // Characters between 0000 and 007F are encoded as a single character
  if (V8_LIKELY(first <= kMaxOneByteChar)) {
    *cursor += 1;
    return first;
  }
  return CalculateValue(bytes, length, cursor);
}

unsigned Utf8::Length(uchar c, int previous) {
  if (c <= kMaxOneByteChar) {
    return 1;
  } else if (c <= kMaxTwoByteChar) {
    return 2;
  } else if (c <= kMaxThreeByteChar) {
    DCHECK(!Utf16::IsLeadSurrogate(Utf16::kNoPreviousCharacter));
    if (Utf16::IsSurrogatePair(previous, c)) {
      return kSizeOfUnmatchedSurrogate - kBytesSavedByCombiningSurrogates;
    }
    return 3;
  } else {
    return 4;
  }
}

bool Utf8::IsValidCharacter(uchar c) {
  return c < 0xD800u || (c >= 0xE000u && c < 0xFDD0u) ||
         (c > 0xFDEFu && c <= 0x10FFFFu && (c & 0xFFFEu) != 0xFFFEu &&
          c != kBadChar);
}

}  // namespace unibrow

#endif  // V8_UNICODE_INL_H_

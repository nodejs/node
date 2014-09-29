// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_URI_H_
#define V8_URI_H_

#include "src/v8.h"

#include "src/conversions.h"
#include "src/string-search.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


template <typename Char>
static INLINE(Vector<const Char> GetCharVector(Handle<String> string));


template <>
Vector<const uint8_t> GetCharVector(Handle<String> string) {
  String::FlatContent flat = string->GetFlatContent();
  DCHECK(flat.IsAscii());
  return flat.ToOneByteVector();
}


template <>
Vector<const uc16> GetCharVector(Handle<String> string) {
  String::FlatContent flat = string->GetFlatContent();
  DCHECK(flat.IsTwoByte());
  return flat.ToUC16Vector();
}


class URIUnescape : public AllStatic {
 public:
  template<typename Char>
  MUST_USE_RESULT static MaybeHandle<String> Unescape(Isolate* isolate,
                                                      Handle<String> source);

 private:
  static const signed char kHexValue['g'];

  template<typename Char>
  MUST_USE_RESULT static MaybeHandle<String> UnescapeSlow(
      Isolate* isolate, Handle<String> string, int start_index);

  static INLINE(int TwoDigitHex(uint16_t character1, uint16_t character2));

  template <typename Char>
  static INLINE(int UnescapeChar(Vector<const Char> vector,
                                 int i,
                                 int length,
                                 int* step));
};


const signed char URIUnescape::kHexValue[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15 };


template<typename Char>
MaybeHandle<String> URIUnescape::Unescape(Isolate* isolate,
                                          Handle<String> source) {
  int index;
  { DisallowHeapAllocation no_allocation;
    StringSearch<uint8_t, Char> search(isolate, STATIC_ASCII_VECTOR("%"));
    index = search.Search(GetCharVector<Char>(source), 0);
    if (index < 0) return source;
  }
  return UnescapeSlow<Char>(isolate, source, index);
}


template <typename Char>
MaybeHandle<String> URIUnescape::UnescapeSlow(
    Isolate* isolate, Handle<String> string, int start_index) {
  bool one_byte = true;
  int length = string->length();

  int unescaped_length = 0;
  { DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = GetCharVector<Char>(string);
    for (int i = start_index; i < length; unescaped_length++) {
      int step;
      if (UnescapeChar(vector, i, length, &step) >
              String::kMaxOneByteCharCode) {
        one_byte = false;
      }
      i += step;
    }
  }

  DCHECK(start_index < length);
  Handle<String> first_part =
      isolate->factory()->NewProperSubString(string, 0, start_index);

  int dest_position = 0;
  Handle<String> second_part;
  DCHECK(unescaped_length <= String::kMaxLength);
  if (one_byte) {
    Handle<SeqOneByteString> dest = isolate->factory()->NewRawOneByteString(
        unescaped_length).ToHandleChecked();
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = GetCharVector<Char>(string);
    for (int i = start_index; i < length; dest_position++) {
      int step;
      dest->SeqOneByteStringSet(dest_position,
                                UnescapeChar(vector, i, length, &step));
      i += step;
    }
    second_part = dest;
  } else {
    Handle<SeqTwoByteString> dest = isolate->factory()->NewRawTwoByteString(
        unescaped_length).ToHandleChecked();
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = GetCharVector<Char>(string);
    for (int i = start_index; i < length; dest_position++) {
      int step;
      dest->SeqTwoByteStringSet(dest_position,
                                UnescapeChar(vector, i, length, &step));
      i += step;
    }
    second_part = dest;
  }
  return isolate->factory()->NewConsString(first_part, second_part);
}


int URIUnescape::TwoDigitHex(uint16_t character1, uint16_t character2) {
  if (character1 > 'f') return -1;
  int hi = kHexValue[character1];
  if (hi == -1) return -1;
  if (character2 > 'f') return -1;
  int lo = kHexValue[character2];
  if (lo == -1) return -1;
  return (hi << 4) + lo;
}


template <typename Char>
int URIUnescape::UnescapeChar(Vector<const Char> vector,
                              int i,
                              int length,
                              int* step) {
  uint16_t character = vector[i];
  int32_t hi = 0;
  int32_t lo = 0;
  if (character == '%' &&
      i <= length - 6 &&
      vector[i + 1] == 'u' &&
      (hi = TwoDigitHex(vector[i + 2],
                        vector[i + 3])) != -1 &&
      (lo = TwoDigitHex(vector[i + 4],
                        vector[i + 5])) != -1) {
    *step = 6;
    return (hi << 8) + lo;
  } else if (character == '%' &&
      i <= length - 3 &&
      (lo = TwoDigitHex(vector[i + 1],
                        vector[i + 2])) != -1) {
    *step = 3;
    return lo;
  } else {
    *step = 1;
    return character;
  }
}


class URIEscape : public AllStatic {
 public:
  template<typename Char>
  MUST_USE_RESULT static MaybeHandle<String> Escape(Isolate* isolate,
                                                    Handle<String> string);

 private:
  static const char kHexChars[17];
  static const char kNotEscaped[256];

  static bool IsNotEscaped(uint16_t c) { return kNotEscaped[c] != 0; }
};


const char URIEscape::kHexChars[] = "0123456789ABCDEF";


// kNotEscaped is generated by the following:
//
// #!/bin/perl
// for (my $i = 0; $i < 256; $i++) {
//   print "\n" if $i % 16 == 0;
//   my $c = chr($i);
//   my $escaped = 1;
//   $escaped = 0 if $c =~ m#[A-Za-z0-9@*_+./-]#;
//   print $escaped ? "0, " : "1, ";
// }

const char URIEscape::kNotEscaped[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


template<typename Char>
MaybeHandle<String> URIEscape::Escape(Isolate* isolate, Handle<String> string) {
  DCHECK(string->IsFlat());
  int escaped_length = 0;
  int length = string->length();

  { DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = GetCharVector<Char>(string);
    for (int i = 0; i < length; i++) {
      uint16_t c = vector[i];
      if (c >= 256) {
        escaped_length += 6;
      } else if (IsNotEscaped(c)) {
        escaped_length++;
      } else {
        escaped_length += 3;
      }

      // We don't allow strings that are longer than a maximal length.
      DCHECK(String::kMaxLength < 0x7fffffff - 6);  // Cannot overflow.
      if (escaped_length > String::kMaxLength) break;  // Provoke exception.
    }
  }

  // No length change implies no change.  Return original string if no change.
  if (escaped_length == length) return string;

  Handle<SeqOneByteString> dest;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, dest,
      isolate->factory()->NewRawOneByteString(escaped_length),
      String);
  int dest_position = 0;

  { DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = GetCharVector<Char>(string);
    for (int i = 0; i < length; i++) {
      uint16_t c = vector[i];
      if (c >= 256) {
        dest->SeqOneByteStringSet(dest_position, '%');
        dest->SeqOneByteStringSet(dest_position+1, 'u');
        dest->SeqOneByteStringSet(dest_position+2, kHexChars[c >> 12]);
        dest->SeqOneByteStringSet(dest_position+3, kHexChars[(c >> 8) & 0xf]);
        dest->SeqOneByteStringSet(dest_position+4, kHexChars[(c >> 4) & 0xf]);
        dest->SeqOneByteStringSet(dest_position+5, kHexChars[c & 0xf]);
        dest_position += 6;
      } else if (IsNotEscaped(c)) {
        dest->SeqOneByteStringSet(dest_position, c);
        dest_position++;
      } else {
        dest->SeqOneByteStringSet(dest_position, '%');
        dest->SeqOneByteStringSet(dest_position+1, kHexChars[c >> 4]);
        dest->SeqOneByteStringSet(dest_position+2, kHexChars[c & 0xf]);
        dest_position += 3;
      }
    }
  }

  return dest;
}

} }  // namespace v8::internal

#endif  // V8_URI_H_

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/uri.h"

#include "src/char-predicates-inl.h"
#include "src/handles.h"
#include "src/isolate-inl.h"
#include "src/list.h"
#include "src/string-search.h"

namespace v8 {
namespace internal {

namespace {  // anonymous namespace for DecodeURI helper functions
bool IsReservedPredicate(uc16 c) {
  switch (c) {
    case '#':
    case '$':
    case '&':
    case '+':
    case ',':
    case '/':
    case ':':
    case ';':
    case '=':
    case '?':
    case '@':
      return true;
    default:
      return false;
  }
}

bool IsReplacementCharacter(const uint8_t* octets, int length) {
  // The replacement character is at codepoint U+FFFD in the Unicode Specials
  // table. Its UTF-8 encoding is 0xEF 0xBF 0xBD.
  if (length != 3 || octets[0] != 0xef || octets[1] != 0xbf ||
      octets[2] != 0xbd) {
    return false;
  }
  return true;
}

bool DecodeOctets(const uint8_t* octets, int length, List<uc16>* buffer) {
  size_t cursor = 0;
  uc32 value = unibrow::Utf8::ValueOf(octets, length, &cursor);
  if (value == unibrow::Utf8::kBadChar &&
      !IsReplacementCharacter(octets, length)) {
    return false;
  }

  if (value <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
    buffer->Add(value);
  } else {
    buffer->Add(unibrow::Utf16::LeadSurrogate(value));
    buffer->Add(unibrow::Utf16::TrailSurrogate(value));
  }
  return true;
}

int TwoDigitHex(uc16 character1, uc16 character2) {
  if (character1 > 'f') return -1;
  int high = HexValue(character1);
  if (high == -1) return -1;
  if (character2 > 'f') return -1;
  int low = HexValue(character2);
  if (low == -1) return -1;
  return (high << 4) + low;
}

template <typename T>
void AddToBuffer(uc16 decoded, String::FlatContent* uri_content, int index,
                 bool is_uri, List<T>* buffer) {
  if (is_uri && IsReservedPredicate(decoded)) {
    buffer->Add('%');
    uc16 first = uri_content->Get(index + 1);
    uc16 second = uri_content->Get(index + 2);
    DCHECK_GT(std::numeric_limits<T>::max(), first);
    DCHECK_GT(std::numeric_limits<T>::max(), second);

    buffer->Add(first);
    buffer->Add(second);
  } else {
    buffer->Add(decoded);
  }
}

bool IntoTwoByte(int index, bool is_uri, int uri_length,
                 String::FlatContent* uri_content, List<uc16>* buffer) {
  for (int k = index; k < uri_length; k++) {
    uc16 code = uri_content->Get(k);
    if (code == '%') {
      int two_digits;
      if (k + 2 >= uri_length ||
          (two_digits = TwoDigitHex(uri_content->Get(k + 1),
                                    uri_content->Get(k + 2))) < 0) {
        return false;
      }
      k += 2;
      uc16 decoded = static_cast<uc16>(two_digits);
      if (decoded > unibrow::Utf8::kMaxOneByteChar) {
        uint8_t octets[unibrow::Utf8::kMaxEncodedSize];
        octets[0] = decoded;

        int number_of_continuation_bytes = 0;
        while ((decoded << ++number_of_continuation_bytes) & 0x80) {
          if (number_of_continuation_bytes > 3 || k + 3 >= uri_length) {
            return false;
          }
          if (uri_content->Get(++k) != '%' ||
              (two_digits = TwoDigitHex(uri_content->Get(k + 1),
                                        uri_content->Get(k + 2))) < 0) {
            return false;
          }
          k += 2;
          uc16 continuation_byte = static_cast<uc16>(two_digits);
          octets[number_of_continuation_bytes] = continuation_byte;
        }

        if (!DecodeOctets(octets, number_of_continuation_bytes, buffer)) {
          return false;
        }
      } else {
        AddToBuffer(decoded, uri_content, k - 2, is_uri, buffer);
      }
    } else {
      buffer->Add(code);
    }
  }
  return true;
}

bool IntoOneAndTwoByte(Handle<String> uri, bool is_uri,
                       List<uint8_t>* one_byte_buffer,
                       List<uc16>* two_byte_buffer) {
  DisallowHeapAllocation no_gc;
  String::FlatContent uri_content = uri->GetFlatContent();

  int uri_length = uri->length();
  for (int k = 0; k < uri_length; k++) {
    uc16 code = uri_content.Get(k);
    if (code == '%') {
      int two_digits;
      if (k + 2 >= uri_length ||
          (two_digits = TwoDigitHex(uri_content.Get(k + 1),
                                    uri_content.Get(k + 2))) < 0) {
        return false;
      }

      uc16 decoded = static_cast<uc16>(two_digits);
      if (decoded > unibrow::Utf8::kMaxOneByteChar) {
        return IntoTwoByte(k, is_uri, uri_length, &uri_content,
                           two_byte_buffer);
      }

      AddToBuffer(decoded, &uri_content, k, is_uri, one_byte_buffer);
      k += 2;
    } else {
      if (code > unibrow::Utf8::kMaxOneByteChar) {
        return IntoTwoByte(k, is_uri, uri_length, &uri_content,
                           two_byte_buffer);
      }
      one_byte_buffer->Add(code);
    }
  }
  return true;
}

}  // anonymous namespace

MaybeHandle<String> Uri::Decode(Isolate* isolate, Handle<String> uri,
                                bool is_uri) {
  uri = String::Flatten(uri);
  List<uint8_t> one_byte_buffer;
  List<uc16> two_byte_buffer;

  if (!IntoOneAndTwoByte(uri, is_uri, &one_byte_buffer, &two_byte_buffer)) {
    THROW_NEW_ERROR(isolate, NewURIError(), String);
  }

  if (two_byte_buffer.is_empty()) {
    return isolate->factory()->NewStringFromOneByte(
        one_byte_buffer.ToConstVector());
  }

  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewRawTwoByteString(
                           one_byte_buffer.length() + two_byte_buffer.length()),
      String);

  CopyChars(result->GetChars(), one_byte_buffer.ToConstVector().start(),
            one_byte_buffer.length());
  CopyChars(result->GetChars() + one_byte_buffer.length(),
            two_byte_buffer.ToConstVector().start(), two_byte_buffer.length());

  return result;
}

namespace {  // anonymous namespace for EncodeURI helper functions
bool IsUnescapePredicateInUriComponent(uc16 c) {
  if (IsAlphaNumeric(c)) {
    return true;
  }

  switch (c) {
    case '!':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '-':
    case '.':
    case '_':
    case '~':
      return true;
    default:
      return false;
  }
}

bool IsUriSeparator(uc16 c) {
  switch (c) {
    case '#':
    case ':':
    case ';':
    case '/':
    case '?':
    case '$':
    case '&':
    case '+':
    case ',':
    case '@':
    case '=':
      return true;
    default:
      return false;
  }
}

void AddEncodedOctetToBuffer(uint8_t octet, List<uint8_t>* buffer) {
  buffer->Add('%');
  buffer->Add(HexCharOfValue(octet >> 4));
  buffer->Add(HexCharOfValue(octet & 0x0F));
}

void EncodeSingle(uc16 c, List<uint8_t>* buffer) {
  char s[4] = {};
  int number_of_bytes;
  number_of_bytes =
      unibrow::Utf8::Encode(s, c, unibrow::Utf16::kNoPreviousCharacter, false);
  for (int k = 0; k < number_of_bytes; k++) {
    AddEncodedOctetToBuffer(s[k], buffer);
  }
}

void EncodePair(uc16 cc1, uc16 cc2, List<uint8_t>* buffer) {
  char s[4] = {};
  int number_of_bytes =
      unibrow::Utf8::Encode(s, unibrow::Utf16::CombineSurrogatePair(cc1, cc2),
                            unibrow::Utf16::kNoPreviousCharacter, false);
  for (int k = 0; k < number_of_bytes; k++) {
    AddEncodedOctetToBuffer(s[k], buffer);
  }
}

}  // anonymous namespace

MaybeHandle<String> Uri::Encode(Isolate* isolate, Handle<String> uri,
                                bool is_uri) {
  uri = String::Flatten(uri);
  int uri_length = uri->length();
  List<uint8_t> buffer(uri_length);

  {
    DisallowHeapAllocation no_gc;
    String::FlatContent uri_content = uri->GetFlatContent();

    for (int k = 0; k < uri_length; k++) {
      uc16 cc1 = uri_content.Get(k);
      if (unibrow::Utf16::IsLeadSurrogate(cc1)) {
        k++;
        if (k < uri_length) {
          uc16 cc2 = uri->Get(k);
          if (unibrow::Utf16::IsTrailSurrogate(cc2)) {
            EncodePair(cc1, cc2, &buffer);
            continue;
          }
        }
      } else if (!unibrow::Utf16::IsTrailSurrogate(cc1)) {
        if (IsUnescapePredicateInUriComponent(cc1) ||
            (is_uri && IsUriSeparator(cc1))) {
          buffer.Add(cc1);
        } else {
          EncodeSingle(cc1, &buffer);
        }
        continue;
      }

      AllowHeapAllocation allocate_error_and_return;
      THROW_NEW_ERROR(isolate, NewURIError(), String);
    }
  }

  return isolate->factory()->NewStringFromOneByte(buffer.ToConstVector());
}

namespace {  // Anonymous namespace for Escape and Unescape

template <typename Char>
int UnescapeChar(Vector<const Char> vector, int i, int length, int* step) {
  uint16_t character = vector[i];
  int32_t hi = 0;
  int32_t lo = 0;
  if (character == '%' && i <= length - 6 && vector[i + 1] == 'u' &&
      (hi = TwoDigitHex(vector[i + 2], vector[i + 3])) > -1 &&
      (lo = TwoDigitHex(vector[i + 4], vector[i + 5])) > -1) {
    *step = 6;
    return (hi << 8) + lo;
  } else if (character == '%' && i <= length - 3 &&
             (lo = TwoDigitHex(vector[i + 1], vector[i + 2])) > -1) {
    *step = 3;
    return lo;
  } else {
    *step = 1;
    return character;
  }
}

template <typename Char>
MaybeHandle<String> UnescapeSlow(Isolate* isolate, Handle<String> string,
                                 int start_index) {
  bool one_byte = true;
  int length = string->length();

  int unescaped_length = 0;
  {
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = string->GetCharVector<Char>();
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
    Handle<SeqOneByteString> dest = isolate->factory()
                                        ->NewRawOneByteString(unescaped_length)
                                        .ToHandleChecked();
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = string->GetCharVector<Char>();
    for (int i = start_index; i < length; dest_position++) {
      int step;
      dest->SeqOneByteStringSet(dest_position,
                                UnescapeChar(vector, i, length, &step));
      i += step;
    }
    second_part = dest;
  } else {
    Handle<SeqTwoByteString> dest = isolate->factory()
                                        ->NewRawTwoByteString(unescaped_length)
                                        .ToHandleChecked();
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = string->GetCharVector<Char>();
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

bool IsNotEscaped(uint16_t c) {
  if (IsAlphaNumeric(c)) {
    return true;
  }
  //  @*_+-./
  switch (c) {
    case '@':
    case '*':
    case '_':
    case '+':
    case '-':
    case '.':
    case '/':
      return true;
    default:
      return false;
  }
}

template <typename Char>
static MaybeHandle<String> UnescapePrivate(Isolate* isolate,
                                           Handle<String> source) {
  int index;
  {
    DisallowHeapAllocation no_allocation;
    StringSearch<uint8_t, Char> search(isolate, STATIC_CHAR_VECTOR("%"));
    index = search.Search(source->GetCharVector<Char>(), 0);
    if (index < 0) return source;
  }
  return UnescapeSlow<Char>(isolate, source, index);
}

template <typename Char>
static MaybeHandle<String> EscapePrivate(Isolate* isolate,
                                         Handle<String> string) {
  DCHECK(string->IsFlat());
  int escaped_length = 0;
  int length = string->length();

  {
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = string->GetCharVector<Char>();
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
      DCHECK(String::kMaxLength < 0x7fffffff - 6);     // Cannot overflow.
      if (escaped_length > String::kMaxLength) break;  // Provoke exception.
    }
  }

  // No length change implies no change.  Return original string if no change.
  if (escaped_length == length) return string;

  Handle<SeqOneByteString> dest;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, dest, isolate->factory()->NewRawOneByteString(escaped_length),
      String);
  int dest_position = 0;

  {
    DisallowHeapAllocation no_allocation;
    Vector<const Char> vector = string->GetCharVector<Char>();
    for (int i = 0; i < length; i++) {
      uint16_t c = vector[i];
      if (c >= 256) {
        dest->SeqOneByteStringSet(dest_position, '%');
        dest->SeqOneByteStringSet(dest_position + 1, 'u');
        dest->SeqOneByteStringSet(dest_position + 2, HexCharOfValue(c >> 12));
        dest->SeqOneByteStringSet(dest_position + 3,
                                  HexCharOfValue((c >> 8) & 0xf));
        dest->SeqOneByteStringSet(dest_position + 4,
                                  HexCharOfValue((c >> 4) & 0xf));
        dest->SeqOneByteStringSet(dest_position + 5, HexCharOfValue(c & 0xf));
        dest_position += 6;
      } else if (IsNotEscaped(c)) {
        dest->SeqOneByteStringSet(dest_position, c);
        dest_position++;
      } else {
        dest->SeqOneByteStringSet(dest_position, '%');
        dest->SeqOneByteStringSet(dest_position + 1, HexCharOfValue(c >> 4));
        dest->SeqOneByteStringSet(dest_position + 2, HexCharOfValue(c & 0xf));
        dest_position += 3;
      }
    }
  }

  return dest;
}

}  // Anonymous namespace

MaybeHandle<String> Uri::Escape(Isolate* isolate, Handle<String> string) {
  Handle<String> result;
  string = String::Flatten(string);
  return string->IsOneByteRepresentationUnderneath()
             ? EscapePrivate<uint8_t>(isolate, string)
             : EscapePrivate<uc16>(isolate, string);
}

MaybeHandle<String> Uri::Unescape(Isolate* isolate, Handle<String> string) {
  Handle<String> result;
  string = String::Flatten(string);
  return string->IsOneByteRepresentationUnderneath()
             ? UnescapePrivate<uint8_t>(isolate, string)
             : UnescapePrivate<uc16>(isolate, string);
}

}  // namespace internal
}  // namespace v8

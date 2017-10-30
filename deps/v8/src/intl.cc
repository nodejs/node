// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/intl.h"

#include <memory>

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/string-case.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/timezone.h"
#include "unicode/ustring.h"
#include "unicode/uvernum.h"
#include "unicode/uversion.h"

namespace v8 {
namespace internal {

namespace {
inline bool IsASCIIUpper(uint16_t ch) { return ch >= 'A' && ch <= 'Z'; }

const uint8_t kToLower[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
    0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
    0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB,
    0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xD7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
    0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB,
    0xFC, 0xFD, 0xFE, 0xFF,
};

inline uint16_t ToLatin1Lower(uint16_t ch) {
  return static_cast<uint16_t>(kToLower[ch]);
}

inline uint16_t ToASCIIUpper(uint16_t ch) {
  return ch & ~((ch >= 'a' && ch <= 'z') << 5);
}

// Does not work for U+00DF (sharp-s), U+00B5 (micron), U+00FF.
inline uint16_t ToLatin1Upper(uint16_t ch) {
  DCHECK(ch != 0xDF && ch != 0xB5 && ch != 0xFF);
  return ch &
         ~(((ch >= 'a' && ch <= 'z') || (((ch & 0xE0) == 0xE0) && ch != 0xF7))
           << 5);
}

template <typename Char>
bool ToUpperFastASCII(const Vector<const Char>& src,
                      Handle<SeqOneByteString> result) {
  // Do a faster loop for the case where all the characters are ASCII.
  uint16_t ored = 0;
  int32_t index = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    ored |= ch;
    result->SeqOneByteStringSet(index++, ToASCIIUpper(ch));
  }
  return !(ored & ~0x7F);
}

const uint16_t sharp_s = 0xDF;

template <typename Char>
bool ToUpperOneByte(const Vector<const Char>& src, uint8_t* dest,
                    int* sharp_s_count) {
  // Still pretty-fast path for the input with non-ASCII Latin-1 characters.

  // There are two special cases.
  //  1. U+00B5 and U+00FF are mapped to a character beyond U+00FF.
  //  2. Lower case sharp-S converts to "SS" (two characters)
  *sharp_s_count = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    if (V8_UNLIKELY(ch == sharp_s)) {
      ++(*sharp_s_count);
      continue;
    }
    if (V8_UNLIKELY(ch == 0xB5 || ch == 0xFF)) {
      // Since this upper-cased character does not fit in an 8-bit string, we
      // need to take the 16-bit path.
      return false;
    }
    *dest++ = ToLatin1Upper(ch);
  }

  return true;
}

template <typename Char>
void ToUpperWithSharpS(const Vector<const Char>& src,
                       Handle<SeqOneByteString> result) {
  int32_t dest_index = 0;
  for (auto it = src.begin(); it != src.end(); ++it) {
    uint16_t ch = static_cast<uint16_t>(*it);
    if (ch == sharp_s) {
      result->SeqOneByteStringSet(dest_index++, 'S');
      result->SeqOneByteStringSet(dest_index++, 'S');
    } else {
      result->SeqOneByteStringSet(dest_index++, ToLatin1Upper(ch));
    }
  }
}

inline int FindFirstUpperOrNonAscii(String* s, int length) {
  for (int index = 0; index < length; ++index) {
    uint16_t ch = s->Get(index);
    if (V8_UNLIKELY(IsASCIIUpper(ch) || ch & ~0x7F)) {
      return index;
    }
  }
  return length;
}

}  // namespace

const uint8_t* ToLatin1LowerTable() { return &kToLower[0]; }

const UChar* GetUCharBufferFromFlat(const String::FlatContent& flat,
                                    std::unique_ptr<uc16[]>* dest,
                                    int32_t length) {
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    if (!*dest) {
      dest->reset(NewArray<uc16>(length));
      CopyChars(dest->get(), flat.ToOneByteVector().start(), length);
    }
    return reinterpret_cast<const UChar*>(dest->get());
  } else {
    return reinterpret_cast<const UChar*>(flat.ToUC16Vector().start());
  }
}

MUST_USE_RESULT Object* LocaleConvertCase(Handle<String> s, Isolate* isolate,
                                          bool is_to_upper, const char* lang) {
  auto case_converter = is_to_upper ? u_strToUpper : u_strToLower;
  int32_t src_length = s->length();
  int32_t dest_length = src_length;
  UErrorCode status;
  Handle<SeqTwoByteString> result;
  std::unique_ptr<uc16[]> sap;

  if (dest_length == 0) return isolate->heap()->empty_string();

  // This is not a real loop. It'll be executed only once (no overflow) or
  // twice (overflow).
  for (int i = 0; i < 2; ++i) {
    // Case conversion can increase the string length (e.g. sharp-S => SS) so
    // that we have to handle RangeError exceptions here.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(dest_length));
    DisallowHeapAllocation no_gc;
    DCHECK(s->IsFlat());
    String::FlatContent flat = s->GetFlatContent();
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, src_length);
    status = U_ZERO_ERROR;
    dest_length = case_converter(reinterpret_cast<UChar*>(result->GetChars()),
                                 dest_length, src, src_length, lang, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) break;
  }

  // In most cases, the output will fill the destination buffer completely
  // leading to an unterminated string (U_STRING_NOT_TERMINATED_WARNING).
  // Only in rare cases, it'll be shorter than the destination buffer and
  // |result| has to be truncated.
  DCHECK(U_SUCCESS(status));
  if (V8_LIKELY(status == U_STRING_NOT_TERMINATED_WARNING)) {
    DCHECK(dest_length == result->length());
    return *result;
  }
  if (U_SUCCESS(status)) {
    DCHECK(dest_length < result->length());
    return *Handle<SeqTwoByteString>::cast(
        SeqString::Truncate(result, dest_length));
  }
  return *s;
}

// A stripped-down version of ConvertToLower that can only handle flat one-byte
// strings and does not allocate. Note that {src} could still be, e.g., a
// one-byte sliced string with a two-byte parent string.
// Called from TF builtins.
MUST_USE_RESULT Object* ConvertOneByteToLower(String* src, String* dst,
                                              Isolate* isolate) {
  DCHECK_EQ(src->length(), dst->length());
  DCHECK(src->HasOnlyOneByteChars());
  DCHECK(src->IsFlat());
  DCHECK(dst->IsSeqOneByteString());

  DisallowHeapAllocation no_gc;

  const int length = src->length();
  String::FlatContent src_flat = src->GetFlatContent();
  uint8_t* dst_data = SeqOneByteString::cast(dst)->GetChars();

  if (src_flat.IsOneByte()) {
    const uint8_t* src_data = src_flat.ToOneByteVector().start();

    bool has_changed_character = false;
    int index_to_first_unprocessed =
        FastAsciiConvert<true>(reinterpret_cast<char*>(dst_data),
                               reinterpret_cast<const char*>(src_data), length,
                               &has_changed_character);

    if (index_to_first_unprocessed == length) {
      return has_changed_character ? dst : src;
    }

    // If not ASCII, we keep the result up to index_to_first_unprocessed and
    // process the rest.
    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dst_data[index] = ToLatin1Lower(static_cast<uint16_t>(src_data[index]));
    }
  } else {
    DCHECK(src_flat.IsTwoByte());
    int index_to_first_unprocessed = FindFirstUpperOrNonAscii(src, length);
    if (index_to_first_unprocessed == length) return src;

    const uint16_t* src_data = src_flat.ToUC16Vector().start();
    CopyChars(dst_data, src_data, index_to_first_unprocessed);
    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dst_data[index] = ToLatin1Lower(static_cast<uint16_t>(src_data[index]));
    }
  }

  return dst;
}

MUST_USE_RESULT Object* ConvertToLower(Handle<String> s, Isolate* isolate) {
  if (!s->HasOnlyOneByteChars()) {
    // Use a slower implementation for strings with characters beyond U+00FF.
    return LocaleConvertCase(s, isolate, false, "");
  }

  int length = s->length();

  // We depend here on the invariant that the length of a Latin1
  // string is invariant under ToLowerCase, and the result always
  // fits in the Latin1 range in the *root locale*. It does not hold
  // for ToUpperCase even in the root locale.

  // Scan the string for uppercase and non-ASCII characters for strings
  // shorter than a machine-word without any memory allocation overhead.
  // TODO(jshin): Apply this to a longer input by breaking FastAsciiConvert()
  // to two parts, one for scanning the prefix with no change and the other for
  // handling ASCII-only characters.

  bool is_short = length < static_cast<int>(sizeof(uintptr_t));
  if (is_short) {
    bool is_lower_ascii = FindFirstUpperOrNonAscii(*s, length) == length;
    if (is_lower_ascii) return *s;
  }

  Handle<SeqOneByteString> result =
      isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

  return ConvertOneByteToLower(*s, *result, isolate);
}

MUST_USE_RESULT Object* ConvertToUpper(Handle<String> s, Isolate* isolate) {
  int32_t length = s->length();
  if (s->HasOnlyOneByteChars() && length > 0) {
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

    DCHECK(s->IsFlat());
    int sharp_s_count;
    bool is_result_single_byte;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent();
      uint8_t* dest = result->GetChars();
      if (flat.IsOneByte()) {
        Vector<const uint8_t> src = flat.ToOneByteVector();
        bool has_changed_character = false;
        int index_to_first_unprocessed =
            FastAsciiConvert<false>(reinterpret_cast<char*>(result->GetChars()),
                                    reinterpret_cast<const char*>(src.start()),
                                    length, &has_changed_character);
        if (index_to_first_unprocessed == length)
          return has_changed_character ? *result : *s;
        // If not ASCII, we keep the result up to index_to_first_unprocessed and
        // process the rest.
        is_result_single_byte =
            ToUpperOneByte(src.SubVector(index_to_first_unprocessed, length),
                           dest + index_to_first_unprocessed, &sharp_s_count);
      } else {
        DCHECK(flat.IsTwoByte());
        Vector<const uint16_t> src = flat.ToUC16Vector();
        if (ToUpperFastASCII(src, result)) return *result;
        is_result_single_byte = ToUpperOneByte(src, dest, &sharp_s_count);
      }
    }

    // Go to the full Unicode path if there are characters whose uppercase
    // is beyond the Latin-1 range (cannot be represented in OneByteString).
    if (V8_UNLIKELY(!is_result_single_byte)) {
      return LocaleConvertCase(s, isolate, true, "");
    }

    if (sharp_s_count == 0) return *result;

    // We have sharp_s_count sharp-s characters, but the result is still
    // in the Latin-1 range.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        isolate->factory()->NewRawOneByteString(length + sharp_s_count));
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    if (flat.IsOneByte()) {
      ToUpperWithSharpS(flat.ToOneByteVector(), result);
    } else {
      ToUpperWithSharpS(flat.ToUC16Vector(), result);
    }

    return *result;
  }

  return LocaleConvertCase(s, isolate, true, "");
}

MUST_USE_RESULT Object* ConvertCase(Handle<String> s, bool is_upper,
                                    Isolate* isolate) {
  return is_upper ? ConvertToUpper(s, isolate) : ConvertToLower(s, isolate);
}

ICUTimezoneCache::ICUTimezoneCache() : timezone_(nullptr) { Clear(); }

ICUTimezoneCache::~ICUTimezoneCache() { Clear(); }

const char* ICUTimezoneCache::LocalTimezone(double time_ms) {
  bool is_dst = DaylightSavingsOffset(time_ms) != 0;
  char* name = is_dst ? dst_timezone_name_ : timezone_name_;
  if (name[0] == '\0') {
    icu::UnicodeString result;
    GetTimeZone()->getDisplayName(is_dst, icu::TimeZone::LONG, result);
    result += '\0';

    icu::CheckedArrayByteSink byte_sink(name, kMaxTimezoneChars);
    result.toUTF8(byte_sink);
    CHECK(!byte_sink.Overflowed());
  }
  return const_cast<const char*>(name);
}

icu::TimeZone* ICUTimezoneCache::GetTimeZone() {
  if (timezone_ == nullptr) {
    timezone_ = icu::TimeZone::createDefault();
  }
  return timezone_;
}

bool ICUTimezoneCache::GetOffsets(double time_ms, int32_t* raw_offset,
                                  int32_t* dst_offset) {
  UErrorCode status = U_ZERO_ERROR;
  GetTimeZone()->getOffset(time_ms, false, *raw_offset, *dst_offset, status);
  return U_SUCCESS(status);
}

double ICUTimezoneCache::DaylightSavingsOffset(double time_ms) {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(time_ms, &raw_offset, &dst_offset)) return 0;
  return dst_offset;
}

double ICUTimezoneCache::LocalTimeOffset() {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(icu::Calendar::getNow(), &raw_offset, &dst_offset)) return 0;
  return raw_offset;
}

void ICUTimezoneCache::Clear() {
  delete timezone_;
  timezone_ = nullptr;
  timezone_name_[0] = '\0';
  dst_timezone_name_[0] = '\0';
}

}  // namespace internal
}  // namespace v8

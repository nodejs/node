// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/intl-objects.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "src/api-inl.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/string.h"
#include "src/property-descriptor.h"
#include "src/string-case.h"
#include "unicode/basictz.h"
#include "unicode/brkiter.h"
#include "unicode/coll.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/normalizer2.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/regex.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/ucol.h"
#include "unicode/ures.h"
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

}  // namespace

const uint8_t* Intl::ToLatin1LowerTable() { return &kToLower[0]; }

icu::UnicodeString Intl::ToICUUnicodeString(Isolate* isolate,
                                            Handle<String> string) {
  string = String::Flatten(isolate, string);
  {
    DisallowHeapAllocation no_gc;
    std::unique_ptr<uc16[]> sap;
    return icu::UnicodeString(GetUCharBufferFromFlat(string->GetFlatContent(),
                                                     &sap, string->length()),
                              string->length());
  }
}

namespace {
MaybeHandle<String> LocaleConvertCase(Isolate* isolate, Handle<String> s,
                                      bool is_to_upper, const char* lang) {
  auto case_converter = is_to_upper ? u_strToUpper : u_strToLower;
  int32_t src_length = s->length();
  int32_t dest_length = src_length;
  UErrorCode status;
  Handle<SeqTwoByteString> result;
  std::unique_ptr<uc16[]> sap;

  if (dest_length == 0) return ReadOnlyRoots(isolate).empty_string_handle();

  // This is not a real loop. It'll be executed only once (no overflow) or
  // twice (overflow).
  for (int i = 0; i < 2; ++i) {
    // Case conversion can increase the string length (e.g. sharp-S => SS) so
    // that we have to handle RangeError exceptions here.
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(dest_length),
        String);
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
    return result;
  }
  DCHECK(dest_length < result->length());
  return SeqString::Truncate(result, dest_length);
}

}  // namespace

// A stripped-down version of ConvertToLower that can only handle flat one-byte
// strings and does not allocate. Note that {src} could still be, e.g., a
// one-byte sliced string with a two-byte parent string.
// Called from TF builtins.
String* Intl::ConvertOneByteToLower(String* src, String* dst) {
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

MaybeHandle<String> Intl::ConvertToLower(Isolate* isolate, Handle<String> s) {
  if (!s->HasOnlyOneByteChars()) {
    // Use a slower implementation for strings with characters beyond U+00FF.
    return LocaleConvertCase(isolate, s, false, "");
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
    if (is_lower_ascii) return s;
  }

  Handle<SeqOneByteString> result =
      isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

  return Handle<String>(Intl::ConvertOneByteToLower(*s, *result), isolate);
}

MaybeHandle<String> Intl::ConvertToUpper(Isolate* isolate, Handle<String> s) {
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
        if (index_to_first_unprocessed == length) {
          return has_changed_character ? result : s;
        }
        // If not ASCII, we keep the result up to index_to_first_unprocessed and
        // process the rest.
        is_result_single_byte =
            ToUpperOneByte(src.SubVector(index_to_first_unprocessed, length),
                           dest + index_to_first_unprocessed, &sharp_s_count);
      } else {
        DCHECK(flat.IsTwoByte());
        Vector<const uint16_t> src = flat.ToUC16Vector();
        if (ToUpperFastASCII(src, result)) return result;
        is_result_single_byte = ToUpperOneByte(src, dest, &sharp_s_count);
      }
    }

    // Go to the full Unicode path if there are characters whose uppercase
    // is beyond the Latin-1 range (cannot be represented in OneByteString).
    if (V8_UNLIKELY(!is_result_single_byte)) {
      return LocaleConvertCase(isolate, s, true, "");
    }

    if (sharp_s_count == 0) return result;

    // We have sharp_s_count sharp-s characters, but the result is still
    // in the Latin-1 range.
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        isolate->factory()->NewRawOneByteString(length + sharp_s_count),
        String);
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    if (flat.IsOneByte()) {
      ToUpperWithSharpS(flat.ToOneByteVector(), result);
    } else {
      ToUpperWithSharpS(flat.ToUC16Vector(), result);
    }

    return result;
  }

  return LocaleConvertCase(isolate, s, true, "");
}

std::string Intl::GetNumberingSystem(const icu::Locale& icu_locale) {
  // Ugly hack. ICU doesn't expose numbering system in any way, so we have
  // to assume that for given locale NumberingSystem constructor produces the
  // same digits as NumberFormat/Calendar would.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::NumberingSystem> numbering_system(
      icu::NumberingSystem::createInstance(icu_locale, status));
  std::string value;
  if (U_SUCCESS(status)) {
    value = numbering_system->getName();
  }
  return value;
}

MaybeHandle<JSObject> Intl::CachedOrNewService(
    Isolate* isolate, Handle<String> service, Handle<Object> locales,
    Handle<Object> options, Handle<Object> internal_options) {
  Handle<Object> result;
  Handle<Object> undefined_value(ReadOnlyRoots(isolate).undefined_value(),
                                 isolate);
  Handle<Object> args[] = {service, locales, options, internal_options};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, isolate->cached_or_new_service(),
                      undefined_value, arraysize(args), args),
      JSArray);
  return Handle<JSObject>::cast(result);
}

icu::Locale Intl::CreateICULocale(const std::string& bcp47_locale) {
  DisallowHeapAllocation no_gc;

  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int parsed_length = 0;

  // bcp47_locale_str should be a canonicalized language tag, which
  // means this shouldn't fail.
  uloc_forLanguageTag(bcp47_locale.c_str(), icu_result, ULOC_FULLNAME_CAPACITY,
                      &parsed_length, &status);
  CHECK(U_SUCCESS(status));

  // bcp47_locale is already checked for its structural validity
  // so that it should be parsed completely.
  size_t bcp47_length = bcp47_locale.length();
  CHECK_EQ(bcp47_length, parsed_length);

  icu::Locale icu_locale(icu_result);
  if (icu_locale.isBogus()) {
    FATAL("Failed to create ICU locale, are ICU data files missing?");
  }

  return icu_locale;
}

// static

MaybeHandle<String> Intl::ToString(Isolate* isolate,
                                   const icu::UnicodeString& string) {
  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(string.getBuffer()), string.length()));
}

MaybeHandle<String> Intl::ToString(Isolate* isolate,
                                   const icu::UnicodeString& string,
                                   int32_t begin, int32_t end) {
  return Intl::ToString(isolate, string.tempSubStringBetween(begin, end));
}

namespace {

Handle<JSObject> InnerAddElement(Isolate* isolate, Handle<JSArray> array,
                                 int index, Handle<String> field_type_string,
                                 Handle<String> value) {
  // let element = $array[$index] = {
  //   type: $field_type_string,
  //   value: $value
  // }
  // return element;
  Factory* factory = isolate->factory();
  Handle<JSObject> element = factory->NewJSObject(isolate->object_function());
  JSObject::AddProperty(isolate, element, factory->type_string(),
                        field_type_string, NONE);

  JSObject::AddProperty(isolate, element, factory->value_string(), value, NONE);
  JSObject::AddDataElement(array, index, element, NONE);
  return element;
}

}  // namespace

void Intl::AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                      Handle<String> field_type_string, Handle<String> value) {
  // Same as $array[$index] = {type: $field_type_string, value: $value};
  InnerAddElement(isolate, array, index, field_type_string, value);
}

void Intl::AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                      Handle<String> field_type_string, Handle<String> value,
                      Handle<String> additional_property_name,
                      Handle<String> additional_property_value) {
  // Same as $array[$index] = {
  //   type: $field_type_string, value: $value,
  //   $additional_property_name: $additional_property_value
  // }
  Handle<JSObject> element =
      InnerAddElement(isolate, array, index, field_type_string, value);
  JSObject::AddProperty(isolate, element, additional_property_name,
                        additional_property_value, NONE);
}

namespace {

// Build the shortened locale; eg, convert xx_Yyyy_ZZ  to xx_ZZ.
//
// If locale has a script tag then return true and the locale without the
// script else return false and an empty string.
bool RemoveLocaleScriptTag(const std::string& icu_locale,
                           std::string* locale_less_script) {
  icu::Locale new_locale = icu::Locale::createCanonical(icu_locale.c_str());
  const char* icu_script = new_locale.getScript();
  if (icu_script == nullptr || strlen(icu_script) == 0) {
    *locale_less_script = std::string();
    return false;
  }

  const char* icu_language = new_locale.getLanguage();
  const char* icu_country = new_locale.getCountry();
  icu::Locale short_locale = icu::Locale(icu_language, icu_country);
  *locale_less_script = short_locale.getName();
  return true;
}

}  // namespace

std::set<std::string> Intl::GetAvailableLocales(Intl::ICUService service) {
  const icu::Locale* icu_available_locales = nullptr;
  int32_t count = 0;
  std::set<std::string> locales;

  switch (service) {
    case Intl::ICUService::kBreakIterator:
    case Intl::ICUService::kSegmenter:
      icu_available_locales = icu::BreakIterator::getAvailableLocales(count);
      break;
    case Intl::ICUService::kCollator:
      icu_available_locales = icu::Collator::getAvailableLocales(count);
      break;
    case Intl::ICUService::kRelativeDateTimeFormatter:
    case Intl::ICUService::kDateFormat:
      icu_available_locales = icu::DateFormat::getAvailableLocales(count);
      break;
    case Intl::ICUService::kNumberFormat:
      icu_available_locales = icu::NumberFormat::getAvailableLocales(count);
      break;
    case Intl::ICUService::kPluralRules:
      // TODO(littledan): For PluralRules, filter out locales that
      // don't support PluralRules.
      // PluralRules is missing an appropriate getAvailableLocales method,
      // so we should filter from all locales, but it's not clear how; see
      // https://ssl.icu-project.org/trac/ticket/12756
      icu_available_locales = icu::Locale::getAvailableLocales(count);
      break;
    case Intl::ICUService::kListFormatter: {
      // TODO(ftang): for now just use
      // icu::Locale::getAvailableLocales(count) until we migrate to
      // Intl::GetAvailableLocales().
      // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20015
      icu_available_locales = icu::Locale::getAvailableLocales(count);
      break;
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = icu_available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }
    std::string locale(result);
    locales.insert(locale);

    std::string shortened_locale;
    if (RemoveLocaleScriptTag(icu_name, &shortened_locale)) {
      std::replace(shortened_locale.begin(), shortened_locale.end(), '_', '-');
      locales.insert(shortened_locale);
    }
  }

  return locales;
}

namespace {

const char* ICUServiceToString(Intl::ICUService service) {
  switch (service) {
    case Intl::ICUService::kCollator:
      return "Intl.Collator";
    case Intl::ICUService::kNumberFormat:
      return "Intl.NumberFormat";
    case Intl::ICUService::kDateFormat:
      return "Intl.DateFormat";
    case Intl::ICUService::kBreakIterator:
      return "Intl.v8BreakIterator";
    case Intl::ICUService::kPluralRules:
      return "Intl.PluralRules";
    case Intl::ICUService::kRelativeDateTimeFormatter:
      return "Intl.RelativeTimeFormat";
    case Intl::ICUService::kListFormatter:
      return "Intl.kListFormat";
    case Intl::ICUService::kSegmenter:
      return "Intl.kSegmenter";
  }
  UNREACHABLE();
}

}  // namespace

std::string Intl::DefaultLocale(Isolate* isolate) {
  if (isolate->default_locale().empty()) {
    icu::Locale default_locale;
    // Translate ICU's fallback locale to a well-known locale.
    if (strcmp(default_locale.getName(), "en_US_POSIX") == 0) {
      isolate->set_default_locale("en-US");
    } else {
      // Set the locale
      char result[ULOC_FULLNAME_CAPACITY];
      UErrorCode status = U_ZERO_ERROR;
      int32_t length =
          uloc_toLanguageTag(default_locale.getName(), result,
                             ULOC_FULLNAME_CAPACITY, FALSE, &status);
      isolate->set_default_locale(
          U_SUCCESS(status) ? std::string(result, length) : "und");
    }
    DCHECK(!isolate->default_locale().empty());
  }
  return isolate->default_locale();
}

// See ecma402/#legacy-constructor.
MaybeHandle<Object> Intl::LegacyUnwrapReceiver(Isolate* isolate,
                                               Handle<JSReceiver> receiver,
                                               Handle<JSFunction> constructor,
                                               bool has_initialized_slot) {
  Handle<Object> obj_is_instance_of;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj_is_instance_of,
                             Object::InstanceOf(isolate, receiver, constructor),
                             Object);
  bool is_instance_of = obj_is_instance_of->BooleanValue(isolate);

  // 2. If receiver does not have an [[Initialized...]] internal slot
  //    and ? InstanceofOperator(receiver, constructor) is true, then
  if (!has_initialized_slot && is_instance_of) {
    // 2. a. Let new_receiver be ? Get(receiver, %Intl%.[[FallbackSymbol]]).
    Handle<Object> new_receiver;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, new_receiver,
        JSReceiver::GetProperty(isolate, receiver,
                                isolate->factory()->intl_fallback_symbol()),
        Object);
    return new_receiver;
  }

  return receiver;
}

namespace {

#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
// Define general regexp macros.
// Note "(?:" means the regexp group a non-capture group.
#define REGEX_ALPHA "[a-z]"
#define REGEX_DIGIT "[0-9]"
#define REGEX_ALPHANUM "(?:" REGEX_ALPHA "|" REGEX_DIGIT ")"

void BuildLanguageTagRegexps(Isolate* isolate) {
// Define the language tag regexp macros.
// For info on BCP 47 see https://tools.ietf.org/html/bcp47 .
// Because language tags are case insensitive per BCP 47 2.1.1 and regexp's
// defined below will always be used after lowercasing the input, uppercase
// ranges in BCP 47 2.1 are dropped and grandfathered tags are all lowercased.
// clang-format off
#define BCP47_REGULAR                                          \
  "(?:art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|" \
  "zh-min|zh-min-nan|zh-xiang)"
#define BCP47_IRREGULAR                                  \
  "(?:en-gb-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|" \
  "i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|"  \
  "i-tsu|sgn-be-fr|sgn-be-nl|sgn-ch-de)"
#define BCP47_GRANDFATHERED "(?:" BCP47_IRREGULAR "|" BCP47_REGULAR ")"
#define BCP47_PRIVATE_USE "(?:x(?:-" REGEX_ALPHANUM "{1,8})+)"

#define BCP47_SINGLETON "(?:" REGEX_DIGIT "|" "[a-wy-z])"

#define BCP47_EXTENSION "(?:" BCP47_SINGLETON "(?:-" REGEX_ALPHANUM "{2,8})+)"
#define BCP47_VARIANT  \
  "(?:" REGEX_ALPHANUM "{5,8}" "|" "(?:" REGEX_DIGIT REGEX_ALPHANUM "{3}))"

#define BCP47_REGION "(?:" REGEX_ALPHA "{2}" "|" REGEX_DIGIT "{3})"
#define BCP47_SCRIPT "(?:" REGEX_ALPHA "{4})"
#define BCP47_EXT_LANG "(?:" REGEX_ALPHA "{3}(?:-" REGEX_ALPHA "{3}){0,2})"
#define BCP47_LANGUAGE "(?:" REGEX_ALPHA "{2,3}(?:-" BCP47_EXT_LANG ")?" \
  "|" REGEX_ALPHA "{4}" "|" REGEX_ALPHA "{5,8})"
#define BCP47_LANG_TAG         \
  BCP47_LANGUAGE               \
  "(?:-" BCP47_SCRIPT ")?"     \
  "(?:-" BCP47_REGION ")?"     \
  "(?:-" BCP47_VARIANT ")*"    \
  "(?:-" BCP47_EXTENSION ")*"  \
  "(?:-" BCP47_PRIVATE_USE ")?"
  // clang-format on

  constexpr char kLanguageTagSingletonRegexp[] = "^" BCP47_SINGLETON "$";
  constexpr char kLanguageTagVariantRegexp[] = "^" BCP47_VARIANT "$";
  constexpr char kLanguageTagRegexp[] =
      "^(?:" BCP47_LANG_TAG "|" BCP47_PRIVATE_USE "|" BCP47_GRANDFATHERED ")$";

  UErrorCode status = U_ZERO_ERROR;
  icu::RegexMatcher* language_singleton_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagSingletonRegexp, -1, US_INV), 0, status);
  icu::RegexMatcher* language_tag_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagRegexp, -1, US_INV), 0, status);
  icu::RegexMatcher* language_variant_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagVariantRegexp, -1, US_INV), 0, status);
  CHECK(U_SUCCESS(status));

  isolate->set_language_tag_regexp_matchers(language_singleton_regexp_matcher,
                                            language_tag_regexp_matcher,
                                            language_variant_regexp_matcher);
// Undefine the language tag regexp macros.
#undef BCP47_EXTENSION
#undef BCP47_EXT_LANG
#undef BCP47_GRANDFATHERED
#undef BCP47_IRREGULAR
#undef BCP47_LANG_TAG
#undef BCP47_LANGUAGE
#undef BCP47_PRIVATE_USE
#undef BCP47_REGION
#undef BCP47_REGULAR
#undef BCP47_SCRIPT
#undef BCP47_SINGLETON
#undef BCP47_VARIANT
}

// Undefine the general regexp macros.
#undef REGEX_ALPHA
#undef REGEX_DIGIT
#undef REGEX_ALPHANUM

icu::RegexMatcher* GetLanguageSingletonRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_singleton_regexp_matcher =
      isolate->language_singleton_regexp_matcher();
  if (language_singleton_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_singleton_regexp_matcher =
        isolate->language_singleton_regexp_matcher();
  }
  return language_singleton_regexp_matcher;
}

icu::RegexMatcher* GetLanguageTagRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_tag_regexp_matcher =
      isolate->language_tag_regexp_matcher();
  if (language_tag_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_tag_regexp_matcher = isolate->language_tag_regexp_matcher();
  }
  return language_tag_regexp_matcher;
}

icu::RegexMatcher* GetLanguageVariantRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_variant_regexp_matcher =
      isolate->language_variant_regexp_matcher();
  if (language_variant_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_variant_regexp_matcher =
        isolate->language_variant_regexp_matcher();
  }
  return language_variant_regexp_matcher;
}
#endif  // USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63

}  // anonymous namespace

Maybe<bool> Intl::GetStringOption(Isolate* isolate, Handle<JSReceiver> options,
                                  const char* property,
                                  std::vector<const char*> values,
                                  const char* service,
                                  std::unique_ptr<char[]>* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  if (value->IsUndefined(isolate)) {
    return Just(false);
  }

  // 2. c. Let value be ? ToString(value).
  Handle<String> value_str;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_str, Object::ToString(isolate, value), Nothing<bool>());
  std::unique_ptr<char[]> value_cstr = value_str->ToCString();

  // 2. d. if values is not undefined, then
  if (values.size() > 0) {
    // 2. d. i. If values does not contain an element equal to value,
    // throw a RangeError exception.
    for (size_t i = 0; i < values.size(); i++) {
      if (strcmp(values.at(i), value_cstr.get()) == 0) {
        // 2. e. return value
        *result = std::move(value_cstr);
        return Just(true);
      }
    }

    Handle<String> service_str =
        isolate->factory()->NewStringFromAsciiChecked(service);
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kValueOutOfRange, value, service_str,
                      property_str),
        Nothing<bool>());
  }

  // 2. e. return value
  *result = std::move(value_cstr);
  return Just(true);
}

V8_WARN_UNUSED_RESULT Maybe<bool> Intl::GetBoolOption(
    Isolate* isolate, Handle<JSReceiver> options, const char* property,
    const char* service, bool* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  // 2. If value is not undefined, then
  if (!value->IsUndefined(isolate)) {
    // 2. b. i. Let value be ToBoolean(value).
    *result = value->BooleanValue(isolate);

    // 2. e. return value
    return Just(true);
  }

  return Just(false);
}

namespace {

char AsciiToLower(char c) {
  if (c < 'A' || c > 'Z') {
    return c;
  }
  return c | (1 << 5);
}

#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
/**
 * Check the structural Validity of the language tag per ECMA 402 6.2.2:
 *   - Well-formed per RFC 5646 2.1
 *   - There are no duplicate variant subtags
 *   - There are no duplicate singleton (extension) subtags
 *
 * One extra-check is done (from RFC 5646 2.2.9): the tag is compared
 * against the list of grandfathered tags. However, subtags for
 * primary/extended language, script, region, variant are not checked
 * against the IANA language subtag registry.
 *
 * ICU 62 or earlier is too permissible and lets invalid tags, like
 * hant-cmn-cn, through.
 *
 * Returns false if the language tag is invalid.
 */
bool IsStructurallyValidLanguageTag(Isolate* isolate,
                                    const std::string& locale_in) {
  if (!String::IsAscii(locale_in.c_str(),
                       static_cast<int>(locale_in.length()))) {
    return false;
  }
  std::string locale(locale_in);
  icu::RegexMatcher* language_tag_regexp_matcher =
      GetLanguageTagRegexMatcher(isolate);

  // Check if it's well-formed, including grandfathered tags.
  icu::UnicodeString locale_uni(locale.c_str(), -1, US_INV);
  // Note: icu::RegexMatcher::reset does not make a copy of the input string
  // so cannot use a temp value; ie: cannot create it as a call parameter.
  language_tag_regexp_matcher->reset(locale_uni);
  UErrorCode status = U_ZERO_ERROR;
  bool is_valid_lang_tag = language_tag_regexp_matcher->matches(status);
  if (!is_valid_lang_tag || V8_UNLIKELY(U_FAILURE(status))) {
    return false;
  }

  // Just return if it's a x- form. It's all private.
  if (locale.find("x-") == 0) {
    return true;
  }

  // Check if there are any duplicate variants or singletons (extensions).

  // Remove private use section.
  locale = locale.substr(0, locale.find("-x-"));

  // Skip language since it can match variant regex, so we start from 1.
  // We are matching i-klingon here, but that's ok, since i-klingon-klingon
  // is not valid and would fail LANGUAGE_TAG_RE test.
  size_t pos = 0;
  std::vector<std::string> parts;
  while ((pos = locale.find('-')) != std::string::npos) {
    std::string token = locale.substr(0, pos);
    parts.push_back(token);
    locale = locale.substr(pos + 1);
  }
  if (locale.length() != 0) {
    parts.push_back(locale);
  }

  icu::RegexMatcher* language_variant_regexp_matcher =
      GetLanguageVariantRegexMatcher(isolate);

  icu::RegexMatcher* language_singleton_regexp_matcher =
      GetLanguageSingletonRegexMatcher(isolate);

  std::vector<std::string> variants;
  std::vector<std::string> extensions;
  for (auto it = parts.begin() + 1; it != parts.end(); it++) {
    icu::UnicodeString part(it->data(), -1, US_INV);
    language_variant_regexp_matcher->reset(part);
    bool is_language_variant = language_variant_regexp_matcher->matches(status);
    if (V8_UNLIKELY(U_FAILURE(status))) {
      return false;
    }
    if (is_language_variant && extensions.size() == 0) {
      if (std::find(variants.begin(), variants.end(), *it) == variants.end()) {
        variants.push_back(*it);
      } else {
        return false;
      }
    }

    language_singleton_regexp_matcher->reset(part);
    bool is_language_singleton =
        language_singleton_regexp_matcher->matches(status);
    if (V8_UNLIKELY(U_FAILURE(status))) {
      return false;
    }
    if (is_language_singleton) {
      if (std::find(extensions.begin(), extensions.end(), *it) ==
          extensions.end()) {
        extensions.push_back(*it);
      } else {
        return false;
      }
    }
  }

  return true;
}
#endif  // USE_CHROMIUM_ICU == 0 || U_ICU_VERSION_MAJOR_NUM < 63

bool IsLowerAscii(char c) { return c >= 'a' && c < 'z'; }

bool IsTwoLetterLanguage(const std::string& locale) {
  // Two letters, both in range 'a'-'z'...
  return locale.length() == 2 && IsLowerAscii(locale[0]) &&
         IsLowerAscii(locale[1]);
}

bool IsDeprecatedLanguage(const std::string& locale) {
  //  Check if locale is one of the deprecated language tags:
  return locale == "in" || locale == "iw" || locale == "ji" || locale == "jw";
}

// Reference:
// https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
bool IsGrandfatheredTagWithoutPreferredVaule(const std::string& locale) {
  if (V8_UNLIKELY(locale == "zh-min" || locale == "cel-gaulish")) return true;
  if (locale.length() > 6 /* i-mingo is 7 chars long */ &&
      V8_UNLIKELY(locale[0] == 'i' && locale[1] == '-')) {
    return locale.substr(2) == "default" || locale.substr(2) == "enochian" ||
           locale.substr(2) == "mingo";
  }
  return false;
}

}  // anonymous namespace

Maybe<std::string> Intl::CanonicalizeLanguageTag(Isolate* isolate,
                                                 Handle<Object> locale_in) {
  Handle<String> locale_str;
  // This does part of the validity checking spec'ed in CanonicalizeLocaleList:
  // 7c ii. If Type(kValue) is not String or Object, throw a TypeError
  // exception.
  // 7c iii. Let tag be ? ToString(kValue).
  // 7c iv. If IsStructurallyValidLanguageTag(tag) is false, throw a
  // RangeError exception.

  if (locale_in->IsString()) {
    locale_str = Handle<String>::cast(locale_in);
  } else if (locale_in->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, locale_str,
                                     Object::ToString(isolate, locale_in),
                                     Nothing<std::string>());
  } else {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NewTypeError(MessageTemplate::kLanguageID),
                                 Nothing<std::string>());
  }
  std::string locale(locale_str->ToCString().get());

  if (locale.length() == 0 ||
      !String::IsAscii(locale.data(), static_cast<int>(locale.length()))) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  // Optimize for the most common case: a 2-letter language code in the
  // canonical form/lowercase that is not one of the deprecated codes
  // (in, iw, ji, jw). Don't check for ~70 of 3-letter deprecated language
  // codes. Instead, let them be handled by ICU in the slow path. However,
  // fast-track 'fil' (3-letter canonical code).
  if ((IsTwoLetterLanguage(locale) && !IsDeprecatedLanguage(locale)) ||
      locale == "fil") {
    return Just(locale);
  }

  // Because per BCP 47 2.1.1 language tags are case-insensitive, lowercase
  // the input before any more check.
  std::transform(locale.begin(), locale.end(), locale.begin(), AsciiToLower);

#if USE_CHROMIUM_ICU == 0 && U_ICU_VERSION_MAJOR_NUM < 63
  if (!IsStructurallyValidLanguageTag(isolate, locale)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }
#endif

  // ICU maps a few grandfathered tags to what looks like a regular language
  // tag even though IANA language tag registry does not have a preferred
  // entry map for them. Return them as they're with lowercasing.
  if (IsGrandfatheredTagWithoutPreferredVaule(locale)) {
    return Just(locale);
  }

  // // ECMA 402 6.2.3
  // TODO(jshin): uloc_{for,to}TanguageTag can fail even for a structually valid
  // language tag if it's too long (much longer than 100 chars). Even if we
  // allocate a longer buffer, ICU will still fail if it's too long. Either
  // propose to Ecma 402 to put a limit on the locale length or change ICU to
  // handle long locale names better. See
  // https://unicode-org.atlassian.net/browse/ICU-13417
  UErrorCode error = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  // uloc_forLanguageTag checks the structrual validity. If the input BCP47
  // language tag is parsed all the way to the end, it indicates that the input
  // is structurally valid. Due to a couple of bugs, we can't use it
  // without Chromium patches or ICU 62 or earlier.
  int parsed_length;
  uloc_forLanguageTag(locale.c_str(), icu_result, ULOC_FULLNAME_CAPACITY,
                      &parsed_length, &error);
  if (U_FAILURE(error) ||
#if USE_CHROMIUM_ICU == 1 || U_ICU_VERSION_MAJOR_NUM >= 63
      static_cast<size_t>(parsed_length) < locale.length() ||
#endif
      error == U_STRING_NOT_TERMINATED_WARNING) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  // Force strict BCP47 rules.
  char result[ULOC_FULLNAME_CAPACITY];
  int32_t result_len = uloc_toLanguageTag(icu_result, result,
                                          ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  return Just(std::string(result, result_len));
}

Maybe<std::vector<std::string>> Intl::CanonicalizeLocaleList(
    Isolate* isolate, Handle<Object> locales, bool only_return_one_result) {
  // 1. If locales is undefined, then
  if (locales->IsUndefined(isolate)) {
    // 1a. Return a new empty List.
    return Just(std::vector<std::string>());
  }
  // 2. Let seen be a new empty List.
  std::vector<std::string> seen;
  // 3. If Type(locales) is String, then
  if (locales->IsString()) {
    // 3a. Let O be CreateArrayFromList(« locales »).
    // Instead of creating a one-element array and then iterating over it,
    // we inline the body of the iteration:
    std::string canonicalized_tag;
    if (!CanonicalizeLanguageTag(isolate, locales).To(&canonicalized_tag)) {
      return Nothing<std::vector<std::string>>();
    }
    seen.push_back(canonicalized_tag);
    return Just(seen);
  }
  // 4. Else,
  // 4a. Let O be ? ToObject(locales).
  Handle<JSReceiver> o;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, o,
                                   Object::ToObject(isolate, locales),
                                   Nothing<std::vector<std::string>>());
  // 5. Let len be ? ToLength(? Get(O, "length")).
  Handle<Object> length_obj;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, length_obj,
                                   Object::GetLengthFromArrayLike(isolate, o),
                                   Nothing<std::vector<std::string>>());
  // TODO(jkummerow): Spec violation: strictly speaking, we have to iterate
  // up to 2^53-1 if {length_obj} says so. Since cases above 2^32 probably
  // don't happen in practice (and would be very slow if they do), we'll keep
  // the code simple for now by using a saturating to-uint32 conversion.
  double raw_length = length_obj->Number();
  uint32_t len =
      raw_length >= kMaxUInt32 ? kMaxUInt32 : static_cast<uint32_t>(raw_length);
  // 6. Let k be 0.
  // 7. Repeat, while k < len
  for (uint32_t k = 0; k < len; k++) {
    // 7a. Let Pk be ToString(k).
    // 7b. Let kPresent be ? HasProperty(O, Pk).
    LookupIterator it(isolate, o, k);
    Maybe<bool> maybe_found = JSReceiver::HasProperty(&it);
    MAYBE_RETURN(maybe_found, Nothing<std::vector<std::string>>());
    // 7c. If kPresent is true, then
    if (!maybe_found.FromJust()) continue;
    // 7c i. Let kValue be ? Get(O, Pk).
    Handle<Object> k_value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, k_value, Object::GetProperty(&it),
                                     Nothing<std::vector<std::string>>());
    // 7c ii. If Type(kValue) is not String or Object, throw a TypeError
    // exception.
    // 7c iii. Let tag be ? ToString(kValue).
    // 7c iv. If IsStructurallyValidLanguageTag(tag) is false, throw a
    // RangeError exception.
    // 7c v. Let canonicalizedTag be CanonicalizeLanguageTag(tag).
    std::string canonicalized_tag;
    if (!CanonicalizeLanguageTag(isolate, k_value).To(&canonicalized_tag)) {
      return Nothing<std::vector<std::string>>();
    }
    // 7c vi. If canonicalizedTag is not an element of seen, append
    // canonicalizedTag as the last element of seen.
    if (std::find(seen.begin(), seen.end(), canonicalized_tag) == seen.end()) {
      seen.push_back(canonicalized_tag);
    }
    // 7d. Increase k by 1. (See loop header.)
    // Optimization: some callers only need one result.
    if (only_return_one_result) return Just(seen);
  }
  // 8. Return seen.
  return Just(seen);
}

// ecma402 #sup-string.prototype.tolocalelowercase
// ecma402 #sup-string.prototype.tolocaleuppercase
MaybeHandle<String> Intl::StringLocaleConvertCase(Isolate* isolate,
                                                  Handle<String> s,
                                                  bool to_upper,
                                                  Handle<Object> locales) {
  std::vector<std::string> requested_locales;
  if (!CanonicalizeLocaleList(isolate, locales, true).To(&requested_locales)) {
    return MaybeHandle<String>();
  }
  std::string requested_locale = requested_locales.size() == 0
                                     ? Intl::DefaultLocale(isolate)
                                     : requested_locales[0];
  size_t dash = requested_locale.find('-');
  if (dash != std::string::npos) {
    requested_locale = requested_locale.substr(0, dash);
  }

  // Primary language tag can be up to 8 characters long in theory.
  // https://tools.ietf.org/html/bcp47#section-2.2.1
  DCHECK_LE(requested_locale.length(), 8);
  s = String::Flatten(isolate, s);

  // All the languages requiring special-handling have two-letter codes.
  // Note that we have to check for '!= 2' here because private-use language
  // tags (x-foo) or grandfathered irregular tags (e.g. i-enochian) would have
  // only 'x' or 'i' when they get here.
  if (V8_UNLIKELY(requested_locale.length() != 2)) {
    if (to_upper) {
      return ConvertToUpper(isolate, s);
    }
    return ConvertToLower(isolate, s);
  }
  // TODO(jshin): Consider adding a fast path for ASCII or Latin-1. The fastpath
  // in the root locale needs to be adjusted for az, lt and tr because even case
  // mapping of ASCII range characters are different in those locales.
  // Greek (el) does not require any adjustment.
  if (V8_UNLIKELY((requested_locale == "tr") || (requested_locale == "el") ||
                  (requested_locale == "lt") || (requested_locale == "az"))) {
    return LocaleConvertCase(isolate, s, to_upper, requested_locale.c_str());
  } else {
    if (to_upper) {
      return ConvertToUpper(isolate, s);
    }
    return ConvertToLower(isolate, s);
  }
}

MaybeHandle<Object> Intl::StringLocaleCompare(Isolate* isolate,
                                              Handle<String> string1,
                                              Handle<String> string2,
                                              Handle<Object> locales,
                                              Handle<Object> options) {
  Factory* factory = isolate->factory();
  Handle<JSObject> collator;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, collator,
      CachedOrNewService(isolate, factory->NewStringFromStaticChars("collator"),
                         locales, options, factory->undefined_value()),
      Object);
  CHECK(collator->IsJSCollator());
  return Intl::CompareStrings(isolate, Handle<JSCollator>::cast(collator),
                              string1, string2);
}

// ecma402/#sec-collator-comparestrings
Handle<Object> Intl::CompareStrings(Isolate* isolate,
                                    Handle<JSCollator> collator,
                                    Handle<String> string1,
                                    Handle<String> string2) {
  Factory* factory = isolate->factory();
  icu::Collator* icu_collator = collator->icu_collator()->raw();
  CHECK_NOT_NULL(icu_collator);

  string1 = String::Flatten(isolate, string1);
  string2 = String::Flatten(isolate, string2);

  UCollationResult result;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString string_val1 = Intl::ToICUUnicodeString(isolate, string1);
  icu::UnicodeString string_val2 = Intl::ToICUUnicodeString(isolate, string2);
  result = icu_collator->compare(string_val1, string_val2, status);
  DCHECK(U_SUCCESS(status));

  return factory->NewNumberFromInt(result);
}

// ecma402/#sup-properties-of-the-number-prototype-object
MaybeHandle<String> Intl::NumberToLocaleString(Isolate* isolate,
                                               Handle<Object> num,
                                               Handle<Object> locales,
                                               Handle<Object> options) {
  Factory* factory = isolate->factory();
  Handle<JSObject> number_format_holder;
  // 2. Let numberFormat be ? Construct(%NumberFormat%, « locales, options »).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, number_format_holder,
      CachedOrNewService(isolate,
                         factory->NewStringFromStaticChars("numberformat"),
                         locales, options, factory->undefined_value()),
      String);
  DCHECK(number_format_holder->IsJSNumberFormat());
  Handle<JSNumberFormat> number_format = Handle<JSNumberFormat>(
      JSNumberFormat::cast(*number_format_holder), isolate);

  Handle<Object> number_obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number_obj,
                             Object::ToNumber(isolate, num), String);

  // Spec treats -0 and +0 as 0.
  double number = number_obj->Number() + 0;

  // Return FormatNumber(numberFormat, x).
  return JSNumberFormat::FormatNumber(isolate, number_format, number);
}

namespace {

// ecma402/#sec-defaultnumberoption
Maybe<int> DefaultNumberOption(Isolate* isolate, Handle<Object> value, int min,
                               int max, int fallback, Handle<String> property) {
  // 2. Else, return fallback.
  if (value->IsUndefined()) return Just(fallback);

  // 1. If value is not undefined, then
  // a. Let value be ? ToNumber(value).
  Handle<Object> value_num;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_num, Object::ToNumber(isolate, value), Nothing<int>());
  DCHECK(value_num->IsNumber());

  // b. If value is NaN or less than minimum or greater than maximum, throw a
  // RangeError exception.
  if (value_num->IsNaN() || value_num->Number() < min ||
      value_num->Number() > max) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property),
        Nothing<int>());
  }

  // The max and min arguments are integers and the above check makes
  // sure that we are within the integer range making this double to
  // int conversion safe.
  //
  // c. Return floor(value).
  return Just(FastD2I(floor(value_num->Number())));
}

// ecma402/#sec-getnumberoption
Maybe<int> GetNumberOption(Isolate* isolate, Handle<JSReceiver> options,
                           Handle<String> property, int min, int max,
                           int fallback) {
  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, JSReceiver::GetProperty(isolate, options, property),
      Nothing<int>());

  // Return ? DefaultNumberOption(value, minimum, maximum, fallback).
  return DefaultNumberOption(isolate, value, min, max, fallback, property);
}

Maybe<int> GetNumberOption(Isolate* isolate, Handle<JSReceiver> options,
                           const char* property, int min, int max,
                           int fallback) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);
  return GetNumberOption(isolate, options, property_str, min, max, fallback);
}

}  // namespace

Maybe<bool> Intl::SetNumberFormatDigitOptions(Isolate* isolate,
                                              icu::DecimalFormat* number_format,
                                              Handle<JSReceiver> options,
                                              int mnfd_default,
                                              int mxfd_default) {
  CHECK_NOT_NULL(number_format);

  // 5. Let mnid be ? GetNumberOption(options, "minimumIntegerDigits,", 1, 21,
  // 1).
  int mnid;
  if (!GetNumberOption(isolate, options, "minimumIntegerDigits", 1, 21, 1)
           .To(&mnid)) {
    return Nothing<bool>();
  }

  // 6. Let mnfd be ? GetNumberOption(options, "minimumFractionDigits", 0, 20,
  // mnfdDefault).
  int mnfd;
  if (!GetNumberOption(isolate, options, "minimumFractionDigits", 0, 20,
                       mnfd_default)
           .To(&mnfd)) {
    return Nothing<bool>();
  }

  // 7. Let mxfdActualDefault be max( mnfd, mxfdDefault ).
  int mxfd_actual_default = std::max(mnfd, mxfd_default);

  // 8. Let mxfd be ? GetNumberOption(options,
  // "maximumFractionDigits", mnfd, 20, mxfdActualDefault).
  int mxfd;
  if (!GetNumberOption(isolate, options, "maximumFractionDigits", mnfd, 20,
                       mxfd_actual_default)
           .To(&mxfd)) {
    return Nothing<bool>();
  }

  // 9.  Let mnsd be ? Get(options, "minimumSignificantDigits").
  Handle<Object> mnsd_obj;
  Handle<String> mnsd_str =
      isolate->factory()->minimumSignificantDigits_string();
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mnsd_obj, JSReceiver::GetProperty(isolate, options, mnsd_str),
      Nothing<bool>());

  // 10. Let mxsd be ? Get(options, "maximumSignificantDigits").
  Handle<Object> mxsd_obj;
  Handle<String> mxsd_str =
      isolate->factory()->maximumSignificantDigits_string();
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mxsd_obj, JSReceiver::GetProperty(isolate, options, mxsd_str),
      Nothing<bool>());

  // 11. Set intlObj.[[MinimumIntegerDigits]] to mnid.
  number_format->setMinimumIntegerDigits(mnid);

  // 12. Set intlObj.[[MinimumFractionDigits]] to mnfd.
  number_format->setMinimumFractionDigits(mnfd);

  // 13. Set intlObj.[[MaximumFractionDigits]] to mxfd.
  number_format->setMaximumFractionDigits(mxfd);

  bool significant_digits_used = false;
  // 14. If mnsd is not undefined or mxsd is not undefined, then
  if (!mnsd_obj->IsUndefined(isolate) || !mxsd_obj->IsUndefined(isolate)) {
    // 14. a. Let mnsd be ? DefaultNumberOption(mnsd, 1, 21, 1).
    int mnsd;
    if (!DefaultNumberOption(isolate, mnsd_obj, 1, 21, 1, mnsd_str).To(&mnsd)) {
      return Nothing<bool>();
    }

    // 14. b. Let mxsd be ? DefaultNumberOption(mxsd, mnsd, 21, 21).
    int mxsd;
    if (!DefaultNumberOption(isolate, mxsd_obj, mnsd, 21, 21, mxsd_str)
             .To(&mxsd)) {
      return Nothing<bool>();
    }

    significant_digits_used = true;

    // 14. c. Set intlObj.[[MinimumSignificantDigits]] to mnsd.
    number_format->setMinimumSignificantDigits(mnsd);

    // 14. d. Set intlObj.[[MaximumSignificantDigits]] to mxsd.
    number_format->setMaximumSignificantDigits(mxsd);
  }

  number_format->setSignificantDigitsUsed(significant_digits_used);
  number_format->setRoundingMode(icu::DecimalFormat::kRoundHalfUp);
  return Just(true);
}

namespace {

// ecma402/#sec-bestavailablelocale
std::string BestAvailableLocale(const std::set<std::string>& available_locales,
                                const std::string& locale) {
  // 1. Let candidate be locale.
  std::string candidate = locale;

  // 2. Repeat,
  while (true) {
    // 2.a. If availableLocales contains an element equal to candidate, return
    //      candidate.
    if (available_locales.find(candidate) != available_locales.end()) {
      return candidate;
    }

    // 2.b. Let pos be the character index of the last occurrence of "-"
    //      (U+002D) within candidate. If that character does not occur, return
    //      undefined.
    size_t pos = candidate.rfind('-');
    if (pos == std::string::npos) {
      return std::string();
    }

    // 2.c. If pos ≥ 2 and the character "-" occurs at index pos-2 of candidate,
    //      decrease pos by 2.
    if (pos >= 2 && candidate[pos - 2] == '-') {
      pos -= 2;
    }

    // 2.d. Let candidate be the substring of candidate from position 0,
    //      inclusive, to position pos, exclusive.
    candidate = candidate.substr(0, pos);
  }
}

struct ParsedLocale {
  std::string no_extensions_locale;
  std::string extension;
};

// Returns a struct containing a bcp47 tag without unicode extensions
// and the removed unicode extensions.
//
// For example, given 'en-US-u-co-emoji' returns 'en-US' and
// 'u-co-emoji'.
ParsedLocale ParseBCP47Locale(const std::string& locale) {
  size_t length = locale.length();
  ParsedLocale parsed_locale;

  // Privateuse or grandfathered locales have no extension sequences.
  if ((length > 1) && (locale[1] == '-')) {
    // Check to make sure that this really is a grandfathered or
    // privateuse extension. ICU can sometimes mess up the
    // canonicalization.
    CHECK(locale[0] == 'x' || locale[0] == 'i');
    parsed_locale.no_extensions_locale = locale;
    return parsed_locale;
  }

  size_t unicode_extension_start = locale.find("-u-");

  // No unicode extensions found.
  if (unicode_extension_start == std::string::npos) {
    parsed_locale.no_extensions_locale = locale;
    return parsed_locale;
  }

  size_t private_extension_start = locale.find("-x-");

  // Unicode extensions found within privateuse subtags don't count.
  if (private_extension_start != std::string::npos &&
      private_extension_start < unicode_extension_start) {
    parsed_locale.no_extensions_locale = locale;
    return parsed_locale;
  }

  const std::string beginning = locale.substr(0, unicode_extension_start);
  size_t unicode_extension_end = length;
  DCHECK_GT(length, 2);

  // Find the end of the extension production as per the bcp47 grammar
  // by looking for '-' followed by 2 chars and then another '-'.
  for (size_t i = unicode_extension_start + 1; i < length - 2; i++) {
    if (locale[i] != '-') continue;

    if (locale[i + 2] == '-') {
      unicode_extension_end = i;
      break;
    }

    i += 2;
  }

  const std::string end = locale.substr(unicode_extension_end);
  parsed_locale.no_extensions_locale = beginning + end;
  parsed_locale.extension =
      locale.substr(unicode_extension_start, unicode_extension_end);
  return parsed_locale;
}

// ecma402/#sec-lookupsupportedlocales
std::vector<std::string> LookupSupportedLocales(
    const std::set<std::string>& available_locales,
    const std::vector<std::string>& requested_locales) {
  // 1. Let subset be a new empty List.
  std::vector<std::string> subset;

  // 2. For each element locale of requestedLocales in List order, do
  for (const std::string& locale : requested_locales) {
    // 2. a. Let noExtensionsLocale be the String value that is locale
    //       with all Unicode locale extension sequences removed.
    std::string no_extension_locale =
        ParseBCP47Locale(locale).no_extensions_locale;

    // 2. b. Let availableLocale be
    //       BestAvailableLocale(availableLocales, noExtensionsLocale).
    std::string available_locale =
        BestAvailableLocale(available_locales, no_extension_locale);

    // 2. c. If availableLocale is not undefined, append locale to the
    //       end of subset.
    if (!available_locale.empty()) {
      subset.push_back(locale);
    }
  }

  // 3. Return subset.
  return subset;
}

// ECMA 402 9.2.8 BestFitSupportedLocales(availableLocales, requestedLocales)
// https://tc39.github.io/ecma402/#sec-bestfitsupportedlocales
std::vector<std::string> BestFitSupportedLocales(
    const std::set<std::string>& available_locales,
    const std::vector<std::string>& requested_locales) {
  return LookupSupportedLocales(available_locales, requested_locales);
}

// ecma262 #sec-createarrayfromlist
Handle<JSArray> CreateArrayFromList(Isolate* isolate,
                                    std::vector<std::string> elements,
                                    PropertyAttributes attr) {
  Factory* factory = isolate->factory();
  // Let array be ! ArrayCreate(0).
  Handle<JSArray> array = factory->NewJSArray(0);

  uint32_t length = static_cast<uint32_t>(elements.size());
  // 3. Let n be 0.
  // 4. For each element e of elements, do
  for (uint32_t i = 0; i < length; i++) {
    // a. Let status be CreateDataProperty(array, ! ToString(n), e).
    const std::string& part = elements[i];
    Handle<String> value =
        factory->NewStringFromUtf8(CStrVector(part.c_str())).ToHandleChecked();
    JSObject::AddDataElement(array, i, value, attr);
  }
  // 5. Return array.
  return array;
}

// TODO(bstell): should this be moved somewhere where it is reusable?
// Implement steps 5, 6, 7 for ECMA 402 9.2.9 SupportedLocales
// https://tc39.github.io/ecma402/#sec-supportedlocales
MaybeHandle<JSObject> CreateReadOnlyArray(Isolate* isolate,
                                          std::vector<std::string> elements) {
  if (elements.size() >= kMaxUInt32) {
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayLength), JSObject);
  }

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);

  // 5. Let subset be CreateArrayFromList(elements).
  Handle<JSArray> subset = CreateArrayFromList(isolate, elements, attr);

  // 6. Let keys be subset.[[OwnPropertyKeys]]().

  // 7.a. Let desc be PropertyDescriptor { [[Configurable]]: false,
  //          [[Writable]]: false }.
  PropertyDescriptor desc;
  desc.set_writable(false);
  desc.set_configurable(false);

  // 7.b. Perform ! DefinePropertyOrThrow(subset, P, desc).
  JSArray::ArraySetLength(isolate, subset, &desc, kThrowOnError).ToChecked();
  return subset;
}

// ECMA 402 9.2.9 SupportedLocales(availableLocales, requestedLocales, options)
// https://tc39.github.io/ecma402/#sec-supportedlocales
MaybeHandle<JSObject> SupportedLocales(
    Isolate* isolate, Intl::ICUService service,
    const std::set<std::string>& available_locales,
    const std::vector<std::string>& requested_locales, Handle<Object> options) {
  std::vector<std::string> supported_locales;

  // 2. Else, let matcher be "best fit".
  Intl::MatcherOption matcher = Intl::MatcherOption::kBestFit;

  // 1. If options is not undefined, then
  if (!options->IsUndefined(isolate)) {
    // 1. a. Let options be ? ToObject(options).
    Handle<JSReceiver> options_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_obj,
                               Object::ToObject(isolate, options), JSObject);

    // 1. b. Let matcher be ? GetOption(options, "localeMatcher", "string",
    //       « "lookup", "best fit" », "best fit").
    std::unique_ptr<char[]> matcher_str = nullptr;
    std::vector<const char*> matcher_values = {"lookup", "best fit"};
    Maybe<bool> maybe_found_matcher = Intl::GetStringOption(
        isolate, options_obj, "localeMatcher", matcher_values,
        ICUServiceToString(service), &matcher_str);
    MAYBE_RETURN(maybe_found_matcher, MaybeHandle<JSObject>());
    if (maybe_found_matcher.FromJust()) {
      DCHECK_NOT_NULL(matcher_str.get());
      if (strcmp(matcher_str.get(), "lookup") == 0) {
        matcher = Intl::MatcherOption::kLookup;
      }
    }
  }

  // 3. If matcher is "best fit", then
  //    a. Let supportedLocales be BestFitSupportedLocales(availableLocales,
  //       requestedLocales).
  if (matcher == Intl::MatcherOption::kBestFit) {
    supported_locales =
        BestFitSupportedLocales(available_locales, requested_locales);
  } else {
    // 4. Else,
    //    a. Let supportedLocales be LookupSupportedLocales(availableLocales,
    //       requestedLocales).
    DCHECK_EQ(matcher, Intl::MatcherOption::kLookup);
    supported_locales =
        LookupSupportedLocales(available_locales, requested_locales);
  }

  // TODO(jkummerow): Possibly revisit why the spec has the individual entries
  // readonly but the array is not frozen.
  // https://github.com/tc39/ecma402/issues/258

  // 5. Let subset be CreateArrayFromList(supportedLocales).
  // 6. Let keys be subset.[[OwnPropertyKeys]]().
  // 7. For each element P of keys in List order, do
  //    a. Let desc be PropertyDescriptor { [[Configurable]]: false,
  //       [[Writable]]: false }.
  //    b. Perform ! DefinePropertyOrThrow(subset, P, desc).
  MaybeHandle<JSObject> subset =
      CreateReadOnlyArray(isolate, supported_locales);

  // 8. Return subset.
  return subset;
}
}  // namespace

// ecma-402 #sec-intl.getcanonicallocales
MaybeHandle<JSArray> Intl::GetCanonicalLocales(Isolate* isolate,
                                               Handle<Object> locales) {
  // 1. Let ll be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_ll =
      CanonicalizeLocaleList(isolate, locales, false);
  MAYBE_RETURN(maybe_ll, MaybeHandle<JSArray>());

  // 2. Return CreateArrayFromList(ll).
  PropertyAttributes attr = static_cast<PropertyAttributes>(NONE);
  return CreateArrayFromList(isolate, maybe_ll.FromJust(), attr);
}

// ECMA 402 Intl.*.supportedLocalesOf
MaybeHandle<JSObject> Intl::SupportedLocalesOf(Isolate* isolate,
                                               Intl::ICUService service,
                                               Handle<Object> locales,
                                               Handle<Object> options) {
  // Let availableLocales be %Collator%.[[AvailableLocales]].
  std::set<std::string> available_locales = GetAvailableLocales(service);

  // Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> requested_locales =
      CanonicalizeLocaleList(isolate, locales, false);
  MAYBE_RETURN(requested_locales, MaybeHandle<JSObject>());

  // Return ? SupportedLocales(availableLocales, requestedLocales, options).
  return SupportedLocales(isolate, service, available_locales,
                          requested_locales.FromJust(), options);
}

namespace {

std::map<std::string, std::string> LookupUnicodeExtensions(
    const icu::Locale& icu_locale, const std::set<std::string>& relevant_keys) {
  std::map<std::string, std::string> extensions;

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> keywords(
      icu_locale.createKeywords(status));
  if (U_FAILURE(status)) return extensions;

  if (!keywords) return extensions;
  char value[ULOC_FULLNAME_CAPACITY];

  int32_t length;
  status = U_ZERO_ERROR;
  for (const char* keyword = keywords->next(&length, status);
       keyword != nullptr; keyword = keywords->next(&length, status)) {
    // Ignore failures in ICU and skip to the next keyword.
    //
    // This is fine.™
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    icu_locale.getKeywordValue(keyword, value, ULOC_FULLNAME_CAPACITY, status);

    // Ignore failures in ICU and skip to the next keyword.
    //
    // This is fine.™
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    const char* bcp47_key = uloc_toUnicodeLocaleKey(keyword);

    // Ignore keywords that we don't recognize - spec allows that.
    if (bcp47_key && (relevant_keys.find(bcp47_key) != relevant_keys.end())) {
      const char* bcp47_value = uloc_toUnicodeLocaleType(bcp47_key, value);
      extensions.insert(
          std::pair<std::string, std::string>(bcp47_key, bcp47_value));
    }
  }

  return extensions;
}

// ecma402/#sec-lookupmatcher
std::string LookupMatcher(Isolate* isolate,
                          const std::set<std::string>& available_locales,
                          const std::vector<std::string>& requested_locales) {
  // 1. Let result be a new Record.
  std::string result;

  // 2. For each element locale of requestedLocales in List order, do
  for (const std::string& locale : requested_locales) {
    // 2. a. Let noExtensionsLocale be the String value that is locale
    //       with all Unicode locale extension sequences removed.
    ParsedLocale parsed_locale = ParseBCP47Locale(locale);
    std::string no_extensions_locale = parsed_locale.no_extensions_locale;

    // 2. b. Let availableLocale be
    //       BestAvailableLocale(availableLocales, noExtensionsLocale).
    std::string available_locale =
        BestAvailableLocale(available_locales, no_extensions_locale);

    // 2. c. If availableLocale is not undefined, append locale to the
    //       end of subset.
    if (!available_locale.empty()) {
      // Note: The following steps are not performed here because we
      // can use ICU to parse the unicode locale extension sequence
      // as part of Intl::ResolveLocale.
      //
      // There's no need to separate the unicode locale extensions
      // right here. Instead just return the available locale with the
      // extensions.
      //
      // 2. c. i. Set result.[[locale]] to availableLocale.
      // 2. c. ii. If locale and noExtensionsLocale are not the same
      // String value, then
      // 2. c. ii. 1. Let extension be the String value consisting of
      // the first substring of locale that is a Unicode locale
      // extension sequence.
      // 2. c. ii. 2. Set result.[[extension]] to extension.
      // 2. c. iii. Return result.
      return available_locale + parsed_locale.extension;
    }
  }

  // 3. Let defLocale be DefaultLocale();
  // 4. Set result.[[locale]] to defLocale.
  // 5. Return result.
  return Intl::DefaultLocale(isolate);
}

}  // namespace

// This function doesn't correspond exactly with the spec. Instead
// we use ICU to do all the string manipulations that the spec
// peforms.
//
// The spec uses this function to normalize values for various
// relevant extension keys (such as disallowing "search" for
// collation). Instead of doing this here, we let the callers of
// this method perform such normalization.
//
// ecma402/#sec-resolvelocale
Intl::ResolvedLocale Intl::ResolveLocale(
    Isolate* isolate, const std::set<std::string>& available_locales,
    const std::vector<std::string>& requested_locales, MatcherOption matcher,
    const std::set<std::string>& relevant_extension_keys) {
  std::string locale;
  if (matcher == Intl::MatcherOption::kLookup) {
    locale = LookupMatcher(isolate, available_locales, requested_locales);
  } else if (matcher == Intl::MatcherOption::kBestFit) {
    // TODO(intl): Implement better lookup algorithm.
    locale = LookupMatcher(isolate, available_locales, requested_locales);
  }

  icu::Locale icu_locale = CreateICULocale(locale);
  std::map<std::string, std::string> extensions =
      LookupUnicodeExtensions(icu_locale, relevant_extension_keys);

  // TODO(gsathya): Remove privateuse subtags from extensions.

  return Intl::ResolvedLocale{locale, icu_locale, extensions};
}

Managed<icu::UnicodeString>* Intl::SetTextToBreakIterator(
    Isolate* isolate, Handle<String> text, icu::BreakIterator* break_iterator) {
  icu::UnicodeString* u_text =
      (icu::UnicodeString*)(Intl::ToICUUnicodeString(isolate, text).clone());

  Handle<Managed<icu::UnicodeString>> new_u_text =
      Managed<icu::UnicodeString>::FromRawPtr(isolate, 0, u_text);

  break_iterator->setText(*u_text);
  return *new_u_text;
}

// ecma262 #sec-string.prototype.normalize
MaybeHandle<String> Intl::Normalize(Isolate* isolate, Handle<String> string,
                                    Handle<Object> form_input) {
  const char* form_name;
  UNormalization2Mode form_mode;
  if (form_input->IsUndefined(isolate)) {
    // default is FNC
    form_name = "nfc";
    form_mode = UNORM2_COMPOSE;
  } else {
    Handle<String> form;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, form,
                               Object::ToString(isolate, form_input), String);

    if (String::Equals(isolate, form, isolate->factory()->NFC_string())) {
      form_name = "nfc";
      form_mode = UNORM2_COMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFD_string())) {
      form_name = "nfc";
      form_mode = UNORM2_DECOMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFKC_string())) {
      form_name = "nfkc";
      form_mode = UNORM2_COMPOSE;
    } else if (String::Equals(isolate, form,
                              isolate->factory()->NFKD_string())) {
      form_name = "nfkc";
      form_mode = UNORM2_DECOMPOSE;
    } else {
      Handle<String> valid_forms =
          isolate->factory()->NewStringFromStaticChars("NFC, NFD, NFKC, NFKD");
      THROW_NEW_ERROR(
          isolate,
          NewRangeError(MessageTemplate::kNormalizationForm, valid_forms),
          String);
    }
  }

  int length = string->length();
  string = String::Flatten(isolate, string);
  icu::UnicodeString result;
  std::unique_ptr<uc16[]> sap;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString input = ToICUUnicodeString(isolate, string);
  // Getting a singleton. Should not free it.
  const icu::Normalizer2* normalizer =
      icu::Normalizer2::getInstance(nullptr, form_name, form_mode, status);
  DCHECK(U_SUCCESS(status));
  CHECK_NOT_NULL(normalizer);
  int32_t normalized_prefix_length =
      normalizer->spanQuickCheckYes(input, status);
  // Quick return if the input is already normalized.
  if (length == normalized_prefix_length) return string;
  icu::UnicodeString unnormalized =
      input.tempSubString(normalized_prefix_length);
  // Read-only alias of the normalized prefix.
  result.setTo(false, input.getBuffer(), normalized_prefix_length);
  // copy-on-write; normalize the suffix and append to |result|.
  normalizer->normalizeSecondAndAppend(result, unnormalized, status);

  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), String);
  }

  return Intl::ToString(isolate, result);
}

// ICUTimezoneCache calls out to ICU for TimezoneCache
// functionality in a straightforward way.
class ICUTimezoneCache : public base::TimezoneCache {
 public:
  ICUTimezoneCache() : timezone_(nullptr) { Clear(); }

  ~ICUTimezoneCache() override { Clear(); };

  const char* LocalTimezone(double time_ms) override;

  double DaylightSavingsOffset(double time_ms) override;

  double LocalTimeOffset(double time_ms, bool is_utc) override;

  void Clear() override;

 private:
  icu::TimeZone* GetTimeZone();

  bool GetOffsets(double time_ms, bool is_utc, int32_t* raw_offset,
                  int32_t* dst_offset);

  icu::TimeZone* timezone_;

  std::string timezone_name_;
  std::string dst_timezone_name_;
};

const char* ICUTimezoneCache::LocalTimezone(double time_ms) {
  bool is_dst = DaylightSavingsOffset(time_ms) != 0;
  std::string* name = is_dst ? &dst_timezone_name_ : &timezone_name_;
  if (name->empty()) {
    icu::UnicodeString result;
    GetTimeZone()->getDisplayName(is_dst, icu::TimeZone::LONG, result);
    result += '\0';

    icu::StringByteSink<std::string> byte_sink(name);
    result.toUTF8(byte_sink);
  }
  DCHECK(!name->empty());
  return name->c_str();
}

icu::TimeZone* ICUTimezoneCache::GetTimeZone() {
  if (timezone_ == nullptr) {
    timezone_ = icu::TimeZone::createDefault();
  }
  return timezone_;
}

bool ICUTimezoneCache::GetOffsets(double time_ms, bool is_utc,
                                  int32_t* raw_offset, int32_t* dst_offset) {
  UErrorCode status = U_ZERO_ERROR;
  // TODO(jshin): ICU TimeZone class handles skipped time differently from
  // Ecma 262 (https://github.com/tc39/ecma262/pull/778) and icu::TimeZone
  // class does not expose the necessary API. Fixing
  // http://bugs.icu-project.org/trac/ticket/13268 would make it easy to
  // implement the proposed spec change. A proposed fix for ICU is
  //    https://chromium-review.googlesource.com/851265 .
  // In the meantime, use an internal (still public) API of icu::BasicTimeZone.
  // Once it's accepted by the upstream, get rid of cast. Note that casting
  // TimeZone to BasicTimeZone is safe because we know that icu::TimeZone used
  // here is a BasicTimeZone.
  if (is_utc) {
    GetTimeZone()->getOffset(time_ms, false, *raw_offset, *dst_offset, status);
  } else {
    static_cast<const icu::BasicTimeZone*>(GetTimeZone())
        ->getOffsetFromLocal(time_ms, icu::BasicTimeZone::kFormer,
                             icu::BasicTimeZone::kFormer, *raw_offset,
                             *dst_offset, status);
  }

  return U_SUCCESS(status);
}

double ICUTimezoneCache::DaylightSavingsOffset(double time_ms) {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(time_ms, true, &raw_offset, &dst_offset)) return 0;
  return dst_offset;
}

double ICUTimezoneCache::LocalTimeOffset(double time_ms, bool is_utc) {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(time_ms, is_utc, &raw_offset, &dst_offset)) return 0;
  return raw_offset + dst_offset;
}

void ICUTimezoneCache::Clear() {
  delete timezone_;
  timezone_ = nullptr;
  timezone_name_.clear();
  dst_timezone_name_.clear();
}

base::TimezoneCache* Intl::CreateTimeZoneCache() {
  return FLAG_icu_timezone_data ? new ICUTimezoneCache()
                                : base::OS::CreateTimezoneCache();
}

}  // namespace internal
}  // namespace v8

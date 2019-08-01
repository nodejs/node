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

#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/string.h"
#include "src/strings/string-case.h"
#include "unicode/basictz.h"
#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/datefmt.h"
#include "unicode/decimfmt.h"
#include "unicode/formattedvalue.h"
#include "unicode/locid.h"
#include "unicode/normalizer2.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/timezone.h"
#include "unicode/ustring.h"
#include "unicode/uvernum.h"  // U_ICU_VERSION_MAJOR_NUM

#define XSTR(s) STR(s)
#define STR(s) #s
static_assert(
    V8_MINIMUM_ICU_VERSION <= U_ICU_VERSION_MAJOR_NUM,
    "v8 is required to build with ICU " XSTR(V8_MINIMUM_ICU_VERSION) " and up");
#undef STR
#undef XSTR

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

inline int FindFirstUpperOrNonAscii(String s, int length) {
  for (int index = 0; index < length; ++index) {
    uint16_t ch = s.Get(index);
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
      CopyChars(dest->get(), flat.ToOneByteVector().begin(), length);
    }
    return reinterpret_cast<const UChar*>(dest->get());
  } else {
    return reinterpret_cast<const UChar*>(flat.ToUC16Vector().begin());
  }
}

template <typename T>
MaybeHandle<T> New(Isolate* isolate, Handle<JSFunction> constructor,
                   Handle<Object> locales, Handle<Object> options) {
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSObject::New(constructor, constructor, Handle<AllocationSite>::null()),
      T);
  return T::Initialize(isolate, Handle<T>::cast(result), locales, options);
}
}  // namespace

const uint8_t* Intl::ToLatin1LowerTable() { return &kToLower[0]; }

icu::UnicodeString Intl::ToICUUnicodeString(Isolate* isolate,
                                            Handle<String> string) {
  DCHECK(string->IsFlat());
  DisallowHeapAllocation no_gc;
  std::unique_ptr<uc16[]> sap;
  // Short one-byte strings can be expanded on the stack to avoid allocating a
  // temporary buffer.
  constexpr int kShortStringSize = 80;
  UChar short_string_buffer[kShortStringSize];
  const UChar* uchar_buffer = nullptr;
  const String::FlatContent& flat = string->GetFlatContent(no_gc);
  int32_t length = string->length();
  if (flat.IsOneByte() && length <= kShortStringSize) {
    CopyChars(short_string_buffer, flat.ToOneByteVector().begin(), length);
    uchar_buffer = short_string_buffer;
  } else {
    uchar_buffer = GetUCharBufferFromFlat(flat, &sap, length);
  }
  return icu::UnicodeString(uchar_buffer, length);
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
    String::FlatContent flat = s->GetFlatContent(no_gc);
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, src_length);
    status = U_ZERO_ERROR;
    dest_length =
        case_converter(reinterpret_cast<UChar*>(result->GetChars(no_gc)),
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
String Intl::ConvertOneByteToLower(String src, String dst) {
  DCHECK_EQ(src.length(), dst.length());
  DCHECK(src.IsOneByteRepresentation());
  DCHECK(src.IsFlat());
  DCHECK(dst.IsSeqOneByteString());

  DisallowHeapAllocation no_gc;

  const int length = src.length();
  String::FlatContent src_flat = src.GetFlatContent(no_gc);
  uint8_t* dst_data = SeqOneByteString::cast(dst).GetChars(no_gc);

  if (src_flat.IsOneByte()) {
    const uint8_t* src_data = src_flat.ToOneByteVector().begin();

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

    const uint16_t* src_data = src_flat.ToUC16Vector().begin();
    CopyChars(dst_data, src_data, index_to_first_unprocessed);
    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dst_data[index] = ToLatin1Lower(static_cast<uint16_t>(src_data[index]));
    }
  }

  return dst;
}

MaybeHandle<String> Intl::ConvertToLower(Isolate* isolate, Handle<String> s) {
  if (!s->IsOneByteRepresentation()) {
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
  if (s->IsOneByteRepresentation() && length > 0) {
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

    DCHECK(s->IsFlat());
    int sharp_s_count;
    bool is_result_single_byte;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent(no_gc);
      uint8_t* dest = result->GetChars(no_gc);
      if (flat.IsOneByte()) {
        Vector<const uint8_t> src = flat.ToOneByteVector();
        bool has_changed_character = false;
        int index_to_first_unprocessed = FastAsciiConvert<false>(
            reinterpret_cast<char*>(result->GetChars(no_gc)),
            reinterpret_cast<const char*>(src.begin()), length,
            &has_changed_character);
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
    String::FlatContent flat = s->GetFlatContent(no_gc);
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
  if (U_SUCCESS(status)) return numbering_system->getName();
  return "latn";
}

icu::Locale Intl::CreateICULocale(const std::string& bcp47_locale) {
  DisallowHeapAllocation no_gc;

  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;

  icu::Locale icu_locale = icu::Locale::forLanguageTag(bcp47_locale, status);
  CHECK(U_SUCCESS(status));
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

std::set<std::string> Intl::BuildLocaleSet(
    const icu::Locale* icu_available_locales, int32_t count) {
  std::set<std::string> locales;
  for (int32_t i = 0; i < count; ++i) {
    std::string locale =
        Intl::ToLanguageTag(icu_available_locales[i]).FromJust();
    locales.insert(locale);

    std::string shortened_locale;
    if (RemoveLocaleScriptTag(locale, &shortened_locale)) {
      std::replace(shortened_locale.begin(), shortened_locale.end(), '_', '-');
      locales.insert(shortened_locale);
    }
  }

  return locales;
}

Maybe<std::string> Intl::ToLanguageTag(const icu::Locale& locale) {
  UErrorCode status = U_ZERO_ERROR;
  std::string res = locale.toLanguageTag<std::string>(status);
  if (U_FAILURE(status)) {
    return Nothing<std::string>();
  }
  CHECK(U_SUCCESS(status));

  // Hack to remove -true and -yes from unicode extensions
  // Address https://crbug.com/v8/8565
  // TODO(ftang): Move the following "remove true" logic into ICU toLanguageTag
  // by fixing ICU-20310.
  size_t u_ext_start = res.find("-u-");
  if (u_ext_start != std::string::npos) {
    // remove "-true" and "-yes" after -u-
    const std::vector<std::string> remove_items({"-true", "-yes"});
    for (auto item = remove_items.begin(); item != remove_items.end(); item++) {
      for (size_t sep_remove =
               res.find(*item, u_ext_start + 5 /* strlen("-u-xx") == 5 */);
           sep_remove != std::string::npos; sep_remove = res.find(*item)) {
        size_t end_of_sep_remove = sep_remove + item->length();
        if (res.length() == end_of_sep_remove ||
            res.at(end_of_sep_remove) == '-') {
          res.erase(sep_remove, item->length());
        }
      }
    }
  }
  return Just(res);
}

namespace {
std::string DefaultLocale(Isolate* isolate) {
  if (isolate->default_locale().empty()) {
    icu::Locale default_locale;
    // Translate ICU's fallback locale to a well-known locale.
    if (strcmp(default_locale.getName(), "en_US_POSIX") == 0 ||
        strcmp(default_locale.getName(), "c") == 0) {
      isolate->set_default_locale("en-US");
    } else {
      // Set the locale
      isolate->set_default_locale(
          default_locale.isBogus()
              ? "und"
              : Intl::ToLanguageTag(default_locale).FromJust());
    }
    DCHECK(!isolate->default_locale().empty());
  }
  return isolate->default_locale();
}
}  // namespace

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

bool IsLowerAscii(char c) { return c >= 'a' && c < 'z'; }

bool IsTwoLetterLanguage(const std::string& locale) {
  // Two letters, both in range 'a'-'z'...
  return locale.length() == 2 && IsLowerAscii(locale[0]) &&
         IsLowerAscii(locale[1]);
}

bool IsDeprecatedLanguage(const std::string& locale) {
  //  Check if locale is one of the deprecated language tags:
  return locale == "in" || locale == "iw" || locale == "ji" || locale == "jw" ||
         locale == "mo";
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

  return Intl::CanonicalizeLanguageTag(isolate, locale);
}

Maybe<std::string> Intl::CanonicalizeLanguageTag(Isolate* isolate,
                                                 const std::string& locale_in) {
  std::string locale = locale_in;

  if (locale.length() == 0 ||
      !String::IsAscii(locale.data(), static_cast<int>(locale.length()))) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(
            MessageTemplate::kInvalidLanguageTag,
            isolate->factory()->NewStringFromAsciiChecked(locale.c_str())),
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
  // uloc_forLanguageTag checks the structrual validity. If the input BCP47
  // language tag is parsed all the way to the end, it indicates that the input
  // is structurally valid. Due to a couple of bugs, we can't use it
  // without Chromium patches or ICU 62 or earlier.
  icu::Locale icu_locale = icu::Locale::forLanguageTag(locale.c_str(), error);
  if (U_FAILURE(error) || icu_locale.isBogus()) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(
            MessageTemplate::kInvalidLanguageTag,
            isolate->factory()->NewStringFromAsciiChecked(locale.c_str())),
        Nothing<std::string>());
  }
  Maybe<std::string> maybe_to_language_tag = Intl::ToLanguageTag(icu_locale);
  if (maybe_to_language_tag.IsNothing()) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(
            MessageTemplate::kInvalidLanguageTag,
            isolate->factory()->NewStringFromAsciiChecked(locale.c_str())),
        Nothing<std::string>());
  }

  return maybe_to_language_tag;
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
  // 3. If Type(locales) is String or locales has an [[InitializedLocale]]
  // internal slot,  then
  if (locales->IsJSLocale()) {
    // Since this value came from JSLocale, which is already went though the
    // CanonializeLanguageTag process once, therefore there are no need to
    // call CanonializeLanguageTag again.
    seen.push_back(JSLocale::ToString(Handle<JSLocale>::cast(locales)));
    return Just(seen);
  }
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
    // 7c iii. If Type(kValue) is Object and kValue has an [[InitializedLocale]]
    // internal slot, then
    std::string canonicalized_tag;
    if (k_value->IsJSLocale()) {
      // 7c iii. 1. Let tag be kValue.[[Locale]].
      canonicalized_tag = JSLocale::ToString(Handle<JSLocale>::cast(k_value));
      // 7c iv. Else,
    } else {
      // 7c iv 1. Let tag be ? ToString(kValue).
      // 7c v. If IsStructurallyValidLanguageTag(tag) is false, throw a
      // RangeError exception.
      // 7c vi. Let canonicalizedTag be CanonicalizeLanguageTag(tag).
      if (!CanonicalizeLanguageTag(isolate, k_value).To(&canonicalized_tag)) {
        return Nothing<std::vector<std::string>>();
      }
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
                                     ? DefaultLocale(isolate)
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
  // We only cache the instance when both locales and options are undefined,
  // as that is the only case when the specified side-effects of examining
  // those arguments are unobservable.
  bool can_cache =
      locales->IsUndefined(isolate) && options->IsUndefined(isolate);
  if (can_cache) {
    // Both locales and options are undefined, check the cache.
    icu::Collator* cached_icu_collator =
        static_cast<icu::Collator*>(isolate->get_cached_icu_object(
            Isolate::ICUObjectCacheType::kDefaultCollator));
    // We may use the cached icu::Collator for a fast path.
    if (cached_icu_collator != nullptr) {
      return Intl::CompareStrings(isolate, *cached_icu_collator, string1,
                                  string2);
    }
  }

  Handle<JSFunction> constructor = Handle<JSFunction>(
      JSFunction::cast(
          isolate->context().native_context().intl_collator_function()),
      isolate);

  Handle<JSCollator> collator;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, collator,
      New<JSCollator>(isolate, constructor, locales, options), Object);
  if (can_cache) {
    isolate->set_icu_object_in_cache(
        Isolate::ICUObjectCacheType::kDefaultCollator,
        std::static_pointer_cast<icu::UMemory>(collator->icu_collator().get()));
  }
  icu::Collator* icu_collator = collator->icu_collator().raw();
  return Intl::CompareStrings(isolate, *icu_collator, string1, string2);
}

// ecma402/#sec-collator-comparestrings
Handle<Object> Intl::CompareStrings(Isolate* isolate,
                                    const icu::Collator& icu_collator,
                                    Handle<String> string1,
                                    Handle<String> string2) {
  Factory* factory = isolate->factory();

  // Early return for identical strings.
  if (string1.is_identical_to(string2)) {
    return factory->NewNumberFromInt(UCollationResult::UCOL_EQUAL);
  }

  // Early return for empty strings.
  if (string1->length() == 0) {
    return factory->NewNumberFromInt(string2->length() == 0
                                         ? UCollationResult::UCOL_EQUAL
                                         : UCollationResult::UCOL_LESS);
  }
  if (string2->length() == 0) {
    return factory->NewNumberFromInt(UCollationResult::UCOL_GREATER);
  }

  string1 = String::Flatten(isolate, string1);
  string2 = String::Flatten(isolate, string2);

  UCollationResult result;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString string_val1 = Intl::ToICUUnicodeString(isolate, string1);
  icu::UnicodeString string_val2 = Intl::ToICUUnicodeString(isolate, string2);
  result = icu_collator.compare(string_val1, string_val2, status);
  DCHECK(U_SUCCESS(status));

  return factory->NewNumberFromInt(result);
}

// ecma402/#sup-properties-of-the-number-prototype-object
MaybeHandle<String> Intl::NumberToLocaleString(Isolate* isolate,
                                               Handle<Object> num,
                                               Handle<Object> locales,
                                               Handle<Object> options) {
  Handle<Object> numeric_obj;
  if (FLAG_harmony_intl_bigint) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, numeric_obj,
                               Object::ToNumeric(isolate, num), String);
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, numeric_obj,
                               Object::ToNumber(isolate, num), String);
  }

  // We only cache the instance when both locales and options are undefined,
  // as that is the only case when the specified side-effects of examining
  // those arguments are unobservable.
  bool can_cache =
      locales->IsUndefined(isolate) && options->IsUndefined(isolate);
  if (can_cache) {
    icu::number::LocalizedNumberFormatter* cached_number_format =
        static_cast<icu::number::LocalizedNumberFormatter*>(
            isolate->get_cached_icu_object(
                Isolate::ICUObjectCacheType::kDefaultNumberFormat));
    // We may use the cached icu::NumberFormat for a fast path.
    if (cached_number_format != nullptr) {
      return JSNumberFormat::FormatNumeric(isolate, *cached_number_format,
                                           numeric_obj);
    }
  }

  Handle<JSFunction> constructor = Handle<JSFunction>(
      JSFunction::cast(
          isolate->context().native_context().intl_number_format_function()),
      isolate);
  Handle<JSNumberFormat> number_format;
  // 2. Let numberFormat be ? Construct(%NumberFormat%, « locales, options »).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, number_format,
      New<JSNumberFormat>(isolate, constructor, locales, options), String);

  if (can_cache) {
    isolate->set_icu_object_in_cache(
        Isolate::ICUObjectCacheType::kDefaultNumberFormat,
        std::static_pointer_cast<icu::UMemory>(
            number_format->icu_number_formatter().get()));
  }

  // Return FormatNumber(numberFormat, x).
  icu::number::LocalizedNumberFormatter* icu_number_format =
      number_format->icu_number_formatter().raw();
  return JSNumberFormat::FormatNumeric(isolate, *icu_number_format,
                                       numeric_obj);
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

Maybe<Intl::NumberFormatDigitOptions> Intl::SetNumberFormatDigitOptions(
    Isolate* isolate, Handle<JSReceiver> options, int mnfd_default,
    int mxfd_default) {
  Intl::NumberFormatDigitOptions digit_options;

  // 5. Let mnid be ? GetNumberOption(options, "minimumIntegerDigits,", 1, 21,
  // 1).
  int mnid;
  if (!GetNumberOption(isolate, options, "minimumIntegerDigits", 1, 21, 1)
           .To(&mnid)) {
    return Nothing<NumberFormatDigitOptions>();
  }

  // 6. Let mnfd be ? GetNumberOption(options, "minimumFractionDigits", 0, 20,
  // mnfdDefault).
  int mnfd;
  if (!GetNumberOption(isolate, options, "minimumFractionDigits", 0, 20,
                       mnfd_default)
           .To(&mnfd)) {
    return Nothing<NumberFormatDigitOptions>();
  }

  // 7. Let mxfdActualDefault be max( mnfd, mxfdDefault ).
  int mxfd_actual_default = std::max(mnfd, mxfd_default);

  // 8. Let mxfd be ? GetNumberOption(options,
  // "maximumFractionDigits", mnfd, 20, mxfdActualDefault).
  int mxfd;
  if (!GetNumberOption(isolate, options, "maximumFractionDigits", mnfd, 20,
                       mxfd_actual_default)
           .To(&mxfd)) {
    return Nothing<NumberFormatDigitOptions>();
  }

  // 9.  Let mnsd be ? Get(options, "minimumSignificantDigits").
  Handle<Object> mnsd_obj;
  Handle<String> mnsd_str =
      isolate->factory()->minimumSignificantDigits_string();
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mnsd_obj, JSReceiver::GetProperty(isolate, options, mnsd_str),
      Nothing<NumberFormatDigitOptions>());

  // 10. Let mxsd be ? Get(options, "maximumSignificantDigits").
  Handle<Object> mxsd_obj;
  Handle<String> mxsd_str =
      isolate->factory()->maximumSignificantDigits_string();
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mxsd_obj, JSReceiver::GetProperty(isolate, options, mxsd_str),
      Nothing<NumberFormatDigitOptions>());

  // 11. Set intlObj.[[MinimumIntegerDigits]] to mnid.
  digit_options.minimum_integer_digits = mnid;

  // 12. Set intlObj.[[MinimumFractionDigits]] to mnfd.
  digit_options.minimum_fraction_digits = mnfd;

  // 13. Set intlObj.[[MaximumFractionDigits]] to mxfd.
  digit_options.maximum_fraction_digits = mxfd;

  // 14. If mnsd is not undefined or mxsd is not undefined, then
  if (!mnsd_obj->IsUndefined(isolate) || !mxsd_obj->IsUndefined(isolate)) {
    // 14. a. Let mnsd be ? DefaultNumberOption(mnsd, 1, 21, 1).
    int mnsd;
    if (!DefaultNumberOption(isolate, mnsd_obj, 1, 21, 1, mnsd_str).To(&mnsd)) {
      return Nothing<NumberFormatDigitOptions>();
    }

    // 14. b. Let mxsd be ? DefaultNumberOption(mxsd, mnsd, 21, 21).
    int mxsd;
    if (!DefaultNumberOption(isolate, mxsd_obj, mnsd, 21, 21, mxsd_str)
             .To(&mxsd)) {
      return Nothing<NumberFormatDigitOptions>();
    }

    // 14. c. Set intlObj.[[MinimumSignificantDigits]] to mnsd.
    digit_options.minimum_significant_digits = mnsd;

    // 14. d. Set intlObj.[[MaximumSignificantDigits]] to mxsd.
    digit_options.maximum_significant_digits = mxsd;
  } else {
    digit_options.minimum_significant_digits = 0;
    digit_options.maximum_significant_digits = 0;
  }

  return Just(digit_options);
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
  parsed_locale.extension = locale.substr(
      unicode_extension_start, unicode_extension_end - unicode_extension_start);
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

// ECMA 402 9.2.9 SupportedLocales(availableLocales, requestedLocales, options)
// https://tc39.github.io/ecma402/#sec-supportedlocales
MaybeHandle<JSObject> SupportedLocales(
    Isolate* isolate, const char* method,
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
    Maybe<Intl::MatcherOption> maybe_locale_matcher =
        Intl::GetLocaleMatcher(isolate, options_obj, method);
    MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSObject>());
    matcher = maybe_locale_matcher.FromJust();
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

  // 5. Return CreateArrayFromList(supportedLocales).
  PropertyAttributes attr = static_cast<PropertyAttributes>(NONE);
  return CreateArrayFromList(isolate, supported_locales, attr);
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
MaybeHandle<JSObject> Intl::SupportedLocalesOf(
    Isolate* isolate, const char* method,
    const std::set<std::string>& available_locales, Handle<Object> locales,
    Handle<Object> options) {
  // Let availableLocales be %Collator%.[[AvailableLocales]].

  // Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> requested_locales =
      CanonicalizeLocaleList(isolate, locales, false);
  MAYBE_RETURN(requested_locales, MaybeHandle<JSObject>());

  // Return ? SupportedLocales(availableLocales, requestedLocales, options).
  return SupportedLocales(isolate, method, available_locales,
                          requested_locales.FromJust(), options);
}

namespace {

template <typename T>
bool IsValidExtension(const icu::Locale& locale, const char* key,
                      const std::string& value) {
  const char* legacy_type = uloc_toLegacyType(key, value.c_str());
  if (legacy_type == nullptr) {
    return false;
  }
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> enumeration(
      T::getKeywordValuesForLocale(key, icu::Locale(locale.getBaseName()),
                                   false, status));
  if (U_FAILURE(status)) {
    return false;
  }
  int32_t length;
  for (const char* item = enumeration->next(&length, status);
       U_SUCCESS(status) && item != nullptr;
       item = enumeration->next(&length, status)) {
    if (strcmp(legacy_type, item) == 0) {
      return true;
    }
  }
  return false;
}

bool IsValidCollation(const icu::Locale& locale, const std::string& value) {
  std::set<std::string> invalid_values = {"standard", "search"};
  if (invalid_values.find(value) != invalid_values.end()) return false;
  return IsValidExtension<icu::Collator>(locale, "collation", value);
}

}  // namespace

bool Intl::IsValidCalendar(const icu::Locale& locale,
                           const std::string& value) {
  return IsValidExtension<icu::Calendar>(locale, "calendar", value);
}

namespace {

bool IsValidNumberingSystem(const std::string& value) {
  std::set<std::string> invalid_values = {"native", "traditio", "finance"};
  if (invalid_values.find(value) != invalid_values.end()) return false;
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::NumberingSystem> numbering_system(
      icu::NumberingSystem::createInstanceByName(value.c_str(), status));
  return U_SUCCESS(status) && numbering_system.get() != nullptr;
}

std::map<std::string, std::string> LookupAndValidateUnicodeExtensions(
    icu::Locale* icu_locale, const std::set<std::string>& relevant_keys) {
  std::map<std::string, std::string> extensions;

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> keywords(
      icu_locale->createKeywords(status));
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

    icu_locale->getKeywordValue(keyword, value, ULOC_FULLNAME_CAPACITY, status);

    // Ignore failures in ICU and skip to the next keyword.
    //
    // This is fine.™
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    const char* bcp47_key = uloc_toUnicodeLocaleKey(keyword);

    if (bcp47_key && (relevant_keys.find(bcp47_key) != relevant_keys.end())) {
      const char* bcp47_value = uloc_toUnicodeLocaleType(bcp47_key, value);
      bool is_valid_value = false;
      // 8.h.ii.1.a If keyLocaleData contains requestedValue, then
      if (strcmp("ca", bcp47_key) == 0) {
        is_valid_value = Intl::IsValidCalendar(*icu_locale, bcp47_value);
      } else if (strcmp("co", bcp47_key) == 0) {
        is_valid_value = IsValidCollation(*icu_locale, bcp47_value);
      } else if (strcmp("hc", bcp47_key) == 0) {
        // https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/calendar.xml
        std::set<std::string> valid_values = {"h11", "h12", "h23", "h24"};
        is_valid_value = valid_values.find(bcp47_value) != valid_values.end();
      } else if (strcmp("lb", bcp47_key) == 0) {
        // https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/segmentation.xml
        std::set<std::string> valid_values = {"strict", "normal", "loose"};
        is_valid_value = valid_values.find(bcp47_value) != valid_values.end();
      } else if (strcmp("kn", bcp47_key) == 0) {
        // https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/collation.xml
        std::set<std::string> valid_values = {"true", "false"};
        is_valid_value = valid_values.find(bcp47_value) != valid_values.end();
      } else if (strcmp("kf", bcp47_key) == 0) {
        // https://www.unicode.org/repos/cldr/tags/latest/common/bcp47/collation.xml
        std::set<std::string> valid_values = {"upper", "lower", "false"};
        is_valid_value = valid_values.find(bcp47_value) != valid_values.end();
      } else if (strcmp("nu", bcp47_key) == 0) {
        is_valid_value = IsValidNumberingSystem(bcp47_value);
      }
      if (is_valid_value) {
        extensions.insert(
            std::pair<std::string, std::string>(bcp47_key, bcp47_value));
        continue;
      }
    }
    status = U_ZERO_ERROR;
    icu_locale->setUnicodeKeywordValue(
        bcp47_key == nullptr ? keyword : bcp47_key, nullptr, status);
    CHECK(U_SUCCESS(status));
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
  return DefaultLocale(isolate);
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
      LookupAndValidateUnicodeExtensions(&icu_locale, relevant_extension_keys);

  std::string canonicalized_locale = Intl::ToLanguageTag(icu_locale).FromJust();

  // TODO(gsathya): Remove privateuse subtags from extensions.

  return Intl::ResolvedLocale{canonicalized_locale, icu_locale, extensions};
}

Managed<icu::UnicodeString> Intl::SetTextToBreakIterator(
    Isolate* isolate, Handle<String> text, icu::BreakIterator* break_iterator) {
  text = String::Flatten(isolate, text);
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
  ICUTimezoneCache() : timezone_(nullptr) { Clear(TimeZoneDetection::kSkip); }

  ~ICUTimezoneCache() override { Clear(TimeZoneDetection::kSkip); }

  const char* LocalTimezone(double time_ms) override;

  double DaylightSavingsOffset(double time_ms) override;

  double LocalTimeOffset(double time_ms, bool is_utc) override;

  void Clear(TimeZoneDetection time_zone_detection) override;

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

void ICUTimezoneCache::Clear(TimeZoneDetection time_zone_detection) {
  delete timezone_;
  timezone_ = nullptr;
  timezone_name_.clear();
  dst_timezone_name_.clear();
  if (time_zone_detection == TimeZoneDetection::kRedetect) {
    icu::TimeZone::adoptDefault(icu::TimeZone::detectHostTimeZone());
  }
}

base::TimezoneCache* Intl::CreateTimeZoneCache() {
  return FLAG_icu_timezone_data ? new ICUTimezoneCache()
                                : base::OS::CreateTimezoneCache();
}

Maybe<Intl::CaseFirst> Intl::GetCaseFirst(Isolate* isolate,
                                          Handle<JSReceiver> options,
                                          const char* method) {
  return Intl::GetStringOption<Intl::CaseFirst>(
      isolate, options, "caseFirst", method, {"upper", "lower", "false"},
      {Intl::CaseFirst::kUpper, Intl::CaseFirst::kLower,
       Intl::CaseFirst::kFalse},
      Intl::CaseFirst::kUndefined);
}

Maybe<Intl::HourCycle> Intl::GetHourCycle(Isolate* isolate,
                                          Handle<JSReceiver> options,
                                          const char* method) {
  return Intl::GetStringOption<Intl::HourCycle>(
      isolate, options, "hourCycle", method, {"h11", "h12", "h23", "h24"},
      {Intl::HourCycle::kH11, Intl::HourCycle::kH12, Intl::HourCycle::kH23,
       Intl::HourCycle::kH24},
      Intl::HourCycle::kUndefined);
}

Maybe<Intl::MatcherOption> Intl::GetLocaleMatcher(Isolate* isolate,
                                                  Handle<JSReceiver> options,
                                                  const char* method) {
  return Intl::GetStringOption<Intl::MatcherOption>(
      isolate, options, "localeMatcher", method, {"best fit", "lookup"},
      {Intl::MatcherOption::kLookup, Intl::MatcherOption::kBestFit},
      Intl::MatcherOption::kLookup);
}

Maybe<bool> Intl::GetNumberingSystem(Isolate* isolate,
                                     Handle<JSReceiver> options,
                                     const char* method,
                                     std::unique_ptr<char[]>* result) {
  const std::vector<const char*> empty_values = {};
  Maybe<bool> maybe = Intl::GetStringOption(isolate, options, "numberingSystem",
                                            empty_values, method, result);
  MAYBE_RETURN(maybe, Nothing<bool>());
  if (maybe.FromJust() && *result != nullptr) {
    if (!IsValidNumberingSystem(result->get())) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(
              MessageTemplate::kInvalid,
              isolate->factory()->numberingSystem_string(),
              isolate->factory()->NewStringFromAsciiChecked(result->get())),
          Nothing<bool>());
    }
    return Just(true);
  }
  return Just(false);
}

Intl::HourCycle Intl::ToHourCycle(const std::string& hc) {
  if (hc == "h11") return Intl::HourCycle::kH11;
  if (hc == "h12") return Intl::HourCycle::kH12;
  if (hc == "h23") return Intl::HourCycle::kH23;
  if (hc == "h24") return Intl::HourCycle::kH24;
  return Intl::HourCycle::kUndefined;
}

const std::set<std::string>& Intl::GetAvailableLocalesForLocale() {
  static base::LazyInstance<Intl::AvailableLocales<icu::Locale>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

const std::set<std::string>& Intl::GetAvailableLocalesForDateFormat() {
  static base::LazyInstance<Intl::AvailableLocales<icu::DateFormat>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

Handle<String> Intl::NumberFieldToType(Isolate* isolate,
                                       Handle<Object> numeric_obj,
                                       int32_t field_id) {
  DCHECK(numeric_obj->IsNumeric());
  switch (static_cast<UNumberFormatFields>(field_id)) {
    case UNUM_INTEGER_FIELD:
      if (numeric_obj->IsBigInt()) {
        // Neither NaN nor Infinite could be stored into BigInt
        // so just return integer.
        return isolate->factory()->integer_string();
      } else {
        double number = numeric_obj->Number();
        if (std::isfinite(number)) return isolate->factory()->integer_string();
        if (std::isnan(number)) return isolate->factory()->nan_string();
        return isolate->factory()->infinity_string();
      }
    case UNUM_FRACTION_FIELD:
      return isolate->factory()->fraction_string();
    case UNUM_DECIMAL_SEPARATOR_FIELD:
      return isolate->factory()->decimal_string();
    case UNUM_GROUPING_SEPARATOR_FIELD:
      return isolate->factory()->group_string();
    case UNUM_CURRENCY_FIELD:
      return isolate->factory()->currency_string();
    case UNUM_PERCENT_FIELD:
      return isolate->factory()->percentSign_string();
    case UNUM_SIGN_FIELD:
      if (numeric_obj->IsBigInt()) {
        Handle<BigInt> big_int = Handle<BigInt>::cast(numeric_obj);
        return big_int->IsNegative() ? isolate->factory()->minusSign_string()
                                     : isolate->factory()->plusSign_string();
      } else {
        double number = numeric_obj->Number();
        return number < 0 ? isolate->factory()->minusSign_string()
                          : isolate->factory()->plusSign_string();
      }
    case UNUM_EXPONENT_SYMBOL_FIELD:
    case UNUM_EXPONENT_SIGN_FIELD:
    case UNUM_EXPONENT_FIELD:
      // We should never get these because we're not using any scientific
      // formatter.
      UNREACHABLE();
      return Handle<String>();

    case UNUM_PERMILL_FIELD:
      // We're not creating any permill formatter, and it's not even clear how
      // that would be possible with the ICU API.
      UNREACHABLE();
      return Handle<String>();

    case UNUM_COMPACT_FIELD:
      return isolate->factory()->compact_string();
    case UNUM_MEASURE_UNIT_FIELD:
      return isolate->factory()->unit_string();

    default:
      UNREACHABLE();
      return Handle<String>();
  }
}

// A helper function to convert the FormattedValue for several Intl objects.
MaybeHandle<String> Intl::FormattedToString(
    Isolate* isolate, const icu::FormattedValue& formatted) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString result = formatted.toString(status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), String);
  }
  return Intl::ToString(isolate, result);
}

}  // namespace internal
}  // namespace v8

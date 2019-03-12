// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-locale.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/api.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-locale-inl.h"
#include "unicode/char16ptr.h"
#include "unicode/locid.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"

namespace v8 {
namespace internal {

namespace {

// Helper function to check a locale is valid. It will return false if
// the length of the extension fields are incorrect. For example, en-u-a or
// en-u-co-b will return false.
bool IsValidLocale(const icu::Locale& locale) {
  // icu::Locale::toLanguageTag won't return U_STRING_NOT_TERMINATED_WARNING for
  // incorrect locale yet. So we still need the following uloc_toLanguageTag
  // TODO(ftang): Change to use icu::Locale::toLanguageTag once it indicate
  // error.
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(locale.getName(), result, ULOC_FULLNAME_CAPACITY, true,
                     &status);
  return U_SUCCESS(status) && status != U_STRING_NOT_TERMINATED_WARNING;
}

struct OptionData {
  const char* name;
  const char* key;
  const std::vector<const char*>* possible_values;
  bool is_bool_value;
};

// Inserts tags from options into locale string.
Maybe<bool> InsertOptionsIntoLocale(Isolate* isolate,
                                    Handle<JSReceiver> options,
                                    icu::Locale* icu_locale) {
  CHECK(isolate);
  CHECK(!icu_locale->isBogus());

  const std::vector<const char*> hour_cycle_values = {"h11", "h12", "h23",
                                                      "h24"};
  const std::vector<const char*> case_first_values = {"upper", "lower",
                                                      "false"};
  const std::vector<const char*> empty_values = {};
  const std::array<OptionData, 6> kOptionToUnicodeTagMap = {
      {{"calendar", "ca", &empty_values, false},
       {"collation", "co", &empty_values, false},
       {"hourCycle", "hc", &hour_cycle_values, false},
       {"caseFirst", "kf", &case_first_values, false},
       {"numeric", "kn", &empty_values, true},
       {"numberingSystem", "nu", &empty_values, false}}};

  // TODO(cira): Pass in values as per the spec to make this to be
  // spec compliant.

  UErrorCode status = U_ZERO_ERROR;
  for (const auto& option_to_bcp47 : kOptionToUnicodeTagMap) {
    std::unique_ptr<char[]> value_str = nullptr;
    bool value_bool = false;
    Maybe<bool> maybe_found =
        option_to_bcp47.is_bool_value
            ? Intl::GetBoolOption(isolate, options, option_to_bcp47.name,
                                  "locale", &value_bool)
            : Intl::GetStringOption(isolate, options, option_to_bcp47.name,
                                    *(option_to_bcp47.possible_values),
                                    "locale", &value_str);
    MAYBE_RETURN(maybe_found, Nothing<bool>());

    // TODO(cira): Use fallback value if value is not found to make
    // this spec compliant.
    if (!maybe_found.FromJust()) continue;

    if (option_to_bcp47.is_bool_value) {
      value_str = value_bool ? isolate->factory()->true_string()->ToCString()
                             : isolate->factory()->false_string()->ToCString();
    }
    DCHECK_NOT_NULL(value_str.get());

    // Convert bcp47 key and value into legacy ICU format so we can use
    // uloc_setKeywordValue.
    const char* key = uloc_toLegacyKey(option_to_bcp47.key);
    DCHECK_NOT_NULL(key);

    // Overwrite existing, or insert new key-value to the locale string.
    const char* value = uloc_toLegacyType(key, value_str.get());
    if (value) {
      icu_locale->setKeywordValue(key, value, status);
      if (U_FAILURE(status)) {
        return Just(false);
      }
    } else {
      return Just(false);
    }
  }

  // Check all the unicode extension fields are in the right length.
  if (!IsValidLocale(*icu_locale)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NewRangeError(MessageTemplate::kLocaleBadParameters),
        Nothing<bool>());
  }

  return Just(true);
}

Handle<Object> UnicodeKeywordValue(Isolate* isolate, Handle<JSLocale> locale,
                                   const char* key) {
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  UErrorCode status = U_ZERO_ERROR;
  std::string value =
      icu_locale->getUnicodeKeywordValue<std::string>(key, status);
  if (status == U_ILLEGAL_ARGUMENT_ERROR || value == "") {
    return isolate->factory()->undefined_value();
  }
  return isolate->factory()->NewStringFromAsciiChecked(value.c_str());
}

bool InRange(size_t value, size_t start, size_t end) {
  return (start <= value) && (value <= end);
}
bool InRange(char value, char start, char end) {
  return (start <= value) && (value <= end);
}

bool IsCheckRange(const std::string& str, size_t min, size_t max,
                  bool(range_check_func)(char)) {
  if (!InRange(str.length(), min, max)) return false;
  for (size_t i = 0; i < str.length(); i++) {
    if (!range_check_func(str[i])) return false;
  }
  return true;
}
bool IsAlpha(const std::string& str, size_t min, size_t max) {
  return IsCheckRange(str, min, max, [](char c) -> bool {
    return InRange(c, 'a', 'z') || InRange(c, 'A', 'Z');
  });
}

bool IsDigit(const std::string& str, size_t min, size_t max) {
  return IsCheckRange(str, min, max,
                      [](char c) -> bool { return InRange(c, '0', '9'); });
}

bool ValidateLanguageProduction(const std::string& value) {
  // language      = 2*3ALPHA            ; shortest ISO 639 code
  //                 ["-" extlang]       ; sometimes followed by
  //                                     ; extended language subtags
  //               / 4ALPHA              ; or reserved for future use
  //               / 5*8ALPHA            ; or registered language subtag
  //
  // extlang       = 3ALPHA              ; selected ISO 639 codes
  //                 *2("-" 3ALPHA)      ; permanently reserved
  // TODO(ftang) not handling the [extlang] yet
  return IsAlpha(value, 2, 8);
}

bool ValidateScriptProduction(const std::string& value) {
  // script        = 4ALPHA              ; ISO 15924 code
  return IsAlpha(value, 4, 4);
}

bool ValidateRegionProduction(const std::string& value) {
  // region        = 2ALPHA              ; ISO 3166-1 code
  //               / 3DIGIT              ; UN M.49 code
  return IsAlpha(value, 2, 2) || IsDigit(value, 3, 3);
}

Maybe<icu::Locale> ApplyOptionsToTag(Isolate* isolate, Handle<String> tag,
                                     Handle<JSReceiver> options) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (tag->length() == 0) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NewRangeError(MessageTemplate::kLocaleNotEmpty),
        Nothing<icu::Locale>());
  }

  v8::String::Utf8Value bcp47_tag(v8_isolate, v8::Utils::ToLocal(tag));
  CHECK_LT(0, bcp47_tag.length());
  CHECK_NOT_NULL(*bcp47_tag);
  // 2. If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError
  // exception.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale =
      icu::Locale::forLanguageTag({*bcp47_tag, bcp47_tag.length()}, status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NewRangeError(MessageTemplate::kLocaleBadParameters),
        Nothing<icu::Locale>());
  }

  // 3. Let language be ? GetOption(options, "language", "string", undefined,
  // undefined).
  const std::vector<const char*> empty_values = {};
  std::unique_ptr<char[]> language_str = nullptr;
  Maybe<bool> maybe_language =
      Intl::GetStringOption(isolate, options, "language", empty_values,
                            "ApplyOptionsToTag", &language_str);
  MAYBE_RETURN(maybe_language, Nothing<icu::Locale>());
  // 4. If language is not undefined, then
  if (maybe_language.FromJust()) {
    // a. If language does not match the language production, throw a RangeError
    // exception.
    // b. If language matches the grandfathered production, throw a RangeError
    // exception.
    // Currently ValidateLanguageProduction only take 2*3ALPHA / 4ALPHA /
    // 5*8ALPHA and won't take 2*3ALPHA "-" extlang so none of the grandfathered
    // will be matched.
    if (!ValidateLanguageProduction(language_str.get())) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kLocaleBadParameters),
          Nothing<icu::Locale>());
    }
  }
  // 5. Let script be ? GetOption(options, "script", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> script_str = nullptr;
  Maybe<bool> maybe_script =
      Intl::GetStringOption(isolate, options, "script", empty_values,
                            "ApplyOptionsToTag", &script_str);
  MAYBE_RETURN(maybe_script, Nothing<icu::Locale>());
  // 6. If script is not undefined, then
  if (maybe_script.FromJust()) {
    // a. If script does not match the script production, throw a RangeError
    // exception.
    if (!ValidateScriptProduction(script_str.get())) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kLocaleBadParameters),
          Nothing<icu::Locale>());
    }
  }
  // 7. Let region be ? GetOption(options, "region", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> region_str = nullptr;
  Maybe<bool> maybe_region =
      Intl::GetStringOption(isolate, options, "region", empty_values,
                            "ApplyOptionsToTag", &region_str);
  MAYBE_RETURN(maybe_region, Nothing<icu::Locale>());
  // 8. If region is not undefined, then
  if (maybe_region.FromJust()) {
    // a. If region does not match the region production, throw a RangeError
    // exception.
    if (!ValidateRegionProduction(region_str.get())) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate, NewRangeError(MessageTemplate::kLocaleBadParameters),
          Nothing<icu::Locale>());
    }
  }
  // 9. Set tag to CanonicalizeLanguageTag(tag).

  // 10.  If language is not undefined,
  std::string locale_str;
  if (maybe_language.FromJust()) {
    // a. Assert: tag matches the langtag production.
    // b. Set tag to tag with the substring corresponding to the language
    // production replaced by the string language.
    locale_str = language_str.get();
  } else {
    locale_str = icu_locale.getLanguage();
  }
  // 11. If script is not undefined, then
  const char* script_ptr = nullptr;
  if (maybe_script.FromJust()) {
    // a. If tag does not contain a script production, then
    // i. Set tag to the concatenation of the language production of tag, "-",
    // script, and the rest of tag.
    // i. Set tag to tag with the substring corresponding to the script
    // production replaced by the string script.
    script_ptr = script_str.get();
  } else {
    script_ptr = icu_locale.getScript();
  }
  if (script_ptr != nullptr && strlen(script_ptr) > 0) {
    locale_str.append("-");
    locale_str.append(script_ptr);
  }
  // 12. If region is not undefined, then
  const char* region_ptr = nullptr;
  if (maybe_region.FromJust()) {
    // a. If tag does not contain a region production, then
    //
    // i. Set tag to the concatenation of the language production of tag, the
    // substring corresponding to the "-" script production if present,  "-",
    // region, and the rest of tag.
    //
    // b. Else,
    //
    // i. Set tag to tag with the substring corresponding to the region
    // production replaced by the string region.
    region_ptr = region_str.get();
  } else {
    region_ptr = icu_locale.getCountry();
  }

  std::string without_options(icu_locale.getName());

  // replace with values from options
  icu_locale =
      icu::Locale(locale_str.c_str(), region_ptr, icu_locale.getVariant());
  locale_str = icu_locale.getName();

  // Append extensions from tag
  size_t others = without_options.find("@");
  if (others != std::string::npos) {
    locale_str += without_options.substr(others);
  }

  // 13.  Return CanonicalizeLanguageTag(tag).
  icu_locale = icu::Locale::createCanonical(locale_str.c_str());
  return Just(icu_locale);
}

}  // namespace

MaybeHandle<JSLocale> JSLocale::Initialize(Isolate* isolate,
                                           Handle<JSLocale> locale,
                                           Handle<String> locale_str,
                                           Handle<JSReceiver> options) {
  Maybe<icu::Locale> maybe_locale =
      ApplyOptionsToTag(isolate, locale_str, options);
  MAYBE_RETURN(maybe_locale, MaybeHandle<JSLocale>());
  icu::Locale icu_locale = maybe_locale.FromJust();

  Maybe<bool> error = InsertOptionsIntoLocale(isolate, options, &icu_locale);
  MAYBE_RETURN(error, MaybeHandle<JSLocale>());
  if (!error.FromJust()) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters),
                    JSLocale);
  }

  // 31. Set locale.[[Locale]] to r.[[locale]].
  Handle<Managed<icu::Locale>> managed_locale =
      Managed<icu::Locale>::FromRawPtr(isolate, 0, icu_locale.clone());
  locale->set_icu_locale(*managed_locale);

  return locale;
}

namespace {
Handle<String> MorphLocale(Isolate* isolate, String locale,
                           void (*morph_func)(icu::Locale*, UErrorCode*)) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale =
      icu::Locale::forLanguageTag(locale.ToCString().get(), status);
  CHECK(U_SUCCESS(status));
  CHECK(!icu_locale.isBogus());
  (*morph_func)(&icu_locale, &status);
  CHECK(U_SUCCESS(status));
  CHECK(!icu_locale.isBogus());
  std::string locale_str = Intl::ToLanguageTag(icu_locale).FromJust();
  return isolate->factory()->NewStringFromAsciiChecked(locale_str.c_str());
}

}  // namespace

Handle<String> JSLocale::Maximize(Isolate* isolate, String locale) {
  return MorphLocale(isolate, locale,
                     [](icu::Locale* icu_locale, UErrorCode* status) {
                       icu_locale->addLikelySubtags(*status);
                     });
}

Handle<String> JSLocale::Minimize(Isolate* isolate, String locale) {
  return MorphLocale(isolate, locale,
                     [](icu::Locale* icu_locale, UErrorCode* status) {
                       icu_locale->minimizeSubtags(*status);
                     });
}

Handle<Object> JSLocale::Language(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* language = locale->icu_locale()->raw()->getLanguage();
  if (strlen(language) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(language);
}

Handle<Object> JSLocale::Script(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* script = locale->icu_locale()->raw()->getScript();
  if (strlen(script) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(script);
}

Handle<Object> JSLocale::Region(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* region = locale->icu_locale()->raw()->getCountry();
  if (strlen(region) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(region);
}

Handle<String> JSLocale::BaseName(Isolate* isolate, Handle<JSLocale> locale) {
  icu::Locale icu_locale =
      icu::Locale::createFromName(locale->icu_locale()->raw()->getBaseName());
  std::string base_name = Intl::ToLanguageTag(icu_locale).FromJust();
  return isolate->factory()->NewStringFromAsciiChecked(base_name.c_str());
}

Handle<Object> JSLocale::Calendar(Isolate* isolate, Handle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "ca");
}

Handle<Object> JSLocale::CaseFirst(Isolate* isolate, Handle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "kf");
}

Handle<Object> JSLocale::Collation(Isolate* isolate, Handle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "co");
}

Handle<Object> JSLocale::HourCycle(Isolate* isolate, Handle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "hc");
}

Handle<Object> JSLocale::Numeric(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  UErrorCode status = U_ZERO_ERROR;
  std::string numeric =
      icu_locale->getUnicodeKeywordValue<std::string>("kn", status);
  return (numeric == "true") ? factory->true_value() : factory->false_value();
}

Handle<Object> JSLocale::NumberingSystem(Isolate* isolate,
                                         Handle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "nu");
}

std::string JSLocale::ToString(Handle<JSLocale> locale) {
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  return Intl::ToLanguageTag(*icu_locale).FromJust();
}

Handle<String> JSLocale::ToString(Isolate* isolate, Handle<JSLocale> locale) {
  std::string locale_str = JSLocale::ToString(locale);
  return isolate->factory()->NewStringFromAsciiChecked(locale_str.c_str());
}

}  // namespace internal
}  // namespace v8

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

#include "src/api/api.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/objects-inl.h"
#include "unicode/char16ptr.h"
#include "unicode/localebuilder.h"
#include "unicode/locid.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"

namespace v8 {
namespace internal {

namespace {

struct OptionData {
  const char* name;
  const char* key;
  const std::vector<const char*>* possible_values;
  bool is_bool_value;
};

// Inserts tags from options into locale string.
Maybe<bool> InsertOptionsIntoLocale(Isolate* isolate,
                                    Handle<JSReceiver> options,
                                    icu::LocaleBuilder* builder) {
  CHECK(isolate);

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

    // Overwrite existing, or insert new key-value to the locale string.
    if (!uloc_toLegacyType(uloc_toLegacyKey(option_to_bcp47.key),
                           value_str.get())) {
      return Just(false);
    }
    builder->setUnicodeLocaleKeyword(option_to_bcp47.key, value_str.get());
  }
  return Just(true);
}

Handle<Object> UnicodeKeywordValue(Isolate* isolate, Handle<JSLocale> locale,
                                   const char* key) {
  icu::Locale* icu_locale = locale->icu_locale().raw();
  UErrorCode status = U_ZERO_ERROR;
  std::string value =
      icu_locale->getUnicodeKeywordValue<std::string>(key, status);
  if (status == U_ILLEGAL_ARGUMENT_ERROR || value == "") {
    return isolate->factory()->undefined_value();
  }
  if (value == "yes") {
    value = "true";
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

bool IsAlphanum(const std::string& str, size_t min, size_t max) {
  return IsCheckRange(str, min, max, [](char c) -> bool {
    return InRange(c, 'a', 'z') || InRange(c, 'A', 'Z') || InRange(c, '0', '9');
  });
}

bool IsUnicodeLanguageSubtag(const std::string& value) {
  // unicode_language_subtag = alpha{2,3} | alpha{5,8};
  return IsAlpha(value, 2, 3) || IsAlpha(value, 5, 8);
}

bool IsUnicodeScriptSubtag(const std::string& value) {
  // unicode_script_subtag = alpha{4} ;
  return IsAlpha(value, 4, 4);
}

bool IsUnicodeRegionSubtag(const std::string& value) {
  // unicode_region_subtag = (alpha{2} | digit{3});
  return IsAlpha(value, 2, 2) || IsDigit(value, 3, 3);
}

bool IsDigitAlphanum3(const std::string& value) {
  return value.length() == 4 && InRange(value[0], '0', '9') &&
         IsAlphanum(value.substr(1), 3, 3);
}

bool IsUnicodeVariantSubtag(const std::string& value) {
  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  return IsAlphanum(value, 5, 8) || IsDigitAlphanum3(value);
}

bool IsExtensionSingleton(const std::string& value) {
  return IsAlphanum(value, 1, 1);
}

// TODO(ftang) Replace the following check w/ icu::LocaleBuilder
// once ICU64 land in March 2019.
bool StartsWithUnicodeLanguageId(const std::string& value) {
  // unicode_language_id =
  // unicode_language_subtag (sep unicode_script_subtag)?
  //   (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream token_stream(value);
  while (std::getline(token_stream, token, '-')) {
    tokens.push_back(token);
  }
  if (tokens.size() == 0) return false;

  // length >= 1
  if (!IsUnicodeLanguageSubtag(tokens[0])) return false;

  if (tokens.size() == 1) return true;

  // length >= 2
  if (IsExtensionSingleton(tokens[1])) return true;

  size_t index = 1;
  if (IsUnicodeScriptSubtag(tokens[index])) {
    index++;
    if (index == tokens.size()) return true;
  }
  if (IsUnicodeRegionSubtag(tokens[index])) {
    index++;
  }
  while (index < tokens.size()) {
    if (IsExtensionSingleton(tokens[index])) return true;
    if (!IsUnicodeVariantSubtag(tokens[index])) return false;
    index++;
  }
  return true;
}

Maybe<bool> ApplyOptionsToTag(Isolate* isolate, Handle<String> tag,
                              Handle<JSReceiver> options,
                              icu::LocaleBuilder* builder) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (tag->length() == 0) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NewRangeError(MessageTemplate::kLocaleNotEmpty),
        Nothing<bool>());
  }

  v8::String::Utf8Value bcp47_tag(v8_isolate, v8::Utils::ToLocal(tag));
  builder->setLanguageTag({*bcp47_tag, bcp47_tag.length()});
  CHECK_LT(0, bcp47_tag.length());
  CHECK_NOT_NULL(*bcp47_tag);
  // 2. If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError
  // exception.
  if (!StartsWithUnicodeLanguageId(*bcp47_tag)) {
    return Just(false);
  }
  UErrorCode status = U_ZERO_ERROR;
  builder->build(status);
  if (U_FAILURE(status)) {
    return Just(false);
  }

  // 3. Let language be ? GetOption(options, "language", "string", undefined,
  // undefined).
  const std::vector<const char*> empty_values = {};
  std::unique_ptr<char[]> language_str = nullptr;
  Maybe<bool> maybe_language =
      Intl::GetStringOption(isolate, options, "language", empty_values,
                            "ApplyOptionsToTag", &language_str);
  MAYBE_RETURN(maybe_language, Nothing<bool>());
  // 4. If language is not undefined, then
  if (maybe_language.FromJust()) {
    builder->setLanguage(language_str.get());
    builder->build(status);
    // a. If language does not match the unicode_language_subtag production,
    //    throw a RangeError exception.
    if (U_FAILURE(status) || language_str[0] == '\0' ||
        IsAlpha(language_str.get(), 4, 4)) {
      return Just(false);
    }
  }
  // 5. Let script be ? GetOption(options, "script", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> script_str = nullptr;
  Maybe<bool> maybe_script =
      Intl::GetStringOption(isolate, options, "script", empty_values,
                            "ApplyOptionsToTag", &script_str);
  MAYBE_RETURN(maybe_script, Nothing<bool>());
  // 6. If script is not undefined, then
  if (maybe_script.FromJust()) {
    builder->setScript(script_str.get());
    builder->build(status);
    // a. If script does not match the unicode_script_subtag production, throw
    //    a RangeError exception.
    if (U_FAILURE(status) || script_str[0] == '\0') {
      return Just(false);
    }
  }
  // 7. Let region be ? GetOption(options, "region", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> region_str = nullptr;
  Maybe<bool> maybe_region =
      Intl::GetStringOption(isolate, options, "region", empty_values,
                            "ApplyOptionsToTag", &region_str);
  MAYBE_RETURN(maybe_region, Nothing<bool>());
  // 8. If region is not undefined, then
  if (maybe_region.FromJust()) {
    // a. If region does not match the region production, throw a RangeError
    // exception.
    builder->setRegion(region_str.get());
    builder->build(status);
    if (U_FAILURE(status) || region_str[0] == '\0') {
      return Just(false);
    }
  }

  // 9. Set tag to CanonicalizeLanguageTag(tag).
  // 10.  If language is not undefined,
  // a. Assert: tag matches the unicode_locale_id production.
  // b. Set tag to tag with the substring corresponding to the
  //    unicode_language_subtag production replaced by the string language.
  // 11. If script is not undefined, then
  // a. If tag does not contain a unicode_script_subtag production, then
  //   i. Set tag to the concatenation of the unicode_language_subtag
  //      production of tag, "-", script, and the rest of tag.
  // b. Else,
  //   i. Set tag to tag with the substring corresponding to the
  //      unicode_script_subtag production replaced by the string script.
  // 12. If region is not undefined, then
  // a. If tag does not contain a unicode_region_subtag production, then
  //   i. Set tag to the concatenation of the unicode_language_subtag
  //      production of tag, the substring corresponding to the  "-"
  //      unicode_script_subtag production if present, "-", region, and
  //      the rest of tag.
  // b. Else,
  // i. Set tag to tag with the substring corresponding to the
  //    unicode_region_subtag production replaced by the string region.
  // 13.  Return CanonicalizeLanguageTag(tag).
  return Just(true);
}

}  // namespace

MaybeHandle<JSLocale> JSLocale::Initialize(Isolate* isolate,
                                           Handle<JSLocale> locale,
                                           Handle<String> locale_str,
                                           Handle<JSReceiver> options) {
  icu::LocaleBuilder builder;
  Maybe<bool> maybe_apply =
      ApplyOptionsToTag(isolate, locale_str, options, &builder);
  MAYBE_RETURN(maybe_apply, MaybeHandle<JSLocale>());
  if (!maybe_apply.FromJust()) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters),
                    JSLocale);
  }

  Maybe<bool> maybe_insert =
      InsertOptionsIntoLocale(isolate, options, &builder);
  MAYBE_RETURN(maybe_insert, MaybeHandle<JSLocale>());
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale = builder.build(status);
  if (!maybe_insert.FromJust() || U_FAILURE(status)) {
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
  // TODO(ftang): Remove the following lines after ICU-8420 fixed.
  // Due to ICU-8420 "und" is turn into "" by forLanguageTag,
  // we have to work around to use icu::Locale("und") directly
  if (icu_locale.getName()[0] == '\0') icu_locale = icu::Locale("und");
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
  const char* language = locale->icu_locale().raw()->getLanguage();
  if (strlen(language) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(language);
}

Handle<Object> JSLocale::Script(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* script = locale->icu_locale().raw()->getScript();
  if (strlen(script) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(script);
}

Handle<Object> JSLocale::Region(Isolate* isolate, Handle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* region = locale->icu_locale().raw()->getCountry();
  if (strlen(region) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(region);
}

Handle<String> JSLocale::BaseName(Isolate* isolate, Handle<JSLocale> locale) {
  icu::Locale icu_locale =
      icu::Locale::createFromName(locale->icu_locale().raw()->getBaseName());
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
  icu::Locale* icu_locale = locale->icu_locale().raw();
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
  icu::Locale* icu_locale = locale->icu_locale().raw();
  return Intl::ToLanguageTag(*icu_locale).FromJust();
}

Handle<String> JSLocale::ToString(Isolate* isolate, Handle<JSLocale> locale) {
  std::string locale_str = JSLocale::ToString(locale);
  return isolate->factory()->NewStringFromAsciiChecked(locale_str.c_str());
}

}  // namespace internal
}  // namespace v8

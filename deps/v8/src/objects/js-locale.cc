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
#include "unicode/locid.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/uvernum.h"
#include "unicode/uversion.h"

#if U_ICU_VERSION_MAJOR_NUM >= 59
#include "unicode/char16ptr.h"
#endif

namespace v8 {
namespace internal {

namespace {

JSLocale::CaseFirst GetCaseFirst(const char* str) {
  if (strcmp(str, "upper") == 0) return JSLocale::CaseFirst::UPPER;
  if (strcmp(str, "lower") == 0) return JSLocale::CaseFirst::LOWER;
  if (strcmp(str, "false") == 0) return JSLocale::CaseFirst::FALSE_VALUE;
  UNREACHABLE();
}

JSLocale::HourCycle GetHourCycle(const char* str) {
  if (strcmp(str, "h11") == 0) return JSLocale::HourCycle::H11;
  if (strcmp(str, "h12") == 0) return JSLocale::HourCycle::H12;
  if (strcmp(str, "h23") == 0) return JSLocale::HourCycle::H23;
  if (strcmp(str, "h24") == 0) return JSLocale::HourCycle::H24;
  UNREACHABLE();
}

JSLocale::Numeric GetNumeric(const char* str) {
  return strcmp(str, "true") == 0 ? JSLocale::Numeric::TRUE_VALUE
                                  : JSLocale::Numeric::FALSE_VALUE;
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
                                    char* icu_locale) {
  CHECK(isolate);
  CHECK(icu_locale);

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

    // Convert bcp47 key and value into legacy ICU format so we can use
    // uloc_setKeywordValue.
    const char* key = uloc_toLegacyKey(option_to_bcp47.key);
    DCHECK_NOT_NULL(key);

    // Overwrite existing, or insert new key-value to the locale string.
    const char* value = uloc_toLegacyType(key, value_str.get());
    UErrorCode status = U_ZERO_ERROR;
    if (value) {
      // TODO(cira): ICU puts artificial limit on locale length, while BCP47
      // doesn't. Switch to C++ API when it's ready.
      // Related ICU bug - https://ssl.icu-project.org/trac/ticket/13417.
      uloc_setKeywordValue(key, value, icu_locale, ULOC_FULLNAME_CAPACITY,
                           &status);
      if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
        return Just(false);
      }
    } else {
      return Just(false);
    }
  }

  return Just(true);
}

// Fills in the JSLocale object slots with Unicode tag/values.
bool PopulateLocaleWithUnicodeTags(Isolate* isolate, const char* icu_locale,
                                   Handle<JSLocale> locale_holder) {
  CHECK(isolate);
  CHECK(icu_locale);

  Factory* factory = isolate->factory();

  UErrorCode status = U_ZERO_ERROR;
  UEnumeration* keywords = uloc_openKeywords(icu_locale, &status);
  if (!keywords) return true;

  char value[ULOC_FULLNAME_CAPACITY];
  while (const char* keyword = uenum_next(keywords, nullptr, &status)) {
    uloc_getKeywordValue(icu_locale, keyword, value, ULOC_FULLNAME_CAPACITY,
                         &status);
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    // Ignore those we don't recognize - spec allows that.
    const char* bcp47_key = uloc_toUnicodeLocaleKey(keyword);
    if (bcp47_key) {
      const char* bcp47_value = uloc_toUnicodeLocaleType(bcp47_key, value);
      if (bcp47_value) {
          if (strcmp(bcp47_key, "kn") == 0) {
            locale_holder->set_numeric(GetNumeric(bcp47_value));
          } else if (strcmp(bcp47_key, "ca") == 0) {
            Handle<String> bcp47_handle =
                factory->NewStringFromAsciiChecked(bcp47_value);
            locale_holder->set_calendar(*bcp47_handle);
          } else if (strcmp(bcp47_key, "kf") == 0) {
            locale_holder->set_case_first(GetCaseFirst(bcp47_value));
          } else if (strcmp(bcp47_key, "co") == 0) {
            Handle<String> bcp47_handle =
                factory->NewStringFromAsciiChecked(bcp47_value);
            locale_holder->set_collation(*bcp47_handle);
          } else if (strcmp(bcp47_key, "hc") == 0) {
            locale_holder->set_hour_cycle(GetHourCycle(bcp47_value));
          } else if (strcmp(bcp47_key, "nu") == 0) {
            Handle<String> bcp47_handle =
                factory->NewStringFromAsciiChecked(bcp47_value);
            locale_holder->set_numbering_system(*bcp47_handle);
          }
      }
    }
  }

  uenum_close(keywords);

  return true;
}
}  // namespace

MaybeHandle<JSLocale> JSLocale::Initialize(Isolate* isolate,
                                           Handle<JSLocale> locale_holder,
                                           Handle<String> locale,
                                           Handle<JSReceiver> options) {
  locale_holder->set_flags(0);
  static const char* const kMethod = "Intl.Locale";
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  UErrorCode status = U_ZERO_ERROR;

  // Get ICU locale format, and canonicalize it.
  char icu_result[ULOC_FULLNAME_CAPACITY];

  if (locale->length() == 0) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kLocaleNotEmpty),
                    JSLocale);
  }

  v8::String::Utf8Value bcp47_locale(v8_isolate, v8::Utils::ToLocal(locale));
  CHECK_LT(0, bcp47_locale.length());
  CHECK_NOT_NULL(*bcp47_locale);

  int parsed_length = 0;
  int icu_length =
      uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                          &parsed_length, &status);

  if (U_FAILURE(status) ||
      parsed_length < static_cast<int>(bcp47_locale.length()) ||
      status == U_STRING_NOT_TERMINATED_WARNING || icu_length == 0) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }

  Maybe<bool> error = InsertOptionsIntoLocale(isolate, options, icu_result);
  MAYBE_RETURN(error, MaybeHandle<JSLocale>());
  if (!error.FromJust()) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }

  if (!PopulateLocaleWithUnicodeTags(isolate, icu_result, locale_holder)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }

  // Extract language, script and region parts.
  char icu_language[ULOC_LANG_CAPACITY];
  uloc_getLanguage(icu_result, icu_language, ULOC_LANG_CAPACITY, &status);

  char icu_script[ULOC_SCRIPT_CAPACITY];
  uloc_getScript(icu_result, icu_script, ULOC_SCRIPT_CAPACITY, &status);

  char icu_region[ULOC_COUNTRY_CAPACITY];
  uloc_getCountry(icu_result, icu_region, ULOC_COUNTRY_CAPACITY, &status);

  if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }

  Factory* factory = isolate->factory();

  // NOTE: One shouldn't use temporary handles, because they can go out of
  // scope and be garbage collected before properly assigned.
  // DON'T DO THIS: locale_holder->set_language(*f->NewStringAscii...);
  Handle<String> language = factory->NewStringFromAsciiChecked(icu_language);
  locale_holder->set_language(*language);

  if (strlen(icu_script) != 0) {
    Handle<String> script = factory->NewStringFromAsciiChecked(icu_script);
    locale_holder->set_script(*script);
  }

  if (strlen(icu_region) != 0) {
    Handle<String> region = factory->NewStringFromAsciiChecked(icu_region);
    locale_holder->set_region(*region);
  }

  char icu_base_name[ULOC_FULLNAME_CAPACITY];
  uloc_getBaseName(icu_result, icu_base_name, ULOC_FULLNAME_CAPACITY, &status);
  // We need to convert it back to BCP47.
  char bcp47_result[ULOC_FULLNAME_CAPACITY];
  uloc_toLanguageTag(icu_base_name, bcp47_result, ULOC_FULLNAME_CAPACITY, true,
                     &status);
  if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }
  Handle<String> base_name = factory->NewStringFromAsciiChecked(bcp47_result);
  locale_holder->set_base_name(*base_name);

  // Produce final representation of the locale string, for toString().
  uloc_toLanguageTag(icu_result, bcp47_result, ULOC_FULLNAME_CAPACITY, true,
                     &status);
  if (U_FAILURE(status) || status == U_STRING_NOT_TERMINATED_WARNING) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kLocaleBadParameters,
                      isolate->factory()->NewStringFromAsciiChecked(kMethod),
                      locale_holder),
        JSLocale);
  }
  Handle<String> locale_handle =
      factory->NewStringFromAsciiChecked(bcp47_result);
  locale_holder->set_locale(*locale_handle);

  return locale_holder;
}

namespace {

Handle<String> MorphLocale(Isolate* isolate, String* language_tag,
                           int32_t (*morph_func)(const char*, char*, int32_t,
                                                 UErrorCode*)) {
  Factory* factory = isolate->factory();
  char localeBuffer[ULOC_FULLNAME_CAPACITY];
  char morphBuffer[ULOC_FULLNAME_CAPACITY];

  UErrorCode status = U_ZERO_ERROR;
  // Convert from language id to locale.
  int32_t parsed_length;
  int32_t length =
      uloc_forLanguageTag(language_tag->ToCString().get(), localeBuffer,
                          ULOC_FULLNAME_CAPACITY, &parsed_length, &status);
  CHECK(parsed_length == language_tag->length());
  DCHECK(U_SUCCESS(status));
  DCHECK_GT(length, 0);
  DCHECK_NOT_NULL(morph_func);
  // Add the likely subtags or Minimize the subtags on the locale id
  length =
      (*morph_func)(localeBuffer, morphBuffer, ULOC_FULLNAME_CAPACITY, &status);
  DCHECK(U_SUCCESS(status));
  DCHECK_GT(length, 0);
  // Returns a well-formed language tag
  length = uloc_toLanguageTag(morphBuffer, localeBuffer, ULOC_FULLNAME_CAPACITY,
                              false, &status);
  DCHECK(U_SUCCESS(status));
  DCHECK_GT(length, 0);
  std::string lang(localeBuffer, length);
  std::replace(lang.begin(), lang.end(), '_', '-');

  return factory->NewStringFromAsciiChecked(lang.c_str());
}

}  // namespace

Handle<String> JSLocale::Maximize(Isolate* isolate, String* locale) {
  return MorphLocale(isolate, locale, uloc_addLikelySubtags);
}

Handle<String> JSLocale::Minimize(Isolate* isolate, String* locale) {
  return MorphLocale(isolate, locale, uloc_minimizeSubtags);
}

Handle<String> JSLocale::CaseFirstAsString() const {
  switch (case_first()) {
    case CaseFirst::UPPER:
      return GetReadOnlyRoots().upper_string_handle();
    case CaseFirst::LOWER:
      return GetReadOnlyRoots().lower_string_handle();
    case CaseFirst::FALSE_VALUE:
      return GetReadOnlyRoots().false_string_handle();
    case CaseFirst::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSLocale::HourCycleAsString() const {
  switch (hour_cycle()) {
    case HourCycle::H11:
      return GetReadOnlyRoots().h11_string_handle();
    case HourCycle::H12:
      return GetReadOnlyRoots().h12_string_handle();
    case HourCycle::H23:
      return GetReadOnlyRoots().h23_string_handle();
    case HourCycle::H24:
      return GetReadOnlyRoots().h24_string_handle();
    case HourCycle::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSLocale::NumericAsString() const {
  switch (numeric()) {
    case Numeric::NOTSET:
      return GetReadOnlyRoots().undefined_string_handle();
    case Numeric::TRUE_VALUE:
      return GetReadOnlyRoots().true_string_handle();
    case Numeric::FALSE_VALUE:
      return GetReadOnlyRoots().false_string_handle();
    case Numeric::COUNT:
      UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8

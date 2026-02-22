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
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/option-utils.h"
#include "unicode/calendar.h"
#include "unicode/char16ptr.h"
#include "unicode/coll.h"
#include "unicode/dtptngen.h"
#include "unicode/localebuilder.h"
#include "unicode/locid.h"
#include "unicode/ucal.h"
#include "unicode/uloc.h"
#include "unicode/ulocdata.h"
#include "unicode/unistr.h"

namespace v8 {
namespace internal {

namespace {

struct OptionData {
  Handle<String> (Factory::*object_key)();
  const char* key;
  const std::span<const std::string_view> possible_values;
  bool is_bool_value;
};
struct ValueAndType {
  const char* value;
  const char* type;
};

// Inserts tags from options into locale string.
Maybe<bool> InsertOptionsIntoLocale(Isolate* isolate,
                                    DirectHandle<JSReceiver> options,
                                    icu::LocaleBuilder* builder) {
  DCHECK(isolate);

  static const auto hour_cycle_values =
      std::to_array<const std::string_view>({"h11", "h12", "h23", "h24"});
  static const auto case_first_values =
      std::to_array<const std::string_view>({"upper", "lower", "false"});
  const auto empty_values = std::span<std::string_view>();
  const std::array<OptionData, 7> kOptionToUnicodeTagMap = {
      {{&Factory::calendar_string, "ca", empty_values, false},
       {&Factory::collation_string, "co", empty_values, false},
       {&Factory::firstDayOfWeek_string, "fw", empty_values, false},
       {&Factory::hourCycle_string, "hc", hour_cycle_values, false},
       {&Factory::caseFirst_string, "kf", case_first_values, false},
       {&Factory::numeric_string, "kn", empty_values, true},
       {&Factory::numberingSystem_string, "nu", empty_values, false}}};

  // TODO(cira): Pass in values as per the spec to make this to be
  // spec compliant.

  for (const auto& option_to_bcp47 : kOptionToUnicodeTagMap) {
    bool value_bool = false;
    DirectHandle<String> name =
        (isolate->factory()->*option_to_bcp47.object_key)();

    bool found = false;
    std::string_view value_str;
    std::string owned;
    if (option_to_bcp47.is_bool_value) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, found,
          GetBoolOption(isolate, options, name, "locale", &value_bool), {});
    } else if (option_to_bcp47.possible_values.empty()) {
      // We just wish to fetch the string
      DirectHandle<String> output;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, found,
          GetStringOption(isolate, options, name, "locale", &output), {});
      if (found) {
        owned = output->ToStdString();
        value_str = owned;
      }
    } else {
      // The string is expected to be in a particular set.
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, value_str,
          GetStringOption<std::string_view>(
              isolate, options, name, "locale", option_to_bcp47.possible_values,
              option_to_bcp47.possible_values, std::string_view()),
          {});
      if (!value_str.empty()) {
        found = true;
      }
    }

    // TODO(cira): Use fallback value if value is not found to make
    // this spec compliant.
    if (!found) continue;

    const char* type = value_str.data();

    if (strcmp(option_to_bcp47.key, "fw") == 0) {
      const std::array<ValueAndType, 8> kFirstDayValuesAndTypes = {
          {{"0", "sun"},
           {"1", "mon"},
           {"2", "tue"},
           {"3", "wed"},
           {"4", "thu"},
           {"5", "fri"},
           {"6", "sat"},
           {"7", "sun"}}};
      for (const auto& value_to_type : kFirstDayValuesAndTypes) {
        if (strcmp(type, value_to_type.value) == 0) {
          type = value_to_type.type;
          break;
        }
      }
    } else if (option_to_bcp47.is_bool_value) {
      type = value_bool ? "true" : "false";
    }
    DCHECK_NOT_NULL(type);

    // Overwrite existing, or insert new key-value to the locale string.
    if (!uloc_toLegacyType(uloc_toLegacyKey(option_to_bcp47.key), type)) {
      return Just(false);
    }
    builder->setUnicodeLocaleKeyword(option_to_bcp47.key, type);
  }
  return Just(true);
}

DirectHandle<Object> UnicodeKeywordValue(Isolate* isolate,
                                         DirectHandle<JSLocale> locale,
                                         const char* key) {
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  UErrorCode status = U_ZERO_ERROR;
  std::string value =
      icu_locale->getUnicodeKeywordValue<std::string>(key, status);
  if (status == U_ILLEGAL_ARGUMENT_ERROR || value.empty()) {
    return isolate->factory()->undefined_value();
  }
  if (value == "yes") {
    value = "true";
  }
  if (value == "true" && strcmp(key, "kf") == 0) {
    return isolate->factory()->NewStringFromStaticChars("");
  }
  return isolate->factory()->NewStringFromAsciiChecked(value.c_str());
}

bool IsCheckRange(std::string_view str, size_t min, size_t max,
                  bool(range_check_func)(char)) {
  if (!base::IsInRange(str.length(), min, max)) return false;
  for (size_t i = 0; i < str.length(); i++) {
    if (!range_check_func(str[i])) return false;
  }
  return true;
}
bool IsAlpha(std::string_view str, size_t min, size_t max) {
  return IsCheckRange(str, min, max, [](char c) -> bool {
    return base::IsInRange(c, 'a', 'z') || base::IsInRange(c, 'A', 'Z');
  });
}

bool IsDigit(std::string_view str, size_t min, size_t max) {
  return IsCheckRange(str, min, max, [](char c) -> bool {
    return base::IsInRange(c, '0', '9');
  });
}

bool IsAlphanum(std::string_view str, size_t min, size_t max) {
  return IsCheckRange(str, min, max, [](char c) -> bool {
    return base::IsInRange(c, 'a', 'z') || base::IsInRange(c, 'A', 'Z') ||
           base::IsInRange(c, '0', '9');
  });
}

bool IsUnicodeLanguageSubtag(std::string_view value) {
  // unicode_language_subtag = alpha{2,3} | alpha{5,8};
  return IsAlpha(value, 2, 3) || IsAlpha(value, 5, 8);
}

bool IsUnicodeScriptSubtag(std::string_view value) {
  // unicode_script_subtag = alpha{4} ;
  return IsAlpha(value, 4, 4);
}

bool IsUnicodeRegionSubtag(std::string_view value) {
  // unicode_region_subtag = (alpha{2} | digit{3});
  return IsAlpha(value, 2, 2) || IsDigit(value, 3, 3);
}

bool IsDigitAlphanum3(std::string_view value) {
  return value.length() == 4 && base::IsInRange(value[0], '0', '9') &&
         IsAlphanum(value.substr(1), 3, 3);
}

bool IsUnicodeVariantSubtag(std::string_view value) {
  // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
  return IsAlphanum(value, 5, 8) || IsDigitAlphanum3(value);
}

bool IsExtensionSingleton(std::string_view value) {
  return IsAlphanum(value, 1, 1);
}

int32_t weekdayFromEDaysOfWeek(icu::Calendar::EDaysOfWeek eDaysOfWeek) {
  return (eDaysOfWeek == icu::Calendar::SUNDAY) ? 7 : eDaysOfWeek - 1;
}

}  // namespace

// Implemented as iteration instead of recursion to avoid stack overflow for
// very long input strings.
bool JSLocale::Is38AlphaNumList(std::string_view in) {
  std::string_view value = in;
  while (true) {
    std::size_t found_dash = value.find('-');
    if (found_dash == std::string::npos) {
      return IsAlphanum(value, 3, 8);
    }
    if (!IsAlphanum(value.substr(0, found_dash), 3, 8)) return false;
    value = value.substr(found_dash + 1);
  }
}

bool JSLocale::Is3Alpha(std::string_view value) { return IsAlpha(value, 3, 3); }

// TODO(ftang) Replace the following check w/ icu::LocaleBuilder
// once ICU64 land in March 2019.
bool JSLocale::StartsWithUnicodeLanguageId(std::string_view value) {
  // unicode_language_id =
  // unicode_language_subtag (sep unicode_script_subtag)?
  //   (sep unicode_region_subtag)? (sep unicode_variant_subtag)* ;
  if (value.empty()) return false;
  std::vector<std::string_view> tokens;
  size_t token_start = 0;
  size_t token_end;
  for (token_end = 0; token_end < value.size(); ++token_end) {
    if (value[token_end] == '-') {
      tokens.emplace_back(&value[token_start], token_end - token_start);
      token_start = token_end + 1;
    }
  }
  if (token_start != token_end) {
    tokens.emplace_back(&value[token_start], token_end - token_start);
  }
  DCHECK(!tokens.empty());

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

namespace {
// Return true if variants contain duplicate elements.
bool DuplicateVariants(std::string variants) {
  // The length of one unicode_variant_subtag is between 4-8. To have
  // duplicate, the length of the variants need to be >= 4+1+4 = 9.
  if (variants.length() >= 9) {
    std::transform(
        variants.begin(), variants.end(), variants.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t pos = 0;
    size_t count = 1;
    std::set<std::string> set;
    while ((pos = variants.find('-')) != std::string::npos) {
      set.insert(variants.substr(0, pos));
      variants.erase(0, pos + 1);
      count++;
    }
    set.insert(variants);
    if (count != set.size()) {
      return true;
    }
  }
  return false;
}
Maybe<bool> ApplyOptionsToTag(Isolate* isolate, DirectHandle<String> tag,
                              DirectHandle<JSReceiver> options,
                              icu::LocaleBuilder* builder) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  if (tag->length() == 0) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kLocaleNotEmpty));
  }

  v8::String::Utf8Value bcp47_tag(v8_isolate, v8::Utils::ToLocal(tag));
  builder->setLanguageTag(
      {*bcp47_tag, static_cast<int32_t>(bcp47_tag.length())});
  DCHECK_LT(0, bcp47_tag.length());
  DCHECK_NOT_NULL(*bcp47_tag);
  // 2. If IsStructurallyValidLanguageTag(tag) is false, throw a RangeError
  // exception.
  if (!JSLocale::StartsWithUnicodeLanguageId(*bcp47_tag)) {
    return Just(false);
  }
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale canonicalized = builder->build(status);
  canonicalized.canonicalize(status);
  if (U_FAILURE(status)) {
    return Just(false);
  }
  builder->setLocale(canonicalized);

  // 3. Let language be ? GetOption(options, "language", "string", undefined,
  // undefined).
  DirectHandle<String> language_str;
  Maybe<bool> maybe_language =
      GetStringOption(isolate, options, isolate->factory()->language_string(),
                      "ApplyOptionsToTag", &language_str);
  MAYBE_RETURN(maybe_language, Nothing<bool>());

  // 4. If language is not undefined, then
  if (maybe_language.FromJust()) {
    std::string language_stdstr = language_str->ToStdString();
    builder->setLanguage(language_stdstr);
    builder->build(status);
    // a. If language does not match the unicode_language_subtag production,
    //    throw a RangeError exception.
    if (U_FAILURE(status) || language_stdstr.empty() ||
        IsAlpha(language_stdstr, 4, 4)) {
      return Just(false);
    }
  }
  // 5. Let script be ? GetOption(options, "script", "string", undefined,
  // undefined).
  DirectHandle<String> script_str;
  Maybe<bool> maybe_script =
      GetStringOption(isolate, options, isolate->factory()->script_string(),
                      "ApplyOptionsToTag", &script_str);
  MAYBE_RETURN(maybe_script, Nothing<bool>());
  // 6. If script is not undefined, then
  if (maybe_script.FromJust()) {
    std::string script_stdstr = script_str->ToStdString();
    builder->setScript(script_stdstr);
    builder->build(status);
    // a. If script does not match the unicode_script_subtag production, throw
    //    a RangeError exception.
    if (U_FAILURE(status) || script_stdstr.empty()) {
      return Just(false);
    }
  }
  // 7. Let region be ? GetOption(options, "region", "string", undefined,
  // undefined).
  DirectHandle<String> region_str;
  Maybe<bool> maybe_region =
      GetStringOption(isolate, options, isolate->factory()->region_string(),
                      "ApplyOptionsToTag", &region_str);
  MAYBE_RETURN(maybe_region, Nothing<bool>());
  // 8. If region is not undefined, then
  if (maybe_region.FromJust()) {
    std::string region_stdstr = region_str->ToStdString();
    // a. If region does not match the region production, throw a RangeError
    // exception.
    builder->setRegion(region_stdstr);
    builder->build(status);
    if (U_FAILURE(status) || region_stdstr.empty()) {
      return Just(false);
    }
  }

  if (v8_flags.js_intl_locale_variants) {
    // 8. Let variants be ? GetOption(options, "variants", string, empty,
    // GetLocaleVariants(baseName)).
    DirectHandle<String> variants_str;
    Maybe<bool> maybe_variants =
        GetStringOption(isolate, options, isolate->factory()->variants_string(),
                        "ApplyOptionsToTag", &variants_str);
    MAYBE_RETURN(maybe_variants, Nothing<bool>());
    // 9. If variants is not undefined, then
    if (maybe_variants.FromJust()) {
      // a. If variants is the empty String, throw a RangeError exception.
      // b. Let lowerVariants be the ASCII-lowercase of variants.
      std::string variants_stdstr = variants_str->ToStdString();
      // c. Let variantSubtags be StringSplitToList(lowerVariants, "-").
      // d. For each element variant of variantSubtags, do
      // i. If variant cannot be matched by the unicode_variant_subtag Unicode
      // locale nonterminal, throw a RangeError exception.
      builder->setVariant(variants_stdstr);
      builder->build(status);
      if (U_FAILURE(status) || variants_stdstr.empty()) {
        return Just(false);
      }
      // e. If variantSubtags contains any duplicate elements, throw a
      // RangeError exception.
      if (DuplicateVariants(variants_stdstr)) {
        return Just(false);
      }
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

MaybeDirectHandle<JSLocale> JSLocale::New(Isolate* isolate,
                                          DirectHandle<Map> map,
                                          DirectHandle<String> locale_str,
                                          DirectHandle<JSReceiver> options) {
  icu::LocaleBuilder builder;
  Maybe<bool> maybe_apply =
      ApplyOptionsToTag(isolate, locale_str, options, &builder);
  MAYBE_RETURN(maybe_apply, MaybeDirectHandle<JSLocale>());
  if (!maybe_apply.FromJust()) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters));
  }

  Maybe<bool> maybe_insert =
      InsertOptionsIntoLocale(isolate, options, &builder);
  MAYBE_RETURN(maybe_insert, MaybeDirectHandle<JSLocale>());
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale = builder.build(status);

  icu_locale.canonicalize(status);

  if (!maybe_insert.FromJust() || U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters));
  }

  // 31. Set locale.[[Locale]] to r.[[locale]].
  DirectHandle<Managed<icu::Locale>> managed_locale =
      Managed<icu::Locale>::From(
          isolate, 0, std::shared_ptr<icu::Locale>{icu_locale.clone()});

  // Now all properties are ready, so we can allocate the result object.
  DirectHandle<JSLocale> locale =
      Cast<JSLocale>(isolate->factory()->NewFastOrSlowJSObjectFromMap(map));
  DisallowGarbageCollection no_gc;
  locale->set_icu_locale(*managed_locale);
  return locale;
}

namespace {

MaybeDirectHandle<JSLocale> Construct(Isolate* isolate,
                                      const icu::Locale& icu_locale) {
  DirectHandle<Managed<icu::Locale>> managed_locale =
      Managed<icu::Locale>::From(
          isolate, 0, std::shared_ptr<icu::Locale>{icu_locale.clone()});

  DirectHandle<JSFunction> constructor(
      isolate->native_context()->intl_locale_function(), isolate);

  DirectHandle<Map> map;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, map,
      JSFunction::GetDerivedMap(isolate, constructor, constructor));

  DirectHandle<JSLocale> locale =
      Cast<JSLocale>(isolate->factory()->NewFastOrSlowJSObjectFromMap(map));
  DisallowGarbageCollection no_gc;
  locale->set_icu_locale(*managed_locale);
  return locale;
}

}  // namespace

MaybeDirectHandle<JSLocale> JSLocale::Maximize(Isolate* isolate,
                                               DirectHandle<JSLocale> locale) {
  // ICU has limitation on the length of the locale while addLikelySubtags
  // is called. Work around the issue by only perform addLikelySubtags
  // on the base locale and merge the extension if needed.
  icu::Locale source(*(locale->icu_locale()->raw()));
  icu::Locale result = icu::Locale::createFromName(source.getBaseName());
  UErrorCode status = U_ZERO_ERROR;
  result.addLikelySubtags(status);
  if (strlen(source.getBaseName()) != strlen(result.getBaseName())) {
    // Base name is changed
    if (strlen(source.getBaseName()) != strlen(source.getName())) {
      // the source has extensions, get the extensions from the source.
      result = icu::LocaleBuilder()
                   .setLocale(source)
                   .setLanguage(result.getLanguage())
                   .setRegion(result.getCountry())
                   .setScript(result.getScript())
                   .setVariant(result.getVariant())
                   .build(status);
    }
  } else {
    // Base name is not changed
    result = source;
  }
  if (U_FAILURE(status) || result.isBogus()) {
    // Due to https://unicode-org.atlassian.net/browse/ICU-21639
    // Valid but super long locale will fail. Just throw here for now.
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters));
  }
  return Construct(isolate, result);
}

MaybeDirectHandle<JSLocale> JSLocale::Minimize(Isolate* isolate,
                                               DirectHandle<JSLocale> locale) {
  // ICU has limitation on the length of the locale while minimizeSubtags
  // is called. Work around the issue by only perform addLikelySubtags
  // on the base locale and merge the extension if needed.
  icu::Locale source(*(locale->icu_locale()->raw()));
  icu::Locale result = icu::Locale::createFromName(source.getBaseName());
  UErrorCode status = U_ZERO_ERROR;
  result.minimizeSubtags(status);
  if (strcmp(source.getBaseName(), result.getBaseName()) != 0) {
    // Base name is changed
    if (strlen(source.getBaseName()) != strlen(source.getName())) {
      // the source has extensions, get the extensions from the source.
      result = icu::LocaleBuilder()
                   .setLocale(source)
                   .setLanguage(result.getLanguage())
                   .setRegion(result.getCountry())
                   .setScript(result.getScript())
                   .setVariant(result.getVariant())
                   .build(status);
    }
  } else {
    // Base name is not changed
    result = source;
  }
  if (U_FAILURE(status) || result.isBogus()) {
    // Due to https://unicode-org.atlassian.net/browse/ICU-21639
    // Valid but super long locale will fail. Just throw here for now.
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kLocaleBadParameters));
  }
  return Construct(isolate, result);
}

template <typename T>
MaybeDirectHandle<JSArray> GetKeywordValuesFromLocale(
    Isolate* isolate, const char* key, const char* unicode_key,
    const icu::Locale& locale, bool (*removes)(const char*), bool commonly_used,
    bool sort) {
  Factory* factory = isolate->factory();
  UErrorCode status = U_ZERO_ERROR;
  std::string ext =
      locale.getUnicodeKeywordValue<std::string>(unicode_key, status);
  if (!ext.empty()) {
    DirectHandle<FixedArray> fixed_array = factory->NewFixedArray(1);
    DirectHandle<String> str = factory->NewStringFromAsciiChecked(ext.c_str());
    fixed_array->set(0, *str);
    return factory->NewJSArrayWithElements(fixed_array);
  }
  status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> enumeration(
      T::getKeywordValuesForLocale(key, locale, commonly_used, status));
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }
  return Intl::ToJSArray(
      isolate,
      [unicode_key](const char* value) {
        return std::string(uloc_toUnicodeLocaleType(unicode_key, value));
      },
      enumeration.get(), removes, sort);
}

namespace {

MaybeDirectHandle<JSArray> CalendarsForLocale(Isolate* isolate,
                                              const icu::Locale& icu_locale,
                                              bool commonly_used, bool sort) {
  return GetKeywordValuesFromLocale<icu::Calendar>(
      isolate, "calendar", "ca", icu_locale, nullptr, commonly_used, sort);
}

}  // namespace

MaybeDirectHandle<JSArray> JSLocale::GetCalendars(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  icu::Locale icu_locale(*(locale->icu_locale()->raw()));
  return CalendarsForLocale(isolate, icu_locale, true, false);
}

MaybeDirectHandle<JSArray> Intl::AvailableCalendars(Isolate* isolate) {
  icu::Locale icu_locale("und");
  return CalendarsForLocale(isolate, icu_locale, false, true);
}

MaybeDirectHandle<JSArray> JSLocale::GetCollations(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  icu::Locale icu_locale(*(locale->icu_locale()->raw()));
  return GetKeywordValuesFromLocale<icu::Collator>(
      isolate, "collations", "co", icu_locale, Intl::RemoveCollation, true,
      true);
}

MaybeDirectHandle<JSArray> JSLocale::GetHourCycles(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  // Let preferred be loc.[[HourCycle]].
  // Let locale be loc.[[Locale]].
  icu::Locale icu_locale(*(locale->icu_locale()->raw()));
  Factory* factory = isolate->factory();

  // Assert: locale matches the unicode_locale_id production.

  // Let list be a List of 1 or more hour cycle identifiers, which must be
  // String values indicating either the 12-hour format ("h11", "h12") or the
  // 24-hour format ("h23", "h24"), sorted in descending preference of those in
  // common use in the locale for date and time formatting.

  // Return CreateArrayFromListAndPreferred( list, preferred ).
  DirectHandle<FixedArray> fixed_array = factory->NewFixedArray(1);
  UErrorCode status = U_ZERO_ERROR;
  std::string ext =
      icu_locale.getUnicodeKeywordValue<std::string>("hc", status);
  if (!ext.empty()) {
    DirectHandle<String> str = factory->NewStringFromAsciiChecked(ext.c_str());
    fixed_array->set(0, *str);
    return factory->NewJSArrayWithElements(fixed_array);
  }
  status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(icu_locale, status));
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }

  UDateFormatHourCycle hc = generator->getDefaultHourCycle(status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }
  DirectHandle<String> hour_cycle;

  switch (hc) {
    case UDAT_HOUR_CYCLE_11:
      hour_cycle = factory->h11_string();
      break;
    case UDAT_HOUR_CYCLE_12:
      hour_cycle = factory->h12_string();
      break;
    case UDAT_HOUR_CYCLE_23:
      hour_cycle = factory->h23_string();
      break;
    case UDAT_HOUR_CYCLE_24:
      hour_cycle = factory->h24_string();
      break;
    default:
      break;
  }
  fixed_array->set(0, *hour_cycle);
  return factory->NewJSArrayWithElements(fixed_array);
}

MaybeDirectHandle<JSArray> JSLocale::GetNumberingSystems(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  // Let preferred be loc.[[NumberingSystem]].

  // Let locale be loc.[[Locale]].
  icu::Locale icu_locale(*(locale->icu_locale()->raw()));
  Factory* factory = isolate->factory();

  // Assert: locale matches the unicode_locale_id production.

  // Let list be a List of 1 or more numbering system identifiers, which must be
  // String values conforming to the type sequence from UTS 35 Unicode Locale
  // Identifier, section 3.2, sorted in descending preference of those in common
  // use in the locale for formatting numeric values.

  // Return CreateArrayFromListAndPreferred( list, preferred ).
  UErrorCode status = U_ZERO_ERROR;
  DirectHandle<FixedArray> fixed_array = factory->NewFixedArray(1);
  std::string numbering_system =
      icu_locale.getUnicodeKeywordValue<std::string>("nu", status);
  if (numbering_system.empty()) {
    numbering_system = Intl::GetNumberingSystem(icu_locale);
  }
  DirectHandle<String> str =
      factory->NewStringFromAsciiChecked(numbering_system.c_str());

  fixed_array->set(0, *str);
  return factory->NewJSArrayWithElements(fixed_array);
}

MaybeDirectHandle<Object> JSLocale::GetTimeZones(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  // Let loc be the this value.

  // Perform ? RequireInternalSlot(loc, [[InitializedLocale]])

  // Let locale be loc.[[Locale]].
  icu::Locale icu_locale(*(locale->icu_locale()->raw()));
  Factory* factory = isolate->factory();

  // If the unicode_language_id production of locale does not contain the
  // ["-" unicode_region_subtag] sequence, return undefined.
  const char* region = icu_locale.getCountry();
  if (region == nullptr || strlen(region) == 0) {
    return factory->undefined_value();
  }

  // Return TimeZonesOfLocale(loc).

  // Let locale be loc.[[Locale]].

  // Assert: locale matches the unicode_locale_id production.

  // Let region be the substring of locale corresponding to the
  // unicode_region_subtag production of the unicode_language_id.

  // Let list be a List of 1 or more time zone identifiers, which must be String
  // values indicating a Zone or Link name of the IANA Time Zone Database,
  // sorted in descending preference of those in common use in region.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> enumeration(
      icu::TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_CANONICAL,
                                                 region, nullptr, status));
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }
  return Intl::ToJSArray(isolate, nullptr, enumeration.get(), nullptr, true);
}

MaybeDirectHandle<JSObject> JSLocale::GetTextInfo(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  // Let loc be the this value.

  // Perform ? RequireInternalSlot(loc, [[InitializedLocale]]).

  // Let locale be loc.[[Locale]].

  // Assert: locale matches the unicode_locale_id production.

  Factory* factory = isolate->factory();
  // Let info be ! ObjectCreate(%Object.prototype%).
  DirectHandle<JSObject> info =
      factory->NewJSObject(isolate->object_function());

  // Let dir be "ltr".
  DirectHandle<String> dir = locale->icu_locale()->raw()->isRightToLeft()
                                 ? factory->rtl_string()
                                 : factory->ltr_string();

  // Perform ! CreateDataPropertyOrThrow(info, "direction", dir).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, info, factory->direction_string(), dir, Just(kDontThrow))
            .FromJust());

  // Return info.
  return info;
}

MaybeDirectHandle<JSObject> JSLocale::GetWeekInfo(
    Isolate* isolate, DirectHandle<JSLocale> locale) {
  // Let loc be the this value.

  // Perform ? RequireInternalSlot(loc, [[InitializedLocale]]).

  // Let locale be loc.[[Locale]].

  // Assert: locale matches the unicode_locale_id production.
  Factory* factory = isolate->factory();

  // Let info be ! ObjectCreate(%Object.prototype%).
  DirectHandle<JSObject> info =
      factory->NewJSObject(isolate->object_function());
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> calendar(
      icu::Calendar::createInstance(*(locale->icu_locale()->raw()), status));
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }

  // Let fd be the weekday value indicating which day of the week is considered
  // the 'first' day, for calendar purposes, in the locale.
  int32_t fd = weekdayFromEDaysOfWeek(calendar->getFirstDayOfWeek());

  // Let wi be ! WeekInfoOfLocale(loc).
  // Let we be ! CreateArrayFromList( wi.[[Weekend]] ).
  Handle<FixedArray> wi = Cast<FixedArray>(factory->NewFixedArray(2));
  int32_t length = 0;
  for (int32_t i = 1; i <= 7; i++) {
    UCalendarDaysOfWeek day =
        (i == 7) ? UCAL_SUNDAY : static_cast<UCalendarDaysOfWeek>(i + 1);
    if (UCAL_WEEKDAY != calendar->getDayOfWeekType(day, status)) {
      wi->set(length++, Smi::FromInt(i));
      CHECK_LE(length, 2);
    }
  }
  if (length != 2) {
    wi = wi->RightTrimOrEmpty(isolate, wi, length);
  }
  DirectHandle<JSArray> we = factory->NewJSArrayWithElements(wi);

  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kIcuError));
  }

  // Perform ! CreateDataPropertyOrThrow(info, "firstDay", fd).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, info, factory->firstDay_string(),
            factory->NewNumberFromInt(fd), Just(kDontThrow))
            .FromJust());

  // Perform ! CreateDataPropertyOrThrow(info, "weekend", we).
  CHECK(JSReceiver::CreateDataProperty(isolate, info, factory->weekend_string(),
                                       we, Just(kDontThrow))
            .FromJust());

  // Return info.
  return info;
}

DirectHandle<Object> JSLocale::Language(Isolate* isolate,
                                        DirectHandle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* language = locale->icu_locale()->raw()->getLanguage();
  constexpr const char kUnd[] = "und";
  if (strlen(language) == 0) {
    language = kUnd;
  }
  return factory->NewStringFromAsciiChecked(language);
}

DirectHandle<Object> JSLocale::Script(Isolate* isolate,
                                      DirectHandle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* script = locale->icu_locale()->raw()->getScript();
  if (strlen(script) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(script);
}
DirectHandle<Object> JSLocale::Variants(Isolate* isolate,
                                        DirectHandle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  std::string variants = locale->icu_locale()->raw()->getVariant();
  if (variants.length() == 0) return factory->undefined_value();
  // icu::Locale::getVariants() return the variants in upper case characters
  // with '_', we need to convert it to lower case and '-' before return.
  std::transform(variants.begin(), variants.end(), variants.begin(),
                 [](unsigned char c) {
                   if (c == '_') return '-';
                   return static_cast<char>(std::tolower(c));
                 });

  return factory->NewStringFromAsciiChecked(variants.c_str());
}

DirectHandle<Object> JSLocale::Region(Isolate* isolate,
                                      DirectHandle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  const char* region = locale->icu_locale()->raw()->getCountry();
  if (strlen(region) == 0) return factory->undefined_value();
  return factory->NewStringFromAsciiChecked(region);
}

DirectHandle<String> JSLocale::BaseName(Isolate* isolate,
                                        DirectHandle<JSLocale> locale) {
  icu::Locale icu_locale =
      icu::Locale::createFromName(locale->icu_locale()->raw()->getBaseName());
  std::string base_name = Intl::ToLanguageTag(icu_locale).FromJust();
  return isolate->factory()->NewStringFromAsciiChecked(base_name.c_str());
}

DirectHandle<Object> JSLocale::Calendar(Isolate* isolate,
                                        DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "ca");
}

DirectHandle<Object> JSLocale::CaseFirst(Isolate* isolate,
                                         DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "kf");
}

DirectHandle<Object> JSLocale::Collation(Isolate* isolate,
                                         DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "co");
}

DirectHandle<Object> JSLocale::FirstDayOfWeek(Isolate* isolate,
                                              DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "fw");
}
DirectHandle<Object> JSLocale::HourCycle(Isolate* isolate,
                                         DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "hc");
}

DirectHandle<Object> JSLocale::Numeric(Isolate* isolate,
                                       DirectHandle<JSLocale> locale) {
  Factory* factory = isolate->factory();
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  UErrorCode status = U_ZERO_ERROR;
  std::string numeric =
      icu_locale->getUnicodeKeywordValue<std::string>("kn", status);
  return factory->ToBoolean(numeric == "true");
}

DirectHandle<Object> JSLocale::NumberingSystem(Isolate* isolate,
                                               DirectHandle<JSLocale> locale) {
  return UnicodeKeywordValue(isolate, locale, "nu");
}

std::string JSLocale::ToString(DirectHandle<JSLocale> locale) {
  icu::Locale* icu_locale = locale->icu_locale()->raw();
  return Intl::ToLanguageTag(*icu_locale).FromJust();
}

DirectHandle<String> JSLocale::ToString(Isolate* isolate,
                                        DirectHandle<JSLocale> locale) {
  std::string locale_str = JSLocale::ToString(locale);
  return isolate->factory()->NewStringFromAsciiChecked(locale_str.c_str());
}

}  // namespace internal
}  // namespace v8

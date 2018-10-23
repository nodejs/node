// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-plural-rules.h"

#include "src/isolate-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-plural-rules-inl.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/plurrule.h"
#include "unicode/strenum.h"

namespace v8 {
namespace internal {

namespace {

bool CreateICUPluralRules(Isolate* isolate, const icu::Locale& icu_locale,
                          const char* type_string,
                          std::unique_ptr<icu::PluralRules>* pl,
                          std::unique_ptr<icu::DecimalFormat>* nf) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;

  UPluralType type = UPLURAL_TYPE_CARDINAL;
  if (strcmp(type_string, "ordinal") == 0) {
    type = UPLURAL_TYPE_ORDINAL;
  } else {
    CHECK_EQ(0, strcmp(type_string, "cardinal"));
  }

  std::unique_ptr<icu::PluralRules> plural_rules(
      icu::PluralRules::forLocale(icu_locale, type, status));
  if (U_FAILURE(status)) {
    return false;
  }
  CHECK_NOT_NULL(plural_rules.get());

  std::unique_ptr<icu::DecimalFormat> number_format(
      static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(icu_locale, UNUM_DECIMAL, status)));
  if (U_FAILURE(status)) {
    return false;
  }
  CHECK_NOT_NULL(number_format.get());

  *pl = std::move(plural_rules);
  *nf = std::move(number_format);

  return true;
}

void InitializeICUPluralRules(
    Isolate* isolate, const icu::Locale& icu_locale, const char* type,
    std::unique_ptr<icu::PluralRules>* plural_rules,
    std::unique_ptr<icu::DecimalFormat>* number_format) {
  bool success = CreateICUPluralRules(isolate, icu_locale, type, plural_rules,
                                      number_format);
  if (!success) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    success = CreateICUPluralRules(isolate, no_extension_locale, type,
                                   plural_rules, number_format);

    if (!success) {
      FATAL("Failed to create ICU PluralRules, are ICU data files missing?");
    }
  }

  CHECK_NOT_NULL((*plural_rules).get());
  CHECK_NOT_NULL((*number_format).get());
}

}  // namespace

// static
MaybeHandle<JSPluralRules> JSPluralRules::Initialize(
    Isolate* isolate, Handle<JSPluralRules> plural_rules,
    Handle<Object> locales, Handle<Object> options_obj) {
  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSPluralRules>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 2. If options is undefined, then
  if (options_obj->IsUndefined(isolate)) {
    // 2. a. Let options be ObjectCreate(null).
    options_obj = isolate->factory()->NewJSObjectWithNullProto();
  } else {
    // 3. Else
    // 3. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, options_obj,
        Object::ToObject(isolate, options_obj, "Intl.PluralRules"),
        JSPluralRules);
  }

  // At this point, options_obj can either be a JSObject or a JSProxy only.
  Handle<JSReceiver> options = Handle<JSReceiver>::cast(options_obj);

  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string",
  // « "lookup", "best fit" », "best fit").
  // 6. Set opt.[[localeMatcher]] to matcher.
  std::vector<const char*> values = {"lookup", "best fit"};
  std::unique_ptr<char[]> matcher_str = nullptr;
  Intl::MatcherOption matcher = Intl::MatcherOption::kBestFit;
  Maybe<bool> found_matcher =
      Intl::GetStringOption(isolate, options, "localeMatcher", values,
                            "Intl.PluralRules", &matcher_str);
  MAYBE_RETURN(found_matcher, MaybeHandle<JSPluralRules>());
  if (found_matcher.FromJust()) {
    DCHECK_NOT_NULL(matcher_str.get());
    if (strcmp(matcher_str.get(), "lookup") == 0) {
      matcher = Intl::MatcherOption::kLookup;
    }
  }

  // 7. Let t be ? GetOption(options, "type", "string", « "cardinal",
  // "ordinal" », "cardinal").
  values = {"cardinal", "ordinal"};
  std::unique_ptr<char[]> type_str = nullptr;
  const char* type_cstr = "cardinal";
  Maybe<bool> found = Intl::GetStringOption(isolate, options, "type", values,
                                            "Intl.PluralRules", &type_str);
  MAYBE_RETURN(found, MaybeHandle<JSPluralRules>());
  if (found.FromJust()) {
    type_cstr = type_str.get();
  }

  // 8. Set pluralRules.[[Type]] to t.
  Handle<String> type =
      isolate->factory()->NewStringFromAsciiChecked(type_cstr);
  plural_rules->set_type(*type);

  // Note: The spec says we should do ResolveLocale after performing
  // SetNumberFormatDigitOptions but we need the locale to create all
  // the ICU data structures.
  //
  // This isn't observable so we aren't violating the spec.

  // 11. Let r be ResolveLocale(%PluralRules%.[[AvailableLocales]],
  // requestedLocales, opt, %PluralRules%.[[RelevantExtensionKeys]],
  // localeData).
  std::set<std::string> available_locales =
      Intl::GetAvailableLocales(Intl::ICUService::kPluralRules);
  Intl::ResolvedLocale r = Intl::ResolveLocale(isolate, available_locales,
                                               requested_locales, matcher, {});

  // 18. Set collator.[[Locale]] to r.[[locale]].
  icu::Locale icu_locale = r.icu_locale;
  DCHECK(!icu_locale.isBogus());

  std::map<std::string, std::string> extensions = r.extensions;

  // 12. Set pluralRules.[[Locale]] to the value of r.[[locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  plural_rules->set_locale(*locale_str);

  std::unique_ptr<icu::PluralRules> icu_plural_rules;
  std::unique_ptr<icu::DecimalFormat> icu_decimal_format;
  InitializeICUPluralRules(isolate, icu_locale, type_cstr, &icu_plural_rules,
                           &icu_decimal_format);
  CHECK_NOT_NULL(icu_plural_rules.get());
  CHECK_NOT_NULL(icu_decimal_format.get());

  // 9. Perform ? SetNumberFormatDigitOptions(pluralRules, options, 0, 3).
  Maybe<bool> done = Intl::SetNumberFormatDigitOptions(
      isolate, icu_decimal_format.get(), options, 0, 3);
  MAYBE_RETURN(done, MaybeHandle<JSPluralRules>());

  Handle<Managed<icu::PluralRules>> managed_plural_rules =
      Managed<icu::PluralRules>::FromUniquePtr(isolate, 0,
                                               std::move(icu_plural_rules));
  plural_rules->set_icu_plural_rules(*managed_plural_rules);

  Handle<Managed<icu::DecimalFormat>> managed_decimal_format =
      Managed<icu::DecimalFormat>::FromUniquePtr(isolate, 0,
                                                 std::move(icu_decimal_format));
  plural_rules->set_icu_decimal_format(*managed_decimal_format);

  // 13. Return pluralRules.
  return plural_rules;
}

MaybeHandle<String> JSPluralRules::ResolvePlural(
    Isolate* isolate, Handle<JSPluralRules> plural_rules, double number) {
  icu::PluralRules* icu_plural_rules = plural_rules->icu_plural_rules()->raw();
  CHECK_NOT_NULL(icu_plural_rules);

  icu::DecimalFormat* icu_decimal_format =
      plural_rules->icu_decimal_format()->raw();
  CHECK_NOT_NULL(icu_decimal_format);

  // Currently, PluralRules doesn't implement all the options for rounding that
  // the Intl spec provides; format and parse the number to round to the
  // appropriate amount, then apply PluralRules.
  //
  // TODO(littledan): If a future ICU version supports an extended API to avoid
  // this step, then switch to that API. Bug thread:
  // http://bugs.icu-project.org/trac/ticket/12763
  icu::UnicodeString rounded_string;
  icu_decimal_format->format(number, rounded_string);

  icu::Formattable formattable;
  UErrorCode status = U_ZERO_ERROR;
  icu_decimal_format->parse(rounded_string, formattable, status);
  CHECK(U_SUCCESS(status));

  double rounded = formattable.getDouble(status);
  CHECK(U_SUCCESS(status));

  icu::UnicodeString result = icu_plural_rules->select(rounded);
  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

namespace {

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<Object> value, const char* key) {
  Handle<String> key_str = isolate->factory()->NewStringFromAsciiChecked(key);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key_str, value,
                                       kDontThrow)
            .FromJust());
}

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  int value, const char* key) {
  Handle<Smi> value_smi(Smi::FromInt(value), isolate);
  CreateDataPropertyForOptions(isolate, options, value_smi, key);
}

}  // namespace

Handle<JSObject> JSPluralRules::ResolvedOptions(
    Isolate* isolate, Handle<JSPluralRules> plural_rules) {
  Handle<JSObject> options =
      isolate->factory()->NewJSObject(isolate->object_function());

  Handle<String> locale_value(plural_rules->locale(), isolate);
  CreateDataPropertyForOptions(isolate, options, locale_value, "locale");

  Handle<String> type_value(plural_rules->type(), isolate);
  CreateDataPropertyForOptions(isolate, options, type_value, "type");

  icu::DecimalFormat* icu_decimal_format =
      plural_rules->icu_decimal_format()->raw();
  CHECK_NOT_NULL(icu_decimal_format);

  // This is a safe upcast as icu::DecimalFormat inherits from
  // icu::NumberFormat.
  icu::NumberFormat* icu_number_format =
      static_cast<icu::NumberFormat*>(icu_decimal_format);

  int min_int_digits = icu_number_format->getMinimumIntegerDigits();
  CreateDataPropertyForOptions(isolate, options, min_int_digits,
                               "minimumIntegerDigits");

  int min_fraction_digits = icu_number_format->getMinimumFractionDigits();
  CreateDataPropertyForOptions(isolate, options, min_fraction_digits,
                               "minimumFractionDigits");

  int max_fraction_digits = icu_number_format->getMaximumFractionDigits();
  CreateDataPropertyForOptions(isolate, options, max_fraction_digits,
                               "maximumFractionDigits");

  if (icu_decimal_format->areSignificantDigitsUsed()) {
    int min_significant_digits =
        icu_decimal_format->getMinimumSignificantDigits();
    CreateDataPropertyForOptions(isolate, options, min_significant_digits,
                                 "minimumSignificantDigits");

    int max_significant_digits =
        icu_decimal_format->getMaximumSignificantDigits();
    CreateDataPropertyForOptions(isolate, options, max_significant_digits,
                                 "maximumSignificantDigits");
  }

  // 6. Let pluralCategories be a List of Strings representing the
  // possible results of PluralRuleSelect for the selected locale pr.
  icu::PluralRules* icu_plural_rules = plural_rules->icu_plural_rules()->raw();
  CHECK_NOT_NULL(icu_plural_rules);

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> categories(
      icu_plural_rules->getKeywords(status));
  CHECK(U_SUCCESS(status));
  int32_t count = categories->count(status);
  CHECK(U_SUCCESS(status));

  Handle<FixedArray> plural_categories =
      isolate->factory()->NewFixedArray(count);
  for (int32_t i = 0; i < count; i++) {
    const icu::UnicodeString* category = categories->snext(status);
    CHECK(U_SUCCESS(status));
    if (category == nullptr) break;

    std::string keyword;
    Handle<String> value = isolate->factory()->NewStringFromAsciiChecked(
        category->toUTF8String(keyword).data());
    plural_categories->set(i, *value);
  }

  // 7. Perform ! CreateDataProperty(options, "pluralCategories",
  // CreateArrayFromList(pluralCategories)).
  Handle<JSArray> plural_categories_value =
      isolate->factory()->NewJSArrayWithElements(plural_categories);
  CreateDataPropertyForOptions(isolate, options, plural_categories_value,
                               "pluralCategories");

  return options;
}

}  // namespace internal
}  // namespace v8

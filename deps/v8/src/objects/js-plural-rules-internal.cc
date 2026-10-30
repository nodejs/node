// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-plural-rules-internal.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-number-format.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "unicode/locid.h"
#include "unicode/numberformatter.h"
#include "unicode/numberrangeformatter.h"
#include "unicode/plurrule.h"
#include "unicode/unumberformatter.h"
#pragma GCC diagnostic pop


namespace v8 {
namespace internal {

namespace {

bool CreateICUPluralRules(Isolate* isolate, const icu::Locale& icu_locale,
                          JSPluralRules::Type type,
                          std::unique_ptr<icu::PluralRules>* pl) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;

  UPluralType icu_type = UPLURAL_TYPE_CARDINAL;
  if (type == JSPluralRules::Type::ORDINAL) {
    icu_type = UPLURAL_TYPE_ORDINAL;
  } else {
    DCHECK_EQ(JSPluralRules::Type::CARDINAL, type);
  }

  std::unique_ptr<icu::PluralRules> plural_rules(
      icu::PluralRules::forLocale(icu_locale, icu_type, status));
  if (U_FAILURE(status)) {
    return false;
  }
  DCHECK_NOT_NULL(plural_rules.get());

  *pl = std::move(plural_rules);
  return true;
}

icu::number::LocalizedNumberFormatter CreateICUNumberFormatter(
    const icu::Locale& icu_locale, Intl::Notation notation,
    Intl::CompactDisplay compact_display,
    const Intl::NumberFormatDigitOptions& digit_options) {
  icu::number::UnlocalizedNumberFormatter settings =
      icu::number::UnlocalizedNumberFormatter().roundingMode(UNUM_ROUND_HALFUP);
  // The default notation in ICU is Simple, which mapped from STANDARD
  // so we can skip setting it.
  if (notation != Intl::Notation::STANDARD) {
    settings =
        settings.notation(Intl::ToICUNotation(notation, compact_display));
  }
  settings =
      JSNumberFormat::SetDigitOptionsToFormatter(settings, digit_options);
  return settings.locale(icu_locale);
}

void CreateDataPropertyForOptions(Isolate* isolate,
                                  DirectHandle<JSObject> options,
                                  DirectHandle<Object> value, const char* key) {
  DirectHandle<String> key_str =
      isolate->factory()->NewStringFromAsciiChecked(key);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  Maybe<bool> maybe = JSReceiver::CreateDataProperty(isolate, options, key_str,
                                                     value, Just(kDontThrow));
  DCHECK(maybe.FromJust());
  USE(maybe);
}

void CreateDataPropertyForOptions(Isolate* isolate,
                                  DirectHandle<JSObject> options, int value,
                                  const char* key) {
  DirectHandle<Smi> value_smi(Smi::FromInt(value), isolate);
  CreateDataPropertyForOptions(isolate, options, value_smi, key);
}

void PopulateResolvedOptionsFromSkeleton(
    Isolate* isolate, DirectHandle<JSObject> options,
    const icu::UnicodeString& skeleton,
    DirectHandle<FixedArray> plural_categories) {
  Factory* factory = isolate->factory();
  Intl::Notation notation = Intl::NotationFromSkeleton(skeleton);
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->notation_string(),
            Intl::NotationAsString(isolate, notation), Just(kDontThrow))
            .FromJust());
  if (notation == Intl::Notation::COMPACT) {
    // https://github.com/tc39/ecma402/issues/1031
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->compactDisplay_string(),
              Intl::CompactDisplayString(isolate, skeleton), Just(kDontThrow))
              .FromJust());
  }

  CreateDataPropertyForOptions(
      isolate, options,
      JSNumberFormat::MinimumIntegerDigitsFromSkeleton(skeleton),
      "minimumIntegerDigits");
  int32_t min = 0, max = 0;

  if (JSNumberFormat::SignificantDigitsFromSkeleton(skeleton, &min, &max)) {
    CreateDataPropertyForOptions(isolate, options, min,
                                 "minimumSignificantDigits");
    CreateDataPropertyForOptions(isolate, options, max,
                                 "maximumSignificantDigits");
  } else {
    JSNumberFormat::FractionDigitsFromSkeleton(skeleton, &min, &max);
    CreateDataPropertyForOptions(isolate, options, min,
                                 "minimumFractionDigits");
    CreateDataPropertyForOptions(isolate, options, max,
                                 "maximumFractionDigits");
  }

  // 7. Perform ! CreateDataProperty(options, "pluralCategories",
  // CreateArrayFromList(pluralCategories)).
  DirectHandle<JSArray> plural_categories_value =
      factory->NewJSArrayWithElements(plural_categories);
  CreateDataPropertyForOptions(isolate, options, plural_categories_value,
                               "pluralCategories");

  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->roundingIncrement_string(),
            JSNumberFormat::RoundingIncrement(isolate, skeleton),
            Just(kDontThrow))
            .FromJust());
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->roundingMode_string(),
            JSNumberFormat::RoundingModeString(isolate, skeleton),
            Just(kDontThrow))
            .FromJust());
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->roundingPriority_string(),
            JSNumberFormat::RoundingPriorityString(isolate, skeleton),
            Just(kDontThrow))
            .FromJust());
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->trailingZeroDisplay_string(),
            JSNumberFormat::TrailingZeroDisplayString(isolate, skeleton),
            Just(kDontThrow))
            .FromJust());
}

class ICU4CPluralRulesBackend final : public PluralRulesInternal {
 public:
  ICU4CPluralRulesBackend(
      std::unique_ptr<icu::PluralRules> icu_plural_rules,
      icu::number::LocalizedNumberFormatter icu_number_formatter)
      : icu_plural_rules_(std::move(icu_plural_rules)),
        icu_number_formatter_(std::move(icu_number_formatter)) {}

  static std::unique_ptr<ICU4CPluralRulesBackend> Create(
      Isolate* isolate, const Intl::ResolvedLocale& resolved_locale,
      JSPluralRules::Type type, Intl::Notation notation,
      Intl::CompactDisplay compact_display,
      const Intl::NumberFormatDigitOptions& digit_options) {
    icu::Locale icu_locale = resolved_locale.icu_locale;
    std::unique_ptr<icu::PluralRules> icu_plural_rules;
    bool success = CreateICUPluralRules(isolate, icu_locale, type,
                                        &icu_plural_rules);
    if (!success || icu_plural_rules == nullptr) {
      // Remove extensions and try again.
      icu::Locale no_extension_locale(icu_locale.getBaseName());
      success = CreateICUPluralRules(isolate, no_extension_locale, type,
                                     &icu_plural_rules);
      icu_locale = no_extension_locale;
      if (!success || icu_plural_rules == nullptr) {
        return nullptr;
      }
    }

    icu::number::LocalizedNumberFormatter icu_number_formatter =
        CreateICUNumberFormatter(icu_locale, notation, compact_display,
                                 digit_options);

    return std::make_unique<ICU4CPluralRulesBackend>(
        std::move(icu_plural_rules), std::move(icu_number_formatter));
  }

  MaybeDirectHandle<String> Select(Isolate* isolate,
                                   double number) const override {
    UErrorCode status = U_ZERO_ERROR;
    icu::number::FormattedNumber formatted_number =
        icu_number_formatter_.formatDouble(number, status);
    DCHECK(U_SUCCESS(status));

    icu::UnicodeString result =
        icu_plural_rules_->select(formatted_number, status);
    DCHECK(U_SUCCESS(status));

    return Intl::ToString(isolate, result);
  }

  MaybeDirectHandle<String> SelectRange(Isolate* isolate,
                                        Tagged<String> locale_str, double x,
                                        double y) const override {
    Maybe<icu::number::LocalizedNumberRangeFormatter> maybe_range_formatter =
        JSNumberFormat::GetRangeFormatter(isolate, locale_str,
                                          icu_number_formatter_);
    MAYBE_RETURN(maybe_range_formatter, MaybeDirectHandle<String>());

    icu::number::LocalizedNumberRangeFormatter nrfmt =
        maybe_range_formatter.FromJust();

    UErrorCode status = U_ZERO_ERROR;
    icu::number::FormattedNumberRange formatted = nrfmt.formatFormattableRange(
        icu::Formattable(x), icu::Formattable(y), status);

    DCHECK(U_SUCCESS(status));
    icu::UnicodeString result = icu_plural_rules_->select(formatted, status);
    DCHECK(U_SUCCESS(status));

    return Intl::ToString(isolate, result);
  }

  void PopulateResolvedOptions(Isolate* isolate,
                               DirectHandle<JSObject> options) const override {
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString skeleton = icu_number_formatter_.toSkeleton(status);
    DCHECK(U_SUCCESS(status));

    // 6. Let pluralCategories be a List of Strings containing all possible
    // results of PluralRuleSelect for the selected locale pr.[[Locale]], sorted
    // according to the following order: "zero", "one", "two", "few", "many",
    // "other".
    std::unique_ptr<icu::StringEnumeration> categories(
        icu_plural_rules_->getKeywords(status));
    DCHECK(U_SUCCESS(status));
    int32_t count = categories->count(status);
    DCHECK(U_SUCCESS(status));

    Factory* factory = isolate->factory();
    DirectHandle<FixedArray> plural_categories = factory->NewFixedArray(count);
    const std::vector<const char*> kCategories = {"zero", "one",  "two",
                                                  "few",  "many", "other"};
    int32_t index = 0;
    std::for_each(
        kCategories.cbegin(), kCategories.cend(), [&](const char* val) {
          categories->reset(status);
          DCHECK(U_SUCCESS(status));
          for (int32_t i = 0; i < count; i++) {
            int32_t len;
            const char* cat = categories->next(&len, status);
            DCHECK(U_SUCCESS(status));
            if (cat == nullptr) break;
            if (std::strcmp(val, cat) == 0) {
              DirectHandle<String> value_string =
                  factory->NewStringFromAsciiChecked(val);
              plural_categories->set(index++, *value_string);
              break;
            }
          }
        });
    DCHECK(count == index);

    PopulateResolvedOptionsFromSkeleton(isolate, options, skeleton,
                                        plural_categories);
  }

 private:
  std::unique_ptr<icu::PluralRules> icu_plural_rules_;
  icu::number::LocalizedNumberFormatter icu_number_formatter_;
};


}  // namespace

// static
std::unique_ptr<PluralRulesInternal> PluralRulesInternal::Create(
    Isolate* isolate, const Intl::ResolvedLocale& resolved_locale,
    JSPluralRules::Type type, Intl::Notation notation,
    Intl::CompactDisplay compact_display,
    const Intl::NumberFormatDigitOptions& digit_options) {
  return ICU4CPluralRulesBackend::Create(
      isolate, resolved_locale, type, notation, compact_display, digit_options);
}

}  // namespace internal
}  // namespace v8

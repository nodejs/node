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
#include "unicode/uchar.h"
#include "unicode/unumberformatter.h"
#pragma GCC diagnostic pop

#ifdef V8_ENABLE_ECMA402_ICU4X
#include "icu4x/Decimal.hpp"
#include "icu4x/Locale.hpp"
#include "icu4x/PluralCategories.hpp"
#include "icu4x/PluralCategory.hpp"
#include "icu4x/PluralOperands.hpp"
#include "icu4x/PluralRulesWithRanges.hpp"
#endif  // V8_ENABLE_ECMA402_ICU4X

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
    bool success =
        CreateICUPluralRules(isolate, icu_locale, type, &icu_plural_rules);
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
    std::for_each(kCategories.cbegin(), kCategories.cend(),
                  [&](const char* val) {
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

#ifdef V8_ENABLE_ECMA402_ICU4X

DirectHandle<String> PluralCategoryToString(Isolate* isolate,
                                            icu4x::PluralCategory category) {
  Factory* factory = isolate->factory();
  switch (category) {
    case icu4x::PluralCategory::Zero:
      return factory->NewStringFromAsciiChecked("zero");
    case icu4x::PluralCategory::One:
      return factory->NewStringFromAsciiChecked("one");
    case icu4x::PluralCategory::Two:
      return factory->NewStringFromAsciiChecked("two");
    case icu4x::PluralCategory::Few:
      return factory->NewStringFromAsciiChecked("few");
    case icu4x::PluralCategory::Many:
      return factory->NewStringFromAsciiChecked("many");
    case icu4x::PluralCategory::Other:
      return factory->other_string();
  }
  UNREACHABLE();
}

// TODO(crbug.com/571709141): Remove this temporary bridge from ICU4C's
// `LocalizedNumberFormatter` to ICU4X's `(icu4x::Decimal, exponent)` once
// `Intl.NumberFormat` is migrated to ICU4X.
//
// In `ICU4CPluralRulesBackend`, `icu::PluralRules::select(FormattedNumber)` is
// compiled inside `libicu18n` and directly reads `FormattedNumber`'s private
// `@internal` `DecimalQuantity` (`fData->quantity`), which preserves the
// rounded significand, trailing fraction zeros, and compact/scientific
// exponent in memory.
//
// Outside `libicu18n`, `DecimalQuantity` (`number_decimalquantity.h`) is a
// private header, and `FormattedNumber`'s only `@stable` public extraction
// method (`toDecimalNumber`) converts `DecimalQuantity` to an IBM `decNumber`
// (`DecNum`), which strips both trailing fraction zeros and the
// compact/scientific exponent. We recover the trailing fraction zero count
// from `UNUM_FRACTION_FIELD` and the suppressed exponent from
// `UNUM_EXPONENT_FIELD` (for scientific/engineering notation) or the magnitude
// delta (for compact notation).
std::pair<std::unique_ptr<icu4x::Decimal>, uint8_t> FormatToDecimalAndExponent(
    const icu::number::LocalizedNumberFormatter& formatter,
    Intl::Notation notation, Intl::RoundingMode rounding_mode, double number) {
  DCHECK(std::isfinite(number));
  UErrorCode status = U_ZERO_ERROR;
  icu::number::FormattedNumber formatted =
      formatter.formatDouble(number, status);
  CHECK(U_SUCCESS(status));

  std::string dec_str = formatted.toDecimalNumber<std::string>(status);
  CHECK(U_SUCCESS(status));

  // `icu4x::Decimal::from_string` natively parses scientific notation (`E` /
  // `E-`), but rejects the explicit `+` in `decNumberToString`'s `E+` output.
  std::erase(dec_str, '+');
  auto dec_res = icu4x::Decimal::from_string(dec_str);
  CHECK(dec_res.is_ok());
  std::unique_ptr<icu4x::Decimal> dec = std::move(dec_res).ok().value();

  // Restore any trailing fraction zeros stripped by `toDecimalNumber`.
  icu::ConstrainedFieldPosition cfpos;
  cfpos.constrainField(UFIELD_CATEGORY_NUMBER, UNUM_FRACTION_FIELD);
  if (formatted.nextPosition(cfpos, status)) {
    CHECK(U_SUCCESS(status));
    icu::UnicodeString temp_str = formatted.toTempString(status);
    CHECK(U_SUCCESS(status));
    int32_t frac_len = temp_str.countChar32(
        cfpos.getStart(), cfpos.getLimit() - cfpos.getStart());
    dec->pad_end(-static_cast<int16_t>(frac_len));
  }
  CHECK(U_SUCCESS(status));

  int32_t exp = 0;
  if (notation == Intl::Notation::SCIENTIFIC ||
      notation == Intl::Notation::ENGINEERING) {
    icu::ConstrainedFieldPosition exp_cfpos;
    exp_cfpos.constrainField(UFIELD_CATEGORY_NUMBER, UNUM_EXPONENT_FIELD);
    if (formatted.nextPosition(exp_cfpos, status)) {
      CHECK(U_SUCCESS(status));
      icu::UnicodeString temp_str = formatted.toTempString(status);
      CHECK(U_SUCCESS(status));
      for (int32_t i = exp_cfpos.getStart(); i < exp_cfpos.getLimit();) {
        UChar32 c = temp_str.char32At(i);
        i += U16_LENGTH(c);
        int32_t digit = u_charDigitValue(c);
        CHECK_GE(digit, 0);
        exp = exp * 10 + digit;
      }
      icu::ConstrainedFieldPosition sign_cfpos;
      sign_cfpos.constrainField(UFIELD_CATEGORY_NUMBER,
                                UNUM_EXPONENT_SIGN_FIELD);
      if (formatted.nextPosition(sign_cfpos, status)) {
        CHECK(U_SUCCESS(status));
        exp = -exp;
      }
    }
    CHECK(U_SUCCESS(status));
  } else if (notation == Intl::Notation::COMPACT && !dec->is_zero() &&
             std::abs(number) >= 1.0) {
    auto orig_res =
        icu4x::Decimal::from_double_with_round_trip_precision(number);
    CHECK(orig_res.is_ok());
    std::unique_ptr<icu4x::Decimal> orig_dec = std::move(orig_res).ok().value();
    int16_t orig_mag = orig_dec->nonzero_magnitude_start();
    int16_t scaled_mag = dec->nonzero_magnitude_start();
    bool is_power_of_ten = dec->digit_at(scaled_mag) == 1 &&
                           dec->nonzero_magnitude_end() == scaled_mag;
    bool rounds_away_from_zero =
        rounding_mode == Intl::RoundingMode::kExpand ||
        (rounding_mode == Intl::RoundingMode::kCeil && number > 0) ||
        (rounding_mode == Intl::RoundingMode::kFloor && number < 0);
    bool carried =
        is_power_of_ten &&
        (orig_dec->digit_at(orig_mag) >= 2 ||
         (rounds_away_from_zero &&
          (orig_dec->nonzero_magnitude_end() < orig_mag || scaled_mag >= 3)));
    int32_t carry = carried ? 1 : 0;
    exp = (carried && scaled_mag >= 3)
              ? (orig_mag + 1)
              : std::max<int32_t>(0, (orig_mag + carry) - scaled_mag);
  }

  uint8_t exponent = 0;
  if (exp < 0) {
    // `icu4x::PluralOperands::from_significand_and_exponent` takes an unsigned
    // `uint8_t` exponent `c` (and shifts `dec` by `+c` when computing `i`, `v`,
    // `w`, `f`, `t`). In CLDR plural rules, `e`/`c` is only tested against `0`
    // and `0..5` (`e = 0 ... or e != 0..5`). Passing `exponent = 6` and
    // pre-shifting `dec` by `exp - 6` gives a net shift of `exp` for `i`, `v`,
    // `w`, `f`, `t` while keeping `c = 6` outside `0..5`.
    exponent = 6;
    dec->multiply_pow10(static_cast<int16_t>(exp - exponent));
  } else if (exp > std::numeric_limits<uint8_t>::max()) {
    exponent = std::numeric_limits<uint8_t>::max();
    dec->multiply_pow10(static_cast<int16_t>(exp - exponent));
  } else {
    exponent = static_cast<uint8_t>(exp);
  }

  return std::make_pair(std::move(dec), exponent);
}

class ICU4XPluralRulesBackend final : public PluralRulesInternal {
 public:
  ICU4XPluralRulesBackend(
      std::unique_ptr<icu4x::PluralRulesWithRanges> plural_rules,
      icu::number::LocalizedNumberFormatter icu_number_formatter,
      Intl::Notation notation, Intl::RoundingMode rounding_mode)
      : plural_rules_(std::move(plural_rules)),
        icu_number_formatter_(std::move(icu_number_formatter)),
        notation_(notation),
        rounding_mode_(rounding_mode) {}

  static std::unique_ptr<ICU4XPluralRulesBackend> Create(
      const Intl::ResolvedLocale& resolved_locale, JSPluralRules::Type type,
      Intl::Notation notation, Intl::CompactDisplay compact_display,
      const Intl::NumberFormatDigitOptions& digit_options) {
    const icu::Locale* icu_locale = &resolved_locale.icu_locale;
    icu::Locale no_extension_locale;
    auto locale_res = icu4x::Locale::from_string(resolved_locale.locale);
    if (!locale_res.is_ok()) {
      no_extension_locale = icu::Locale(icu_locale->getBaseName());
      std::string base_tag =
          Intl::ToLanguageTag(no_extension_locale).FromJust();
      locale_res = icu4x::Locale::from_string(base_tag);
      if (!locale_res.is_ok()) {
        return nullptr;
      }
      icu_locale = &no_extension_locale;
    }
    std::unique_ptr<icu4x::Locale> locale = std::move(locale_res).ok().value();

    auto rules_res =
        (type == JSPluralRules::Type::ORDINAL)
            ? icu4x::PluralRulesWithRanges::create_ordinal(*locale)
            : icu4x::PluralRulesWithRanges::create_cardinal(*locale);
    if (!rules_res.is_ok()) {
      return nullptr;
    }

    icu::number::LocalizedNumberFormatter icu_number_formatter =
        CreateICUNumberFormatter(*icu_locale, notation, compact_display,
                                 digit_options);

    return std::make_unique<ICU4XPluralRulesBackend>(
        std::move(rules_res).ok().value(), std::move(icu_number_formatter),
        notation, digit_options.rounding_mode);
  }

  MaybeDirectHandle<String> Select(Isolate* isolate,
                                   double number) const override {
    if (!std::isfinite(number)) {
      return isolate->factory()->other_string();
    }
    auto [dec, exponent] = FormatToDecimalAndExponent(
        icu_number_formatter_, notation_, rounding_mode_, number);
    std::unique_ptr<icu4x::PluralOperands> operands =
        icu4x::PluralOperands::from_significand_and_exponent(*dec, exponent);

    icu4x::PluralCategory category = plural_rules_->category_for(*operands);
    return PluralCategoryToString(isolate, category);
  }

  MaybeDirectHandle<String> SelectRange(Isolate* isolate,
                                        Tagged<String> locale_str, double x,
                                        double y) const override {
    if (!std::isfinite(x) || !std::isfinite(y)) {
      return isolate->factory()->other_string();
    }
    auto [start_dec, start_exp] = FormatToDecimalAndExponent(
        icu_number_formatter_, notation_, rounding_mode_, x);
    auto [end_dec, end_exp] = FormatToDecimalAndExponent(
        icu_number_formatter_, notation_, rounding_mode_, y);

    std::unique_ptr<icu4x::PluralOperands> start_operands =
        icu4x::PluralOperands::from_significand_and_exponent(*start_dec,
                                                             start_exp);
    if (start_dec->equals(*end_dec) && start_exp == end_exp) {
      icu4x::PluralCategory category =
          plural_rules_->category_for(*start_operands);
      return PluralCategoryToString(isolate, category);
    }

    std::unique_ptr<icu4x::PluralOperands> end_operands =
        icu4x::PluralOperands::from_significand_and_exponent(*end_dec, end_exp);
    icu4x::PluralCategory category =
        plural_rules_->category_for_range(*start_operands, *end_operands);
    return PluralCategoryToString(isolate, category);
  }

  void PopulateResolvedOptions(Isolate* isolate,
                               DirectHandle<JSObject> options) const override {
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString skeleton = icu_number_formatter_.toSkeleton(status);
    DCHECK(U_SUCCESS(status));

    icu4x::PluralCategories cats = plural_rules_->categories();
    int32_t count = (cats.zero ? 1 : 0) + (cats.one ? 1 : 0) +
                    (cats.two ? 1 : 0) + (cats.few ? 1 : 0) +
                    (cats.many ? 1 : 0) + (cats.other ? 1 : 0);

    DirectHandle<FixedArray> plural_categories =
        isolate->factory()->NewFixedArray(count);
    int32_t index = 0;
    auto push_category = [&](icu4x::PluralCategory category) {
      plural_categories->set(index++,
                             *PluralCategoryToString(isolate, category));
    };
    if (cats.zero) push_category(icu4x::PluralCategory::Zero);
    if (cats.one) push_category(icu4x::PluralCategory::One);
    if (cats.two) push_category(icu4x::PluralCategory::Two);
    if (cats.few) push_category(icu4x::PluralCategory::Few);
    if (cats.many) push_category(icu4x::PluralCategory::Many);
    if (cats.other) push_category(icu4x::PluralCategory::Other);
    DCHECK_EQ(count, index);

    PopulateResolvedOptionsFromSkeleton(isolate, options, skeleton,
                                        plural_categories);
  }

 private:
  std::unique_ptr<icu4x::PluralRulesWithRanges> plural_rules_;
  icu::number::LocalizedNumberFormatter icu_number_formatter_;
  Intl::Notation notation_;
  Intl::RoundingMode rounding_mode_;
};

#endif  // V8_ENABLE_ECMA402_ICU4X

}  // namespace

// static
std::unique_ptr<PluralRulesInternal> PluralRulesInternal::Create(
    Isolate* isolate, const Intl::ResolvedLocale& resolved_locale,
    JSPluralRules::Type type, Intl::Notation notation,
    Intl::CompactDisplay compact_display,
    const Intl::NumberFormatDigitOptions& digit_options) {
#ifdef V8_ENABLE_ECMA402_ICU4X
  if (v8_flags.ecma402_icu4x_plural_rules) {
    return ICU4XPluralRulesBackend::Create(resolved_locale, type, notation,
                                           compact_display, digit_options);
  }
#endif  // V8_ENABLE_ECMA402_ICU4X
  return ICU4CPluralRulesBackend::Create(
      isolate, resolved_locale, type, notation, compact_display, digit_options);
}

}  // namespace internal
}  // namespace v8

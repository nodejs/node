// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-number-format.h"

#include <set>
#include <string>

#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-number-format-inl.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/uloc.h"

namespace v8 {
namespace internal {

namespace {

UNumberFormatStyle ToNumberFormatStyle(
    JSNumberFormat::CurrencyDisplay currency_display) {
  switch (currency_display) {
    case JSNumberFormat::CurrencyDisplay::SYMBOL:
      return UNUM_CURRENCY;
    case JSNumberFormat::CurrencyDisplay::CODE:
      return UNUM_CURRENCY_ISO;
    case JSNumberFormat::CurrencyDisplay::NAME:
      return UNUM_CURRENCY_PLURAL;
    case JSNumberFormat::CurrencyDisplay::COUNT:
      UNREACHABLE();
  }
}

// ecma-402/#sec-currencydigits
// The currency is expected to an all upper case string value.
int CurrencyDigits(const icu::UnicodeString& currency) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t fraction_digits = ucurr_getDefaultFractionDigits(
      reinterpret_cast<const UChar*>(currency.getBuffer()), &status);
  // For missing currency codes, default to the most common, 2
  return U_SUCCESS(status) ? fraction_digits : 2;
}

bool IsAToZ(char ch) { return IsInRange(AsciiAlphaToLower(ch), 'a', 'z'); }

// ecma402/#sec-iswellformedcurrencycode
bool IsWellFormedCurrencyCode(const std::string& currency) {
  // Verifies that the input is a well-formed ISO 4217 currency code.
  // ecma402/#sec-currency-codes
  // 2. If the number of elements in normalized is not 3, return false.
  if (currency.length() != 3) return false;
  // 1. Let normalized be the result of mapping currency to upper case as
  //   described in 6.1.
  //
  // 3. If normalized contains any character that is not in
  // the range "A" to "Z" (U+0041 to U+005A), return false.
  //
  // 4. Return true.
  // Don't uppercase to test. It could convert invalid code into a valid one.
  // For example \u00DFP (Eszett+P) becomes SSP.
  return (IsAToZ(currency[0]) && IsAToZ(currency[1]) && IsAToZ(currency[2]));
}

}  // anonymous namespace

// static
// ecma402 #sec-intl.numberformat.prototype.resolvedoptions
Handle<JSObject> JSNumberFormat::ResolvedOptions(
    Isolate* isolate, Handle<JSNumberFormat> number_format_holder) {
  Factory* factory = isolate->factory();

  // 4. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> options = factory->NewJSObject(isolate->object_function());

  icu::NumberFormat* number_format =
      number_format_holder->icu_number_format()->raw();
  CHECK_NOT_NULL(number_format);

  Handle<String> locale =
      Handle<String>(number_format_holder->locale(), isolate);

  std::unique_ptr<char[]> locale_str = locale->ToCString();
  icu::Locale icu_locale = Intl::CreateICULocale(locale_str.get());

  std::string numbering_system = Intl::GetNumberingSystem(icu_locale);

  // 5. For each row of Table 4, except the header row, in table order, do
  // Table 4: Resolved Options of NumberFormat Instances
  //  Internal Slot                    Property
  //    [[Locale]]                      "locale"
  //    [[NumberingSystem]]             "numberingSystem"
  //    [[Style]]                       "style"
  //    [[Currency]]                    "currency"
  //    [[CurrencyDisplay]]             "currencyDisplay"
  //    [[MinimumIntegerDigits]]        "minimumIntegerDigits"
  //    [[MinimumFractionDigits]]       "minimumFractionDigits"
  //    [[MaximumFractionDigits]]       "maximumFractionDigits"
  //    [[MinimumSignificantDigits]]    "minimumSignificantDigits"
  //    [[MaximumSignificantDigits]]    "maximumSignificantDigits"
  //    [[UseGrouping]]                 "useGrouping"
  CHECK(JSReceiver::CreateDataProperty(isolate, options,
                                       factory->locale_string(), locale,
                                       Just(kDontThrow))
            .FromJust());
  if (!numbering_system.empty()) {
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->numberingSystem_string(),
              factory->NewStringFromAsciiChecked(numbering_system.c_str()),
              Just(kDontThrow))
              .FromJust());
  }
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->style_string(),
            number_format_holder->StyleAsString(), Just(kDontThrow))
            .FromJust());
  if (number_format_holder->style() == Style::CURRENCY) {
    icu::UnicodeString currency(number_format->getCurrency());
    DCHECK(!currency.isEmpty());
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->currency_string(),
              factory
                  ->NewStringFromTwoByte(Vector<const uint16_t>(
                      reinterpret_cast<const uint16_t*>(currency.getBuffer()),
                      currency.length()))
                  .ToHandleChecked(),
              Just(kDontThrow))
              .FromJust());

    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->currencyDisplay_string(),
              number_format_holder->CurrencyDisplayAsString(), Just(kDontThrow))
              .FromJust());
  }
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->minimumIntegerDigits_string(),
            factory->NewNumberFromInt(number_format->getMinimumIntegerDigits()),
            Just(kDontThrow))
            .FromJust());
  CHECK(
      JSReceiver::CreateDataProperty(
          isolate, options, factory->minimumFractionDigits_string(),
          factory->NewNumberFromInt(number_format->getMinimumFractionDigits()),
          Just(kDontThrow))
          .FromJust());
  CHECK(
      JSReceiver::CreateDataProperty(
          isolate, options, factory->maximumFractionDigits_string(),
          factory->NewNumberFromInt(number_format->getMaximumFractionDigits()),
          Just(kDontThrow))
          .FromJust());
  CHECK(number_format->getDynamicClassID() ==
        icu::DecimalFormat::getStaticClassID());
  icu::DecimalFormat* decimal_format =
      static_cast<icu::DecimalFormat*>(number_format);
  CHECK_NOT_NULL(decimal_format);
  if (decimal_format->areSignificantDigitsUsed()) {
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->minimumSignificantDigits_string(),
              factory->NewNumberFromInt(
                  decimal_format->getMinimumSignificantDigits()),
              Just(kDontThrow))
              .FromJust());
    CHECK(JSReceiver::CreateDataProperty(
              isolate, options, factory->maximumSignificantDigits_string(),
              factory->NewNumberFromInt(
                  decimal_format->getMaximumSignificantDigits()),
              Just(kDontThrow))
              .FromJust());
  }
  CHECK(JSReceiver::CreateDataProperty(
            isolate, options, factory->useGrouping_string(),
            factory->ToBoolean((number_format->isGroupingUsed() == TRUE)),
            Just(kDontThrow))
            .FromJust());
  return options;
}

// ecma402/#sec-unwrapnumberformat
MaybeHandle<JSNumberFormat> JSNumberFormat::UnwrapNumberFormat(
    Isolate* isolate, Handle<JSReceiver> format_holder) {
  // old code copy from NumberFormat::Unwrap that has no spec comment and
  // compiled but fail unit tests.
  Handle<Context> native_context =
      Handle<Context>(isolate->context()->native_context(), isolate);
  Handle<JSFunction> constructor = Handle<JSFunction>(
      JSFunction::cast(native_context->intl_number_format_function()), isolate);
  Handle<Object> object;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, object,
      Intl::LegacyUnwrapReceiver(isolate, format_holder, constructor,
                                 format_holder->IsJSNumberFormat()),
      JSNumberFormat);
  // 4. If ... or nf does not have an [[InitializedNumberFormat]] internal slot,
  // then
  if (!object->IsJSNumberFormat()) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "UnwrapNumberFormat")),
                    JSNumberFormat);
  }
  // 5. Return nf.
  return Handle<JSNumberFormat>::cast(object);
}

// static
MaybeHandle<JSNumberFormat> JSNumberFormat::Initialize(
    Isolate* isolate, Handle<JSNumberFormat> number_format,
    Handle<Object> locales, Handle<Object> options_obj) {
  // set the flags to 0 ASAP.
  number_format->set_flags(0);
  Factory* factory = isolate->factory();

  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSNumberFormat>());
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
        Object::ToObject(isolate, options_obj, "Intl.NumberFormat"),
        JSNumberFormat);
  }

  // At this point, options_obj can either be a JSObject or a JSProxy only.
  Handle<JSReceiver> options = Handle<JSReceiver>::cast(options_obj);

  // 4. Let opt be a new Record.
  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  // 6. Set opt.[[localeMatcher]] to matcher.
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.NumberFormat");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSNumberFormat>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 7. Let localeData be %NumberFormat%.[[LocaleData]].
  // 8. Let r be ResolveLocale(%NumberFormat%.[[AvailableLocales]],
  // requestedLocales, opt,  %NumberFormat%.[[RelevantExtensionKeys]],
  // localeData).
  std::set<std::string> relevant_extension_keys{"nu"};
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSNumberFormat::GetAvailableLocales(),
                          requested_locales, matcher, relevant_extension_keys);

  // 9. Set numberFormat.[[Locale]] to r.[[locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  number_format->set_locale(*locale_str);

  // 11. Let dataLocale be r.[[dataLocale]].
  //
  // 12. Let style be ? GetOption(options, "style", "string",  « "decimal",
  // "percent", "currency" », "decimal").
  const char* service = "Intl.NumberFormat";
  Maybe<Style> maybe_style = Intl::GetStringOption<Style>(
      isolate, options, "style", service, {"decimal", "percent", "currency"},
      {Style::DECIMAL, Style::PERCENT, Style::CURRENCY}, Style::DECIMAL);
  MAYBE_RETURN(maybe_style, MaybeHandle<JSNumberFormat>());
  Style style = maybe_style.FromJust();

  // 13. Set numberFormat.[[Style]] to style.
  number_format->set_style(style);

  // 14. Let currency be ? GetOption(options, "currency", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> currency_cstr;
  const std::vector<const char*> empty_values = {};
  Maybe<bool> found_currency = Intl::GetStringOption(
      isolate, options, "currency", empty_values, service, &currency_cstr);
  MAYBE_RETURN(found_currency, MaybeHandle<JSNumberFormat>());

  std::string currency;
  // 15. If currency is not undefined, then
  if (found_currency.FromJust()) {
    DCHECK_NOT_NULL(currency_cstr.get());
    currency = currency_cstr.get();
    // 15. a. If the result of IsWellFormedCurrencyCode(currency) is false,
    // throw a RangeError exception.
    if (!IsWellFormedCurrencyCode(currency)) {
      THROW_NEW_ERROR(
          isolate,
          NewRangeError(MessageTemplate::kInvalidCurrencyCode,
                        factory->NewStringFromAsciiChecked(currency.c_str())),
          JSNumberFormat);
    }
  }

  // 16. If style is "currency" and currency is undefined, throw a TypeError
  // exception.
  if (style == Style::CURRENCY && !found_currency.FromJust()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kCurrencyCode),
                    JSNumberFormat);
  }
  // 17. If style is "currency", then
  int c_digits = 0;
  icu::UnicodeString currency_ustr;
  if (style == Style::CURRENCY) {
    // a. Let currency be the result of converting currency to upper case as
    //    specified in 6.1
    std::transform(currency.begin(), currency.end(), currency.begin(), toupper);
    // c. Let cDigits be CurrencyDigits(currency).
    currency_ustr = currency.c_str();
    c_digits = CurrencyDigits(currency_ustr);
  }

  // 18. Let currencyDisplay be ? GetOption(options, "currencyDisplay",
  // "string", « "code",  "symbol", "name" », "symbol").
  Maybe<CurrencyDisplay> maybe_currencyDisplay =
      Intl::GetStringOption<CurrencyDisplay>(
          isolate, options, "currencyDisplay", service,
          {"code", "symbol", "name"},
          {CurrencyDisplay::CODE, CurrencyDisplay::SYMBOL,
           CurrencyDisplay::NAME},
          CurrencyDisplay::SYMBOL);
  MAYBE_RETURN(maybe_currencyDisplay, MaybeHandle<JSNumberFormat>());
  CurrencyDisplay currency_display = maybe_currencyDisplay.FromJust();
  UNumberFormatStyle format_style = ToNumberFormatStyle(currency_display);

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::NumberFormat> icu_number_format;
  icu::Locale no_extension_locale(r.icu_locale.getBaseName());
  if (style == Style::DECIMAL) {
    icu_number_format.reset(
        icu::NumberFormat::createInstance(r.icu_locale, status));
    // If the subclass is not DecimalFormat, fallback to no extension
    // because other subclass has not support the format() with
    // FieldPositionIterator yet.
    if (U_FAILURE(status) || icu_number_format.get() == nullptr ||
        icu_number_format->getDynamicClassID() !=
            icu::DecimalFormat::getStaticClassID()) {
      status = U_ZERO_ERROR;
      icu_number_format.reset(
          icu::NumberFormat::createInstance(no_extension_locale, status));
    }
  } else if (style == Style::PERCENT) {
    icu_number_format.reset(
        icu::NumberFormat::createPercentInstance(r.icu_locale, status));
    // If the subclass is not DecimalFormat, fallback to no extension
    // because other subclass has not support the format() with
    // FieldPositionIterator yet.
    if (U_FAILURE(status) || icu_number_format.get() == nullptr ||
        icu_number_format->getDynamicClassID() !=
            icu::DecimalFormat::getStaticClassID()) {
      status = U_ZERO_ERROR;
      icu_number_format.reset(icu::NumberFormat::createPercentInstance(
          no_extension_locale, status));
    }
  } else {
    DCHECK_EQ(style, Style::CURRENCY);
    icu_number_format.reset(
        icu::NumberFormat::createInstance(r.icu_locale, format_style, status));
    // If the subclass is not DecimalFormat, fallback to no extension
    // because other subclass has not support the format() with
    // FieldPositionIterator yet.
    if (U_FAILURE(status) || icu_number_format.get() == nullptr ||
        icu_number_format->getDynamicClassID() !=
            icu::DecimalFormat::getStaticClassID()) {
      status = U_ZERO_ERROR;
      icu_number_format.reset(icu::NumberFormat::createInstance(
          no_extension_locale, format_style, status));
    }
  }

  if (U_FAILURE(status) || icu_number_format.get() == nullptr) {
    status = U_ZERO_ERROR;
    // Remove extensions and try again.
    icu_number_format.reset(
        icu::NumberFormat::createInstance(no_extension_locale, status));

    if (U_FAILURE(status) || icu_number_format.get() == nullptr) {
      FATAL("Failed to create ICU number_format, are ICU data files missing?");
    }
  }
  DCHECK(U_SUCCESS(status));
  CHECK_NOT_NULL(icu_number_format.get());
  CHECK(icu_number_format->getDynamicClassID() ==
        icu::DecimalFormat::getStaticClassID());
  if (style == Style::CURRENCY) {
    // 19. If style is "currency", set  numberFormat.[[CurrencyDisplay]] to
    // currencyDisplay.
    number_format->set_currency_display(currency_display);

    // 17.b. Set numberFormat.[[Currency]] to currency.
    if (!currency_ustr.isEmpty()) {
      status = U_ZERO_ERROR;
      icu_number_format->setCurrency(currency_ustr.getBuffer(), status);
      CHECK(U_SUCCESS(status));
    }
  }

  // 20. If style is "currency", then
  int mnfd_default, mxfd_default;
  if (style == Style::CURRENCY) {
    //  a. Let mnfdDefault be cDigits.
    //  b. Let mxfdDefault be cDigits.
    mnfd_default = c_digits;
    mxfd_default = c_digits;
  } else {
    // 21. Else,
    // a. Let mnfdDefault be 0.
    mnfd_default = 0;
    // b. If style is "percent", then
    if (style == Style::PERCENT) {
      // i. Let mxfdDefault be 0.
      mxfd_default = 0;
    } else {
      // c. Else,
      // i. Let mxfdDefault be 3.
      mxfd_default = 3;
    }
  }
  // 22. Perform ? SetNumberFormatDigitOptions(numberFormat, options,
  // mnfdDefault, mxfdDefault).
  CHECK(icu_number_format->getDynamicClassID() ==
        icu::DecimalFormat::getStaticClassID());
  icu::DecimalFormat* icu_decimal_format =
      static_cast<icu::DecimalFormat*>(icu_number_format.get());
  Maybe<bool> maybe_set_number_for_digit_options =
      Intl::SetNumberFormatDigitOptions(isolate, icu_decimal_format, options,
                                        mnfd_default, mxfd_default);
  MAYBE_RETURN(maybe_set_number_for_digit_options, Handle<JSNumberFormat>());

  // 23. Let useGrouping be ? GetOption(options, "useGrouping", "boolean",
  // undefined, true).
  bool use_grouping = true;
  Maybe<bool> found_use_grouping = Intl::GetBoolOption(
      isolate, options, "useGrouping", service, &use_grouping);
  MAYBE_RETURN(found_use_grouping, MaybeHandle<JSNumberFormat>());
  // 24. Set numberFormat.[[UseGrouping]] to useGrouping.
  icu_number_format->setGroupingUsed(use_grouping ? TRUE : FALSE);

  // 25. Let dataLocaleData be localeData.[[<dataLocale>]].
  //
  // 26. Let patterns be dataLocaleData.[[patterns]].
  //
  // 27. Assert: patterns is a record (see 11.3.3).
  //
  // 28. Let stylePatterns be patterns.[[<style>]].
  //
  // 29. Set numberFormat.[[PositivePattern]] to
  // stylePatterns.[[positivePattern]].
  //
  // 30. Set numberFormat.[[NegativePattern]] to
  // stylePatterns.[[negativePattern]].

  Handle<Managed<icu::NumberFormat>> managed_number_format =
      Managed<icu::NumberFormat>::FromUniquePtr(isolate, 0,
                                                std::move(icu_number_format));
  number_format->set_icu_number_format(*managed_number_format);
  number_format->set_bound_format(*factory->undefined_value());

  // 31. Return numberFormat.
  return number_format;
}

Handle<String> JSNumberFormat::StyleAsString() const {
  switch (style()) {
    case Style::DECIMAL:
      return GetReadOnlyRoots().decimal_string_handle();
    case Style::PERCENT:
      return GetReadOnlyRoots().percent_string_handle();
    case Style::CURRENCY:
      return GetReadOnlyRoots().currency_string_handle();
    case Style::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSNumberFormat::CurrencyDisplayAsString() const {
  switch (currency_display()) {
    case CurrencyDisplay::CODE:
      return GetReadOnlyRoots().code_string_handle();
    case CurrencyDisplay::SYMBOL:
      return GetReadOnlyRoots().symbol_string_handle();
    case CurrencyDisplay::NAME:
      return GetReadOnlyRoots().name_string_handle();
    case CurrencyDisplay::COUNT:
      UNREACHABLE();
  }
}

namespace {
Maybe<icu::UnicodeString> IcuFormatNumber(
    Isolate* isolate, const icu::NumberFormat& number_format,
    Handle<Object> numeric_obj, icu::FieldPositionIterator* fp_iter) {
  icu::UnicodeString result;
  // If it is BigInt, handle it differently.
  UErrorCode status = U_ZERO_ERROR;
  if (numeric_obj->IsBigInt()) {
    Handle<BigInt> big_int = Handle<BigInt>::cast(numeric_obj);
    Handle<String> big_int_string;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, big_int_string,
                                     BigInt::ToString(isolate, big_int),
                                     Nothing<icu::UnicodeString>());
    number_format.format(
        {big_int_string->ToCString().get(), big_int_string->length()}, result,
        fp_iter, status);
  } else {
    double number = numeric_obj->Number();
    number_format.format(number, result, fp_iter, status);
  }
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NewTypeError(MessageTemplate::kIcuError),
                                 Nothing<icu::UnicodeString>());
  }
  return Just(result);
}

}  // namespace

MaybeHandle<String> JSNumberFormat::FormatNumeric(
    Isolate* isolate, const icu::NumberFormat& number_format,
    Handle<Object> numeric_obj) {
  DCHECK(numeric_obj->IsNumeric());

  Maybe<icu::UnicodeString> maybe_format =
      IcuFormatNumber(isolate, number_format, numeric_obj, nullptr);
  MAYBE_RETURN(maybe_format, Handle<String>());
  icu::UnicodeString result = maybe_format.FromJust();

  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

namespace {

bool cmp_NumberFormatSpan(const NumberFormatSpan& a,
                          const NumberFormatSpan& b) {
  // Regions that start earlier should be encountered earlier.
  if (a.begin_pos < b.begin_pos) return true;
  if (a.begin_pos > b.begin_pos) return false;
  // For regions that start in the same place, regions that last longer should
  // be encountered earlier.
  if (a.end_pos < b.end_pos) return false;
  if (a.end_pos > b.end_pos) return true;
  // For regions that are exactly the same, one of them must be the "literal"
  // backdrop we added, which has a field_id of -1, so consider higher field_ids
  // to be later.
  return a.field_id < b.field_id;
}

}  // namespace

// Flattens a list of possibly-overlapping "regions" to a list of
// non-overlapping "parts". At least one of the input regions must span the
// entire space of possible indexes. The regions parameter will sorted in-place
// according to some criteria; this is done for performance to avoid copying the
// input.
std::vector<NumberFormatSpan> FlattenRegionsToParts(
    std::vector<NumberFormatSpan>* regions) {
  // The intention of this algorithm is that it's used to translate ICU "fields"
  // to JavaScript "parts" of a formatted string. Each ICU field and JavaScript
  // part has an integer field_id, which corresponds to something like "grouping
  // separator", "fraction", or "percent sign", and has a begin and end
  // position. Here's a diagram of:

  // var nf = new Intl.NumberFormat(['de'], {style:'currency',currency:'EUR'});
  // nf.formatToParts(123456.78);

  //               :       6
  //  input regions:    0000000211 7
  // ('-' means -1):    ------------
  // formatted string: "123.456,78 €"
  // output parts:      0006000211-7

  // To illustrate the requirements of this algorithm, here's a contrived and
  // convoluted example of inputs and expected outputs:

  //              :          4
  //              :      22 33    3
  //              :      11111   22
  // input regions:     0000000  111
  //              :     ------------
  // formatted string: "abcdefghijkl"
  // output parts:      0221340--231
  // (The characters in the formatted string are irrelevant to this function.)

  // We arrange the overlapping input regions like a mountain range where
  // smaller regions are "on top" of larger regions, and we output a birds-eye
  // view of the mountains, so that smaller regions take priority over larger
  // regions.
  std::sort(regions->begin(), regions->end(), cmp_NumberFormatSpan);
  std::vector<size_t> overlapping_region_index_stack;
  // At least one item in regions must be a region spanning the entire string.
  // Due to the sorting above, the first item in the vector will be one of them.
  overlapping_region_index_stack.push_back(0);
  NumberFormatSpan top_region = regions->at(0);
  size_t region_iterator = 1;
  int32_t entire_size = top_region.end_pos;

  std::vector<NumberFormatSpan> out_parts;

  // The "climber" is a cursor that advances from left to right climbing "up"
  // and "down" the mountains. Whenever the climber moves to the right, that
  // represents an item of output.
  int32_t climber = 0;
  while (climber < entire_size) {
    int32_t next_region_begin_pos;
    if (region_iterator < regions->size()) {
      next_region_begin_pos = regions->at(region_iterator).begin_pos;
    } else {
      // finish off the rest of the input by proceeding to the end.
      next_region_begin_pos = entire_size;
    }

    if (climber < next_region_begin_pos) {
      while (top_region.end_pos < next_region_begin_pos) {
        if (climber < top_region.end_pos) {
          // step down
          out_parts.push_back(NumberFormatSpan(top_region.field_id, climber,
                                               top_region.end_pos));
          climber = top_region.end_pos;
        } else {
          // drop down
        }
        overlapping_region_index_stack.pop_back();
        top_region = regions->at(overlapping_region_index_stack.back());
      }
      if (climber < next_region_begin_pos) {
        // cross a plateau/mesa/valley
        out_parts.push_back(NumberFormatSpan(top_region.field_id, climber,
                                             next_region_begin_pos));
        climber = next_region_begin_pos;
      }
    }
    if (region_iterator < regions->size()) {
      overlapping_region_index_stack.push_back(region_iterator++);
      top_region = regions->at(overlapping_region_index_stack.back());
    }
  }
  return out_parts;
}

Maybe<int> JSNumberFormat::FormatToParts(Isolate* isolate,
                                         Handle<JSArray> result,
                                         int start_index,
                                         const icu::NumberFormat& number_format,
                                         Handle<Object> numeric_obj,
                                         Handle<String> unit) {
  DCHECK(numeric_obj->IsNumeric());
  icu::FieldPositionIterator fp_iter;
  Maybe<icu::UnicodeString> maybe_format =
      IcuFormatNumber(isolate, number_format, numeric_obj, &fp_iter);
  MAYBE_RETURN(maybe_format, Nothing<int>());
  icu::UnicodeString formatted = maybe_format.FromJust();

  int32_t length = formatted.length();
  int index = start_index;
  if (length == 0) return Just(index);

  std::vector<NumberFormatSpan> regions;
  // Add a "literal" backdrop for the entire string. This will be used if no
  // other region covers some part of the formatted string. It's possible
  // there's another field with exactly the same begin and end as this backdrop,
  // in which case the backdrop's field_id of -1 will give it lower priority.
  regions.push_back(NumberFormatSpan(-1, 0, formatted.length()));

  {
    icu::FieldPosition fp;
    while (fp_iter.next(fp)) {
      regions.push_back(NumberFormatSpan(fp.getField(), fp.getBeginIndex(),
                                         fp.getEndIndex()));
    }
  }

  std::vector<NumberFormatSpan> parts = FlattenRegionsToParts(&regions);

  for (auto it = parts.begin(); it < parts.end(); it++) {
    NumberFormatSpan part = *it;
    Handle<String> field_type_string =
        part.field_id == -1
            ? isolate->factory()->literal_string()
            : Intl::NumberFieldToType(isolate, numeric_obj, part.field_id);
    Handle<String> substring;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, substring,
        Intl::ToString(isolate, formatted, part.begin_pos, part.end_pos),
        Nothing<int>());
    if (unit.is_null()) {
      Intl::AddElement(isolate, result, index, field_type_string, substring);
    } else {
      Intl::AddElement(isolate, result, index, field_type_string, substring,
                       isolate->factory()->unit_string(), unit);
    }
    ++index;
  }
  JSObject::ValidateElements(*result);
  return Just(index);
}

MaybeHandle<JSArray> JSNumberFormat::FormatToParts(
    Isolate* isolate, Handle<JSNumberFormat> number_format,
    Handle<Object> numeric_obj) {
  CHECK(numeric_obj->IsNumeric());
  Factory* factory = isolate->factory();
  icu::NumberFormat* fmt = number_format->icu_number_format()->raw();
  CHECK_NOT_NULL(fmt);

  Handle<JSArray> result = factory->NewJSArray(0);

  Maybe<int> maybe_format_to_parts = JSNumberFormat::FormatToParts(
      isolate, result, 0, *fmt, numeric_obj, Handle<String>());
  MAYBE_RETURN(maybe_format_to_parts, Handle<JSArray>());

  return result;
}

const std::set<std::string>& JSNumberFormat::GetAvailableLocales() {
  static base::LazyInstance<Intl::AvailableLocales<icu::NumberFormat>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8

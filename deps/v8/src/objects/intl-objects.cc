// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/intl-objects.h"

#include <memory>

#include "src/api.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"
#include "unicode/brkiter.h"
#include "unicode/bytestream.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/gregocal.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/unum.h"
#include "unicode/upluralrules.h"
#include "unicode/uvernum.h"
#include "unicode/uversion.h"

#if U_ICU_VERSION_MAJOR_NUM >= 59
#include "unicode/char16ptr.h"
#endif

namespace v8 {
namespace internal {

namespace {

bool ExtractStringSetting(Isolate* isolate, Handle<JSObject> options,
                          const char* key, icu::UnicodeString* setting) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsString()) {
    v8::String::Utf8Value utf8_string(
        v8_isolate, v8::Utils::ToLocal(Handle<String>::cast(object)));
    *setting = icu::UnicodeString::fromUTF8(*utf8_string);
    return true;
  }
  return false;
}

bool ExtractIntegerSetting(Isolate* isolate, Handle<JSObject> options,
                           const char* key, int32_t* value) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsNumber()) {
    object->ToInt32(value);
    return true;
  }
  return false;
}

bool ExtractBooleanSetting(Isolate* isolate, Handle<JSObject> options,
                           const char* key, bool* value) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsBoolean()) {
    *value = object->BooleanValue();
    return true;
  }
  return false;
}

icu::SimpleDateFormat* CreateICUDateFormat(Isolate* isolate,
                                           const icu::Locale& icu_locale,
                                           Handle<JSObject> options) {
  // Create time zone as specified by the user. We have to re-create time zone
  // since calendar takes ownership.
  icu::TimeZone* tz = nullptr;
  icu::UnicodeString timezone;
  if (ExtractStringSetting(isolate, options, "timeZone", &timezone)) {
    tz = icu::TimeZone::createTimeZone(timezone);
  } else {
    tz = icu::TimeZone::createDefault();
  }

  // Create a calendar using locale, and apply time zone to it.
  UErrorCode status = U_ZERO_ERROR;
  icu::Calendar* calendar =
      icu::Calendar::createInstance(tz, icu_locale, status);

  if (calendar->getDynamicClassID() ==
      icu::GregorianCalendar::getStaticClassID()) {
    icu::GregorianCalendar* gc = (icu::GregorianCalendar*)calendar;
    UErrorCode status = U_ZERO_ERROR;
    // The beginning of ECMAScript time, namely -(2**53)
    const double start_of_time = -9007199254740992;
    gc->setGregorianChange(start_of_time, status);
    DCHECK(U_SUCCESS(status));
  }

  // Make formatter from skeleton. Calendar and numbering system are added
  // to the locale as Unicode extension (if they were specified at all).
  icu::SimpleDateFormat* date_format = nullptr;
  icu::UnicodeString skeleton;
  if (ExtractStringSetting(isolate, options, "skeleton", &skeleton)) {
    std::unique_ptr<icu::DateTimePatternGenerator> generator(
        icu::DateTimePatternGenerator::createInstance(icu_locale, status));
    icu::UnicodeString pattern;
    if (U_SUCCESS(status))
      pattern = generator->getBestPattern(skeleton, status);

    date_format = new icu::SimpleDateFormat(pattern, icu_locale, status);
    if (U_SUCCESS(status)) {
      date_format->adoptCalendar(calendar);
    }
  }

  if (U_FAILURE(status)) {
    delete calendar;
    delete date_format;
    date_format = nullptr;
  }

  return date_format;
}

void SetResolvedDateSettings(Isolate* isolate, const icu::Locale& icu_locale,
                             icu::SimpleDateFormat* date_format,
                             Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString pattern;
  date_format->toPattern(pattern);
  JSObject::SetProperty(
      resolved, factory->intl_pattern_symbol(),
      factory
          ->NewStringFromTwoByte(Vector<const uint16_t>(
              reinterpret_cast<const uint16_t*>(pattern.getBuffer()),
              pattern.length()))
          .ToHandleChecked(),
      LanguageMode::kSloppy)
      .Assert();

  // Set time zone and calendar.
  const icu::Calendar* calendar = date_format->getCalendar();
  // getType() returns legacy calendar type name instead of LDML/BCP47 calendar
  // key values. intl.js maps them to BCP47 values for key "ca".
  // TODO(jshin): Consider doing it here, instead.
  const char* calendar_name = calendar->getType();
  JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("calendar"),
                        factory->NewStringFromAsciiChecked(calendar_name),
                        LanguageMode::kSloppy)
      .Assert();

  const icu::TimeZone& tz = calendar->getTimeZone();
  icu::UnicodeString time_zone;
  tz.getID(time_zone);

  icu::UnicodeString canonical_time_zone;
  icu::TimeZone::getCanonicalID(time_zone, canonical_time_zone, status);
  if (U_SUCCESS(status)) {
    // In CLDR (http://unicode.org/cldr/trac/ticket/9943), Etc/UTC is made
    // a separate timezone ID from Etc/GMT even though they're still the same
    // timezone. We'd not have "Etc/GMT" here because we canonicalize it and
    // other GMT-variants to "UTC" in intl.js and "UTC" is turned to "Etc/UTC"
    // by ICU before getting here.
    // TODO(jshin): Figure out the cause of crbug.com/719609 and re-enable
    //  DCHECK(canonical_time_zone != UNICODE_STRING_SIMPLE("Etc/GMT")) .
    if (canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/UTC") ||
        canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/GMT")) {
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("timeZone"),
          factory->NewStringFromStaticChars("UTC"), LanguageMode::kSloppy)
          .Assert();
    } else {
      JSObject::SetProperty(resolved,
                            factory->NewStringFromStaticChars("timeZone"),
                            factory
                                ->NewStringFromTwoByte(Vector<const uint16_t>(
                                    reinterpret_cast<const uint16_t*>(
                                        canonical_time_zone.getBuffer()),
                                    canonical_time_zone.length()))
                                .ToHandleChecked(),
                            LanguageMode::kSloppy)
          .Assert();
    }
  }

  // Ugly hack. ICU doesn't expose numbering system in any way, so we have
  // to assume that for given locale NumberingSystem constructor produces the
  // same digits as NumberFormat/Calendar would.
  status = U_ZERO_ERROR;
  icu::NumberingSystem* numbering_system =
      icu::NumberingSystem::createInstance(icu_locale, status);
  if (U_SUCCESS(status)) {
    const char* ns = numbering_system->getName();
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("numberingSystem"),
        factory->NewStringFromAsciiChecked(ns), LanguageMode::kSloppy)
        .Assert();
  } else {
    JSObject::SetProperty(resolved,
                          factory->NewStringFromStaticChars("numberingSystem"),
                          factory->undefined_value(), LanguageMode::kSloppy)
        .Assert();
  }
  delete numbering_system;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY,
                     FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          LanguageMode::kSloppy)
        .Assert();
  }
}

void SetNumericSettings(Isolate* isolate, icu::DecimalFormat* number_format,
                        Handle<JSObject> options) {
  int32_t digits;
  if (ExtractIntegerSetting(isolate, options, "minimumIntegerDigits",
                            &digits)) {
    number_format->setMinimumIntegerDigits(digits);
  }

  if (ExtractIntegerSetting(isolate, options, "minimumFractionDigits",
                            &digits)) {
    number_format->setMinimumFractionDigits(digits);
  }

  if (ExtractIntegerSetting(isolate, options, "maximumFractionDigits",
                            &digits)) {
    number_format->setMaximumFractionDigits(digits);
  }

  bool significant_digits_used = false;
  if (ExtractIntegerSetting(isolate, options, "minimumSignificantDigits",
                            &digits)) {
    number_format->setMinimumSignificantDigits(digits);
    significant_digits_used = true;
  }

  if (ExtractIntegerSetting(isolate, options, "maximumSignificantDigits",
                            &digits)) {
    number_format->setMaximumSignificantDigits(digits);
    significant_digits_used = true;
  }

  number_format->setSignificantDigitsUsed(significant_digits_used);

  number_format->setRoundingMode(icu::DecimalFormat::kRoundHalfUp);
}

icu::DecimalFormat* CreateICUNumberFormat(Isolate* isolate,
                                          const icu::Locale& icu_locale,
                                          Handle<JSObject> options) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;
  icu::DecimalFormat* number_format = nullptr;
  icu::UnicodeString style;
  icu::UnicodeString currency;
  if (ExtractStringSetting(isolate, options, "style", &style)) {
    if (style == UNICODE_STRING_SIMPLE("currency")) {
      icu::UnicodeString display;
      ExtractStringSetting(isolate, options, "currency", &currency);
      ExtractStringSetting(isolate, options, "currencyDisplay", &display);

#if (U_ICU_VERSION_MAJOR_NUM == 4) && (U_ICU_VERSION_MINOR_NUM <= 6)
      icu::NumberFormat::EStyles format_style;
      if (display == UNICODE_STRING_SIMPLE("code")) {
        format_style = icu::NumberFormat::kIsoCurrencyStyle;
      } else if (display == UNICODE_STRING_SIMPLE("name")) {
        format_style = icu::NumberFormat::kPluralCurrencyStyle;
      } else {
        format_style = icu::NumberFormat::kCurrencyStyle;
      }
#else  // ICU version is 4.8 or above (we ignore versions below 4.0).
      UNumberFormatStyle format_style;
      if (display == UNICODE_STRING_SIMPLE("code")) {
        format_style = UNUM_CURRENCY_ISO;
      } else if (display == UNICODE_STRING_SIMPLE("name")) {
        format_style = UNUM_CURRENCY_PLURAL;
      } else {
        format_style = UNUM_CURRENCY;
      }
#endif

      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(icu_locale, format_style, status));

      if (U_FAILURE(status)) {
        delete number_format;
        return nullptr;
      }
    } else if (style == UNICODE_STRING_SIMPLE("percent")) {
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createPercentInstance(icu_locale, status));
      if (U_FAILURE(status)) {
        delete number_format;
        return nullptr;
      }
      // Make sure 1.1% doesn't go into 2%.
      number_format->setMinimumFractionDigits(1);
    } else {
      // Make a decimal instance by default.
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(icu_locale, status));
    }
  }

  if (U_FAILURE(status)) {
    delete number_format;
    return nullptr;
  }

  // Set all options.
  if (!currency.isEmpty()) {
    number_format->setCurrency(currency.getBuffer(), status);
  }

  SetNumericSettings(isolate, number_format, options);

  bool grouping;
  if (ExtractBooleanSetting(isolate, options, "useGrouping", &grouping)) {
    number_format->setGroupingUsed(grouping);
  }

  return number_format;
}

void SetResolvedNumericSettings(Isolate* isolate, const icu::Locale& icu_locale,
                                icu::DecimalFormat* number_format,
                                Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("minimumIntegerDigits"),
      factory->NewNumberFromInt(number_format->getMinimumIntegerDigits()),
      LanguageMode::kSloppy)
      .Assert();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("minimumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMinimumFractionDigits()),
      LanguageMode::kSloppy)
      .Assert();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("maximumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMaximumFractionDigits()),
      LanguageMode::kSloppy)
      .Assert();

  Handle<String> key =
      factory->NewStringFromStaticChars("minimumSignificantDigits");
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("minimumSignificantDigits"),
        factory->NewNumberFromInt(number_format->getMinimumSignificantDigits()),
        LanguageMode::kSloppy)
        .Assert();
  }

  key = factory->NewStringFromStaticChars("maximumSignificantDigits");
  maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("maximumSignificantDigits"),
        factory->NewNumberFromInt(number_format->getMaximumSignificantDigits()),
        LanguageMode::kSloppy)
        .Assert();
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY,
                     FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          LanguageMode::kSloppy)
        .Assert();
  }
}

void SetResolvedNumberSettings(Isolate* isolate, const icu::Locale& icu_locale,
                               icu::DecimalFormat* number_format,
                               Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();

  // Set resolved currency code in options.currency if not empty.
  icu::UnicodeString currency(number_format->getCurrency());
  if (!currency.isEmpty()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("currency"),
        factory
            ->NewStringFromTwoByte(Vector<const uint16_t>(
                reinterpret_cast<const uint16_t*>(currency.getBuffer()),
                currency.length()))
            .ToHandleChecked(),
        LanguageMode::kSloppy)
        .Assert();
  }

  // Ugly hack. ICU doesn't expose numbering system in any way, so we have
  // to assume that for given locale NumberingSystem constructor produces the
  // same digits as NumberFormat/Calendar would.
  UErrorCode status = U_ZERO_ERROR;
  icu::NumberingSystem* numbering_system =
      icu::NumberingSystem::createInstance(icu_locale, status);
  if (U_SUCCESS(status)) {
    const char* ns = numbering_system->getName();
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("numberingSystem"),
        factory->NewStringFromAsciiChecked(ns), LanguageMode::kSloppy)
        .Assert();
  } else {
    JSObject::SetProperty(resolved,
                          factory->NewStringFromStaticChars("numberingSystem"),
                          factory->undefined_value(), LanguageMode::kSloppy)
        .Assert();
  }
  delete numbering_system;

  JSObject::SetProperty(resolved,
                        factory->NewStringFromStaticChars("useGrouping"),
                        factory->ToBoolean(number_format->isGroupingUsed()),
                        LanguageMode::kSloppy)
      .Assert();

  SetResolvedNumericSettings(isolate, icu_locale, number_format, resolved);
}

icu::Collator* CreateICUCollator(Isolate* isolate,
                                 const icu::Locale& icu_locale,
                                 Handle<JSObject> options) {
  // Make collator from options.
  icu::Collator* collator = nullptr;
  UErrorCode status = U_ZERO_ERROR;
  collator = icu::Collator::createInstance(icu_locale, status);

  if (U_FAILURE(status)) {
    delete collator;
    return nullptr;
  }

  // Set flags first, and then override them with sensitivity if necessary.
  bool numeric;
  if (ExtractBooleanSetting(isolate, options, "numeric", &numeric)) {
    collator->setAttribute(UCOL_NUMERIC_COLLATION, numeric ? UCOL_ON : UCOL_OFF,
                           status);
  }

  // Normalization is always on, by the spec. We are free to optimize
  // if the strings are already normalized (but we don't have a way to tell
  // that right now).
  collator->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);

  icu::UnicodeString case_first;
  if (ExtractStringSetting(isolate, options, "caseFirst", &case_first)) {
    if (case_first == UNICODE_STRING_SIMPLE("upper")) {
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);
    } else if (case_first == UNICODE_STRING_SIMPLE("lower")) {
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_LOWER_FIRST, status);
    } else {
      // Default (false/off).
      collator->setAttribute(UCOL_CASE_FIRST, UCOL_OFF, status);
    }
  }

  icu::UnicodeString sensitivity;
  if (ExtractStringSetting(isolate, options, "sensitivity", &sensitivity)) {
    if (sensitivity == UNICODE_STRING_SIMPLE("base")) {
      collator->setStrength(icu::Collator::PRIMARY);
    } else if (sensitivity == UNICODE_STRING_SIMPLE("accent")) {
      collator->setStrength(icu::Collator::SECONDARY);
    } else if (sensitivity == UNICODE_STRING_SIMPLE("case")) {
      collator->setStrength(icu::Collator::PRIMARY);
      collator->setAttribute(UCOL_CASE_LEVEL, UCOL_ON, status);
    } else {
      // variant (default)
      collator->setStrength(icu::Collator::TERTIARY);
    }
  }

  bool ignore;
  if (ExtractBooleanSetting(isolate, options, "ignorePunctuation", &ignore)) {
    if (ignore) {
      collator->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, status);
    }
  }

  return collator;
}

void SetResolvedCollatorSettings(Isolate* isolate,
                                 const icu::Locale& icu_locale,
                                 icu::Collator* collator,
                                 Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();
  UErrorCode status = U_ZERO_ERROR;

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("numeric"),
      factory->ToBoolean(
          collator->getAttribute(UCOL_NUMERIC_COLLATION, status) == UCOL_ON),
      LanguageMode::kSloppy)
      .Assert();

  switch (collator->getAttribute(UCOL_CASE_FIRST, status)) {
    case UCOL_LOWER_FIRST:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("lower"), LanguageMode::kSloppy)
          .Assert();
      break;
    case UCOL_UPPER_FIRST:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("upper"), LanguageMode::kSloppy)
          .Assert();
      break;
    default:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("false"), LanguageMode::kSloppy)
          .Assert();
  }

  switch (collator->getAttribute(UCOL_STRENGTH, status)) {
    case UCOL_PRIMARY: {
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("primary"), LanguageMode::kSloppy)
          .Assert();

      // case level: true + s1 -> case, s1 -> base.
      if (UCOL_ON == collator->getAttribute(UCOL_CASE_LEVEL, status)) {
        JSObject::SetProperty(
            resolved, factory->NewStringFromStaticChars("sensitivity"),
            factory->NewStringFromStaticChars("case"), LanguageMode::kSloppy)
            .Assert();
      } else {
        JSObject::SetProperty(
            resolved, factory->NewStringFromStaticChars("sensitivity"),
            factory->NewStringFromStaticChars("base"), LanguageMode::kSloppy)
            .Assert();
      }
      break;
    }
    case UCOL_SECONDARY:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("secondary"), LanguageMode::kSloppy)
          .Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("accent"), LanguageMode::kSloppy)
          .Assert();
      break;
    case UCOL_TERTIARY:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("tertiary"), LanguageMode::kSloppy)
          .Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), LanguageMode::kSloppy)
          .Assert();
      break;
    case UCOL_QUATERNARY:
      // We shouldn't get quaternary and identical from ICU, but if we do
      // put them into variant.
      JSObject::SetProperty(resolved,
                            factory->NewStringFromStaticChars("strength"),
                            factory->NewStringFromStaticChars("quaternary"),
                            LanguageMode::kSloppy)
          .Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), LanguageMode::kSloppy)
          .Assert();
      break;
    default:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("identical"), LanguageMode::kSloppy)
          .Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), LanguageMode::kSloppy)
          .Assert();
  }

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("ignorePunctuation"),
      factory->ToBoolean(collator->getAttribute(UCOL_ALTERNATE_HANDLING,
                                                status) == UCOL_SHIFTED),
      LanguageMode::kSloppy)
      .Assert();

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY,
                     FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          LanguageMode::kSloppy)
        .Assert();
  }
}

bool CreateICUPluralRules(Isolate* isolate, const icu::Locale& icu_locale,
                          Handle<JSObject> options, icu::PluralRules** pl,
                          icu::DecimalFormat** nf) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;

  UPluralType type = UPLURAL_TYPE_CARDINAL;

  icu::UnicodeString type_string;
  if (ExtractStringSetting(isolate, options, "type", &type_string)) {
    if (type_string == UNICODE_STRING_SIMPLE("ordinal")) {
      type = UPLURAL_TYPE_ORDINAL;
    } else {
      CHECK(type_string == UNICODE_STRING_SIMPLE("cardinal"));
    }
  }

  icu::PluralRules* plural_rules =
      icu::PluralRules::forLocale(icu_locale, type, status);

  if (U_FAILURE(status)) {
    delete plural_rules;
    return false;
  }

  icu::DecimalFormat* number_format = static_cast<icu::DecimalFormat*>(
      icu::NumberFormat::createInstance(icu_locale, UNUM_DECIMAL, status));

  if (U_FAILURE(status)) {
    delete plural_rules;
    delete number_format;
    return false;
  }

  *pl = plural_rules;
  *nf = number_format;

  SetNumericSettings(isolate, number_format, options);

  // Set rounding mode.

  return true;
}

bool SetResolvedPluralRulesSettings(Isolate* isolate,
                                    const icu::Locale& icu_locale,
                                    icu::PluralRules* plural_rules,
                                    icu::DecimalFormat* number_format,
                                    Handle<JSObject> resolved) {
  SetResolvedNumericSettings(isolate, icu_locale, number_format, resolved);

  Factory* factory = isolate->factory();

  Handle<JSObject> pluralCategories = Handle<JSObject>::cast(
      JSObject::GetProperty(
          resolved, factory->NewStringFromStaticChars("pluralCategories"))
          .ToHandleChecked());

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> categories(
      plural_rules->getKeywords(status));
  if (U_FAILURE(status)) return false;

  if (U_FAILURE(status)) return false;

  for (int32_t i = 0;; i++) {
    const icu::UnicodeString* category = categories->snext(status);
    if (U_FAILURE(status)) return false;
    if (category == nullptr) return true;

    std::string keyword;
    Handle<String> value = factory->NewStringFromAsciiChecked(
        category->toUTF8String(keyword).data());

    LookupIterator it(isolate, pluralCategories, i, LookupIterator::OWN);
    JSObject::DefineOwnPropertyIgnoreAttributes(&it, value,
                                                PropertyAttributes::NONE)
        .ToHandleChecked();
  }
}

icu::BreakIterator* CreateICUBreakIterator(Isolate* isolate,
                                           const icu::Locale& icu_locale,
                                           Handle<JSObject> options) {
  UErrorCode status = U_ZERO_ERROR;
  icu::BreakIterator* break_iterator = nullptr;
  icu::UnicodeString type;
  if (!ExtractStringSetting(isolate, options, "type", &type)) return nullptr;

  if (type == UNICODE_STRING_SIMPLE("character")) {
    break_iterator =
        icu::BreakIterator::createCharacterInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("sentence")) {
    break_iterator =
        icu::BreakIterator::createSentenceInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("line")) {
    break_iterator = icu::BreakIterator::createLineInstance(icu_locale, status);
  } else {
    // Defualt is word iterator.
    break_iterator = icu::BreakIterator::createWordInstance(icu_locale, status);
  }

  if (U_FAILURE(status)) {
    delete break_iterator;
    return nullptr;
  }

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kBreakIterator);

  return break_iterator;
}

void SetResolvedBreakIteratorSettings(Isolate* isolate,
                                      const icu::Locale& icu_locale,
                                      icu::BreakIterator* break_iterator,
                                      Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();
  UErrorCode status = U_ZERO_ERROR;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY,
                     FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          LanguageMode::kSloppy)
        .Assert();
  }
}
}  // namespace

// static
icu::SimpleDateFormat* DateFormat::InitializeDateTimeFormat(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8_isolate, v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return nullptr;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::SimpleDateFormat* date_format =
      CreateICUDateFormat(isolate, icu_locale, options);
  if (!date_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    date_format = CreateICUDateFormat(isolate, no_extension_locale, options);

    if (!date_format) {
      FATAL("Failed to create ICU date format, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system, calendar).
    SetResolvedDateSettings(isolate, no_extension_locale, date_format,
                            resolved);
  } else {
    SetResolvedDateSettings(isolate, icu_locale, date_format, resolved);
  }

  return date_format;
}

icu::SimpleDateFormat* DateFormat::UnpackDateFormat(Isolate* isolate,
                                                    Handle<JSObject> obj) {
  return reinterpret_cast<icu::SimpleDateFormat*>(obj->GetEmbedderField(0));
}

void DateFormat::DeleteDateFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::SimpleDateFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

icu::DecimalFormat* NumberFormat::InitializeNumberFormat(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8_isolate, v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return nullptr;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::DecimalFormat* number_format =
      CreateICUNumberFormat(isolate, icu_locale, options);
  if (!number_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    number_format =
        CreateICUNumberFormat(isolate, no_extension_locale, options);

    if (!number_format) {
      FATAL("Failed to create ICU number format, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system).
    SetResolvedNumberSettings(isolate, no_extension_locale, number_format,
                              resolved);
  } else {
    SetResolvedNumberSettings(isolate, icu_locale, number_format, resolved);
  }

  return number_format;
}

icu::DecimalFormat* NumberFormat::UnpackNumberFormat(Isolate* isolate,
                                                     Handle<JSObject> obj) {
  return reinterpret_cast<icu::DecimalFormat*>(obj->GetEmbedderField(0));
}

void NumberFormat::DeleteNumberFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::DecimalFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

icu::Collator* Collator::InitializeCollator(Isolate* isolate,
                                            Handle<String> locale,
                                            Handle<JSObject> options,
                                            Handle<JSObject> resolved) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8_isolate, v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return nullptr;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::Collator* collator = CreateICUCollator(isolate, icu_locale, options);
  if (!collator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    collator = CreateICUCollator(isolate, no_extension_locale, options);

    if (!collator) {
      FATAL("Failed to create ICU collator, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system).
    SetResolvedCollatorSettings(isolate, no_extension_locale, collator,
                                resolved);
  } else {
    SetResolvedCollatorSettings(isolate, icu_locale, collator, resolved);
  }

  return collator;
}

icu::Collator* Collator::UnpackCollator(Isolate* isolate,
                                        Handle<JSObject> obj) {
  return reinterpret_cast<icu::Collator*>(obj->GetEmbedderField(0));
}

void Collator::DeleteCollator(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::Collator*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

bool PluralRules::InitializePluralRules(Isolate* isolate, Handle<String> locale,
                                        Handle<JSObject> options,
                                        Handle<JSObject> resolved,
                                        icu::PluralRules** plural_rules,
                                        icu::DecimalFormat** number_format) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char locale_name[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(reinterpret_cast<v8::Isolate*>(isolate),
                                     v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, locale_name, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return false;
    }
    icu_locale = icu::Locale(locale_name);
  }

  bool success = CreateICUPluralRules(isolate, icu_locale, options,
                                      plural_rules, number_format);
  if (!success) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    success = CreateICUPluralRules(isolate, no_extension_locale, options,
                                   plural_rules, number_format);

    if (!success) {
      FATAL("Failed to create ICU PluralRules, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system).
    success = SetResolvedPluralRulesSettings(
        isolate, no_extension_locale, *plural_rules, *number_format, resolved);
  } else {
    success = SetResolvedPluralRulesSettings(isolate, icu_locale, *plural_rules,
                                             *number_format, resolved);
  }

  return success;
}

icu::PluralRules* PluralRules::UnpackPluralRules(Isolate* isolate,
                                                 Handle<JSObject> obj) {
  return reinterpret_cast<icu::PluralRules*>(obj->GetEmbedderField(0));
}

icu::DecimalFormat* PluralRules::UnpackNumberFormat(Isolate* isolate,
                                                    Handle<JSObject> obj) {
  return reinterpret_cast<icu::DecimalFormat*>(obj->GetEmbedderField(1));
}

void PluralRules::DeletePluralRules(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::PluralRules*>(data.GetInternalField(0));
  delete reinterpret_cast<icu::DecimalFormat*>(data.GetInternalField(1));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

icu::BreakIterator* V8BreakIterator::InitializeBreakIterator(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8_isolate, v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return nullptr;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::BreakIterator* break_iterator =
      CreateICUBreakIterator(isolate, icu_locale, options);
  if (!break_iterator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    break_iterator =
        CreateICUBreakIterator(isolate, no_extension_locale, options);

    if (!break_iterator) {
      FATAL("Failed to create ICU break iterator, are ICU data files missing?");
    }

    // Set resolved settings (locale).
    SetResolvedBreakIteratorSettings(isolate, no_extension_locale,
                                     break_iterator, resolved);
  } else {
    SetResolvedBreakIteratorSettings(isolate, icu_locale, break_iterator,
                                     resolved);
  }

  return break_iterator;
}

icu::BreakIterator* V8BreakIterator::UnpackBreakIterator(Isolate* isolate,
                                                         Handle<JSObject> obj) {
  return reinterpret_cast<icu::BreakIterator*>(obj->GetEmbedderField(0));
}

void V8BreakIterator::DeleteBreakIterator(
    const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::BreakIterator*>(data.GetInternalField(0));
  delete reinterpret_cast<icu::UnicodeString*>(data.GetInternalField(1));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

}  // namespace internal
}  // namespace v8

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// limitations under the License.

#include "src/i18n.h"

#include <memory>

#include "src/api.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/string-case.h"
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
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/unum.h"
#include "unicode/uvernum.h"
#include "unicode/uversion.h"

#if U_ICU_VERSION_MAJOR_NUM >= 59
#include "unicode/char16ptr.h"
#endif

namespace v8 {
namespace internal {

namespace {

bool ExtractStringSetting(Isolate* isolate,
                          Handle<JSObject> options,
                          const char* key,
                          icu::UnicodeString* setting) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsString()) {
    v8::String::Utf8Value utf8_string(
        v8::Utils::ToLocal(Handle<String>::cast(object)));
    *setting = icu::UnicodeString::fromUTF8(*utf8_string);
    return true;
  }
  return false;
}


bool ExtractIntegerSetting(Isolate* isolate,
                           Handle<JSObject> options,
                           const char* key,
                           int32_t* value) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsNumber()) {
    object->ToInt32(value);
    return true;
  }
  return false;
}


bool ExtractBooleanSetting(Isolate* isolate,
                           Handle<JSObject> options,
                           const char* key,
                           bool* value) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(options, str).ToHandleChecked();
  if (object->IsBoolean()) {
    *value = object->BooleanValue();
    return true;
  }
  return false;
}


icu::SimpleDateFormat* CreateICUDateFormat(
    Isolate* isolate,
    const icu::Locale& icu_locale,
    Handle<JSObject> options) {
  // Create time zone as specified by the user. We have to re-create time zone
  // since calendar takes ownership.
  icu::TimeZone* tz = NULL;
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
  icu::SimpleDateFormat* date_format = NULL;
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


void SetResolvedDateSettings(Isolate* isolate,
                             const icu::Locale& icu_locale,
                             icu::SimpleDateFormat* date_format,
                             Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString pattern;
  date_format->toPattern(pattern);
  JSObject::SetProperty(
      resolved, factory->intl_pattern_symbol(),
      factory->NewStringFromTwoByte(
                   Vector<const uint16_t>(
                       reinterpret_cast<const uint16_t*>(pattern.getBuffer()),
                       pattern.length())).ToHandleChecked(),
      SLOPPY).Assert();

  // Set time zone and calendar.
  const icu::Calendar* calendar = date_format->getCalendar();
  // getType() returns legacy calendar type name instead of LDML/BCP47 calendar
  // key values. i18n.js maps them to BCP47 values for key "ca".
  // TODO(jshin): Consider doing it here, instead.
  const char* calendar_name = calendar->getType();
  JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("calendar"),
                        factory->NewStringFromAsciiChecked(calendar_name),
                        SLOPPY).Assert();

  const icu::TimeZone& tz = calendar->getTimeZone();
  icu::UnicodeString time_zone;
  tz.getID(time_zone);

  icu::UnicodeString canonical_time_zone;
  icu::TimeZone::getCanonicalID(time_zone, canonical_time_zone, status);
  if (U_SUCCESS(status)) {
    if (canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/GMT")) {
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("timeZone"),
          factory->NewStringFromStaticChars("UTC"), SLOPPY).Assert();
    } else {
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("timeZone"),
          factory->NewStringFromTwoByte(
                       Vector<const uint16_t>(
                           reinterpret_cast<const uint16_t*>(
                               canonical_time_zone.getBuffer()),
                           canonical_time_zone.length())).ToHandleChecked(),
          SLOPPY).Assert();
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
        factory->NewStringFromAsciiChecked(ns), SLOPPY).Assert();
  } else {
    JSObject::SetProperty(resolved,
                          factory->NewStringFromStaticChars("numberingSystem"),
                          factory->undefined_value(), SLOPPY).Assert();
  }
  delete numbering_system;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          SLOPPY).Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          SLOPPY).Assert();
  }
}


icu::DecimalFormat* CreateICUNumberFormat(
    Isolate* isolate,
    const icu::Locale& icu_locale,
    Handle<JSObject> options) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;
  icu::DecimalFormat* number_format = NULL;
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
        return NULL;
      }
    } else if (style == UNICODE_STRING_SIMPLE("percent")) {
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createPercentInstance(icu_locale, status));
      if (U_FAILURE(status)) {
        delete number_format;
        return NULL;
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
    return NULL;
  }

  // Set all options.
  if (!currency.isEmpty()) {
    number_format->setCurrency(currency.getBuffer(), status);
  }

  int32_t digits;
  if (ExtractIntegerSetting(
          isolate, options, "minimumIntegerDigits", &digits)) {
    number_format->setMinimumIntegerDigits(digits);
  }

  if (ExtractIntegerSetting(
          isolate, options, "minimumFractionDigits", &digits)) {
    number_format->setMinimumFractionDigits(digits);
  }

  if (ExtractIntegerSetting(
          isolate, options, "maximumFractionDigits", &digits)) {
    number_format->setMaximumFractionDigits(digits);
  }

  bool significant_digits_used = false;
  if (ExtractIntegerSetting(
          isolate, options, "minimumSignificantDigits", &digits)) {
    number_format->setMinimumSignificantDigits(digits);
    significant_digits_used = true;
  }

  if (ExtractIntegerSetting(
          isolate, options, "maximumSignificantDigits", &digits)) {
    number_format->setMaximumSignificantDigits(digits);
    significant_digits_used = true;
  }

  number_format->setSignificantDigitsUsed(significant_digits_used);

  bool grouping;
  if (ExtractBooleanSetting(isolate, options, "useGrouping", &grouping)) {
    number_format->setGroupingUsed(grouping);
  }

  // Set rounding mode.
  number_format->setRoundingMode(icu::DecimalFormat::kRoundHalfUp);

  return number_format;
}


void SetResolvedNumberSettings(Isolate* isolate,
                               const icu::Locale& icu_locale,
                               icu::DecimalFormat* number_format,
                               Handle<JSObject> resolved) {
  Factory* factory = isolate->factory();
  icu::UnicodeString pattern;
  number_format->toPattern(pattern);
  JSObject::SetProperty(
      resolved, factory->intl_pattern_symbol(),
      factory->NewStringFromTwoByte(
                   Vector<const uint16_t>(
                       reinterpret_cast<const uint16_t*>(pattern.getBuffer()),
                       pattern.length())).ToHandleChecked(),
      SLOPPY).Assert();

  // Set resolved currency code in options.currency if not empty.
  icu::UnicodeString currency(number_format->getCurrency());
  if (!currency.isEmpty()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("currency"),
        factory->NewStringFromTwoByte(Vector<const uint16_t>(
                                          reinterpret_cast<const uint16_t*>(
                                              currency.getBuffer()),
                                          currency.length())).ToHandleChecked(),
        SLOPPY).Assert();
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
        factory->NewStringFromAsciiChecked(ns), SLOPPY).Assert();
  } else {
    JSObject::SetProperty(resolved,
                          factory->NewStringFromStaticChars("numberingSystem"),
                          factory->undefined_value(), SLOPPY).Assert();
  }
  delete numbering_system;

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("useGrouping"),
      factory->ToBoolean(number_format->isGroupingUsed()), SLOPPY).Assert();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("minimumIntegerDigits"),
      factory->NewNumberFromInt(number_format->getMinimumIntegerDigits()),
      SLOPPY).Assert();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("minimumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMinimumFractionDigits()),
      SLOPPY).Assert();

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("maximumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMaximumFractionDigits()),
      SLOPPY).Assert();

  Handle<String> key =
      factory->NewStringFromStaticChars("minimumSignificantDigits");
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("minimumSignificantDigits"),
        factory->NewNumberFromInt(number_format->getMinimumSignificantDigits()),
        SLOPPY).Assert();
  }

  key = factory->NewStringFromStaticChars("maximumSignificantDigits");
  maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        resolved, factory->NewStringFromStaticChars("maximumSignificantDigits"),
        factory->NewNumberFromInt(number_format->getMaximumSignificantDigits()),
        SLOPPY).Assert();
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          SLOPPY).Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          SLOPPY).Assert();
  }
}


icu::Collator* CreateICUCollator(
    Isolate* isolate,
    const icu::Locale& icu_locale,
    Handle<JSObject> options) {
  // Make collator from options.
  icu::Collator* collator = NULL;
  UErrorCode status = U_ZERO_ERROR;
  collator = icu::Collator::createInstance(icu_locale, status);

  if (U_FAILURE(status)) {
    delete collator;
    return NULL;
  }

  // Set flags first, and then override them with sensitivity if necessary.
  bool numeric;
  if (ExtractBooleanSetting(isolate, options, "numeric", &numeric)) {
    collator->setAttribute(
        UCOL_NUMERIC_COLLATION, numeric ? UCOL_ON : UCOL_OFF, status);
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
      SLOPPY).Assert();

  switch (collator->getAttribute(UCOL_CASE_FIRST, status)) {
    case UCOL_LOWER_FIRST:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("lower"), SLOPPY).Assert();
      break;
    case UCOL_UPPER_FIRST:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("upper"), SLOPPY).Assert();
      break;
    default:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("caseFirst"),
          factory->NewStringFromStaticChars("false"), SLOPPY).Assert();
  }

  switch (collator->getAttribute(UCOL_STRENGTH, status)) {
    case UCOL_PRIMARY: {
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("primary"), SLOPPY).Assert();

      // case level: true + s1 -> case, s1 -> base.
      if (UCOL_ON == collator->getAttribute(UCOL_CASE_LEVEL, status)) {
        JSObject::SetProperty(
            resolved, factory->NewStringFromStaticChars("sensitivity"),
            factory->NewStringFromStaticChars("case"), SLOPPY).Assert();
      } else {
        JSObject::SetProperty(
            resolved, factory->NewStringFromStaticChars("sensitivity"),
            factory->NewStringFromStaticChars("base"), SLOPPY).Assert();
      }
      break;
    }
    case UCOL_SECONDARY:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("secondary"), SLOPPY).Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("accent"), SLOPPY).Assert();
      break;
    case UCOL_TERTIARY:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("tertiary"), SLOPPY).Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), SLOPPY).Assert();
      break;
    case UCOL_QUATERNARY:
      // We shouldn't get quaternary and identical from ICU, but if we do
      // put them into variant.
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("quaternary"), SLOPPY).Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), SLOPPY).Assert();
      break;
    default:
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("strength"),
          factory->NewStringFromStaticChars("identical"), SLOPPY).Assert();
      JSObject::SetProperty(
          resolved, factory->NewStringFromStaticChars("sensitivity"),
          factory->NewStringFromStaticChars("variant"), SLOPPY).Assert();
  }

  JSObject::SetProperty(
      resolved, factory->NewStringFromStaticChars("ignorePunctuation"),
      factory->ToBoolean(collator->getAttribute(UCOL_ALTERNATE_HANDLING,
                                                status) == UCOL_SHIFTED),
      SLOPPY).Assert();

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          SLOPPY).Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          SLOPPY).Assert();
  }
}


icu::BreakIterator* CreateICUBreakIterator(
    Isolate* isolate,
    const icu::Locale& icu_locale,
    Handle<JSObject> options) {
  UErrorCode status = U_ZERO_ERROR;
  icu::BreakIterator* break_iterator = NULL;
  icu::UnicodeString type;
  if (!ExtractStringSetting(isolate, options, "type", &type)) return NULL;

  if (type == UNICODE_STRING_SIMPLE("character")) {
    break_iterator =
      icu::BreakIterator::createCharacterInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("sentence")) {
    break_iterator =
      icu::BreakIterator::createSentenceInstance(icu_locale, status);
  } else if (type == UNICODE_STRING_SIMPLE("line")) {
    break_iterator =
      icu::BreakIterator::createLineInstance(icu_locale, status);
  } else {
    // Defualt is word iterator.
    break_iterator =
      icu::BreakIterator::createWordInstance(icu_locale, status);
  }

  if (U_FAILURE(status)) {
    delete break_iterator;
    return NULL;
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
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromAsciiChecked(result),
                          SLOPPY).Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(resolved, factory->NewStringFromStaticChars("locale"),
                          factory->NewStringFromStaticChars("und"),
                          SLOPPY).Assert();
  }
}
}  // namespace

// static
icu::SimpleDateFormat* DateFormat::InitializeDateTimeFormat(
    Isolate* isolate,
    Handle<String> locale,
    Handle<JSObject> options,
    Handle<JSObject> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::SimpleDateFormat* date_format = CreateICUDateFormat(
      isolate, icu_locale, options);
  if (!date_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    date_format = CreateICUDateFormat(isolate, no_extension_locale, options);

    if (!date_format) {
      FATAL("Failed to create ICU date format, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system, calendar).
    SetResolvedDateSettings(
        isolate, no_extension_locale, date_format, resolved);
  } else {
    SetResolvedDateSettings(isolate, icu_locale, date_format, resolved);
  }

  return date_format;
}


icu::SimpleDateFormat* DateFormat::UnpackDateFormat(
    Isolate* isolate,
    Handle<JSObject> obj) {
  return reinterpret_cast<icu::SimpleDateFormat*>(obj->GetEmbedderField(0));
}

void DateFormat::DeleteDateFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::SimpleDateFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}


icu::DecimalFormat* NumberFormat::InitializeNumberFormat(
    Isolate* isolate,
    Handle<String> locale,
    Handle<JSObject> options,
    Handle<JSObject> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::DecimalFormat* number_format =
      CreateICUNumberFormat(isolate, icu_locale, options);
  if (!number_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    number_format = CreateICUNumberFormat(
        isolate, no_extension_locale, options);

    if (!number_format) {
      FATAL("Failed to create ICU number format, are ICU data files missing?");
    }

    // Set resolved settings (pattern, numbering system).
    SetResolvedNumberSettings(
        isolate, no_extension_locale, number_format, resolved);
  } else {
    SetResolvedNumberSettings(isolate, icu_locale, number_format, resolved);
  }

  return number_format;
}


icu::DecimalFormat* NumberFormat::UnpackNumberFormat(
    Isolate* isolate,
    Handle<JSObject> obj) {
  return reinterpret_cast<icu::DecimalFormat*>(obj->GetEmbedderField(0));
}

void NumberFormat::DeleteNumberFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::DecimalFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}


icu::Collator* Collator::InitializeCollator(
    Isolate* isolate,
    Handle<String> locale,
    Handle<JSObject> options,
    Handle<JSObject> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
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
    SetResolvedCollatorSettings(
        isolate, no_extension_locale, collator, resolved);
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

icu::BreakIterator* V8BreakIterator::InitializeBreakIterator(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::Utf8Value bcp47_locale(v8::Utils::ToLocal(locale));
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::BreakIterator* break_iterator = CreateICUBreakIterator(
      isolate, icu_locale, options);
  if (!break_iterator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    break_iterator = CreateICUBreakIterator(
        isolate, no_extension_locale, options);

    if (!break_iterator) {
      FATAL("Failed to create ICU break iterator, are ICU data files missing?");
    }

    // Set resolved settings (locale).
    SetResolvedBreakIteratorSettings(
        isolate, no_extension_locale, break_iterator, resolved);
  } else {
    SetResolvedBreakIteratorSettings(
        isolate, icu_locale, break_iterator, resolved);
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

inline int FindFirstUpperOrNonAscii(Handle<String> s, int length) {
  for (int index = 0; index < length; ++index) {
    uint16_t ch = s->Get(index);
    if (V8_UNLIKELY(IsASCIIUpper(ch) || ch & ~0x7F)) {
      return index;
    }
  }
  return length;
}

}  // namespace

const UChar* GetUCharBufferFromFlat(const String::FlatContent& flat,
                                    std::unique_ptr<uc16[]>* dest,
                                    int32_t length) {
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    if (!*dest) {
      dest->reset(NewArray<uc16>(length));
      CopyChars(dest->get(), flat.ToOneByteVector().start(), length);
    }
    return reinterpret_cast<const UChar*>(dest->get());
  } else {
    return reinterpret_cast<const UChar*>(flat.ToUC16Vector().start());
  }
}

MUST_USE_RESULT Object* LocaleConvertCase(Handle<String> s, Isolate* isolate,
                                          bool is_to_upper, const char* lang) {
  auto case_converter = is_to_upper ? u_strToUpper : u_strToLower;
  int32_t src_length = s->length();
  int32_t dest_length = src_length;
  UErrorCode status;
  Handle<SeqTwoByteString> result;
  std::unique_ptr<uc16[]> sap;

  if (dest_length == 0) return isolate->heap()->empty_string();

  // This is not a real loop. It'll be executed only once (no overflow) or
  // twice (overflow).
  for (int i = 0; i < 2; ++i) {
    // Case conversion can increase the string length (e.g. sharp-S => SS) so
    // that we have to handle RangeError exceptions here.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(dest_length));
    DisallowHeapAllocation no_gc;
    DCHECK(s->IsFlat());
    String::FlatContent flat = s->GetFlatContent();
    const UChar* src = GetUCharBufferFromFlat(flat, &sap, src_length);
    status = U_ZERO_ERROR;
    dest_length = case_converter(reinterpret_cast<UChar*>(result->GetChars()),
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
    return *result;
  }
  if (U_SUCCESS(status)) {
    DCHECK(dest_length < result->length());
    return *Handle<SeqTwoByteString>::cast(
        SeqString::Truncate(result, dest_length));
  }
  return *s;
}

MUST_USE_RESULT Object* ConvertToLower(Handle<String> s, Isolate* isolate) {
  if (!s->HasOnlyOneByteChars()) {
    // Use a slower implementation for strings with characters beyond U+00FF.
    return LocaleConvertCase(s, isolate, false, "");
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
  int index_to_first_unprocessed = length;
  const bool is_short = length < static_cast<int>(sizeof(uintptr_t));
  if (is_short) {
    index_to_first_unprocessed = FindFirstUpperOrNonAscii(s, length);
    // Nothing to do if the string is all ASCII with no uppercase.
    if (index_to_first_unprocessed == length) return *s;
  }

  Handle<SeqOneByteString> result =
      isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

  DisallowHeapAllocation no_gc;
  DCHECK(s->IsFlat());
  String::FlatContent flat = s->GetFlatContent();
  uint8_t* dest = result->GetChars();
  if (flat.IsOneByte()) {
    const uint8_t* src = flat.ToOneByteVector().start();
    bool has_changed_character = false;
    index_to_first_unprocessed = FastAsciiConvert<true>(
        reinterpret_cast<char*>(dest), reinterpret_cast<const char*>(src),
        length, &has_changed_character);
    // If not ASCII, we keep the result up to index_to_first_unprocessed and
    // process the rest.
    if (index_to_first_unprocessed == length)
      return has_changed_character ? *result : *s;

    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dest[index] = ToLatin1Lower(static_cast<uint16_t>(src[index]));
    }
  } else {
    if (index_to_first_unprocessed == length) {
      DCHECK(!is_short);
      index_to_first_unprocessed = FindFirstUpperOrNonAscii(s, length);
    }
    // Nothing to do if the string is all ASCII with no uppercase.
    if (index_to_first_unprocessed == length) return *s;
    const uint16_t* src = flat.ToUC16Vector().start();
    CopyChars(dest, src, index_to_first_unprocessed);
    for (int index = index_to_first_unprocessed; index < length; ++index) {
      dest[index] = ToLatin1Lower(static_cast<uint16_t>(src[index]));
    }
  }

  return *result;
}

MUST_USE_RESULT Object* ConvertToUpper(Handle<String> s, Isolate* isolate) {
  int32_t length = s->length();
  if (s->HasOnlyOneByteChars() && length > 0) {
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();

    DCHECK(s->IsFlat());
    int sharp_s_count;
    bool is_result_single_byte;
    {
      DisallowHeapAllocation no_gc;
      String::FlatContent flat = s->GetFlatContent();
      uint8_t* dest = result->GetChars();
      if (flat.IsOneByte()) {
        Vector<const uint8_t> src = flat.ToOneByteVector();
        bool has_changed_character = false;
        int index_to_first_unprocessed =
            FastAsciiConvert<false>(reinterpret_cast<char*>(result->GetChars()),
                                    reinterpret_cast<const char*>(src.start()),
                                    length, &has_changed_character);
        if (index_to_first_unprocessed == length)
          return has_changed_character ? *result : *s;
        // If not ASCII, we keep the result up to index_to_first_unprocessed and
        // process the rest.
        is_result_single_byte =
            ToUpperOneByte(src.SubVector(index_to_first_unprocessed, length),
                           dest + index_to_first_unprocessed, &sharp_s_count);
      } else {
        DCHECK(flat.IsTwoByte());
        Vector<const uint16_t> src = flat.ToUC16Vector();
        if (ToUpperFastASCII(src, result)) return *result;
        is_result_single_byte = ToUpperOneByte(src, dest, &sharp_s_count);
      }
    }

    // Go to the full Unicode path if there are characters whose uppercase
    // is beyond the Latin-1 range (cannot be represented in OneByteString).
    if (V8_UNLIKELY(!is_result_single_byte)) {
      return LocaleConvertCase(s, isolate, true, "");
    }

    if (sharp_s_count == 0) return *result;

    // We have sharp_s_count sharp-s characters, but the result is still
    // in the Latin-1 range.
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        isolate->factory()->NewRawOneByteString(length + sharp_s_count));
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = s->GetFlatContent();
    if (flat.IsOneByte()) {
      ToUpperWithSharpS(flat.ToOneByteVector(), result);
    } else {
      ToUpperWithSharpS(flat.ToUC16Vector(), result);
    }

    return *result;
  }

  return LocaleConvertCase(s, isolate, true, "");
}

MUST_USE_RESULT Object* ConvertCase(Handle<String> s, bool is_upper,
                                    Isolate* isolate) {
  return is_upper ? ConvertToUpper(s, isolate) : ConvertToLower(s, isolate);
}

ICUTimezoneCache::ICUTimezoneCache() : timezone_(nullptr) { Clear(); }

ICUTimezoneCache::~ICUTimezoneCache() { Clear(); }

const char* ICUTimezoneCache::LocalTimezone(double time_ms) {
  bool is_dst = DaylightSavingsOffset(time_ms) != 0;
  char* name = is_dst ? dst_timezone_name_ : timezone_name_;
  if (name[0] == '\0') {
    icu::UnicodeString result;
    GetTimeZone()->getDisplayName(is_dst, icu::TimeZone::LONG, result);
    result += '\0';

    icu::CheckedArrayByteSink byte_sink(name, kMaxTimezoneChars);
    result.toUTF8(byte_sink);
    CHECK(!byte_sink.Overflowed());
  }
  return const_cast<const char*>(name);
}

icu::TimeZone* ICUTimezoneCache::GetTimeZone() {
  if (timezone_ == nullptr) {
    timezone_ = icu::TimeZone::createDefault();
  }
  return timezone_;
}

bool ICUTimezoneCache::GetOffsets(double time_ms, int32_t* raw_offset,
                                  int32_t* dst_offset) {
  UErrorCode status = U_ZERO_ERROR;
  GetTimeZone()->getOffset(time_ms, false, *raw_offset, *dst_offset, status);
  return U_SUCCESS(status);
}

double ICUTimezoneCache::DaylightSavingsOffset(double time_ms) {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(time_ms, &raw_offset, &dst_offset)) return 0;
  return dst_offset;
}

double ICUTimezoneCache::LocalTimeOffset() {
  int32_t raw_offset, dst_offset;
  if (!GetOffsets(icu::Calendar::getNow(), &raw_offset, &dst_offset)) return 0;
  return raw_offset;
}

void ICUTimezoneCache::Clear() {
  delete timezone_;
  timezone_ = nullptr;
  timezone_name_[0] = '\0';
  dst_timezone_name_[0] = '\0';
}

}  // namespace internal
}  // namespace v8

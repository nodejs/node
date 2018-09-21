// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/intl-objects.h"
#include "src/objects/intl-objects-inl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "src/api-inl.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/intl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/managed.h"
#include "src/objects/string.h"
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
#include "unicode/regex.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/unum.h"
#include "unicode/upluralrules.h"
#include "unicode/ures.h"
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
      JSReceiver::GetProperty(isolate, options, str).ToHandleChecked();
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
      JSReceiver::GetProperty(isolate, options, str).ToHandleChecked();
  if (object->IsNumber()) {
    return object->ToInt32(value);
  }
  return false;
}

bool ExtractBooleanSetting(Isolate* isolate, Handle<JSObject> options,
                           const char* key, bool* value) {
  Handle<String> str = isolate->factory()->NewStringFromAsciiChecked(key);
  Handle<Object> object =
      JSReceiver::GetProperty(isolate, options, str).ToHandleChecked();
  if (object->IsBoolean()) {
    *value = object->BooleanValue(isolate);
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
    // See https://github.com/tc39/ecma402/issues/225 . The best pattern
    // generation needs to be done in the base locale according to the
    // current spec however odd it may be. See also crbug.com/826549 .
    // This is a temporary work-around to get v8's external behavior to match
    // the current spec, but does not follow the spec provisions mentioned
    // in the above Ecma 402 issue.
    // TODO(jshin): The spec may need to be revised because using the base
    // locale for the pattern match is not quite right. Moreover, what to
    // do with 'related year' part when 'chinese/dangi' calendar is specified
    // has to be discussed. Revisit once the spec is clarified/revised.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    std::unique_ptr<icu::DateTimePatternGenerator> generator(
        icu::DateTimePatternGenerator::createInstance(no_extension_locale,
                                                      status));
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
      isolate, resolved, factory->intl_pattern_symbol(),
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
  JSObject::SetProperty(
      isolate, resolved, factory->NewStringFromStaticChars("calendar"),
      factory->NewStringFromAsciiChecked(calendar_name), LanguageMode::kSloppy)
      .Assert();

  const icu::TimeZone& tz = calendar->getTimeZone();
  icu::UnicodeString time_zone;
  tz.getID(time_zone);

  icu::UnicodeString canonical_time_zone;
  icu::TimeZone::getCanonicalID(time_zone, canonical_time_zone, status);
  if (U_SUCCESS(status)) {
    // In CLDR (http://unicode.org/cldr/trac/ticket/9943), Etc/UTC is made
    // a separate timezone ID from Etc/GMT even though they're still the same
    // timezone. We have Etc/UTC because 'UTC', 'Etc/Universal',
    // 'Etc/Zulu' and others are turned to 'Etc/UTC' by ICU. Etc/GMT comes
    // from Etc/GMT0, Etc/GMT+0, Etc/GMT-0, Etc/Greenwich.
    // ecma402##sec-canonicalizetimezonename step 3
    if (canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/UTC") ||
        canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/GMT")) {
      JSObject::SetProperty(
          isolate, resolved, factory->NewStringFromStaticChars("timeZone"),
          factory->NewStringFromStaticChars("UTC"), LanguageMode::kSloppy)
          .Assert();
    } else {
      JSObject::SetProperty(isolate, resolved,
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
        isolate, resolved, factory->NewStringFromStaticChars("numberingSystem"),
        factory->NewStringFromAsciiChecked(ns), LanguageMode::kSloppy)
        .Assert();
  } else {
    JSObject::SetProperty(isolate, resolved,
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
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromAsciiChecked(result), LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromStaticChars("und"), LanguageMode::kSloppy)
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
      isolate, resolved,
      factory->NewStringFromStaticChars("minimumIntegerDigits"),
      factory->NewNumberFromInt(number_format->getMinimumIntegerDigits()),
      LanguageMode::kSloppy)
      .Assert();

  JSObject::SetProperty(
      isolate, resolved,
      factory->NewStringFromStaticChars("minimumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMinimumFractionDigits()),
      LanguageMode::kSloppy)
      .Assert();

  JSObject::SetProperty(
      isolate, resolved,
      factory->NewStringFromStaticChars("maximumFractionDigits"),
      factory->NewNumberFromInt(number_format->getMaximumFractionDigits()),
      LanguageMode::kSloppy)
      .Assert();

  Handle<String> key =
      factory->NewStringFromStaticChars("minimumSignificantDigits");
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        isolate, resolved,
        factory->NewStringFromStaticChars("minimumSignificantDigits"),
        factory->NewNumberFromInt(number_format->getMinimumSignificantDigits()),
        LanguageMode::kSloppy)
        .Assert();
  }

  key = factory->NewStringFromStaticChars("maximumSignificantDigits");
  maybe = JSReceiver::HasOwnProperty(resolved, key);
  CHECK(maybe.IsJust());
  if (maybe.FromJust()) {
    JSObject::SetProperty(
        isolate, resolved,
        factory->NewStringFromStaticChars("maximumSignificantDigits"),
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
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromAsciiChecked(result), LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromStaticChars("und"), LanguageMode::kSloppy)
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
        isolate, resolved, factory->NewStringFromStaticChars("currency"),
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
        isolate, resolved, factory->NewStringFromStaticChars("numberingSystem"),
        factory->NewStringFromAsciiChecked(ns), LanguageMode::kSloppy)
        .Assert();
  } else {
    JSObject::SetProperty(isolate, resolved,
                          factory->NewStringFromStaticChars("numberingSystem"),
                          factory->undefined_value(), LanguageMode::kSloppy)
        .Assert();
  }
  delete numbering_system;

  JSObject::SetProperty(isolate, resolved,
                        factory->NewStringFromStaticChars("useGrouping"),
                        factory->ToBoolean(number_format->isGroupingUsed()),
                        LanguageMode::kSloppy)
      .Assert();

  SetResolvedNumericSettings(isolate, icu_locale, number_format, resolved);
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
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromAsciiChecked(result), LanguageMode::kSloppy)
        .Assert();
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        isolate, resolved, factory->NewStringFromStaticChars("locale"),
        factory->NewStringFromStaticChars("und"), LanguageMode::kSloppy)
        .Assert();
  }
}

MaybeHandle<JSObject> CachedOrNewService(Isolate* isolate,
                                         Handle<String> service,
                                         Handle<Object> locales,
                                         Handle<Object> options,
                                         Handle<Object> internal_options) {
  Handle<Object> result;
  Handle<Object> undefined_value(ReadOnlyRoots(isolate).undefined_value(),
                                 isolate);
  Handle<Object> args[] = {service, locales, options, internal_options};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, isolate->cached_or_new_service(),
                      undefined_value, arraysize(args), args),
      JSArray);
  return Handle<JSObject>::cast(result);
}
}  // namespace

icu::Locale Intl::CreateICULocale(Isolate* isolate,
                                  Handle<String> bcp47_locale_str) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::String::Utf8Value bcp47_locale(v8_isolate,
                                     v8::Utils::ToLocal(bcp47_locale_str));
  CHECK_NOT_NULL(*bcp47_locale);

  DisallowHeapAllocation no_gc;

  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;

  // bcp47_locale_str should be a canonicalized language tag, which
  // means this shouldn't fail.
  uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                      &icu_length, &status);
  CHECK(U_SUCCESS(status));
  CHECK_LT(0, icu_length);

  icu::Locale icu_locale(icu_result);
  if (icu_locale.isBogus()) {
    FATAL("Failed to create ICU locale, are ICU data files missing?");
  }

  return icu_locale;
}

// static
icu::SimpleDateFormat* DateFormat::InitializeDateTimeFormat(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  icu::Locale icu_locale = Intl::CreateICULocale(isolate, locale);
  DCHECK(!icu_locale.isBogus());

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

  CHECK_NOT_NULL(date_format);
  return date_format;
}

icu::SimpleDateFormat* DateFormat::UnpackDateFormat(Handle<JSObject> obj) {
  return reinterpret_cast<icu::SimpleDateFormat*>(
      obj->GetEmbedderField(DateFormat::kSimpleDateFormatIndex));
}

void DateFormat::DeleteDateFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::SimpleDateFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

MaybeHandle<JSObject> DateFormat::Unwrap(Isolate* isolate,
                                         Handle<JSReceiver> receiver,
                                         const char* method_name) {
  Handle<Context> native_context =
      Handle<Context>(isolate->context()->native_context(), isolate);
  Handle<JSFunction> constructor = Handle<JSFunction>(
      JSFunction::cast(native_context->intl_date_time_format_function()),
      isolate);
  Handle<String> method_name_str =
      isolate->factory()->NewStringFromAsciiChecked(method_name);

  return Intl::UnwrapReceiver(isolate, receiver, constructor,
                              Intl::Type::kDateTimeFormat, method_name_str,
                              true);
}

// ecma402/#sec-formatdatetime
// FormatDateTime( dateTimeFormat, x )
MaybeHandle<String> DateFormat::FormatDateTime(
    Isolate* isolate, Handle<JSObject> date_time_format_holder, double x) {
  double date_value = DateCache::TimeClip(x);
  if (std::isnan(date_value)) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kInvalidTimeValue),
                    String);
  }

  CHECK(Intl::IsObjectOfType(isolate, date_time_format_holder,
                             Intl::Type::kDateTimeFormat));
  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(date_time_format_holder);
  CHECK_NOT_NULL(date_format);

  icu::UnicodeString result;
  date_format->format(date_value, result);

  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

// ecma402/#sec-datetime-format-functions
// DateTime Format Functions
MaybeHandle<String> DateFormat::DateTimeFormat(
    Isolate* isolate, Handle<JSObject> date_time_format_holder,
    Handle<Object> date) {
  // 2. Assert: Type(dtf) is Object and dtf has an [[InitializedDateTimeFormat]]
  // internal slot.
  DCHECK(Intl::IsObjectOfType(isolate, date_time_format_holder,
                              Intl::Type::kDateTimeFormat));

  // 3. If date is not provided or is undefined, then
  double x;
  if (date->IsUndefined()) {
    // 3.a Let x be Call(%Date_now%, undefined).
    x = JSDate::CurrentTimeValue(isolate);
  } else {
    // 4. Else,
    //    a. Let x be ? ToNumber(date).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, date, Object::ToNumber(isolate, date),
                               String);
    CHECK(date->IsNumber());
    x = date->Number();
  }
  // 5. Return FormatDateTime(dtf, x).
  return DateFormat::FormatDateTime(isolate, date_time_format_holder, x);
}

MaybeHandle<String> DateFormat::ToLocaleDateTime(
    Isolate* isolate, Handle<Object> date, Handle<Object> locales,
    Handle<Object> options, const char* required, const char* defaults,
    const char* service) {
  Factory* factory = isolate->factory();
  // 1. Let x be ? thisTimeValue(this value);
  if (!date->IsJSDate()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 factory->NewStringFromStaticChars("Date")),
                    String);
  }

  double const x = Handle<JSDate>::cast(date)->value()->Number();
  // 2. If x is NaN, return "Invalid Date"
  if (std::isnan(x)) {
    return factory->NewStringFromStaticChars("Invalid Date");
  }

  // 3. Let options be ? ToDateTimeOptions(options, required, defaults).
  Handle<JSObject> internal_options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, internal_options,
      DateFormat::ToDateTimeOptions(isolate, options, required, defaults),
      String);

  // 4. Let dateFormat be ? Construct(%DateTimeFormat%, « locales, options »).
  Handle<JSObject> date_format;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, date_format,
      CachedOrNewService(isolate, factory->NewStringFromAsciiChecked(service),
                         locales, options, internal_options),
      String);

  // 5. Return FormatDateTime(dateFormat, x).
  return DateFormat::FormatDateTime(isolate, date_format, x);
}

icu::DecimalFormat* NumberFormat::InitializeNumberFormat(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  icu::Locale icu_locale = Intl::CreateICULocale(isolate, locale);
  DCHECK(!icu_locale.isBogus());

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

  CHECK_NOT_NULL(number_format);
  return number_format;
}

icu::DecimalFormat* NumberFormat::UnpackNumberFormat(Handle<JSObject> obj) {
  return reinterpret_cast<icu::DecimalFormat*>(
      obj->GetEmbedderField(NumberFormat::kDecimalFormatIndex));
}

void NumberFormat::DeleteNumberFormat(const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::DecimalFormat*>(data.GetInternalField(0));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

icu::BreakIterator* V8BreakIterator::InitializeBreakIterator(
    Isolate* isolate, Handle<String> locale, Handle<JSObject> options,
    Handle<JSObject> resolved) {
  icu::Locale icu_locale = Intl::CreateICULocale(isolate, locale);
  DCHECK(!icu_locale.isBogus());

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

  CHECK_NOT_NULL(break_iterator);
  return break_iterator;
}

icu::BreakIterator* V8BreakIterator::UnpackBreakIterator(Handle<JSObject> obj) {
  return reinterpret_cast<icu::BreakIterator*>(
      obj->GetEmbedderField(V8BreakIterator::kBreakIteratorIndex));
}

void V8BreakIterator::DeleteBreakIterator(
    const v8::WeakCallbackInfo<void>& data) {
  delete reinterpret_cast<icu::BreakIterator*>(data.GetInternalField(0));
  delete reinterpret_cast<icu::UnicodeString*>(data.GetInternalField(1));
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}

void V8BreakIterator::AdoptText(Isolate* isolate,
                                Handle<JSObject> break_iterator_holder,
                                Handle<String> text) {
  icu::BreakIterator* break_iterator =
      V8BreakIterator::UnpackBreakIterator(break_iterator_holder);
  CHECK_NOT_NULL(break_iterator);

  icu::UnicodeString* u_text = reinterpret_cast<icu::UnicodeString*>(
      break_iterator_holder->GetEmbedderField(
          V8BreakIterator::kUnicodeStringIndex));
  delete u_text;

  int length = text->length();
  text = String::Flatten(isolate, text);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = text->GetFlatContent();
  std::unique_ptr<uc16[]> sap;
  const UChar* text_value = GetUCharBufferFromFlat(flat, &sap, length);
  u_text = new icu::UnicodeString(text_value, length);
  break_iterator_holder->SetEmbedderField(V8BreakIterator::kUnicodeStringIndex,
                                          reinterpret_cast<Smi*>(u_text));

  break_iterator->setText(*u_text);
}

MaybeHandle<String> Intl::ToString(Isolate* isolate,
                                   const icu::UnicodeString& string) {
  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(string.getBuffer()), string.length()));
}

MaybeHandle<String> Intl::ToString(Isolate* isolate,
                                   const icu::UnicodeString& string,
                                   int32_t begin, int32_t end) {
  return Intl::ToString(isolate, string.tempSubStringBetween(begin, end));
}

namespace {

Handle<JSObject> InnerAddElement(Isolate* isolate, Handle<JSArray> array,
                                 int index, Handle<String> field_type_string,
                                 Handle<String> value) {
  // let element = $array[$index] = {
  //   type: $field_type_string,
  //   value: $value
  // }
  // return element;
  Factory* factory = isolate->factory();
  Handle<JSObject> element = factory->NewJSObject(isolate->object_function());
  JSObject::AddProperty(isolate, element, factory->type_string(),
                        field_type_string, NONE);

  JSObject::AddProperty(isolate, element, factory->value_string(), value, NONE);
  JSObject::AddDataElement(array, index, element, NONE);
  return element;
}

}  // namespace

void Intl::AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                      Handle<String> field_type_string, Handle<String> value) {
  // Same as $array[$index] = {type: $field_type_string, value: $value};
  InnerAddElement(isolate, array, index, field_type_string, value);
}

void Intl::AddElement(Isolate* isolate, Handle<JSArray> array, int index,
                      Handle<String> field_type_string, Handle<String> value,
                      Handle<String> additional_property_name,
                      Handle<String> additional_property_value) {
  // Same as $array[$index] = {
  //   type: $field_type_string, value: $value,
  //   $additional_property_name: $additional_property_value
  // }
  Handle<JSObject> element =
      InnerAddElement(isolate, array, index, field_type_string, value);
  JSObject::AddProperty(isolate, element, additional_property_name,
                        additional_property_value, NONE);
}
// Build the shortened locale; eg, convert xx_Yyyy_ZZ  to xx_ZZ.
bool Intl::RemoveLocaleScriptTag(const std::string& icu_locale,
                                 std::string* locale_less_script) {
  icu::Locale new_locale = icu::Locale::createCanonical(icu_locale.c_str());
  const char* icu_script = new_locale.getScript();
  if (icu_script == NULL || strlen(icu_script) == 0) {
    *locale_less_script = std::string();
    return false;
  }

  const char* icu_language = new_locale.getLanguage();
  const char* icu_country = new_locale.getCountry();
  icu::Locale short_locale = icu::Locale(icu_language, icu_country);
  const char* icu_name = short_locale.getName();
  *locale_less_script = std::string(icu_name);
  return true;
}

namespace {

Maybe<bool> IsPropertyUndefined(Isolate* isolate, Handle<JSObject> options,
                                const char* property) {
  Factory* factory = isolate->factory();
  // i. Let prop be the property name.
  // ii. Let value be ? Get(options, prop).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(
          isolate, options, factory->NewStringFromAsciiChecked(property)),
      Nothing<bool>());
  return Just(value->IsUndefined(isolate));
}

}  // namespace

// ecma-402/#sec-todatetimeoptions
MaybeHandle<JSObject> DateFormat::ToDateTimeOptions(
    Isolate* isolate, Handle<Object> input_options, const char* required,
    const char* defaults) {
  Factory* factory = isolate->factory();
  // 1. If options is undefined, let options be null; otherwise let options be ?
  //    ToObject(options).
  Handle<JSObject> options;
  if (input_options->IsUndefined(isolate)) {
    options = factory->NewJSObjectWithNullProto();
  } else {
    Handle<JSReceiver> options_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_obj,
                               Object::ToObject(isolate, input_options),
                               JSObject);
    // 2. Let options be ObjectCreate(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               JSObject::ObjectCreate(isolate, options_obj),
                               JSObject);
  }

  // 3. Let needDefaults be true.
  bool needs_default = true;

  bool required_is_any = strcmp(required, "any") == 0;
  // 4. If required is "date" or "any", then
  if (required_is_any || (strcmp(required, "date") == 0)) {
    // a. For each of the property names "weekday", "year", "month", "day", do
    for (auto& prop : {"weekday", "year", "month", "day"}) {
      //  i. Let prop be the property name.
      // ii. Let value be ? Get(options, prop)
      Maybe<bool> maybe_undefined = IsPropertyUndefined(isolate, options, prop);
      MAYBE_RETURN(maybe_undefined, Handle<JSObject>());
      // iii. If value is not undefined, let needDefaults be false.
      if (!maybe_undefined.FromJust()) {
        needs_default = false;
      }
    }
  }

  // 5. If required is "time" or "any", then
  if (required_is_any || (strcmp(required, "time") == 0)) {
    // a. For each of the property names "hour", "minute", "second", do
    for (auto& prop : {"hour", "minute", "second"}) {
      //  i. Let prop be the property name.
      // ii. Let value be ? Get(options, prop)
      Maybe<bool> maybe_undefined = IsPropertyUndefined(isolate, options, prop);
      MAYBE_RETURN(maybe_undefined, Handle<JSObject>());
      // iii. If value is not undefined, let needDefaults be false.
      if (!maybe_undefined.FromJust()) {
        needs_default = false;
      }
    }
  }

  // 6. If needDefaults is true and defaults is either "date" or "all", then
  if (needs_default) {
    bool default_is_all = strcmp(defaults, "all") == 0;
    if (default_is_all || (strcmp(defaults, "date") == 0)) {
      // a. For each of the property names "year", "month", "day", do
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      for (auto& prop : {"year", "month", "day"}) {
        MAYBE_RETURN(
            JSReceiver::CreateDataProperty(
                isolate, options, factory->NewStringFromAsciiChecked(prop),
                factory->numeric_string(), kThrowOnError),
            Handle<JSObject>());
      }
    }
    // 7. If needDefaults is true and defaults is either "time" or "all", then
    if (default_is_all || (strcmp(defaults, "time") == 0)) {
      // a. For each of the property names "hour", "minute", "second", do
      // i. Perform ? CreateDataPropertyOrThrow(options, prop, "numeric").
      for (auto& prop : {"hour", "minute", "second"}) {
        MAYBE_RETURN(
            JSReceiver::CreateDataProperty(
                isolate, options, factory->NewStringFromAsciiChecked(prop),
                factory->numeric_string(), kThrowOnError),
            Handle<JSObject>());
      }
    }
  }
  // 8. Return options.
  return options;
}

std::set<std::string> Intl::GetAvailableLocales(const IcuService& service) {
  const icu::Locale* icu_available_locales = nullptr;
  int32_t count = 0;
  std::set<std::string> locales;

  switch (service) {
    case IcuService::kBreakIterator:
      icu_available_locales = icu::BreakIterator::getAvailableLocales(count);
      break;
    case IcuService::kCollator:
      icu_available_locales = icu::Collator::getAvailableLocales(count);
      break;
    case IcuService::kDateFormat:
      icu_available_locales = icu::DateFormat::getAvailableLocales(count);
      break;
    case IcuService::kNumberFormat:
      icu_available_locales = icu::NumberFormat::getAvailableLocales(count);
      break;
    case IcuService::kPluralRules:
      // TODO(littledan): For PluralRules, filter out locales that
      // don't support PluralRules.
      // PluralRules is missing an appropriate getAvailableLocales method,
      // so we should filter from all locales, but it's not clear how; see
      // https://ssl.icu-project.org/trac/ticket/12756
      icu_available_locales = icu::Locale::getAvailableLocales(count);
      break;
    case IcuService::kResourceBundle: {
      UErrorCode status = U_ZERO_ERROR;
      UEnumeration* en = ures_openAvailableLocales(nullptr, &status);
      int32_t length = 0;
      const char* locale_str = uenum_next(en, &length, &status);
      while (U_SUCCESS(status) && (locale_str != nullptr)) {
        std::string locale(locale_str, length);
        std::replace(locale.begin(), locale.end(), '_', '-');
        locales.insert(locale);
        std::string shortened_locale;
        if (Intl::RemoveLocaleScriptTag(locale_str, &shortened_locale)) {
          std::replace(shortened_locale.begin(), shortened_locale.end(), '_',
                       '-');
          locales.insert(shortened_locale);
        }
        locale_str = uenum_next(en, &length, &status);
      }
      uenum_close(en);
      return locales;
    }
    case IcuService::kRelativeDateTimeFormatter: {
      // ICU RelativeDateTimeFormatter does not provide a getAvailableLocales()
      // interface, because RelativeDateTimeFormatter depends on
      // 1. NumberFormat and 2. ResourceBundle, return the
      // intersection of these two set.
      // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20009
      // TODO(ftang): change to call ICU's getAvailableLocales() after it is
      // added.
      std::set<std::string> number_format_set(
          Intl::GetAvailableLocales(IcuService::kNumberFormat));
      std::set<std::string> resource_bundle_set(
          Intl::GetAvailableLocales(IcuService::kResourceBundle));
      set_intersection(resource_bundle_set.begin(), resource_bundle_set.end(),
                       number_format_set.begin(), number_format_set.end(),
                       std::inserter(locales, locales.begin()));
      return locales;
    }
    case IcuService::kListFormatter: {
      // TODO(ftang): for now just use
      // icu::Locale::getAvailableLocales(count) until we migrate to
      // Intl::GetAvailableLocales().
      // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20015
      icu_available_locales = icu::Locale::getAvailableLocales(count);
      break;
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = icu_available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }
    std::string locale(result);
    locales.insert(locale);

    std::string shortened_locale;
    if (Intl::RemoveLocaleScriptTag(icu_name, &shortened_locale)) {
      std::replace(shortened_locale.begin(), shortened_locale.end(), '_', '-');
      locales.insert(shortened_locale);
    }
  }

  return locales;
}

IcuService Intl::StringToIcuService(Handle<String> service) {
  if (service->IsUtf8EqualTo(CStrVector("collator"))) {
    return IcuService::kCollator;
  } else if (service->IsUtf8EqualTo(CStrVector("numberformat"))) {
    return IcuService::kNumberFormat;
  } else if (service->IsUtf8EqualTo(CStrVector("dateformat"))) {
    return IcuService::kDateFormat;
  } else if (service->IsUtf8EqualTo(CStrVector("breakiterator"))) {
    return IcuService::kBreakIterator;
  } else if (service->IsUtf8EqualTo(CStrVector("pluralrules"))) {
    return IcuService::kPluralRules;
  } else if (service->IsUtf8EqualTo(CStrVector("relativetimeformat"))) {
    return IcuService::kRelativeDateTimeFormatter;
  } else if (service->IsUtf8EqualTo(CStrVector("listformat"))) {
    return IcuService::kListFormatter;
  }
  UNREACHABLE();
}

V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> Intl::AvailableLocalesOf(
    Isolate* isolate, Handle<String> service) {
  Factory* factory = isolate->factory();
  std::set<std::string> results =
      Intl::GetAvailableLocales(StringToIcuService(service));
  Handle<JSObject> locales = factory->NewJSObjectWithNullProto();

  int32_t i = 0;
  for (auto iter = results.begin(); iter != results.end(); ++iter) {
    RETURN_ON_EXCEPTION(
        isolate,
        JSObject::SetOwnPropertyIgnoreAttributes(
            locales, factory->NewStringFromAsciiChecked(iter->c_str()),
            factory->NewNumber(i++), NONE),
        JSObject);
  }
  return locales;
}

std::string Intl::DefaultLocale(Isolate* isolate) {
  if (isolate->default_locale().empty()) {
    icu::Locale default_locale;
    // Translate ICU's fallback locale to a well-known locale.
    if (strcmp(default_locale.getName(), "en_US_POSIX") == 0) {
      isolate->set_default_locale("en-US");
    } else {
      // Set the locale
      char result[ULOC_FULLNAME_CAPACITY];
      UErrorCode status = U_ZERO_ERROR;
      int32_t length =
          uloc_toLanguageTag(default_locale.getName(), result,
                             ULOC_FULLNAME_CAPACITY, FALSE, &status);
      isolate->set_default_locale(
          U_SUCCESS(status) ? std::string(result, length) : "und");
    }
    DCHECK(!isolate->default_locale().empty());
  }
  return isolate->default_locale();
}

bool Intl::IsObjectOfType(Isolate* isolate, Handle<Object> input,
                          Intl::Type expected_type) {
  if (!input->IsJSObject()) return false;
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<Symbol> marker = isolate->factory()->intl_initialized_marker_symbol();
  Handle<Object> tag = JSReceiver::GetDataProperty(obj, marker);

  if (!tag->IsSmi()) return false;

  Intl::Type type = Intl::TypeFromSmi(Smi::cast(*tag));
  return type == expected_type;
}

namespace {

// In ECMA 402 v1, Intl constructors supported a mode of operation
// where calling them with an existing object as a receiver would
// transform the receiver into the relevant Intl instance with all
// internal slots. In ECMA 402 v2, this capability was removed, to
// avoid adding internal slots on existing objects. In ECMA 402 v3,
// the capability was re-added as "normative optional" in a mode
// which chains the underlying Intl instance on any object, when the
// constructor is called
//
// See ecma402/#legacy-constructor.
MaybeHandle<Object> LegacyUnwrapReceiver(Isolate* isolate,
                                         Handle<JSReceiver> receiver,
                                         Handle<JSFunction> constructor,
                                         Intl::Type type) {
  bool has_initialized_slot = Intl::IsObjectOfType(isolate, receiver, type);

  Handle<Object> obj_is_instance_of;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj_is_instance_of,
                             Object::InstanceOf(isolate, receiver, constructor),
                             Object);
  bool is_instance_of = obj_is_instance_of->BooleanValue(isolate);

  // 2. If receiver does not have an [[Initialized...]] internal slot
  //    and ? InstanceofOperator(receiver, constructor) is true, then
  if (!has_initialized_slot && is_instance_of) {
    // 2. a. Let new_receiver be ? Get(receiver, %Intl%.[[FallbackSymbol]]).
    Handle<Object> new_receiver;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, new_receiver,
        JSReceiver::GetProperty(isolate, receiver,
                                isolate->factory()->intl_fallback_symbol()),
        Object);
    return new_receiver;
  }

  return receiver;
}

}  // namespace

MaybeHandle<JSObject> Intl::UnwrapReceiver(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           Handle<JSFunction> constructor,
                                           Intl::Type type,
                                           Handle<String> method_name,
                                           bool check_legacy_constructor) {
  DCHECK(type == Intl::Type::kCollator || type == Intl::Type::kNumberFormat ||
         type == Intl::Type::kDateTimeFormat ||
         type == Intl::Type::kBreakIterator);
  Handle<Object> new_receiver = receiver;
  if (check_legacy_constructor) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, new_receiver,
        LegacyUnwrapReceiver(isolate, receiver, constructor, type), JSObject);
  }

  // Collator has been ported to use regular instance types. We
  // shouldn't be using Intl::IsObjectOfType anymore.
  if (type == Intl::Type::kCollator) {
    if (!receiver->IsJSCollator()) {
      // 3. a. Throw a TypeError exception.
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                   method_name, receiver),
                      JSObject);
    }
    return Handle<JSCollator>::cast(receiver);
  }

  DCHECK_NE(type, Intl::Type::kCollator);
  // 3. If Type(new_receiver) is not Object or nf does not have an
  //    [[Initialized...]]  internal slot, then
  if (!Intl::IsObjectOfType(isolate, new_receiver, type)) {
    // 3. a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                 method_name, receiver),
                    JSObject);
  }

  // The above IsObjectOfType returns true only for JSObjects, which
  // makes this cast safe.
  return Handle<JSObject>::cast(new_receiver);
}

MaybeHandle<JSObject> NumberFormat::Unwrap(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           const char* method_name) {
  Handle<Context> native_context =
      Handle<Context>(isolate->context()->native_context(), isolate);
  Handle<JSFunction> constructor = Handle<JSFunction>(
      JSFunction::cast(native_context->intl_number_format_function()), isolate);
  Handle<String> method_name_str =
      isolate->factory()->NewStringFromAsciiChecked(method_name);

  return Intl::UnwrapReceiver(isolate, receiver, constructor,
                              Intl::Type::kNumberFormat, method_name_str, true);
}

MaybeHandle<String> NumberFormat::FormatNumber(
    Isolate* isolate, Handle<JSObject> number_format_holder, double value) {
  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(number_format_holder);
  CHECK_NOT_NULL(number_format);

  icu::UnicodeString result;
  number_format->format(value, result);

  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

void Intl::DefineWEProperty(Isolate* isolate, Handle<JSObject> target,
                            Handle<Name> key, Handle<Object> value) {
  PropertyDescriptor desc;
  desc.set_writable(true);
  desc.set_enumerable(true);
  desc.set_value(value);
  Maybe<bool> success =
      JSReceiver::DefineOwnProperty(isolate, target, key, &desc, kDontThrow);
  DCHECK(success.IsJust() && success.FromJust());
  USE(success);
}

namespace {

// Define general regexp macros.
// Note "(?:" means the regexp group a non-capture group.
#define REGEX_ALPHA "[a-z]"
#define REGEX_DIGIT "[0-9]"
#define REGEX_ALPHANUM "(?:" REGEX_ALPHA "|" REGEX_DIGIT ")"

void BuildLanguageTagRegexps(Isolate* isolate) {
// Define the language tag regexp macros.
// For info on BCP 47 see https://tools.ietf.org/html/bcp47 .
// Because language tags are case insensitive per BCP 47 2.1.1 and regexp's
// defined below will always be used after lowercasing the input, uppercase
// ranges in BCP 47 2.1 are dropped and grandfathered tags are all lowercased.
// clang-format off
#define BCP47_REGULAR                                          \
  "(?:art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|" \
  "zh-min|zh-min-nan|zh-xiang)"
#define BCP47_IRREGULAR                                  \
  "(?:en-gb-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|" \
  "i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|"  \
  "i-tsu|sgn-be-fr|sgn-be-nl|sgn-ch-de)"
#define BCP47_GRANDFATHERED "(?:" BCP47_IRREGULAR "|" BCP47_REGULAR ")"
#define BCP47_PRIVATE_USE "(?:x(?:-" REGEX_ALPHANUM "{1,8})+)"

#define BCP47_SINGLETON "(?:" REGEX_DIGIT "|" "[a-wy-z])"

#define BCP47_EXTENSION "(?:" BCP47_SINGLETON "(?:-" REGEX_ALPHANUM "{2,8})+)"
#define BCP47_VARIANT  \
  "(?:" REGEX_ALPHANUM "{5,8}" "|" "(?:" REGEX_DIGIT REGEX_ALPHANUM "{3}))"

#define BCP47_REGION "(?:" REGEX_ALPHA "{2}" "|" REGEX_DIGIT "{3})"
#define BCP47_SCRIPT "(?:" REGEX_ALPHA "{4})"
#define BCP47_EXT_LANG "(?:" REGEX_ALPHA "{3}(?:-" REGEX_ALPHA "{3}){0,2})"
#define BCP47_LANGUAGE "(?:" REGEX_ALPHA "{2,3}(?:-" BCP47_EXT_LANG ")?" \
  "|" REGEX_ALPHA "{4}" "|" REGEX_ALPHA "{5,8})"
#define BCP47_LANG_TAG         \
  BCP47_LANGUAGE               \
  "(?:-" BCP47_SCRIPT ")?"     \
  "(?:-" BCP47_REGION ")?"     \
  "(?:-" BCP47_VARIANT ")*"    \
  "(?:-" BCP47_EXTENSION ")*"  \
  "(?:-" BCP47_PRIVATE_USE ")?"
  // clang-format on

  constexpr char kLanguageTagSingletonRegexp[] = "^" BCP47_SINGLETON "$";
  constexpr char kLanguageTagVariantRegexp[] = "^" BCP47_VARIANT "$";
  constexpr char kLanguageTagRegexp[] =
      "^(?:" BCP47_LANG_TAG "|" BCP47_PRIVATE_USE "|" BCP47_GRANDFATHERED ")$";

  UErrorCode status = U_ZERO_ERROR;
  icu::RegexMatcher* language_singleton_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagSingletonRegexp, -1, US_INV), 0, status);
  icu::RegexMatcher* language_tag_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagRegexp, -1, US_INV), 0, status);
  icu::RegexMatcher* language_variant_regexp_matcher = new icu::RegexMatcher(
      icu::UnicodeString(kLanguageTagVariantRegexp, -1, US_INV), 0, status);
  CHECK(U_SUCCESS(status));

  isolate->set_language_tag_regexp_matchers(language_singleton_regexp_matcher,
                                            language_tag_regexp_matcher,
                                            language_variant_regexp_matcher);
// Undefine the language tag regexp macros.
#undef BCP47_EXTENSION
#undef BCP47_EXT_LANG
#undef BCP47_GRANDFATHERED
#undef BCP47_IRREGULAR
#undef BCP47_LANG_TAG
#undef BCP47_LANGUAGE
#undef BCP47_PRIVATE_USE
#undef BCP47_REGION
#undef BCP47_REGULAR
#undef BCP47_SCRIPT
#undef BCP47_SINGLETON
#undef BCP47_VARIANT
}

// Undefine the general regexp macros.
#undef REGEX_ALPHA
#undef REGEX_DIGIT
#undef REGEX_ALPHANUM

icu::RegexMatcher* GetLanguageSingletonRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_singleton_regexp_matcher =
      isolate->language_singleton_regexp_matcher();
  if (language_singleton_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_singleton_regexp_matcher =
        isolate->language_singleton_regexp_matcher();
  }
  return language_singleton_regexp_matcher;
}

icu::RegexMatcher* GetLanguageTagRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_tag_regexp_matcher =
      isolate->language_tag_regexp_matcher();
  if (language_tag_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_tag_regexp_matcher = isolate->language_tag_regexp_matcher();
  }
  return language_tag_regexp_matcher;
}

icu::RegexMatcher* GetLanguageVariantRegexMatcher(Isolate* isolate) {
  icu::RegexMatcher* language_variant_regexp_matcher =
      isolate->language_variant_regexp_matcher();
  if (language_variant_regexp_matcher == nullptr) {
    BuildLanguageTagRegexps(isolate);
    language_variant_regexp_matcher =
        isolate->language_variant_regexp_matcher();
  }
  return language_variant_regexp_matcher;
}

}  // anonymous namespace

MaybeHandle<JSObject> Intl::ResolveLocale(Isolate* isolate, const char* service,
                                          Handle<Object> requestedLocales,
                                          Handle<Object> options) {
  Handle<String> service_str =
      isolate->factory()->NewStringFromAsciiChecked(service);

  Handle<JSFunction> resolve_locale_function = isolate->resolve_locale();

  Handle<Object> result;
  Handle<Object> undefined_value = isolate->factory()->undefined_value();
  Handle<Object> args[] = {service_str, requestedLocales, options};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, resolve_locale_function, undefined_value,
                      arraysize(args), args),
      JSObject);

  return Handle<JSObject>::cast(result);
}

MaybeHandle<JSObject> Intl::CanonicalizeLocaleListJS(Isolate* isolate,
                                                     Handle<Object> locales) {
  Handle<JSFunction> canonicalize_locale_list_function =
      isolate->canonicalize_locale_list();

  Handle<Object> result;
  Handle<Object> undefined_value = isolate->factory()->undefined_value();
  Handle<Object> args[] = {locales};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, canonicalize_locale_list_function,
                      undefined_value, arraysize(args), args),
      JSObject);

  return Handle<JSObject>::cast(result);
}

Maybe<bool> Intl::GetStringOption(Isolate* isolate, Handle<JSReceiver> options,
                                  const char* property,
                                  std::vector<const char*> values,
                                  const char* service,
                                  std::unique_ptr<char[]>* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  if (value->IsUndefined(isolate)) {
    return Just(false);
  }

  // 2. c. Let value be ? ToString(value).
  Handle<String> value_str;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_str, Object::ToString(isolate, value), Nothing<bool>());
  std::unique_ptr<char[]> value_cstr = value_str->ToCString();

  // 2. d. if values is not undefined, then
  if (values.size() > 0) {
    // 2. d. i. If values does not contain an element equal to value,
    // throw a RangeError exception.
    for (size_t i = 0; i < values.size(); i++) {
      if (strcmp(values.at(i), value_cstr.get()) == 0) {
        // 2. e. return value
        *result = std::move(value_cstr);
        return Just(true);
      }
    }

    Handle<String> service_str =
        isolate->factory()->NewStringFromAsciiChecked(service);
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kValueOutOfRange, value, service_str,
                      property_str),
        Nothing<bool>());
  }

  // 2. e. return value
  *result = std::move(value_cstr);
  return Just(true);
}

V8_WARN_UNUSED_RESULT Maybe<bool> Intl::GetBoolOption(
    Isolate* isolate, Handle<JSReceiver> options, const char* property,
    const char* service, bool* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  // 2. If value is not undefined, then
  if (!value->IsUndefined(isolate)) {
    // 2. b. i. Let value be ToBoolean(value).
    *result = value->BooleanValue(isolate);

    // 2. e. return value
    return Just(true);
  }

  return Just(false);
}

namespace {

char AsciiToLower(char c) {
  if (c < 'A' || c > 'Z') {
    return c;
  }
  return c | (1 << 5);
}

/**
 * Check the structural Validity of the language tag per ECMA 402 6.2.2:
 *   - Well-formed per RFC 5646 2.1
 *   - There are no duplicate variant subtags
 *   - There are no duplicate singleton (extension) subtags
 *
 * One extra-check is done (from RFC 5646 2.2.9): the tag is compared
 * against the list of grandfathered tags. However, subtags for
 * primary/extended language, script, region, variant are not checked
 * against the IANA language subtag registry.
 *
 * ICU is too permissible and lets invalid tags, like
 * hant-cmn-cn, through.
 *
 * Returns false if the language tag is invalid.
 */
bool IsStructurallyValidLanguageTag(Isolate* isolate,
                                    const std::string& locale_in) {
  if (!String::IsAscii(locale_in.c_str(),
                       static_cast<int>(locale_in.length()))) {
    return false;
  }
  std::string locale(locale_in);
  icu::RegexMatcher* language_tag_regexp_matcher =
      GetLanguageTagRegexMatcher(isolate);

  // Check if it's well-formed, including grandfathered tags.
  icu::UnicodeString locale_uni(locale.c_str(), -1, US_INV);
  // Note: icu::RegexMatcher::reset does not make a copy of the input string
  // so cannot use a temp value; ie: cannot create it as a call parameter.
  language_tag_regexp_matcher->reset(locale_uni);
  UErrorCode status = U_ZERO_ERROR;
  bool is_valid_lang_tag = language_tag_regexp_matcher->matches(status);
  if (!is_valid_lang_tag || V8_UNLIKELY(U_FAILURE(status))) {
    return false;
  }

  // Just return if it's a x- form. It's all private.
  if (locale.find("x-") == 0) {
    return true;
  }

  // Check if there are any duplicate variants or singletons (extensions).

  // Remove private use section.
  locale = locale.substr(0, locale.find("-x-"));

  // Skip language since it can match variant regex, so we start from 1.
  // We are matching i-klingon here, but that's ok, since i-klingon-klingon
  // is not valid and would fail LANGUAGE_TAG_RE test.
  size_t pos = 0;
  std::vector<std::string> parts;
  while ((pos = locale.find("-")) != std::string::npos) {
    std::string token = locale.substr(0, pos);
    parts.push_back(token);
    locale = locale.substr(pos + 1);
  }
  if (locale.length() != 0) {
    parts.push_back(locale);
  }

  icu::RegexMatcher* language_variant_regexp_matcher =
      GetLanguageVariantRegexMatcher(isolate);

  icu::RegexMatcher* language_singleton_regexp_matcher =
      GetLanguageSingletonRegexMatcher(isolate);

  std::vector<std::string> variants;
  std::vector<std::string> extensions;
  for (auto it = parts.begin() + 1; it != parts.end(); it++) {
    icu::UnicodeString part(it->data(), -1, US_INV);
    language_variant_regexp_matcher->reset(part);
    bool is_language_variant = language_variant_regexp_matcher->matches(status);
    if (V8_UNLIKELY(U_FAILURE(status))) {
      return false;
    }
    if (is_language_variant && extensions.size() == 0) {
      if (std::find(variants.begin(), variants.end(), *it) == variants.end()) {
        variants.push_back(*it);
      } else {
        return false;
      }
    }

    language_singleton_regexp_matcher->reset(part);
    bool is_language_singleton =
        language_singleton_regexp_matcher->matches(status);
    if (V8_UNLIKELY(U_FAILURE(status))) {
      return false;
    }
    if (is_language_singleton) {
      if (std::find(extensions.begin(), extensions.end(), *it) ==
          extensions.end()) {
        extensions.push_back(*it);
      } else {
        return false;
      }
    }
  }

  return true;
}

bool IsLowerAscii(char c) { return c >= 'a' && c < 'z'; }

bool IsTwoLetterLanguage(const std::string& locale) {
  // Two letters, both in range 'a'-'z'...
  return locale.length() == 2 && IsLowerAscii(locale[0]) &&
         IsLowerAscii(locale[1]);
}

bool IsDeprecatedLanguage(const std::string& locale) {
  //  Check if locale is one of the deprecated language tags:
  return locale == "in" || locale == "iw" || locale == "ji" || locale == "jw";
}

// Reference:
// https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
bool IsGrandfatheredTagWithoutPreferredVaule(const std::string& locale) {
  if (V8_UNLIKELY(locale == "zh-min" || locale == "cel-gaulish")) return true;
  if (locale.length() > 6 /* i-mingo is 7 chars long */ &&
      V8_UNLIKELY(locale[0] == 'i' && locale[1] == '-')) {
    return locale.substr(2) == "default" || locale.substr(2) == "enochian" ||
           locale.substr(2) == "mingo";
  }
  return false;
}

}  // anonymous namespace

Maybe<std::string> Intl::CanonicalizeLanguageTag(Isolate* isolate,
                                                 Handle<Object> locale_in) {
  Handle<String> locale_str;
  // This does part of the validity checking spec'ed in CanonicalizeLocaleList:
  // 7c ii. If Type(kValue) is not String or Object, throw a TypeError
  // exception.
  // 7c iii. Let tag be ? ToString(kValue).
  // 7c iv. If IsStructurallyValidLanguageTag(tag) is false, throw a
  // RangeError exception.

  if (locale_in->IsString()) {
    locale_str = Handle<String>::cast(locale_in);
  } else if (locale_in->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, locale_str,
                                     Object::ToString(isolate, locale_in),
                                     Nothing<std::string>());
  } else {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NewTypeError(MessageTemplate::kLanguageID),
                                 Nothing<std::string>());
  }
  std::string locale(locale_str->ToCString().get());

  // Optimize for the most common case: a 2-letter language code in the
  // canonical form/lowercase that is not one of the deprecated codes
  // (in, iw, ji, jw). Don't check for ~70 of 3-letter deprecated language
  // codes. Instead, let them be handled by ICU in the slow path. However,
  // fast-track 'fil' (3-letter canonical code).
  if ((IsTwoLetterLanguage(locale) && !IsDeprecatedLanguage(locale)) ||
      locale == "fil") {
    return Just(locale);
  }

  // Because per BCP 47 2.1.1 language tags are case-insensitive, lowercase
  // the input before any more check.
  std::transform(locale.begin(), locale.end(), locale.begin(), AsciiToLower);
  if (!IsStructurallyValidLanguageTag(isolate, locale)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  // ICU maps a few grandfathered tags to what looks like a regular language
  // tag even though IANA language tag registry does not have a preferred
  // entry map for them. Return them as they're with lowercasing.
  if (IsGrandfatheredTagWithoutPreferredVaule(locale)) {
    return Just(locale);
  }

  // // ECMA 402 6.2.3
  // TODO(jshin): uloc_{for,to}TanguageTag can fail even for a structually valid
  // language tag if it's too long (much longer than 100 chars). Even if we
  // allocate a longer buffer, ICU will still fail if it's too long. Either
  // propose to Ecma 402 to put a limit on the locale length or change ICU to
  // handle long locale names better. See
  // https://unicode-org.atlassian.net/browse/ICU-13417
  UErrorCode error = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  uloc_forLanguageTag(locale.c_str(), icu_result, ULOC_FULLNAME_CAPACITY,
                      nullptr, &error);
  if (U_FAILURE(error) || error == U_STRING_NOT_TERMINATED_WARNING) {
    // TODO(jshin): This should not happen because the structural validity
    // is already checked. If that's the case, remove this.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  // Force strict BCP47 rules.
  char result[ULOC_FULLNAME_CAPACITY];
  int32_t result_len = uloc_toLanguageTag(icu_result, result,
                                          ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kInvalidLanguageTag, locale_str),
        Nothing<std::string>());
  }

  return Just(std::string(result, result_len));
}

Maybe<std::vector<std::string>> Intl::CanonicalizeLocaleList(
    Isolate* isolate, Handle<Object> locales, bool only_return_one_result) {
  // 1. If locales is undefined, then
  if (locales->IsUndefined(isolate)) {
    // 1a. Return a new empty List.
    return Just(std::vector<std::string>());
  }
  // 2. Let seen be a new empty List.
  std::vector<std::string> seen;
  // 3. If Type(locales) is String, then
  if (locales->IsString()) {
    // 3a. Let O be CreateArrayFromList(« locales »).
    // Instead of creating a one-element array and then iterating over it,
    // we inline the body of the iteration:
    std::string canonicalized_tag;
    if (!CanonicalizeLanguageTag(isolate, locales).To(&canonicalized_tag)) {
      return Nothing<std::vector<std::string>>();
    }
    seen.push_back(canonicalized_tag);
    return Just(seen);
  }
  // 4. Else,
  // 4a. Let O be ? ToObject(locales).
  Handle<JSReceiver> o;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, o,
                                   Object::ToObject(isolate, locales),
                                   Nothing<std::vector<std::string>>());
  // 5. Let len be ? ToLength(? Get(O, "length")).
  Handle<Object> length_obj;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, length_obj,
                                   Object::GetLengthFromArrayLike(isolate, o),
                                   Nothing<std::vector<std::string>>());
  // TODO(jkummerow): Spec violation: strictly speaking, we have to iterate
  // up to 2^53-1 if {length_obj} says so. Since cases above 2^32 probably
  // don't happen in practice (and would be very slow if they do), we'll keep
  // the code simple for now by using a saturating to-uint32 conversion.
  double raw_length = length_obj->Number();
  uint32_t len =
      raw_length >= kMaxUInt32 ? kMaxUInt32 : static_cast<uint32_t>(raw_length);
  // 6. Let k be 0.
  // 7. Repeat, while k < len
  for (uint32_t k = 0; k < len; k++) {
    // 7a. Let Pk be ToString(k).
    // 7b. Let kPresent be ? HasProperty(O, Pk).
    LookupIterator it(isolate, o, k);
    // 7c. If kPresent is true, then
    if (!it.IsFound()) continue;
    // 7c i. Let kValue be ? Get(O, Pk).
    Handle<Object> k_value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, k_value, Object::GetProperty(&it),
                                     Nothing<std::vector<std::string>>());
    // 7c ii. If Type(kValue) is not String or Object, throw a TypeError
    // exception.
    // 7c iii. Let tag be ? ToString(kValue).
    // 7c iv. If IsStructurallyValidLanguageTag(tag) is false, throw a
    // RangeError exception.
    // 7c v. Let canonicalizedTag be CanonicalizeLanguageTag(tag).
    std::string canonicalized_tag;
    if (!CanonicalizeLanguageTag(isolate, k_value).To(&canonicalized_tag)) {
      return Nothing<std::vector<std::string>>();
    }
    // 7c vi. If canonicalizedTag is not an element of seen, append
    // canonicalizedTag as the last element of seen.
    if (std::find(seen.begin(), seen.end(), canonicalized_tag) == seen.end()) {
      seen.push_back(canonicalized_tag);
    }
    // 7d. Increase k by 1. (See loop header.)
    // Optimization: some callers only need one result.
    if (only_return_one_result) return Just(seen);
  }
  // 8. Return seen.
  return Just(seen);
}

// ecma-402/#sec-currencydigits
Handle<Smi> Intl::CurrencyDigits(Isolate* isolate, Handle<String> currency) {
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::String::Value currency_string(v8_isolate, v8::Utils::ToLocal(currency));
  CHECK_NOT_NULL(*currency_string);

  DisallowHeapAllocation no_gc;
  UErrorCode status = U_ZERO_ERROR;
  uint32_t fraction_digits = ucurr_getDefaultFractionDigits(
      reinterpret_cast<const UChar*>(*currency_string), &status);
  // For missing currency codes, default to the most common, 2
  if (U_FAILURE(status)) fraction_digits = 2;
  return Handle<Smi>(Smi::FromInt(fraction_digits), isolate);
}

MaybeHandle<JSObject> Intl::CreateNumberFormat(Isolate* isolate,
                                               Handle<String> locale,
                                               Handle<JSObject> options,
                                               Handle<JSObject> resolved) {
  Handle<JSFunction> constructor(
      isolate->native_context()->intl_number_format_function(), isolate);

  Handle<JSObject> local_object;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, local_object,
                             JSObject::New(constructor, constructor), JSObject);

  // Set number formatter as embedder field of the resulting JS object.
  icu::DecimalFormat* number_format =
      NumberFormat::InitializeNumberFormat(isolate, locale, options, resolved);

  CHECK_NOT_NULL(number_format);

  local_object->SetEmbedderField(NumberFormat::kDecimalFormatIndex,
                                 reinterpret_cast<Smi*>(number_format));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(), wrapper.location(),
                          NumberFormat::DeleteNumberFormat,
                          WeakCallbackType::kInternalFields);
  return local_object;
}

/**
 * Parses Unicode extension into key - value map.
 * Returns empty object if the extension string is invalid.
 * We are not concerned with the validity of the values at this point.
 * 'attribute' in RFC 6047 is not supported. Keys without explicit
 * values are assigned UNDEFINED.
 * TODO(jshin): Fix the handling of 'attribute' (in RFC 6047, but none
 * has been defined so that it's not used) and boolean keys without
 * an explicit value.
 */
void Intl::ParseExtension(Isolate* isolate, const std::string& extension,
                          std::map<std::string, std::string>& out) {
  if (extension.compare(0, 3, "-u-") != 0) return;

  // Key is {2}alphanum, value is {3,8}alphanum.
  // Some keys may not have explicit values (booleans).
  std::string key;
  std::string value;
  // Skip the "-u-".
  size_t start = 3;
  size_t end;
  do {
    end = extension.find("-", start);
    size_t length =
        (end == std::string::npos) ? extension.length() - start : end - start;
    std::string element = extension.substr(start, length);
    // Key is {2}alphanum
    if (length == 2) {
      if (!key.empty()) {
        out.insert(std::pair<std::string, std::string>(key, value));
        value.clear();
      }
      key = element;
      // value is {3,8}alphanum.
    } else if (length >= 3 && length <= 8 && !key.empty()) {
      value = value.empty() ? element : (value + "-" + element);
    } else {
      return;
    }
    start = end + 1;
  } while (end != std::string::npos);
  if (!key.empty()) out.insert(std::pair<std::string, std::string>(key, value));
}

namespace {

bool IsAToZ(char ch) {
  return IsInRange(AsciiAlphaToLower(ch), 'a', 'z');
}

}  // namespace

// Verifies that the input is a well-formed ISO 4217 currency code.
// ecma402/#sec-currency-codes
bool Intl::IsWellFormedCurrencyCode(Isolate* isolate, Handle<String> currency) {
  // 2. If the number of elements in normalized is not 3, return false.
  if (currency->length() != 3) return false;

  currency = String::Flatten(isolate, currency);
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent flat = currency->GetFlatContent();

    // 1. Let normalized be the result of mapping currency to upper case as
    // described in 6.1. 3. If normalized contains any character that is not in
    // the range "A" to "Z" (U+0041 to U+005A), return false. 4. Return true.
    // Don't uppercase to test. It could convert invalid code into a valid one.
    // For example \u00DFP (Eszett+P) becomes SSP.
    return (IsAToZ(flat.Get(0)) && IsAToZ(flat.Get(1)) && IsAToZ(flat.Get(2)));
  }
}

// ecma402 #sup-string.prototype.tolocalelowercase
// ecma402 #sup-string.prototype.tolocaleuppercase
MaybeHandle<String> Intl::StringLocaleConvertCase(Isolate* isolate,
                                                  Handle<String> s,
                                                  bool to_upper,
                                                  Handle<Object> locales) {
  std::vector<std::string> requested_locales;
  if (!CanonicalizeLocaleList(isolate, locales, true).To(&requested_locales)) {
    return MaybeHandle<String>();
  }
  std::string requested_locale = requested_locales.size() == 0
                                     ? Intl::DefaultLocale(isolate)
                                     : requested_locales[0];
  size_t dash = requested_locale.find("-");
  if (dash != std::string::npos) {
    requested_locale = requested_locale.substr(0, dash);
  }

  // Primary language tag can be up to 8 characters long in theory.
  // https://tools.ietf.org/html/bcp47#section-2.2.1
  DCHECK_LE(requested_locale.length(), 8);
  s = String::Flatten(isolate, s);

  // All the languages requiring special-handling have two-letter codes.
  // Note that we have to check for '!= 2' here because private-use language
  // tags (x-foo) or grandfathered irregular tags (e.g. i-enochian) would have
  // only 'x' or 'i' when they get here.
  if (V8_UNLIKELY(requested_locale.length() != 2)) {
    return ConvertCase(s, to_upper, isolate);
  }
  // TODO(jshin): Consider adding a fast path for ASCII or Latin-1. The fastpath
  // in the root locale needs to be adjusted for az, lt and tr because even case
  // mapping of ASCII range characters are different in those locales.
  // Greek (el) does not require any adjustment.
  if (V8_UNLIKELY((requested_locale == "tr") || (requested_locale == "el") ||
                  (requested_locale == "lt") || (requested_locale == "az"))) {
    return LocaleConvertCase(s, isolate, to_upper, requested_locale.c_str());
  } else {
    return ConvertCase(s, to_upper, isolate);
  }
}

MaybeHandle<Object> Intl::StringLocaleCompare(Isolate* isolate,
                                              Handle<String> string1,
                                              Handle<String> string2,
                                              Handle<Object> locales,
                                              Handle<Object> options) {
  Factory* factory = isolate->factory();
  Handle<JSObject> collator;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, collator,
      CachedOrNewService(isolate, factory->NewStringFromStaticChars("collator"),
                         locales, options, factory->undefined_value()),
      Object);
  CHECK(collator->IsJSCollator());
  return Intl::CompareStrings(isolate, Handle<JSCollator>::cast(collator),
                              string1, string2);
}

// ecma402/#sec-collator-comparestrings
Handle<Object> Intl::CompareStrings(Isolate* isolate,
                                    Handle<JSCollator> collator,
                                    Handle<String> string1,
                                    Handle<String> string2) {
  Factory* factory = isolate->factory();
  icu::Collator* icu_collator = collator->icu_collator()->raw();
  CHECK_NOT_NULL(icu_collator);

  string1 = String::Flatten(isolate, string1);
  string2 = String::Flatten(isolate, string2);

  UCollationResult result;
  UErrorCode status = U_ZERO_ERROR;
  {
    DisallowHeapAllocation no_gc;
    int32_t length1 = string1->length();
    int32_t length2 = string2->length();
    String::FlatContent flat1 = string1->GetFlatContent();
    String::FlatContent flat2 = string2->GetFlatContent();
    std::unique_ptr<uc16[]> sap1;
    std::unique_ptr<uc16[]> sap2;
    icu::UnicodeString string_val1(
        FALSE, GetUCharBufferFromFlat(flat1, &sap1, length1), length1);
    icu::UnicodeString string_val2(
        FALSE, GetUCharBufferFromFlat(flat2, &sap2, length2), length2);
    result = icu_collator->compare(string_val1, string_val2, status);
  }
  DCHECK(U_SUCCESS(status));

  return factory->NewNumberFromInt(result);
}

// ecma402/#sup-properties-of-the-number-prototype-object
MaybeHandle<String> Intl::NumberToLocaleString(Isolate* isolate,
                                               Handle<Object> num,
                                               Handle<Object> locales,
                                               Handle<Object> options) {
  Factory* factory = isolate->factory();
  Handle<JSObject> number_format_holder;
  // 2. Let numberFormat be ? Construct(%NumberFormat%, « locales, options »).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, number_format_holder,
      CachedOrNewService(isolate,
                         factory->NewStringFromStaticChars("numberformat"),
                         locales, options, factory->undefined_value()),
      String);
  DCHECK(
      Intl::IsObjectOfType(isolate, number_format_holder, Intl::kNumberFormat));
  Handle<Object> number_obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number_obj,
                             Object::ToNumber(isolate, num), String);

  // Spec treats -0 and +0 as 0.
  double number = number_obj->Number() + 0;
  // Return FormatNumber(numberFormat, x).
  return NumberFormat::FormatNumber(isolate, number_format_holder, number);
}

// ecma402/#sec-defaultnumberoption
Maybe<int> Intl::DefaultNumberOption(Isolate* isolate, Handle<Object> value,
                                     int min, int max, int fallback,
                                     Handle<String> property) {
  // 2. Else, return fallback.
  if (value->IsUndefined()) return Just(fallback);

  // 1. If value is not undefined, then
  // a. Let value be ? ToNumber(value).
  Handle<Object> value_num;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_num, Object::ToNumber(isolate, value), Nothing<int>());
  DCHECK(value_num->IsNumber());

  // b. If value is NaN or less than minimum or greater than maximum, throw a
  // RangeError exception.
  if (value_num->IsNaN() || value_num->Number() < min ||
      value_num->Number() > max) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property),
        Nothing<int>());
  }

  // The max and min arguments are integers and the above check makes
  // sure that we are within the integer range making this double to
  // int conversion safe.
  //
  // c. Return floor(value).
  return Just(FastD2I(floor(value_num->Number())));
}

// ecma402/#sec-getnumberoption
Maybe<int> Intl::GetNumberOption(Isolate* isolate, Handle<JSReceiver> options,
                                 Handle<String> property, int min, int max,
                                 int fallback) {
  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, JSReceiver::GetProperty(isolate, options, property),
      Nothing<int>());

  // Return ? DefaultNumberOption(value, minimum, maximum, fallback).
  return DefaultNumberOption(isolate, value, min, max, fallback, property);
}

Maybe<int> Intl::GetNumberOption(Isolate* isolate, Handle<JSReceiver> options,
                                 const char* property, int min, int max,
                                 int fallback) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);
  return GetNumberOption(isolate, options, property_str, min, max, fallback);
}

Maybe<bool> Intl::SetNumberFormatDigitOptions(Isolate* isolate,
                                              icu::DecimalFormat* number_format,
                                              Handle<JSReceiver> options,
                                              int mnfd_default,
                                              int mxfd_default) {
  CHECK_NOT_NULL(number_format);

  // 5. Let mnid be ? GetNumberOption(options, "minimumIntegerDigits,", 1, 21,
  // 1).
  int mnid;
  if (!GetNumberOption(isolate, options, "minimumIntegerDigits", 1, 21, 1)
           .To(&mnid)) {
    return Nothing<bool>();
  }

  // 6. Let mnfd be ? GetNumberOption(options, "minimumFractionDigits", 0, 20,
  // mnfdDefault).
  int mnfd;
  if (!GetNumberOption(isolate, options, "minimumFractionDigits", 0, 20,
                       mnfd_default)
           .To(&mnfd)) {
    return Nothing<bool>();
  }

  // 7. Let mxfdActualDefault be max( mnfd, mxfdDefault ).
  int mxfd_actual_default = std::max(mnfd, mxfd_default);

  // 8. Let mxfd be ? GetNumberOption(options,
  // "maximumFractionDigits", mnfd, 20, mxfdActualDefault).
  int mxfd;
  if (!GetNumberOption(isolate, options, "maximumFractionDigits", mnfd, 20,
                       mxfd_actual_default)
           .To(&mxfd)) {
    return Nothing<bool>();
  }

  // 9.  Let mnsd be ? Get(options, "minimumSignificantDigits").
  Handle<Object> mnsd_obj;
  Handle<String> mnsd_str =
      isolate->factory()->NewStringFromStaticChars("minimumSignificantDigits");
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mnsd_obj, JSReceiver::GetProperty(isolate, options, mnsd_str),
      Nothing<bool>());

  // 10. Let mxsd be ? Get(options, "maximumSignificantDigits").
  Handle<Object> mxsd_obj;
  Handle<String> mxsd_str =
      isolate->factory()->NewStringFromStaticChars("maximumSignificantDigits");
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, mxsd_obj, JSReceiver::GetProperty(isolate, options, mxsd_str),
      Nothing<bool>());

  // 11. Set intlObj.[[MinimumIntegerDigits]] to mnid.
  number_format->setMinimumIntegerDigits(mnid);

  // 12. Set intlObj.[[MinimumFractionDigits]] to mnfd.
  number_format->setMinimumFractionDigits(mnfd);

  // 13. Set intlObj.[[MaximumFractionDigits]] to mxfd.
  number_format->setMaximumFractionDigits(mxfd);

  bool significant_digits_used = false;
  // 14. If mnsd is not undefined or mxsd is not undefined, then
  if (!mnsd_obj->IsUndefined(isolate) || !mxsd_obj->IsUndefined(isolate)) {
    // 14. a. Let mnsd be ? DefaultNumberOption(mnsd, 1, 21, 1).
    int mnsd;
    if (!DefaultNumberOption(isolate, mnsd_obj, 1, 21, 1, mnsd_str).To(&mnsd)) {
      return Nothing<bool>();
    }

    // 14. b. Let mxsd be ? DefaultNumberOption(mxsd, mnsd, 21, 21).
    int mxsd;
    if (!DefaultNumberOption(isolate, mxsd_obj, mnsd, 21, 21, mxsd_str)
             .To(&mxsd)) {
      return Nothing<bool>();
    }

    significant_digits_used = true;

    // 14. c. Set intlObj.[[MinimumSignificantDigits]] to mnsd.
    number_format->setMinimumSignificantDigits(mnsd);

    // 14. d. Set intlObj.[[MaximumSignificantDigits]] to mxsd.
    number_format->setMaximumSignificantDigits(mxsd);
  }

  number_format->setSignificantDigitsUsed(significant_digits_used);
  number_format->setRoundingMode(icu::DecimalFormat::kRoundHalfUp);
  return Just(true);
}

namespace {

// ECMA 402 9.2.2 BestAvailableLocale(availableLocales, locale)
// https://tc39.github.io/ecma402/#sec-bestavailablelocale
std::string BestAvailableLocale(std::set<std::string> available_locales,
                                std::string locale) {
  const char separator = '-';

  // 1. Let candidate be locale.
  // 2. Repeat,
  do {
    // 2.a. If availableLocales contains an element equal to candidate, return
    //      candidate.
    if (available_locales.find(locale) != available_locales.end()) {
      return locale;
    }
    // 2.b. Let pos be the character index of the last occurrence of "-"
    //      (U+002D) within candidate. If that character does not occur, return
    //      undefined.
    size_t pos = locale.rfind(separator);
    if (pos == std::string::npos) {
      return "";
    }
    // 2.c. If pos ≥ 2 and the character "-" occurs at index pos-2 of candidate,
    //      decrease pos by 2.
    if (pos >= 2 && locale[pos - 2] == separator) {
      pos -= 2;
    }
    // 2.d. Let candidate be the substring of candidate from position 0,
    //      inclusive, to position pos, exclusive.
    locale = locale.substr(0, pos);
  } while (true);
}

#define ANY_EXTENSION_REGEXP "-[a-z0-9]{1}-.*"

std::unique_ptr<icu::RegexMatcher> GetAnyExtensionRegexpMatcher() {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::RegexMatcher> matcher(new icu::RegexMatcher(
      icu::UnicodeString(ANY_EXTENSION_REGEXP, -1, US_INV), 0, status));
  DCHECK(U_SUCCESS(status));
  return matcher;
}

#undef ANY_EXTENSION_REGEXP

// ECMA 402 9.2.7 LookupSupportedLocales(availableLocales, requestedLocales)
// https://tc39.github.io/ecma402/#sec-lookupsupportedlocales
std::vector<std::string> LookupSupportedLocales(
    std::set<std::string> available_locales,
    std::vector<std::string> requested_locales) {
  std::unique_ptr<icu::RegexMatcher> matcher = GetAnyExtensionRegexpMatcher();

  // 1. Let subset be a new empty List.
  std::vector<std::string> subset;

  // 2. For each element locale of requestedLocales in List order, do
  for (auto locale : requested_locales) {
    // 2.a. Let noExtensionsLocale be the String value that is locale with all
    //      Unicode locale extension sequences removed.
    icu::UnicodeString locale_uni(locale.c_str(), -1, US_INV);
    // TODO(bstell): look at using uloc_forLanguageTag to convert the language
    // tag to locale id
    // TODO(bstell): look at using uloc_getBaseName to just get the name without
    // all the keywords
    matcher->reset(locale_uni);
    UErrorCode status = U_ZERO_ERROR;
    // TODO(bstell): need to determine if this is the correct behavior.
    // This matches the JS implementation but might not match the spec.
    // According to
    // https://tc39.github.io/ecma402/#sec-unicode-locale-extension-sequences:
    //
    //     This standard uses the term "Unicode locale extension sequence" for
    //     any substring of a language tag that is not part of a private use
    //     subtag sequence, starts with a separator  "-" and the singleton "u",
    //     and includes the maximum sequence of following non-singleton subtags
    //     and their preceding "-" separators.
    //
    // According to the spec a locale "en-t-aaa-u-bbb-v-ccc-x-u-ddd", should
    // remove only the "-u-bbb" part, and keep everything else, whereas this
    // regexp matcher would leave only the "en".
    icu::UnicodeString no_extensions_locale_uni =
        matcher->replaceAll("", status);
    DCHECK(U_SUCCESS(status));
    std::string no_extensions_locale;
    no_extensions_locale_uni.toUTF8String(no_extensions_locale);
    // 2.b. Let availableLocale be BestAvailableLocale(availableLocales,
    //      noExtensionsLocale).
    std::string available_locale =
        BestAvailableLocale(available_locales, no_extensions_locale);
    // 2.c. If availableLocale is not undefined, append locale to the end of
    //      subset.
    if (!available_locale.empty()) {
      subset.push_back(locale);
    }
  }

  // 3. Return subset.
  return subset;
}

// ECMA 402 9.2.8 BestFitSupportedLocales(availableLocales, requestedLocales)
// https://tc39.github.io/ecma402/#sec-bestfitsupportedlocales
std::vector<std::string> BestFitSupportedLocales(
    std::set<std::string> available_locales,
    std::vector<std::string> requested_locales) {
  return LookupSupportedLocales(available_locales, requested_locales);
}

enum MatcherOption { kBestFit, kLookup };

// TODO(bstell): should this be moved somewhere where it is reusable?
// Implement steps 5, 6, 7 for ECMA 402 9.2.9 SupportedLocales
// https://tc39.github.io/ecma402/#sec-supportedlocales
MaybeHandle<JSObject> CreateReadOnlyArray(Isolate* isolate,
                                          std::vector<std::string> elements) {
  Factory* factory = isolate->factory();
  if (elements.size() >= kMaxUInt32) {
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayLength), JSObject);
  }

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE);

  // 5. Let subset be CreateArrayFromList(elements).
  // 6. Let keys be subset.[[OwnPropertyKeys]]().
  Handle<JSArray> subset = factory->NewJSArray(0);

  // 7. For each element P of keys in List order, do
  uint32_t length = static_cast<uint32_t>(elements.size());
  for (uint32_t i = 0; i < length; i++) {
    const std::string& part = elements[i];
    Handle<String> value =
        factory->NewStringFromUtf8(CStrVector(part.c_str())).ToHandleChecked();
    JSObject::AddDataElement(subset, i, value, attr);
  }

  // 7.a. Let desc be PropertyDescriptor { [[Configurable]]: false,
  //          [[Writable]]: false }.
  PropertyDescriptor desc;
  desc.set_writable(false);
  desc.set_configurable(false);

  // 7.b. Perform ! DefinePropertyOrThrow(subset, P, desc).
  JSArray::ArraySetLength(isolate, subset, &desc, kThrowOnError).ToChecked();
  return subset;
}

// ECMA 402 9.2.9 SupportedLocales(availableLocales, requestedLocales, options)
// https://tc39.github.io/ecma402/#sec-supportedlocales
MaybeHandle<JSObject> SupportedLocales(
    Isolate* isolate, std::string service,
    std::set<std::string> available_locales,
    std::vector<std::string> requested_locales, Handle<Object> options) {
  std::vector<std::string> supported_locales;

  // 1. If options is not undefined, then
  //    a. Let options be ? ToObject(options).
  //    b. Let matcher be ? GetOption(options, "localeMatcher", "string",
  //       « "lookup", "best fit" », "best fit").
  // 2. Else, let matcher be "best fit".
  MatcherOption matcher = kBestFit;
  if (!options->IsUndefined(isolate)) {
    Handle<JSReceiver> options_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_obj,
                               Object::ToObject(isolate, options), JSObject);
    std::unique_ptr<char[]> matcher_str = nullptr;
    std::vector<const char*> matcher_values = {"lookup", "best fit"};
    Maybe<bool> maybe_found_matcher =
        Intl::GetStringOption(isolate, options_obj, "localeMatcher",
                              matcher_values, service.c_str(), &matcher_str);
    MAYBE_RETURN(maybe_found_matcher, MaybeHandle<JSObject>());
    if (maybe_found_matcher.FromJust()) {
      DCHECK_NOT_NULL(matcher_str.get());
      if (strcmp(matcher_str.get(), "lookup") == 0) {
        matcher = kLookup;
      }
    }
  }

  // 3. If matcher is "best fit", then
  //    a. Let supportedLocales be BestFitSupportedLocales(availableLocales,
  //       requestedLocales).
  if (matcher == kBestFit) {
    supported_locales =
        BestFitSupportedLocales(available_locales, requested_locales);
  } else {
    // 4. Else,
    //    a. Let supportedLocales be LookupSupportedLocales(availableLocales,
    //       requestedLocales).
    DCHECK_EQ(matcher, kLookup);
    supported_locales =
        LookupSupportedLocales(available_locales, requested_locales);
  }

  // TODO(jkummerow): Possibly revisit why the spec has the individual entries
  // readonly but the array is not frozen.
  // https://github.com/tc39/ecma402/issues/258

  // 5. Let subset be CreateArrayFromList(supportedLocales).
  // 6. Let keys be subset.[[OwnPropertyKeys]]().
  // 7. For each element P of keys in List order, do
  //    a. Let desc be PropertyDescriptor { [[Configurable]]: false,
  //       [[Writable]]: false }.
  //    b. Perform ! DefinePropertyOrThrow(subset, P, desc).
  MaybeHandle<JSObject> subset =
      CreateReadOnlyArray(isolate, supported_locales);

  // 8. Return subset.
  return subset;
}
}  // namespace

// ECMA 402 10.2.2 Intl.Collator.supportedLocalesOf
// https://tc39.github.io/ecma402/#sec-intl.collator.supportedlocalesof
// of Intl::SupportedLocalesOf thru JS
MaybeHandle<JSObject> Intl::SupportedLocalesOf(Isolate* isolate,
                                               Handle<String> service,
                                               Handle<Object> locales_in,
                                               Handle<Object> options_in) {
  // Let availableLocales be %Collator%.[[AvailableLocales]].
  IcuService icu_service = Intl::StringToIcuService(service);
  std::set<std::string> available_locales = GetAvailableLocales(icu_service);
  std::vector<std::string> requested_locales;
  // Let requestedLocales be ? CanonicalizeLocaleList(locales).
  bool got_requested_locales =
      CanonicalizeLocaleList(isolate, locales_in, false).To(&requested_locales);
  if (!got_requested_locales) {
    return MaybeHandle<JSObject>();
  }

  // Return ? SupportedLocales(availableLocales, requestedLocales, options).
  std::string service_str(service->ToCString().get());
  return SupportedLocales(isolate, service_str, available_locales,
                          requested_locales, options_in);
}

}  // namespace internal
}  // namespace v8

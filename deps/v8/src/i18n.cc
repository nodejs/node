// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// limitations under the License.

#include "i18n.h"

#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
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
#include "unicode/uversion.h"

namespace v8 {
namespace internal {

namespace {

bool ExtractStringSetting(Isolate* isolate,
                          Handle<JSObject> options,
                          const char* key,
                          icu::UnicodeString* setting) {
  Handle<String> str = isolate->factory()->NewStringFromAscii(CStrVector(key));
  MaybeObject* maybe_object = options->GetProperty(*str);
  Object* object;
  if (maybe_object->ToObject(&object) && object->IsString()) {
    v8::String::Utf8Value utf8_string(
        v8::Utils::ToLocal(Handle<String>(String::cast(object))));
    *setting = icu::UnicodeString::fromUTF8(*utf8_string);
    return true;
  }
  return false;
}


bool ExtractIntegerSetting(Isolate* isolate,
                           Handle<JSObject> options,
                           const char* key,
                           int32_t* value) {
  Handle<String> str = isolate->factory()->NewStringFromAscii(CStrVector(key));
  MaybeObject* maybe_object = options->GetProperty(*str);
  Object* object;
  if (maybe_object->ToObject(&object) && object->IsNumber()) {
    object->ToInt32(value);
    return true;
  }
  return false;
}


bool ExtractBooleanSetting(Isolate* isolate,
                           Handle<JSObject> options,
                           const char* key,
                           bool* value) {
  Handle<String> str = isolate->factory()->NewStringFromAscii(CStrVector(key));
  MaybeObject* maybe_object = options->GetProperty(*str);
  Object* object;
  if (maybe_object->ToObject(&object) && object->IsBoolean()) {
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

  // Make formatter from skeleton. Calendar and numbering system are added
  // to the locale as Unicode extension (if they were specified at all).
  icu::SimpleDateFormat* date_format = NULL;
  icu::UnicodeString skeleton;
  if (ExtractStringSetting(isolate, options, "skeleton", &skeleton)) {
    icu::DateTimePatternGenerator* generator =
        icu::DateTimePatternGenerator::createInstance(icu_locale, status);
    icu::UnicodeString pattern;
    if (U_SUCCESS(status)) {
      pattern = generator->getBestPattern(skeleton, status);
      delete generator;
    }

    date_format = new icu::SimpleDateFormat(pattern, icu_locale, status);
    if (U_SUCCESS(status)) {
      date_format->adoptCalendar(calendar);
    }
  }

  if (U_FAILURE(status)) {
    delete calendar;
    delete date_format;
    date_format = NULL;
  }

  return date_format;
}


void SetResolvedDateSettings(Isolate* isolate,
                             const icu::Locale& icu_locale,
                             icu::SimpleDateFormat* date_format,
                             Handle<JSObject> resolved) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString pattern;
  date_format->toPattern(pattern);
  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("pattern")),
      isolate->factory()->NewStringFromTwoByte(
        Vector<const uint16_t>(
            reinterpret_cast<const uint16_t*>(pattern.getBuffer()),
            pattern.length())),
      NONE,
      SLOPPY);

  // Set time zone and calendar.
  const icu::Calendar* calendar = date_format->getCalendar();
  const char* calendar_name = calendar->getType();
  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("calendar")),
      isolate->factory()->NewStringFromAscii(CStrVector(calendar_name)),
      NONE,
      SLOPPY);

  const icu::TimeZone& tz = calendar->getTimeZone();
  icu::UnicodeString time_zone;
  tz.getID(time_zone);

  icu::UnicodeString canonical_time_zone;
  icu::TimeZone::getCanonicalID(time_zone, canonical_time_zone, status);
  if (U_SUCCESS(status)) {
    if (canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/GMT")) {
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("timeZone")),
          isolate->factory()->NewStringFromAscii(CStrVector("UTC")),
          NONE,
          SLOPPY);
    } else {
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("timeZone")),
          isolate->factory()->NewStringFromTwoByte(
            Vector<const uint16_t>(
                reinterpret_cast<const uint16_t*>(
                    canonical_time_zone.getBuffer()),
                canonical_time_zone.length())),
          NONE,
          SLOPPY);
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
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("numberingSystem")),
        isolate->factory()->NewStringFromAscii(CStrVector(ns)),
        NONE,
        SLOPPY);
  } else {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("numberingSystem")),
        isolate->factory()->undefined_value(),
        NONE,
        SLOPPY);
  }
  delete numbering_system;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector(result)),
        NONE,
        SLOPPY);
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector("und")),
        NONE,
        SLOPPY);
  }
}


template<int internal_fields, EternalHandles::SingletonHandle field>
Handle<ObjectTemplateInfo> GetEternal(Isolate* isolate) {
  if (isolate->eternal_handles()->Exists(field)) {
    return Handle<ObjectTemplateInfo>::cast(
        isolate->eternal_handles()->GetSingleton(field));
  }
  v8::Local<v8::ObjectTemplate> raw_template =
      v8::ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(isolate));
  raw_template->SetInternalFieldCount(internal_fields);
  return Handle<ObjectTemplateInfo>::cast(
      isolate->eternal_handles()->CreateSingleton(
        isolate,
        *v8::Utils::OpenHandle(*raw_template),
        field));
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
          icu::NumberFormat::createInstance(icu_locale, format_style,  status));
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
  icu::UnicodeString pattern;
  number_format->toPattern(pattern);
  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("pattern")),
      isolate->factory()->NewStringFromTwoByte(
        Vector<const uint16_t>(
            reinterpret_cast<const uint16_t*>(pattern.getBuffer()),
            pattern.length())),
      NONE,
      SLOPPY);

  // Set resolved currency code in options.currency if not empty.
  icu::UnicodeString currency(number_format->getCurrency());
  if (!currency.isEmpty()) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("currency")),
        isolate->factory()->NewStringFromTwoByte(
          Vector<const uint16_t>(
              reinterpret_cast<const uint16_t*>(currency.getBuffer()),
              currency.length())),
        NONE,
        SLOPPY);
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
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("numberingSystem")),
        isolate->factory()->NewStringFromAscii(CStrVector(ns)),
        NONE,
        SLOPPY);
  } else {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("numberingSystem")),
        isolate->factory()->undefined_value(),
        NONE,
        SLOPPY);
  }
  delete numbering_system;

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("useGrouping")),
      isolate->factory()->ToBoolean(number_format->isGroupingUsed()),
      NONE,
      SLOPPY);

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(
          CStrVector("minimumIntegerDigits")),
      isolate->factory()->NewNumberFromInt(
          number_format->getMinimumIntegerDigits()),
      NONE,
      SLOPPY);

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(
          CStrVector("minimumFractionDigits")),
      isolate->factory()->NewNumberFromInt(
          number_format->getMinimumFractionDigits()),
      NONE,
      SLOPPY);

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(
          CStrVector("maximumFractionDigits")),
      isolate->factory()->NewNumberFromInt(
          number_format->getMaximumFractionDigits()),
      NONE,
      SLOPPY);

  Handle<String> key = isolate->factory()->NewStringFromAscii(
      CStrVector("minimumSignificantDigits"));
  if (JSReceiver::HasLocalProperty(resolved, key)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(
            CStrVector("minimumSignificantDigits")),
        isolate->factory()->NewNumberFromInt(
            number_format->getMinimumSignificantDigits()),
        NONE,
        SLOPPY);
  }

  key = isolate->factory()->NewStringFromAscii(
      CStrVector("maximumSignificantDigits"));
  if (JSReceiver::HasLocalProperty(resolved, key)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(
            CStrVector("maximumSignificantDigits")),
        isolate->factory()->NewNumberFromInt(
            number_format->getMaximumSignificantDigits()),
        NONE,
        SLOPPY);
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector(result)),
        NONE,
        SLOPPY);
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector("und")),
        NONE,
        SLOPPY);
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
  UErrorCode status = U_ZERO_ERROR;

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("numeric")),
      isolate->factory()->ToBoolean(
          collator->getAttribute(UCOL_NUMERIC_COLLATION, status) == UCOL_ON),
      NONE,
      SLOPPY);

  switch (collator->getAttribute(UCOL_CASE_FIRST, status)) {
    case UCOL_LOWER_FIRST:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("caseFirst")),
          isolate->factory()->NewStringFromAscii(CStrVector("lower")),
          NONE,
          SLOPPY);
      break;
    case UCOL_UPPER_FIRST:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("caseFirst")),
          isolate->factory()->NewStringFromAscii(CStrVector("upper")),
          NONE,
          SLOPPY);
      break;
    default:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("caseFirst")),
          isolate->factory()->NewStringFromAscii(CStrVector("false")),
          NONE,
          SLOPPY);
  }

  switch (collator->getAttribute(UCOL_STRENGTH, status)) {
    case UCOL_PRIMARY: {
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("strength")),
          isolate->factory()->NewStringFromAscii(CStrVector("primary")),
          NONE,
          SLOPPY);

      // case level: true + s1 -> case, s1 -> base.
      if (UCOL_ON == collator->getAttribute(UCOL_CASE_LEVEL, status)) {
        JSObject::SetProperty(
            resolved,
            isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
            isolate->factory()->NewStringFromAscii(CStrVector("case")),
            NONE,
            SLOPPY);
      } else {
        JSObject::SetProperty(
            resolved,
            isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
            isolate->factory()->NewStringFromAscii(CStrVector("base")),
            NONE,
            SLOPPY);
      }
      break;
    }
    case UCOL_SECONDARY:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("strength")),
          isolate->factory()->NewStringFromAscii(CStrVector("secondary")),
          NONE,
          SLOPPY);
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
          isolate->factory()->NewStringFromAscii(CStrVector("accent")),
          NONE,
          SLOPPY);
      break;
    case UCOL_TERTIARY:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("strength")),
          isolate->factory()->NewStringFromAscii(CStrVector("tertiary")),
          NONE,
          SLOPPY);
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
          isolate->factory()->NewStringFromAscii(CStrVector("variant")),
          NONE,
          SLOPPY);
      break;
    case UCOL_QUATERNARY:
      // We shouldn't get quaternary and identical from ICU, but if we do
      // put them into variant.
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("strength")),
          isolate->factory()->NewStringFromAscii(CStrVector("quaternary")),
          NONE,
          SLOPPY);
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
          isolate->factory()->NewStringFromAscii(CStrVector("variant")),
          NONE,
          SLOPPY);
      break;
    default:
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("strength")),
          isolate->factory()->NewStringFromAscii(CStrVector("identical")),
          NONE,
          SLOPPY);
      JSObject::SetProperty(
          resolved,
          isolate->factory()->NewStringFromAscii(CStrVector("sensitivity")),
          isolate->factory()->NewStringFromAscii(CStrVector("variant")),
          NONE,
          SLOPPY);
  }

  JSObject::SetProperty(
      resolved,
      isolate->factory()->NewStringFromAscii(CStrVector("ignorePunctuation")),
      isolate->factory()->ToBoolean(collator->getAttribute(
          UCOL_ALTERNATE_HANDLING, status) == UCOL_SHIFTED),
      NONE,
      SLOPPY);

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector(result)),
        NONE,
        SLOPPY);
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector("und")),
        NONE,
        SLOPPY);
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

  return break_iterator;
}


void SetResolvedBreakIteratorSettings(Isolate* isolate,
                                      const icu::Locale& icu_locale,
                                      icu::BreakIterator* break_iterator,
                                      Handle<JSObject> resolved) {
  UErrorCode status = U_ZERO_ERROR;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector(result)),
        NONE,
        SLOPPY);
  } else {
    // This would never happen, since we got the locale from ICU.
    JSObject::SetProperty(
        resolved,
        isolate->factory()->NewStringFromAscii(CStrVector("locale")),
        isolate->factory()->NewStringFromAscii(CStrVector("und")),
        NONE,
        SLOPPY);
  }
}

}  // namespace


// static
Handle<ObjectTemplateInfo> I18N::GetTemplate(Isolate* isolate) {
  return GetEternal<1, i::EternalHandles::I18N_TEMPLATE_ONE>(isolate);
}


// static
Handle<ObjectTemplateInfo> I18N::GetTemplate2(Isolate* isolate) {
  return GetEternal<2, i::EternalHandles::I18N_TEMPLATE_TWO>(isolate);
}


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
  Handle<String> key =
      isolate->factory()->NewStringFromAscii(CStrVector("dateFormat"));
  if (JSReceiver::HasLocalProperty(obj, key)) {
    return reinterpret_cast<icu::SimpleDateFormat*>(
        obj->GetInternalField(0));
  }

  return NULL;
}


template<class T>
void DeleteNativeObjectAt(const v8::WeakCallbackData<v8::Value, void>& data,
                          int index) {
  v8::Local<v8::Object> obj = v8::Handle<v8::Object>::Cast(data.GetValue());
  delete reinterpret_cast<T*>(obj->GetAlignedPointerFromInternalField(index));
}


static void DestroyGlobalHandle(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  GlobalHandles::Destroy(reinterpret_cast<Object**>(data.GetParameter()));
}


void DateFormat::DeleteDateFormat(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  DeleteNativeObjectAt<icu::SimpleDateFormat>(data, 0);
  DestroyGlobalHandle(data);
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
  Handle<String> key =
      isolate->factory()->NewStringFromAscii(CStrVector("numberFormat"));
  if (JSReceiver::HasLocalProperty(obj, key)) {
    return reinterpret_cast<icu::DecimalFormat*>(obj->GetInternalField(0));
  }

  return NULL;
}


void NumberFormat::DeleteNumberFormat(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  DeleteNativeObjectAt<icu::DecimalFormat>(data, 0);
  DestroyGlobalHandle(data);
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
  Handle<String> key =
      isolate->factory()->NewStringFromAscii(CStrVector("collator"));
  if (JSReceiver::HasLocalProperty(obj, key)) {
    return reinterpret_cast<icu::Collator*>(obj->GetInternalField(0));
  }

  return NULL;
}


void Collator::DeleteCollator(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  DeleteNativeObjectAt<icu::Collator>(data, 0);
  DestroyGlobalHandle(data);
}


icu::BreakIterator* BreakIterator::InitializeBreakIterator(
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

  icu::BreakIterator* break_iterator = CreateICUBreakIterator(
      isolate, icu_locale, options);
  if (!break_iterator) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    break_iterator = CreateICUBreakIterator(
        isolate, no_extension_locale, options);

    // Set resolved settings (locale).
    SetResolvedBreakIteratorSettings(
        isolate, no_extension_locale, break_iterator, resolved);
  } else {
    SetResolvedBreakIteratorSettings(
        isolate, icu_locale, break_iterator, resolved);
  }

  return break_iterator;
}


icu::BreakIterator* BreakIterator::UnpackBreakIterator(Isolate* isolate,
                                                       Handle<JSObject> obj) {
  Handle<String> key =
      isolate->factory()->NewStringFromAscii(CStrVector("breakIterator"));
  if (JSReceiver::HasLocalProperty(obj, key)) {
    return reinterpret_cast<icu::BreakIterator*>(obj->GetInternalField(0));
  }

  return NULL;
}


void BreakIterator::DeleteBreakIterator(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  DeleteNativeObjectAt<icu::BreakIterator>(data, 0);
  DeleteNativeObjectAt<icu::UnicodeString>(data, 1);
  DestroyGlobalHandle(data);
}

} }  // namespace v8::internal

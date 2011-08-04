// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "src/extensions/experimental/number-format.h"

#include <string.h>

#include "src/extensions/experimental/i18n-utils.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/uchar.h"
#include "unicode/ucurr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"

namespace v8 {
namespace internal {

const int NumberFormat::kCurrencyCodeLength = 4;

v8::Persistent<v8::FunctionTemplate> NumberFormat::number_format_template_;

static icu::DecimalFormat* CreateNumberFormat(v8::Handle<v8::String>,
                                              v8::Handle<v8::String>,
                                              v8::Handle<v8::Object>);
static icu::DecimalFormat* CreateFormatterFromSkeleton(
    const icu::Locale&, const icu::UnicodeString&, UErrorCode*);
static icu::DecimalFormatSymbols* GetFormatSymbols(const icu::Locale&);
static bool GetCurrencyCode(const icu::Locale&,
                            const char* const,
                            v8::Handle<v8::Object>,
                            UChar*);
static v8::Handle<v8::Value> ThrowUnexpectedObjectError();

icu::DecimalFormat* NumberFormat::UnpackNumberFormat(
    v8::Handle<v8::Object> obj) {
  if (number_format_template_->HasInstance(obj)) {
    return static_cast<icu::DecimalFormat*>(
        obj->GetPointerFromInternalField(0));
  }

  return NULL;
}

void NumberFormat::DeleteNumberFormat(v8::Persistent<v8::Value> object,
                                      void* param) {
  v8::Persistent<v8::Object> persistent_object =
      v8::Persistent<v8::Object>::Cast(object);

  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a number formatter.
  delete UnpackNumberFormat(persistent_object);

  // Then dispose of the persistent handle to JS object.
  persistent_object.Dispose();
}

v8::Handle<v8::Value> NumberFormat::Format(const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  if (args.Length() != 1 || !args[0]->IsNumber()) {
    // Just return NaN on invalid input.
    return v8::String::New("NaN");
  }

  icu::DecimalFormat* number_format = UnpackNumberFormat(args.Holder());
  if (!number_format) {
    return ThrowUnexpectedObjectError();
  }

  // ICU will handle actual NaN value properly and return NaN string.
  icu::UnicodeString result;
  number_format->format(args[0]->NumberValue(), result);

  return v8::String::New(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length());
}

v8::Handle<v8::Value> NumberFormat::JSNumberFormat(const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  // Expect locale id, region id and settings.
  if (args.Length() != 3 ||
      !args[0]->IsString() || !args[1]->IsString() || !args[2]->IsObject()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale, region and number settings are required.")));
  }

  icu::DecimalFormat* number_format = CreateNumberFormat(
      args[0]->ToString(), args[1]->ToString(), args[2]->ToObject());

  if (number_format_template_.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> raw_template(v8::FunctionTemplate::New());

    raw_template->SetClassName(v8::String::New("v8Locale.NumberFormat"));

    // Define internal field count on instance template.
    v8::Local<v8::ObjectTemplate> object_template =
        raw_template->InstanceTemplate();

    // Set aside internal field for icu number formatter.
    object_template->SetInternalFieldCount(1);

    // Define all of the prototype methods on prototype template.
    v8::Local<v8::ObjectTemplate> proto = raw_template->PrototypeTemplate();
    proto->Set(v8::String::New("format"),
               v8::FunctionTemplate::New(Format));

    number_format_template_ =
        v8::Persistent<v8::FunctionTemplate>::New(raw_template);
  }

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object =
      number_format_template_->GetFunction()->NewInstance();
  v8::Persistent<v8::Object> wrapper =
      v8::Persistent<v8::Object>::New(local_object);

  // Set number formatter as internal field of the resulting JS object.
  wrapper->SetPointerInInternalField(0, number_format);

  // Create options key.
  v8::Local<v8::Object> options = v8::Object::New();

  // Show what ICU decided to use for easier problem tracking.
  // Keep it as v8 specific extension.
  icu::UnicodeString pattern;
  number_format->toPattern(pattern);
  options->Set(v8::String::New("v8ResolvedPattern"),
               v8::String::New(reinterpret_cast<const uint16_t*>(
                   pattern.getBuffer()), pattern.length()));

  // Set resolved currency code in options.currency if not empty.
  icu::UnicodeString currency(number_format->getCurrency());
  if (!currency.isEmpty()) {
    options->Set(v8::String::New("currencyCode"),
                 v8::String::New(reinterpret_cast<const uint16_t*>(
                     currency.getBuffer()), currency.length()));
  }

  wrapper->Set(v8::String::New("options"), options);

  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak(NULL, DeleteNumberFormat);

  return wrapper;
}

// Returns DecimalFormat.
static icu::DecimalFormat* CreateNumberFormat(v8::Handle<v8::String> locale,
                                              v8::Handle<v8::String> region,
                                              v8::Handle<v8::Object> settings) {
  v8::HandleScope handle_scope;

  v8::String::AsciiValue ascii_locale(locale);
  icu::Locale icu_locale(*ascii_locale);

  // Make formatter from skeleton.
  icu::DecimalFormat* number_format = NULL;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString setting;

  if (I18NUtils::ExtractStringSetting(settings, "skeleton", &setting)) {
    // TODO(cira): Use ICU skeleton once
    // http://bugs.icu-project.org/trac/ticket/8610 is resolved.
    number_format = CreateFormatterFromSkeleton(icu_locale, setting, &status);
  } else if (I18NUtils::ExtractStringSetting(settings, "pattern", &setting)) {
    number_format =
        new icu::DecimalFormat(setting, GetFormatSymbols(icu_locale), status);
  } else if (I18NUtils::ExtractStringSetting(settings, "style", &setting)) {
    if (setting == UNICODE_STRING_SIMPLE("currency")) {
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createCurrencyInstance(icu_locale, status));
    } else if (setting == UNICODE_STRING_SIMPLE("percent")) {
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createPercentInstance(icu_locale, status));
    } else if (setting == UNICODE_STRING_SIMPLE("scientific")) {
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createScientificInstance(icu_locale, status));
    } else {
      // Make it decimal in any other case.
      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(icu_locale, status));
    }
  }

  if (U_FAILURE(status)) {
    delete number_format;
    status = U_ZERO_ERROR;
    number_format = static_cast<icu::DecimalFormat*>(
        icu::NumberFormat::createInstance(icu_locale, status));
  }

  // Attach appropriate currency code to the formatter.
  // It affects currency formatters only.
  // Region is full language identifier in form 'und_' + region id.
  v8::String::AsciiValue ascii_region(region);

  UChar currency_code[NumberFormat::kCurrencyCodeLength];
  if (GetCurrencyCode(icu_locale, *ascii_region, settings, currency_code)) {
    number_format->setCurrency(currency_code, status);
  }

  return number_format;
}

// Generates ICU number format pattern from given skeleton.
// TODO(cira): Remove once ICU includes equivalent method
// (see http://bugs.icu-project.org/trac/ticket/8610).
static icu::DecimalFormat* CreateFormatterFromSkeleton(
    const icu::Locale& icu_locale,
    const icu::UnicodeString& skeleton,
    UErrorCode* status) {
  icu::DecimalFormat skeleton_format(
      skeleton, GetFormatSymbols(icu_locale), *status);

  // Find out if skeleton contains currency or percent symbol and create
  // proper instance to tweak.
  icu::DecimalFormat* base_format = NULL;

  // UChar representation of U+00A4 currency symbol.
  const UChar currency_symbol = 0xA4u;

  int32_t index = skeleton.indexOf(currency_symbol);
  if (index != -1) {
    // Find how many U+00A4 are there. There is at least one.
    // Case of non-consecutive U+00A4 is taken care of in i18n.js.
    int32_t end_index = skeleton.lastIndexOf(currency_symbol, index);

#if (U_ICU_VERSION_MAJOR_NUM == 4) && (U_ICU_VERSION_MINOR_NUM <= 6)
    icu::NumberFormat::EStyles style;
    switch (end_index - index) {
      case 0:
        style = icu::NumberFormat::kCurrencyStyle;
        break;
      case 1:
        style = icu::NumberFormat::kIsoCurrencyStyle;
        break;
      default:
        style = icu::NumberFormat::kPluralCurrencyStyle;
    }
#else  // ICU version is 4.8 or above (we ignore versions below 4.0).
    UNumberFormatStyle style;
    switch (end_index - index) {
      case 0:
        style = UNUM_CURRENCY;
        break;
      case 1:
        style = UNUM_CURRENCY_ISO;
        break;
      default:
        style = UNUM_CURRENCY_PLURAL;
    }
#endif

    base_format = static_cast<icu::DecimalFormat*>(
        icu::NumberFormat::createInstance(icu_locale, style, *status));
  } else if (skeleton.indexOf('%') != -1) {
    base_format = static_cast<icu::DecimalFormat*>(
        icu::NumberFormat::createPercentInstance(icu_locale, *status));
  } else {
    // TODO(cira): Handle scientific skeleton.
    base_format = static_cast<icu::DecimalFormat*>(
        icu::NumberFormat::createInstance(icu_locale, *status));
  }

  if (U_FAILURE(*status)) {
    delete base_format;
    return NULL;
  }

  // Copy important information from skeleton to the new formatter.
  // TODO(cira): copy rounding information from skeleton?
  base_format->setGroupingUsed(skeleton_format.isGroupingUsed());

  base_format->setMinimumIntegerDigits(
      skeleton_format.getMinimumIntegerDigits());

  base_format->setMinimumFractionDigits(
      skeleton_format.getMinimumFractionDigits());

  base_format->setMaximumFractionDigits(
      skeleton_format.getMaximumFractionDigits());

  return base_format;
}

// Gets decimal symbols for a locale.
static icu::DecimalFormatSymbols* GetFormatSymbols(
    const icu::Locale& icu_locale) {
  UErrorCode status = U_ZERO_ERROR;
  icu::DecimalFormatSymbols* symbols =
      new icu::DecimalFormatSymbols(icu_locale, status);

  if (U_FAILURE(status)) {
    delete symbols;
    // Use symbols from default locale.
    symbols = new icu::DecimalFormatSymbols(status);
  }

  return symbols;
}

// Gets currency ISO 4217 3-letter code.
// Check currencyCode setting first, then @currency=code and in the end
// try to infer currency code from locale in the form 'und_' + region id.
// Returns false in case of error.
static bool GetCurrencyCode(const icu::Locale& icu_locale,
                            const char* const und_region_locale,
                            v8::Handle<v8::Object> settings,
                            UChar* code) {
  UErrorCode status = U_ZERO_ERROR;

  // If there is user specified currency code, use it.
  icu::UnicodeString currency;
  if (I18NUtils::ExtractStringSetting(settings, "currencyCode", &currency)) {
    currency.extract(code, NumberFormat::kCurrencyCodeLength, status);
    return true;
  }

  // If ICU locale has -cu- currency code use it.
  char currency_code[NumberFormat::kCurrencyCodeLength];
  int32_t length = icu_locale.getKeywordValue(
      "currency", currency_code, NumberFormat::kCurrencyCodeLength, status);
  if (length != 0) {
    I18NUtils::AsciiToUChar(currency_code, length + 1,
                            code, NumberFormat::kCurrencyCodeLength);
    return true;
  }

  // Otherwise infer currency code from the region id.
  ucurr_forLocale(
      und_region_locale, code, NumberFormat::kCurrencyCodeLength, &status);

  return !!U_SUCCESS(status);
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("NumberFormat method called on an object "
                      "that is not a NumberFormat.")));
}

} }  // namespace v8::internal

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

#include "number-format.h"

#include <string.h>

#include "i18n-utils.h"
#include "unicode/curramt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/uchar.h"
#include "unicode/ucurr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"

namespace v8_i18n {

static icu::DecimalFormat* InitializeNumberFormat(v8::Handle<v8::String>,
                                                  v8::Handle<v8::Object>,
                                                  v8::Handle<v8::Object>);
static icu::DecimalFormat* CreateICUNumberFormat(const icu::Locale&,
                                                 v8::Handle<v8::Object>);
static void SetResolvedSettings(const icu::Locale&,
                                icu::DecimalFormat*,
                                v8::Handle<v8::Object>);

icu::DecimalFormat* NumberFormat::UnpackNumberFormat(
    v8::Handle<v8::Object> obj) {
  v8::HandleScope handle_scope;

  // v8::ObjectTemplate doesn't have HasInstance method so we can't check
  // if obj is an instance of NumberFormat class. We'll check for a property
  // that has to be in the object. The same applies to other services, like
  // Collator and DateTimeFormat.
  if (obj->HasOwnProperty(v8::String::New("numberFormat"))) {
    return static_cast<icu::DecimalFormat*>(
        obj->GetAlignedPointerFromInternalField(0));
  }

  return NULL;
}

void NumberFormat::DeleteNumberFormat(v8::Isolate* isolate,
                                      v8::Persistent<v8::Object>* object,
                                      void* param) {
  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a date time formatter.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> handle = v8::Local<v8::Object>::New(isolate, *object);
  delete UnpackNumberFormat(handle);

  // Then dispose of the persistent handle to JS object.
  object->Dispose(isolate);
}

void NumberFormat::JSInternalFormat(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsNumber()) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("Formatter and numeric value have to be specified.")));
    return;
  }

  icu::DecimalFormat* number_format = UnpackNumberFormat(args[0]->ToObject());
  if (!number_format) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("NumberFormat method called on an object "
                        "that is not a NumberFormat.")));
    return;
  }

  // ICU will handle actual NaN value properly and return NaN string.
  icu::UnicodeString result;
  number_format->format(args[1]->NumberValue(), result);

  args.GetReturnValue().Set(v8::String::New(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

void NumberFormat::JSInternalParse(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsString()) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("Formatter and string have to be specified.")));
    return;
  }

  icu::DecimalFormat* number_format = UnpackNumberFormat(args[0]->ToObject());
  if (!number_format) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("NumberFormat method called on an object "
                        "that is not a NumberFormat.")));
    return;
  }

  // ICU will handle actual NaN value properly and return NaN string.
  icu::UnicodeString string_number;
  if (!Utils::V8StringToUnicodeString(args[1]->ToString(), &string_number)) {
    string_number = "";
  }

  UErrorCode status = U_ZERO_ERROR;
  icu::Formattable result;
  // ICU 4.6 doesn't support parseCurrency call. We need to wait for ICU49
  // to be part of Chrome.
  // TODO(cira): Include currency parsing code using parseCurrency call.
  // We need to check if the formatter parses all currencies or only the
  // one it was constructed with (it will impact the API - how to return ISO
  // code and the value).
  number_format->parse(string_number, result, status);
  if (U_FAILURE(status)) {
    return;
  }

  switch (result.getType()) {
  case icu::Formattable::kDouble:
    args.GetReturnValue().Set(result.getDouble());
    return;
  case icu::Formattable::kLong:
    args.GetReturnValue().Set(v8::Number::New(result.getLong()));
    return;
  case icu::Formattable::kInt64:
    args.GetReturnValue().Set(v8::Number::New(result.getInt64()));
    return;
  default:
    return;
  }
}

void NumberFormat::JSCreateNumberFormat(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 ||
      !args[0]->IsString() ||
      !args[1]->IsObject() ||
      !args[2]->IsObject()) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("Internal error, wrong parameters.")));
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  v8::Local<v8::ObjectTemplate> number_format_template =
      Utils::GetTemplate(isolate);

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object = number_format_template->NewInstance();
  // But the handle shouldn't be empty.
  // That can happen if there was a stack overflow when creating the object.
  if (local_object.IsEmpty()) {
    args.GetReturnValue().Set(local_object);
    return;
  }

  // Set number formatter as internal field of the resulting JS object.
  icu::DecimalFormat* number_format = InitializeNumberFormat(
      args[0]->ToString(), args[1]->ToObject(), args[2]->ToObject());

  if (!number_format) {
    v8::ThrowException(v8::Exception::Error(v8::String::New(
        "Internal error. Couldn't create ICU number formatter.")));
    return;
  } else {
    local_object->SetAlignedPointerInInternalField(0, number_format);

    v8::TryCatch try_catch;
    local_object->Set(v8::String::New("numberFormat"),
                      v8::String::New("valid"));
    if (try_catch.HasCaught()) {
      v8::ThrowException(v8::Exception::Error(
          v8::String::New("Internal error, couldn't set property.")));
      return;
    }
  }

  v8::Persistent<v8::Object> wrapper(isolate, local_object);
  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak<void>(NULL, &DeleteNumberFormat);
  args.GetReturnValue().Set(wrapper);
  wrapper.ClearAndLeak();
}

static icu::DecimalFormat* InitializeNumberFormat(
    v8::Handle<v8::String> locale,
    v8::Handle<v8::Object> options,
    v8::Handle<v8::Object> resolved) {
  // Convert BCP47 into ICU locale format.
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale icu_locale;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;
  v8::String::AsciiValue bcp47_locale(locale);
  if (bcp47_locale.length() != 0) {
    uloc_forLanguageTag(*bcp47_locale, icu_result, ULOC_FULLNAME_CAPACITY,
                        &icu_length, &status);
    if (U_FAILURE(status) || icu_length == 0) {
      return NULL;
    }
    icu_locale = icu::Locale(icu_result);
  }

  icu::DecimalFormat* number_format =
      CreateICUNumberFormat(icu_locale, options);
  if (!number_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    number_format = CreateICUNumberFormat(no_extension_locale, options);

    // Set resolved settings (pattern, numbering system).
    SetResolvedSettings(no_extension_locale, number_format, resolved);
  } else {
    SetResolvedSettings(icu_locale, number_format, resolved);
  }

  return number_format;
}

static icu::DecimalFormat* CreateICUNumberFormat(
    const icu::Locale& icu_locale, v8::Handle<v8::Object> options) {
  // Make formatter from options. Numbering system is added
  // to the locale as Unicode extension (if it was specified at all).
  UErrorCode status = U_ZERO_ERROR;
  icu::DecimalFormat* number_format = NULL;
  icu::UnicodeString style;
  icu::UnicodeString currency;
  if (Utils::ExtractStringSetting(options, "style", &style)) {
    if (style == UNICODE_STRING_SIMPLE("currency")) {
      Utils::ExtractStringSetting(options, "currency", &currency);

      icu::UnicodeString display;
      Utils::ExtractStringSetting(options, "currencyDisplay", &display);
#if (U_ICU_VERSION_MAJOR_NUM == 4) && (U_ICU_VERSION_MINOR_NUM <= 6)
      icu::NumberFormat::EStyles style;
      if (display == UNICODE_STRING_SIMPLE("code")) {
        style = icu::NumberFormat::kIsoCurrencyStyle;
      } else if (display == UNICODE_STRING_SIMPLE("name")) {
        style = icu::NumberFormat::kPluralCurrencyStyle;
      } else {
        style = icu::NumberFormat::kCurrencyStyle;
      }
#else  // ICU version is 4.8 or above (we ignore versions below 4.0).
      UNumberFormatStyle style;
      if (display == UNICODE_STRING_SIMPLE("code")) {
        style = UNUM_CURRENCY_ISO;
      } else if (display == UNICODE_STRING_SIMPLE("name")) {
        style = UNUM_CURRENCY_PLURAL;
      } else {
        style = UNUM_CURRENCY;
      }
#endif

      number_format = static_cast<icu::DecimalFormat*>(
          icu::NumberFormat::createInstance(icu_locale, style,  status));
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
  if (Utils::ExtractIntegerSetting(
          options, "minimumIntegerDigits", &digits)) {
    number_format->setMinimumIntegerDigits(digits);
  }

  if (Utils::ExtractIntegerSetting(
          options, "minimumFractionDigits", &digits)) {
    number_format->setMinimumFractionDigits(digits);
  }

  if (Utils::ExtractIntegerSetting(
          options, "maximumFractionDigits", &digits)) {
    number_format->setMaximumFractionDigits(digits);
  }

  bool significant_digits_used = false;
  if (Utils::ExtractIntegerSetting(
          options, "minimumSignificantDigits", &digits)) {
    number_format->setMinimumSignificantDigits(digits);
    significant_digits_used = true;
  }

  if (Utils::ExtractIntegerSetting(
          options, "maximumSignificantDigits", &digits)) {
    number_format->setMaximumSignificantDigits(digits);
    significant_digits_used = true;
  }

  number_format->setSignificantDigitsUsed(significant_digits_used);

  bool grouping;
  if (Utils::ExtractBooleanSetting(options, "useGrouping", &grouping)) {
    number_format->setGroupingUsed(grouping);
  }

  // Set rounding mode.
  number_format->setRoundingMode(icu::DecimalFormat::kRoundHalfUp);

  return number_format;
}

static void SetResolvedSettings(const icu::Locale& icu_locale,
                                icu::DecimalFormat* number_format,
                                v8::Handle<v8::Object> resolved) {
  icu::UnicodeString pattern;
  number_format->toPattern(pattern);
  resolved->Set(v8::String::New("pattern"),
                v8::String::New(reinterpret_cast<const uint16_t*>(
                    pattern.getBuffer()), pattern.length()));

  // Set resolved currency code in options.currency if not empty.
  icu::UnicodeString currency(number_format->getCurrency());
  if (!currency.isEmpty()) {
    resolved->Set(v8::String::New("currency"),
                  v8::String::New(reinterpret_cast<const uint16_t*>(
                      currency.getBuffer()), currency.length()));
  }

  // Ugly hack. ICU doesn't expose numbering system in any way, so we have
  // to assume that for given locale NumberingSystem constructor produces the
  // same digits as NumberFormat would.
  UErrorCode status = U_ZERO_ERROR;
  icu::NumberingSystem* numbering_system =
      icu::NumberingSystem::createInstance(icu_locale, status);
  if (U_SUCCESS(status)) {
    const char* ns = numbering_system->getName();
    resolved->Set(v8::String::New("numberingSystem"), v8::String::New(ns));
  } else {
    resolved->Set(v8::String::New("numberingSystem"), v8::Undefined());
  }
  delete numbering_system;

  resolved->Set(v8::String::New("useGrouping"),
                v8::Boolean::New(number_format->isGroupingUsed()));

  resolved->Set(v8::String::New("minimumIntegerDigits"),
                v8::Integer::New(number_format->getMinimumIntegerDigits()));

  resolved->Set(v8::String::New("minimumFractionDigits"),
                v8::Integer::New(number_format->getMinimumFractionDigits()));

  resolved->Set(v8::String::New("maximumFractionDigits"),
                v8::Integer::New(number_format->getMaximumFractionDigits()));

  if (resolved->HasOwnProperty(v8::String::New("minimumSignificantDigits"))) {
    resolved->Set(v8::String::New("minimumSignificantDigits"), v8::Integer::New(
        number_format->getMinimumSignificantDigits()));
  }

  if (resolved->HasOwnProperty(v8::String::New("maximumSignificantDigits"))) {
    resolved->Set(v8::String::New("maximumSignificantDigits"), v8::Integer::New(
        number_format->getMaximumSignificantDigits()));
  }

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      icu_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    resolved->Set(v8::String::New("locale"), v8::String::New(result));
  } else {
    // This would never happen, since we got the locale from ICU.
    resolved->Set(v8::String::New("locale"), v8::String::New("und"));
  }
}

}  // namespace v8_i18n

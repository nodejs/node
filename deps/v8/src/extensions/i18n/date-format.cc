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

#include "date-format.h"

#include <string.h>

#include "i18n-utils.h"
#include "unicode/calendar.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/numsys.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"

namespace v8_i18n {

static icu::SimpleDateFormat* InitializeDateTimeFormat(v8::Handle<v8::String>,
                                                       v8::Handle<v8::Object>,
                                                       v8::Handle<v8::Object>);
static icu::SimpleDateFormat* CreateICUDateFormat(const icu::Locale&,
                                                  v8::Handle<v8::Object>);
static void SetResolvedSettings(const icu::Locale&,
                                icu::SimpleDateFormat*,
                                v8::Handle<v8::Object>);

icu::SimpleDateFormat* DateFormat::UnpackDateFormat(
    v8::Handle<v8::Object> obj) {
  v8::HandleScope handle_scope;

  if (obj->HasOwnProperty(v8::String::New("dateFormat"))) {
    return static_cast<icu::SimpleDateFormat*>(
        obj->GetAlignedPointerFromInternalField(0));
  }

  return NULL;
}

void DateFormat::DeleteDateFormat(v8::Isolate* isolate,
                                  v8::Persistent<v8::Object>* object,
                                  void* param) {
  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a date time formatter.
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> handle = v8::Local<v8::Object>::New(isolate, *object);
  delete UnpackDateFormat(handle);

  // Then dispose of the persistent handle to JS object.
  object->Dispose(isolate);
}

void DateFormat::JSInternalFormat(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  double millis = 0.0;
  if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsDate()) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New(
            "Internal error. Formatter and date value have to be specified.")));
    return;
  } else {
    millis = v8::Date::Cast(*args[1])->NumberValue();
  }

  icu::SimpleDateFormat* date_format = UnpackDateFormat(args[0]->ToObject());
  if (!date_format) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("DateTimeFormat method called on an object "
                        "that is not a DateTimeFormat.")));
    return;
  }

  icu::UnicodeString result;
  date_format->format(millis, result);

  args.GetReturnValue().Set(v8::String::New(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

void DateFormat::JSInternalParse(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  icu::UnicodeString string_date;
  if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsString()) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New(
            "Internal error. Formatter and string have to be specified.")));
    return;
  } else {
    if (!Utils::V8StringToUnicodeString(args[1], &string_date)) {
      string_date = "";
    }
  }

  icu::SimpleDateFormat* date_format = UnpackDateFormat(args[0]->ToObject());
  if (!date_format) {
    v8::ThrowException(v8::Exception::Error(
        v8::String::New("DateTimeFormat method called on an object "
                        "that is not a DateTimeFormat.")));
    return;
  }

  UErrorCode status = U_ZERO_ERROR;
  UDate date = date_format->parse(string_date, status);
  if (U_FAILURE(status)) {
    return;
  }

  args.GetReturnValue().Set(v8::Date::New(static_cast<double>(date)));
}

void DateFormat::JSCreateDateTimeFormat(
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
  v8::Local<v8::ObjectTemplate> date_format_template =
      Utils::GetTemplate(isolate);

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object = date_format_template->NewInstance();
  // But the handle shouldn't be empty.
  // That can happen if there was a stack overflow when creating the object.
  if (local_object.IsEmpty()) {
    args.GetReturnValue().Set(local_object);
    return;
  }

  // Set date time formatter as internal field of the resulting JS object.
  icu::SimpleDateFormat* date_format = InitializeDateTimeFormat(
      args[0]->ToString(), args[1]->ToObject(), args[2]->ToObject());

  if (!date_format) {
    v8::ThrowException(v8::Exception::Error(v8::String::New(
        "Internal error. Couldn't create ICU date time formatter.")));
    return;
  } else {
    local_object->SetAlignedPointerInInternalField(0, date_format);

    v8::TryCatch try_catch;
    local_object->Set(v8::String::New("dateFormat"), v8::String::New("valid"));
    if (try_catch.HasCaught()) {
      v8::ThrowException(v8::Exception::Error(
          v8::String::New("Internal error, couldn't set property.")));
      return;
    }
  }

  v8::Persistent<v8::Object> wrapper(isolate, local_object);
  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak<void>(NULL, &DeleteDateFormat);
  args.GetReturnValue().Set(wrapper);
  wrapper.ClearAndLeak();
}

static icu::SimpleDateFormat* InitializeDateTimeFormat(
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

  icu::SimpleDateFormat* date_format = CreateICUDateFormat(icu_locale, options);
  if (!date_format) {
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    date_format = CreateICUDateFormat(no_extension_locale, options);

    // Set resolved settings (pattern, numbering system, calendar).
    SetResolvedSettings(no_extension_locale, date_format, resolved);
  } else {
    SetResolvedSettings(icu_locale, date_format, resolved);
  }

  return date_format;
}

static icu::SimpleDateFormat* CreateICUDateFormat(
    const icu::Locale& icu_locale, v8::Handle<v8::Object> options) {
  // Create time zone as specified by the user. We have to re-create time zone
  // since calendar takes ownership.
  icu::TimeZone* tz = NULL;
  icu::UnicodeString timezone;
  if (Utils::ExtractStringSetting(options, "timeZone", &timezone)) {
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
  if (Utils::ExtractStringSetting(options, "skeleton", &skeleton)) {
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

static void SetResolvedSettings(const icu::Locale& icu_locale,
                                icu::SimpleDateFormat* date_format,
                                v8::Handle<v8::Object> resolved) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString pattern;
  date_format->toPattern(pattern);
  resolved->Set(v8::String::New("pattern"),
                v8::String::New(reinterpret_cast<const uint16_t*>(
                    pattern.getBuffer()), pattern.length()));

  // Set time zone and calendar.
  if (date_format) {
    const icu::Calendar* calendar = date_format->getCalendar();
    const char* calendar_name = calendar->getType();
    resolved->Set(v8::String::New("calendar"), v8::String::New(calendar_name));

    const icu::TimeZone& tz = calendar->getTimeZone();
    icu::UnicodeString time_zone;
    tz.getID(time_zone);

    icu::UnicodeString canonical_time_zone;
    icu::TimeZone::getCanonicalID(time_zone, canonical_time_zone, status);
    if (U_SUCCESS(status)) {
      if (canonical_time_zone == UNICODE_STRING_SIMPLE("Etc/GMT")) {
        resolved->Set(v8::String::New("timeZone"), v8::String::New("UTC"));
      } else {
        resolved->Set(v8::String::New("timeZone"),
                      v8::String::New(reinterpret_cast<const uint16_t*>(
                          canonical_time_zone.getBuffer()),
                                      canonical_time_zone.length()));
      }
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
    resolved->Set(v8::String::New("numberingSystem"), v8::String::New(ns));
  } else {
    resolved->Set(v8::String::New("numberingSystem"), v8::Undefined());
  }
  delete numbering_system;

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

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

#include "src/extensions/experimental/datetime-format.h"

#include <string.h>

#include "src/extensions/experimental/i18n-utils.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/smpdtfmt.h"

namespace v8 {
namespace internal {

v8::Persistent<v8::FunctionTemplate> DateTimeFormat::datetime_format_template_;

static icu::DateFormat* CreateDateTimeFormat(v8::Handle<v8::String>,
                                             v8::Handle<v8::Object>);
static v8::Handle<v8::Value> GetSymbols(
    const v8::Arguments&,
    const icu::UnicodeString*, int32_t,
    const icu::UnicodeString*, int32_t,
    const icu::UnicodeString*, int32_t);
static v8::Handle<v8::Value> ThrowUnexpectedObjectError();
static icu::DateFormat::EStyle GetDateTimeStyle(const icu::UnicodeString&);

icu::SimpleDateFormat* DateTimeFormat::UnpackDateTimeFormat(
    v8::Handle<v8::Object> obj) {
  if (datetime_format_template_->HasInstance(obj)) {
    return static_cast<icu::SimpleDateFormat*>(
        obj->GetPointerFromInternalField(0));
  }

  return NULL;
}

void DateTimeFormat::DeleteDateTimeFormat(v8::Persistent<v8::Value> object,
                                          void* param) {
  v8::Persistent<v8::Object> persistent_object =
      v8::Persistent<v8::Object>::Cast(object);

  // First delete the hidden C++ object.
  // Unpacking should never return NULL here. That would only happen if
  // this method is used as the weak callback for persistent handles not
  // pointing to a date time formatter.
  delete UnpackDateTimeFormat(persistent_object);

  // Then dispose of the persistent handle to JS object.
  persistent_object.Dispose();
}

v8::Handle<v8::Value> DateTimeFormat::Format(const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  double millis = 0.0;
  if (args.Length() != 1 || !args[0]->IsDate()) {
    // Create a new date.
    v8::TryCatch try_catch;
    v8::Local<v8::Script> date_script =
        v8::Script::Compile(v8::String::New("eval('new Date()')"));
    millis = date_script->Run()->NumberValue();
    if (try_catch.HasCaught()) {
      return try_catch.ReThrow();
    }
  } else {
    millis = v8::Date::Cast(*args[0])->NumberValue();
  }

  icu::SimpleDateFormat* date_format = UnpackDateTimeFormat(args.Holder());
  if (!date_format) {
    return ThrowUnexpectedObjectError();
  }

  icu::UnicodeString result;
  date_format->format(millis, result);

  return v8::String::New(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length());
}

v8::Handle<v8::Value> DateTimeFormat::GetMonths(const v8::Arguments& args) {
  icu::SimpleDateFormat* date_format = UnpackDateTimeFormat(args.Holder());
  if (!date_format) {
    return ThrowUnexpectedObjectError();
  }

  const icu::DateFormatSymbols* symbols = date_format->getDateFormatSymbols();

  int32_t narrow_count;
  const icu::UnicodeString* narrow = symbols->getMonths(
      narrow_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::NARROW);
  int32_t abbrev_count;
  const icu::UnicodeString* abbrev = symbols->getMonths(
      abbrev_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::ABBREVIATED);
  int32_t wide_count;
  const icu::UnicodeString* wide = symbols->getMonths(
      wide_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::WIDE);

  return GetSymbols(
      args, narrow, narrow_count, abbrev, abbrev_count, wide, wide_count);
}

v8::Handle<v8::Value> DateTimeFormat::GetWeekdays(const v8::Arguments& args) {
  icu::SimpleDateFormat* date_format = UnpackDateTimeFormat(args.Holder());
  if (!date_format) {
    return ThrowUnexpectedObjectError();
  }

  const icu::DateFormatSymbols* symbols = date_format->getDateFormatSymbols();

  int32_t narrow_count;
  const icu::UnicodeString* narrow = symbols->getWeekdays(
      narrow_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::NARROW);
  int32_t abbrev_count;
  const icu::UnicodeString* abbrev = symbols->getWeekdays(
      abbrev_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::ABBREVIATED);
  int32_t wide_count;
  const icu::UnicodeString* wide = symbols->getWeekdays(
      wide_count,
      icu::DateFormatSymbols::STANDALONE,
      icu::DateFormatSymbols::WIDE);

  // getXXXWeekdays always returns 8 elements - ICU stable API.
  // We can't use ASSERT_EQ(8, narrow_count) because ASSERT is internal to v8.
  if (narrow_count != 8 || abbrev_count != 8 || wide_count != 8) {
    return v8::ThrowException(v8::Exception::Error(
        v8::String::New("Failed to get weekday information.")));
  }

  // ICU documentation says we should ignore element 0 of the returned array.
  return GetSymbols(args, narrow + 1, narrow_count - 1, abbrev + 1,
                    abbrev_count -1 , wide + 1, wide_count - 1);
}

v8::Handle<v8::Value> DateTimeFormat::GetEras(const v8::Arguments& args) {
  icu::SimpleDateFormat* date_format = UnpackDateTimeFormat(args.Holder());
  if (!date_format) {
    return ThrowUnexpectedObjectError();
  }

  const icu::DateFormatSymbols* symbols = date_format->getDateFormatSymbols();

  int32_t narrow_count;
  const icu::UnicodeString* narrow = symbols->getNarrowEras(narrow_count);
  int32_t abbrev_count;
  const icu::UnicodeString* abbrev = symbols->getEras(abbrev_count);
  int32_t wide_count;
  const icu::UnicodeString* wide = symbols->getEraNames(wide_count);

  return GetSymbols(
      args, narrow, narrow_count, abbrev, abbrev_count, wide, wide_count);
}

v8::Handle<v8::Value> DateTimeFormat::GetAmPm(const v8::Arguments& args) {
  icu::SimpleDateFormat* date_format = UnpackDateTimeFormat(args.Holder());
  if (!date_format) {
    return ThrowUnexpectedObjectError();
  }

  const icu::DateFormatSymbols* symbols = date_format->getDateFormatSymbols();

  // In this case narrow == abbreviated == wide
  int32_t count;
  const icu::UnicodeString* wide = symbols->getAmPmStrings(count);

  return GetSymbols(args, wide, count, wide, count, wide, count);
}

v8::Handle<v8::Value> DateTimeFormat::JSDateTimeFormat(
    const v8::Arguments& args) {
  v8::HandleScope handle_scope;

  if (args.Length() != 2 || !args[0]->IsString() || !args[1]->IsObject()) {
    return v8::ThrowException(v8::Exception::SyntaxError(
        v8::String::New("Locale and date/time options are required.")));
  }

  icu::SimpleDateFormat* date_format = static_cast<icu::SimpleDateFormat*>(
      CreateDateTimeFormat(args[0]->ToString(), args[1]->ToObject()));

  if (datetime_format_template_.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> raw_template(v8::FunctionTemplate::New());

    raw_template->SetClassName(v8::String::New("v8Locale.DateTimeFormat"));

    // Define internal field count on instance template.
    v8::Local<v8::ObjectTemplate> object_template =
        raw_template->InstanceTemplate();

    // Set aside internal field for icu date time formatter.
    object_template->SetInternalFieldCount(1);

    // Define all of the prototype methods on prototype template.
    v8::Local<v8::ObjectTemplate> proto = raw_template->PrototypeTemplate();
    proto->Set(v8::String::New("format"),
               v8::FunctionTemplate::New(Format));
    proto->Set(v8::String::New("getMonths"),
               v8::FunctionTemplate::New(GetMonths));
    proto->Set(v8::String::New("getWeekdays"),
               v8::FunctionTemplate::New(GetWeekdays));
    proto->Set(v8::String::New("getEras"),
               v8::FunctionTemplate::New(GetEras));
    proto->Set(v8::String::New("getAmPm"),
               v8::FunctionTemplate::New(GetAmPm));

    datetime_format_template_ =
        v8::Persistent<v8::FunctionTemplate>::New(raw_template);
  }

  // Create an empty object wrapper.
  v8::Local<v8::Object> local_object =
      datetime_format_template_->GetFunction()->NewInstance();
  v8::Persistent<v8::Object> wrapper =
      v8::Persistent<v8::Object>::New(local_object);

  // Set date time formatter as internal field of the resulting JS object.
  wrapper->SetPointerInInternalField(0, date_format);

  // Set resolved pattern in options.pattern.
  icu::UnicodeString pattern;
  date_format->toPattern(pattern);
  v8::Local<v8::Object> options = v8::Object::New();
  options->Set(v8::String::New("pattern"),
               v8::String::New(reinterpret_cast<const uint16_t*>(
                   pattern.getBuffer()), pattern.length()));
  wrapper->Set(v8::String::New("options"), options);

  // Make object handle weak so we can delete iterator once GC kicks in.
  wrapper.MakeWeak(NULL, DeleteDateTimeFormat);

  return wrapper;
}

// Returns SimpleDateFormat.
static icu::DateFormat* CreateDateTimeFormat(
    v8::Handle<v8::String> locale, v8::Handle<v8::Object> settings) {
  v8::HandleScope handle_scope;

  v8::String::AsciiValue ascii_locale(locale);
  icu::Locale icu_locale(*ascii_locale);

  // Make formatter from skeleton.
  icu::SimpleDateFormat* date_format = NULL;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString skeleton;
  if (I18NUtils::ExtractStringSetting(settings, "skeleton", &skeleton)) {
    v8::Local<icu::DateTimePatternGenerator> generator(
        icu::DateTimePatternGenerator::createInstance(icu_locale, status));
    icu::UnicodeString pattern =
        generator->getBestPattern(skeleton, status);

    date_format = new icu::SimpleDateFormat(pattern, icu_locale, status);
    if (U_SUCCESS(status)) {
      return date_format;
    } else {
      delete date_format;
    }
  }

  // Extract date style and time style from settings.
  icu::UnicodeString date_style;
  icu::DateFormat::EStyle icu_date_style = icu::DateFormat::kNone;
  if (I18NUtils::ExtractStringSetting(settings, "dateStyle", &date_style)) {
    icu_date_style = GetDateTimeStyle(date_style);
  }

  icu::UnicodeString time_style;
  icu::DateFormat::EStyle icu_time_style = icu::DateFormat::kNone;
  if (I18NUtils::ExtractStringSetting(settings, "timeStyle", &time_style)) {
    icu_time_style = GetDateTimeStyle(time_style);
  }

  // Try all combinations of date/time styles.
  if (icu_date_style == icu::DateFormat::kNone &&
      icu_time_style == icu::DateFormat::kNone) {
    // Return default short date, short
    return icu::DateFormat::createDateTimeInstance(
        icu::DateFormat::kShort, icu::DateFormat::kShort, icu_locale);
  } else if (icu_date_style != icu::DateFormat::kNone &&
             icu_time_style != icu::DateFormat::kNone) {
    return icu::DateFormat::createDateTimeInstance(
        icu_date_style, icu_time_style, icu_locale);
  } else if (icu_date_style != icu::DateFormat::kNone) {
    return icu::DateFormat::createDateInstance(icu_date_style, icu_locale);
  } else {
    // icu_time_style != icu::DateFormat::kNone
    return icu::DateFormat::createTimeInstance(icu_time_style, icu_locale);
  }
}

// Creates a v8::Array of narrow, abbrev or wide symbols.
static v8::Handle<v8::Value> GetSymbols(const v8::Arguments& args,
                                        const icu::UnicodeString* narrow,
                                        int32_t narrow_count,
                                        const icu::UnicodeString* abbrev,
                                        int32_t abbrev_count,
                                        const icu::UnicodeString* wide,
                                        int32_t wide_count) {
  v8::HandleScope handle_scope;

  // Make wide width default.
  const icu::UnicodeString* result = wide;
  int32_t count = wide_count;

  if (args.Length() == 1 && args[0]->IsString()) {
    v8::String::AsciiValue ascii_value(args[0]);
    if (strcmp(*ascii_value, "abbreviated") == 0) {
      result = abbrev;
      count = abbrev_count;
    } else if (strcmp(*ascii_value, "narrow") == 0) {
      result = narrow;
      count = narrow_count;
    }
  }

  v8::Handle<v8::Array> symbols = v8::Array::New();
  for (int32_t i = 0; i < count; ++i) {
    symbols->Set(i, v8::String::New(
        reinterpret_cast<const uint16_t*>(result[i].getBuffer()),
        result[i].length()));
  }

  return handle_scope.Close(symbols);
}

// Throws a JavaScript exception.
static v8::Handle<v8::Value> ThrowUnexpectedObjectError() {
  // Returns undefined, and schedules an exception to be thrown.
  return v8::ThrowException(v8::Exception::Error(
      v8::String::New("DateTimeFormat method called on an object "
                      "that is not a DateTimeFormat.")));
}

// Returns icu date/time style.
static icu::DateFormat::EStyle GetDateTimeStyle(
    const icu::UnicodeString& type) {
  if (type == UNICODE_STRING_SIMPLE("medium")) {
    return icu::DateFormat::kMedium;
  } else if (type == UNICODE_STRING_SIMPLE("long")) {
    return icu::DateFormat::kLong;
  } else if (type == UNICODE_STRING_SIMPLE("full")) {
    return icu::DateFormat::kFull;
  }

  return icu::DateFormat::kShort;
}

} }  // namespace v8::internal

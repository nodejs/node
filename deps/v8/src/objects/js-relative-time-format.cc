// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-relative-time-format.h"

#include <map>
#include <memory>
#include <string>

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-relative-time-format-inl.h"
#include "unicode/numfmt.h"
#include "unicode/reldatefmt.h"
#include "unicode/uvernum.h"  // for U_ICU_VERSION_MAJOR_NUM

namespace v8 {
namespace internal {

namespace {
UDateRelativeDateTimeFormatterStyle getIcuStyle(
    JSRelativeTimeFormat::Style style) {
  switch (style) {
    case JSRelativeTimeFormat::Style::LONG:
      return UDAT_STYLE_LONG;
    case JSRelativeTimeFormat::Style::SHORT:
      return UDAT_STYLE_SHORT;
    case JSRelativeTimeFormat::Style::NARROW:
      return UDAT_STYLE_NARROW;
    case JSRelativeTimeFormat::Style::COUNT:
      UNREACHABLE();
  }
}
}  // namespace

JSRelativeTimeFormat::Style JSRelativeTimeFormat::getStyle(const char* str) {
  if (strcmp(str, "long") == 0) return JSRelativeTimeFormat::Style::LONG;
  if (strcmp(str, "short") == 0) return JSRelativeTimeFormat::Style::SHORT;
  if (strcmp(str, "narrow") == 0) return JSRelativeTimeFormat::Style::NARROW;
  UNREACHABLE();
}

JSRelativeTimeFormat::Numeric JSRelativeTimeFormat::getNumeric(
    const char* str) {
  if (strcmp(str, "auto") == 0) return JSRelativeTimeFormat::Numeric::AUTO;
  if (strcmp(str, "always") == 0) return JSRelativeTimeFormat::Numeric::ALWAYS;
  UNREACHABLE();
}

MaybeHandle<JSRelativeTimeFormat> JSRelativeTimeFormat::Initialize(
    Isolate* isolate, Handle<JSRelativeTimeFormat> relative_time_format_holder,
    Handle<Object> input_locales, Handle<Object> input_options) {
  Factory* factory = isolate->factory();
  relative_time_format_holder->set_flags(0);
  // 4. If options is undefined, then
  Handle<JSReceiver> options;
  if (input_options->IsUndefined(isolate)) {
    // a. Let options be ObjectCreate(null).
    options = isolate->factory()->NewJSObjectWithNullProto();
    // 5. Else
  } else {
    // a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSRelativeTimeFormat);
  }

  // 10. Let r be ResolveLocale(%RelativeTimeFormat%.[[AvailableLocales]],
  //                            requestedLocales, opt,
  //                            %RelativeTimeFormat%.[[RelevantExtensionKeys]],
  //                            localeData).
  Handle<JSObject> r;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, r,
                             Intl::ResolveLocale(isolate, "relativetimeformat",
                                                 input_locales, options),
                             JSRelativeTimeFormat);
  Handle<Object> locale_obj =
      JSObject::GetDataProperty(r, factory->locale_string());
  Handle<String> locale;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, locale,
                             Object::ToString(isolate, locale_obj),
                             JSRelativeTimeFormat);

  // 11. Let locale be r.[[Locale]].
  // 12. Set relativeTimeFormat.[[Locale]] to locale.
  relative_time_format_holder->set_locale(*locale);

  // 14. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  std::unique_ptr<char[]> style_str = nullptr;
  std::vector<const char*> style_values = {"long", "short", "narrow"};
  Maybe<bool> maybe_found_style =
      Intl::GetStringOption(isolate, options, "style", style_values,
                            "Intl.RelativeTimeFormat", &style_str);
  Style style_enum = Style::LONG;
  MAYBE_RETURN(maybe_found_style, MaybeHandle<JSRelativeTimeFormat>());
  if (maybe_found_style.FromJust()) {
    DCHECK_NOT_NULL(style_str.get());
    style_enum = getStyle(style_str.get());
  }

  // 15. Set relativeTimeFormat.[[Style]] to s.
  relative_time_format_holder->set_style(style_enum);

  // 16. Let numeric be ? GetOption(options, "numeric", "string",
  //                                «"always", "auto"», "always").
  std::unique_ptr<char[]> numeric_str = nullptr;
  std::vector<const char*> numeric_values = {"always", "auto"};
  Maybe<bool> maybe_found_numeric =
      Intl::GetStringOption(isolate, options, "numeric", numeric_values,
                            "Intl.RelativeTimeFormat", &numeric_str);
  Numeric numeric_enum = Numeric::ALWAYS;
  MAYBE_RETURN(maybe_found_numeric, MaybeHandle<JSRelativeTimeFormat>());
  if (maybe_found_numeric.FromJust()) {
    DCHECK_NOT_NULL(numeric_str.get());
    numeric_enum = getNumeric(numeric_str.get());
  }

  // 17. Set relativeTimeFormat.[[Numeric]] to numeric.
  relative_time_format_holder->set_numeric(numeric_enum);

  std::unique_ptr<char[]> locale_name = locale->ToCString();
  icu::Locale icu_locale(locale_name.get());
  UErrorCode status = U_ZERO_ERROR;

  // 25. Let relativeTimeFormat.[[NumberFormat]] be
  //     ? Construct(%NumberFormat%, « nfLocale, nfOptions »).
  icu::NumberFormat* number_format =
      icu::NumberFormat::createInstance(icu_locale, UNUM_DECIMAL, status);
  if (U_FAILURE(status)) {
    delete number_format;
    FATAL("Failed to create ICU number format, are ICU data files missing?");
  }
  CHECK_NOT_NULL(number_format);

  // Change UDISPCTX_CAPITALIZATION_NONE to other values if
  // ECMA402 later include option to change capitalization.
  // Ref: https://github.com/tc39/proposal-intl-relative-time/issues/11
  icu::RelativeDateTimeFormatter* icu_formatter =
      new icu::RelativeDateTimeFormatter(icu_locale, number_format,
                                         getIcuStyle(style_enum),
                                         UDISPCTX_CAPITALIZATION_NONE, status);
  if (U_FAILURE(status)) {
    delete icu_formatter;
    FATAL(
        "Failed to create ICU relative date time formatter, are ICU data files "
        "missing?");
  }
  CHECK_NOT_NULL(icu_formatter);

  Handle<Managed<icu::RelativeDateTimeFormatter>> managed_formatter =
      Managed<icu::RelativeDateTimeFormatter>::FromRawPtr(isolate, 0,
                                                          icu_formatter);

  // 30. Set relativeTimeFormat.[[InitializedRelativeTimeFormat]] to true.
  relative_time_format_holder->set_icu_formatter(*managed_formatter);
  // 31. Return relativeTimeFormat.
  return relative_time_format_holder;
}

Handle<JSObject> JSRelativeTimeFormat::ResolvedOptions(
    Isolate* isolate, Handle<JSRelativeTimeFormat> format_holder) {
  Factory* factory = isolate->factory();
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
  Handle<String> locale(format_holder->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->style_string(),
                        format_holder->StyleAsString(), NONE);
  JSObject::AddProperty(isolate, result, factory->numeric_string(),
                        format_holder->NumericAsString(), NONE);
  return result;
}

Handle<String> JSRelativeTimeFormat::StyleAsString() const {
  switch (style()) {
    case Style::LONG:
      return GetReadOnlyRoots().long_string_handle();
    case Style::SHORT:
      return GetReadOnlyRoots().short_string_handle();
    case Style::NARROW:
      return GetReadOnlyRoots().narrow_string_handle();
    case Style::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSRelativeTimeFormat::NumericAsString() const {
  switch (numeric()) {
    case Numeric::ALWAYS:
      return GetReadOnlyRoots().always_string_handle();
    case Numeric::AUTO:
      return GetReadOnlyRoots().auto_string_handle();
    case Numeric::COUNT:
      UNREACHABLE();
  }
}

namespace {

Handle<String> UnitAsString(Isolate* isolate, URelativeDateTimeUnit unit_enum) {
  Factory* factory = isolate->factory();
  switch (unit_enum) {
    case UDAT_REL_UNIT_SECOND:
      return factory->second_string();
    case UDAT_REL_UNIT_MINUTE:
      return factory->minute_string();
    case UDAT_REL_UNIT_HOUR:
      return factory->hour_string();
    case UDAT_REL_UNIT_DAY:
      return factory->day_string();
    case UDAT_REL_UNIT_WEEK:
      return factory->week_string();
    case UDAT_REL_UNIT_MONTH:
      return factory->month_string();
    case UDAT_REL_UNIT_QUARTER:
      return factory->quarter_string();
    case UDAT_REL_UNIT_YEAR:
      return factory->year_string();
    default:
      UNREACHABLE();
  }
}

MaybeHandle<JSArray> GenerateRelativeTimeFormatParts(
    Isolate* isolate, const icu::UnicodeString& formatted,
    const icu::UnicodeString& integer_part, URelativeDateTimeUnit unit_enum) {
  Factory* factory = isolate->factory();
  Handle<JSArray> array = factory->NewJSArray(0);
  int32_t found = formatted.indexOf(integer_part);

  Handle<String> substring;
  if (found < 0) {
    // Cannot find the integer_part in the formatted.
    // Return [{'type': 'literal', 'value': formatted}]
    ASSIGN_RETURN_ON_EXCEPTION(isolate, substring,
                               Intl::ToString(isolate, formatted), JSArray);
    Intl::AddElement(isolate, array,
                     0,                          // index
                     factory->literal_string(),  // field_type_string
                     substring);
  } else {
    // Found the formatted integer in the result.
    int index = 0;

    // array.push({
    //     'type': 'literal',
    //     'value': formatted.substring(0, found)})
    if (found > 0) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, substring,
                                 Intl::ToString(isolate, formatted, 0, found),
                                 JSArray);
      Intl::AddElement(isolate, array, index++,
                       factory->literal_string(),  // field_type_string
                       substring);
    }

    // array.push({
    //     'type': 'integer',
    //     'value': formatted.substring(found, found + integer_part.length),
    //     'unit': unit})
    ASSIGN_RETURN_ON_EXCEPTION(isolate, substring,
                               Intl::ToString(isolate, formatted, found,
                                              found + integer_part.length()),
                               JSArray);
    Handle<String> unit = UnitAsString(isolate, unit_enum);
    Intl::AddElement(isolate, array, index++,
                     factory->integer_string(),  // field_type_string
                     substring, factory->unit_string(), unit);

    // array.push({
    //     'type': 'literal',
    //     'value': formatted.substring(
    //         found + integer_part.length, formatted.length)})
    if (found + integer_part.length() < formatted.length()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, substring,
          Intl::ToString(isolate, formatted, found + integer_part.length(),
                         formatted.length()),
          JSArray);
      Intl::AddElement(isolate, array, index,
                       factory->literal_string(),  // field_type_string
                       substring);
    }
  }
  return array;
}

bool GetURelativeDateTimeUnit(Handle<String> unit,
                              URelativeDateTimeUnit* unit_enum) {
  std::unique_ptr<char[]> unit_str = unit->ToCString();
  if ((strcmp("second", unit_str.get()) == 0) ||
      (strcmp("seconds", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_SECOND;
  } else if ((strcmp("minute", unit_str.get()) == 0) ||
             (strcmp("minutes", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_MINUTE;
  } else if ((strcmp("hour", unit_str.get()) == 0) ||
             (strcmp("hours", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_HOUR;
  } else if ((strcmp("day", unit_str.get()) == 0) ||
             (strcmp("days", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_DAY;
  } else if ((strcmp("week", unit_str.get()) == 0) ||
             (strcmp("weeks", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_WEEK;
  } else if ((strcmp("month", unit_str.get()) == 0) ||
             (strcmp("months", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_MONTH;
  } else if ((strcmp("quarter", unit_str.get()) == 0) ||
             (strcmp("quarters", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_QUARTER;
  } else if ((strcmp("year", unit_str.get()) == 0) ||
             (strcmp("years", unit_str.get()) == 0)) {
    *unit_enum = UDAT_REL_UNIT_YEAR;
  } else {
    return false;
  }
  return true;
}

}  // namespace

MaybeHandle<Object> JSRelativeTimeFormat::Format(
    Isolate* isolate, Handle<Object> value_obj, Handle<Object> unit_obj,
    Handle<JSRelativeTimeFormat> format_holder, const char* func_name,
    bool to_parts) {
  Factory* factory = isolate->factory();

  // 3. Let value be ? ToNumber(value).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                             Object::ToNumber(isolate, value_obj), Object);
  double number = value->Number();
  // 4. Let unit be ? ToString(unit).
  Handle<String> unit;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, unit, Object::ToString(isolate, unit_obj),
                             Object);

  // 4. If isFinite(value) is false, then throw a RangeError exception.
  if (!std::isfinite(number)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kNotFiniteNumber,
                      isolate->factory()->NewStringFromAsciiChecked(func_name)),
        Object);
  }

  icu::RelativeDateTimeFormatter* formatter =
      format_holder->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  URelativeDateTimeUnit unit_enum;
  if (!GetURelativeDateTimeUnit(unit, &unit_enum)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kInvalidUnit,
                      isolate->factory()->NewStringFromAsciiChecked(func_name),
                      unit),
        Object);
  }

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;

#if USE_CHROMIUM_ICU != 1 && U_ICU_VERSION_MAJOR_NUM < 63
  if (unit_enum != UDAT_REL_UNIT_QUARTER) {  // ICU did not implement
                                             // UDAT_REL_UNIT_QUARTER < 63
#endif  // USE_CHROMIUM_ICU != 1 && U_ICU_VERSION_MAJOR_NUM < 63
    if (format_holder->numeric() == JSRelativeTimeFormat::Numeric::ALWAYS) {
      formatter->formatNumeric(number, unit_enum, formatted, status);
    } else {
      DCHECK_EQ(JSRelativeTimeFormat::Numeric::AUTO, format_holder->numeric());
      formatter->format(number, unit_enum, formatted, status);
    }
#if USE_CHROMIUM_ICU != 1 && U_ICU_VERSION_MAJOR_NUM < 63
  }
#endif  // USE_CHROMIUM_ICU != 1 && U_ICU_VERSION_MAJOR_NUM < 63

  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), Object);
  }

  if (to_parts) {
    icu::UnicodeString integer;
    icu::FieldPosition pos;
    formatter->getNumberFormat().format(std::abs(number), integer, pos, status);
    if (U_FAILURE(status)) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError),
                      Object);
    }

    Handle<JSArray> elements;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, elements,
        GenerateRelativeTimeFormatParts(isolate, formatted, integer, unit_enum),
        Object);
    return elements;
  }

  return factory->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(formatted.getBuffer()),
      formatted.length()));
}

}  // namespace internal
}  // namespace v8

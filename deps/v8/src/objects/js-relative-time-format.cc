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

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/objects-inl.h"
#include "unicode/numfmt.h"
#include "unicode/reldatefmt.h"

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
    Handle<Object> locales, Handle<Object> input_options) {
  relative_time_format_holder->set_flags(0);

  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSRelativeTimeFormat>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 2. If options is undefined, then
  Handle<JSReceiver> options;
  if (input_options->IsUndefined(isolate)) {
    // 2. a. Let options be ObjectCreate(null).
    options = isolate->factory()->NewJSObjectWithNullProto();
    // 3. Else
  } else {
    // 3. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSRelativeTimeFormat);
  }

  // 4. Let opt be a new Record.
  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  // 6. Set opt.[[localeMatcher]] to matcher.
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.RelativeTimeFormat");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSRelativeTimeFormat>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 7. Let _numberingSystem_ be ? GetOption(_options_, `"numberingSystem"`,
  //    `"string"`, *undefined*, *undefined*).
  std::unique_ptr<char[]> numbering_system_str = nullptr;
  Maybe<bool> maybe_numberingSystem = Intl::GetNumberingSystem(
      isolate, options, "Intl.RelativeTimeFormat", &numbering_system_str);
  // 8. If _numberingSystem_ is not *undefined*, then
  // a. If _numberingSystem_ does not match the
  //    `(3*8alphanum) *("-" (3*8alphanum))` sequence, throw a *RangeError*
  //     exception.
  MAYBE_RETURN(maybe_numberingSystem, MaybeHandle<JSRelativeTimeFormat>());

  // 9. Set _opt_.[[nu]] to _numberingSystem_.

  // 10. Let localeData be %RelativeTimeFormat%.[[LocaleData]].
  // 11. Let r be
  // ResolveLocale(%RelativeTimeFormat%.[[AvailableLocales]],
  //               requestedLocales, opt,
  //               %RelativeTimeFormat%.[[RelevantExtensionKeys]], localeData).
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSRelativeTimeFormat::GetAvailableLocales(),
                          requested_locales, matcher, {"nu"});

  // 12. Let locale be r.[[Locale]].
  // 13. Set relativeTimeFormat.[[Locale]] to locale.
  // 14. Let dataLocale be r.[[DataLocale]].
  icu::Locale icu_locale = r.icu_locale;
  UErrorCode status = U_ZERO_ERROR;
  if (numbering_system_str != nullptr) {
    icu_locale.setUnicodeKeywordValue("nu", numbering_system_str.get(), status);
    CHECK(U_SUCCESS(status));
  }

  Maybe<std::string> maybe_locale_str = Intl::ToLanguageTag(icu_locale);
  MAYBE_RETURN(maybe_locale_str, MaybeHandle<JSRelativeTimeFormat>());

  Handle<String> locale_str = isolate->factory()->NewStringFromAsciiChecked(
      maybe_locale_str.FromJust().c_str());
  relative_time_format_holder->set_locale(*locale_str);

  // 15. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  Maybe<Style> maybe_style = Intl::GetStringOption<Style>(
      isolate, options, "style", "Intl.RelativeTimeFormat",
      {"long", "short", "narrow"}, {Style::LONG, Style::SHORT, Style::NARROW},
      Style::LONG);
  MAYBE_RETURN(maybe_style, MaybeHandle<JSRelativeTimeFormat>());
  Style style_enum = maybe_style.FromJust();

  // 16. Set relativeTimeFormat.[[Style]] to s.
  relative_time_format_holder->set_style(style_enum);

  // 17. Let numeric be ? GetOption(options, "numeric", "string",
  //                                «"always", "auto"», "always").
  Maybe<Numeric> maybe_numeric = Intl::GetStringOption<Numeric>(
      isolate, options, "numeric", "Intl.RelativeTimeFormat",
      {"always", "auto"}, {Numeric::ALWAYS, Numeric::AUTO}, Numeric::ALWAYS);
  MAYBE_RETURN(maybe_numeric, MaybeHandle<JSRelativeTimeFormat>());
  Numeric numeric_enum = maybe_numeric.FromJust();

  // 18. Set relativeTimeFormat.[[Numeric]] to numeric.
  relative_time_format_holder->set_numeric(numeric_enum);

  // 19. Let relativeTimeFormat.[[NumberFormat]] be
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

  // 21. Set relativeTimeFormat.[[InitializedRelativeTimeFormat]] to true.
  relative_time_format_holder->set_icu_formatter(*managed_formatter);

  // 22. Return relativeTimeFormat.
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
  std::string locale_str(format_holder->locale().ToCString().get());
  icu::Locale icu_locale = Intl::CreateICULocale(locale_str);
  std::string numbering_system = Intl::GetNumberingSystem(icu_locale);
  JSObject::AddProperty(
      isolate, result, factory->numberingSystem_string(),
      factory->NewStringFromAsciiChecked(numbering_system.c_str()), NONE);
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

template <typename T>
MaybeHandle<T> FormatCommon(
    Isolate* isolate, Handle<JSRelativeTimeFormat> format,
    Handle<Object> value_obj, Handle<Object> unit_obj, const char* func_name,
    MaybeHandle<T> (*formatToResult)(Isolate*,
                                     const icu::FormattedRelativeDateTime&,
                                     Handle<Object>, Handle<String>)) {
  // 3. Let value be ? ToNumber(value).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                             Object::ToNumber(isolate, value_obj), T);
  double number = value->Number();
  // 4. Let unit be ? ToString(unit).
  Handle<String> unit;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, unit, Object::ToString(isolate, unit_obj),
                             T);
  // 4. If isFinite(value) is false, then throw a RangeError exception.
  if (!std::isfinite(number)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kNotFiniteNumber,
                      isolate->factory()->NewStringFromAsciiChecked(func_name)),
        T);
  }
  icu::RelativeDateTimeFormatter* formatter = format->icu_formatter().raw();
  CHECK_NOT_NULL(formatter);
  URelativeDateTimeUnit unit_enum;
  if (!GetURelativeDateTimeUnit(unit, &unit_enum)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kInvalidUnit,
                      isolate->factory()->NewStringFromAsciiChecked(func_name),
                      unit),
        T);
  }
  UErrorCode status = U_ZERO_ERROR;
  icu::FormattedRelativeDateTime formatted =
      (format->numeric() == JSRelativeTimeFormat::Numeric::ALWAYS)
          ? formatter->formatNumericToValue(number, unit_enum, status)
          : formatter->formatToValue(number, unit_enum, status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), T);
  }
  return formatToResult(isolate, formatted, value,
                        UnitAsString(isolate, unit_enum));
}

MaybeHandle<String> FormatToString(
    Isolate* isolate, const icu::FormattedRelativeDateTime& formatted,
    Handle<Object> value, Handle<String> unit) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString result = formatted.toString(status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), String);
  }
  return Intl::ToString(isolate, result);
}

Maybe<bool> AddLiteral(Isolate* isolate, Handle<JSArray> array,
                       const icu::UnicodeString& string, int32_t index,
                       int32_t start, int32_t limit) {
  Handle<String> substring;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, substring, Intl::ToString(isolate, string, start, limit),
      Nothing<bool>());
  Intl::AddElement(isolate, array, index, isolate->factory()->literal_string(),
                   substring);
  return Just(true);
}

Maybe<bool> AddUnit(Isolate* isolate, Handle<JSArray> array,
                    const icu::UnicodeString& string, int32_t index,
                    int32_t start, int32_t limit, int32_t field_id,
                    Handle<Object> value, Handle<String> unit) {
  Handle<String> substring;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, substring, Intl::ToString(isolate, string, start, limit),
      Nothing<bool>());
  Intl::AddElement(isolate, array, index,
                   Intl::NumberFieldToType(isolate, value, field_id), substring,
                   isolate->factory()->unit_string(), unit);
  return Just(true);
}

MaybeHandle<JSArray> FormatToJSArray(
    Isolate* isolate, const icu::FormattedRelativeDateTime& formatted,
    Handle<Object> value, Handle<String> unit) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString string = formatted.toString(status);

  Factory* factory = isolate->factory();
  Handle<JSArray> array = factory->NewJSArray(0);
  icu::ConstrainedFieldPosition cfpos;
  cfpos.constrainCategory(UFIELD_CATEGORY_NUMBER);
  int32_t index = 0;

  int32_t previous_end = 0;
  Handle<String> substring;
  std::vector<std::pair<int32_t, int32_t>> groups;
  while (formatted.nextPosition(cfpos, status) && U_SUCCESS(status)) {
    int32_t category = cfpos.getCategory();
    int32_t field = cfpos.getField();
    int32_t start = cfpos.getStart();
    int32_t limit = cfpos.getLimit();
    if (category == UFIELD_CATEGORY_NUMBER) {
      if (field == UNUM_GROUPING_SEPARATOR_FIELD) {
        groups.push_back(std::pair<int32_t, int32_t>(start, limit));
        continue;
      }
      if (start > previous_end) {
        Maybe<bool> maybe_added =
            AddLiteral(isolate, array, string, index++, previous_end, start);
        MAYBE_RETURN(maybe_added, Handle<JSArray>());
      }
      if (field == UNUM_INTEGER_FIELD) {
        for (auto start_limit : groups) {
          if (start_limit.first > start) {
            Maybe<bool> maybe_added =
                AddUnit(isolate, array, string, index++, start,
                        start_limit.first, field, value, unit);
            MAYBE_RETURN(maybe_added, Handle<JSArray>());
            maybe_added = AddUnit(isolate, array, string, index++,
                                  start_limit.first, start_limit.second,
                                  UNUM_GROUPING_SEPARATOR_FIELD, value, unit);
            MAYBE_RETURN(maybe_added, Handle<JSArray>());
            start = start_limit.second;
          }
        }
      }
      Maybe<bool> maybe_added = AddUnit(isolate, array, string, index++, start,
                                        limit, field, value, unit);
      MAYBE_RETURN(maybe_added, Handle<JSArray>());
      previous_end = limit;
    }
  }
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), JSArray);
  }
  if (string.length() > previous_end) {
    Maybe<bool> maybe_added = AddLiteral(isolate, array, string, index,
                                         previous_end, string.length());
    MAYBE_RETURN(maybe_added, Handle<JSArray>());
  }

  JSObject::ValidateElements(*array);
  return array;
}

}  // namespace

MaybeHandle<String> JSRelativeTimeFormat::Format(
    Isolate* isolate, Handle<Object> value_obj, Handle<Object> unit_obj,
    Handle<JSRelativeTimeFormat> format) {
  return FormatCommon<String>(isolate, format, value_obj, unit_obj,
                              "Intl.RelativeTimeFormat.prototype.format",
                              FormatToString);
}

MaybeHandle<JSArray> JSRelativeTimeFormat::FormatToParts(
    Isolate* isolate, Handle<Object> value_obj, Handle<Object> unit_obj,
    Handle<JSRelativeTimeFormat> format) {
  return FormatCommon<JSArray>(
      isolate, format, value_obj, unit_obj,
      "Intl.RelativeTimeFormat.prototype.formatToParts", FormatToJSArray);
}

const std::set<std::string>& JSRelativeTimeFormat::GetAvailableLocales() {
  // Since RelativeTimeFormatter does not have a method to list all
  // available locales, work around by calling the DateFormat.
  return Intl::GetAvailableLocalesForDateFormat();
}

}  // namespace internal
}  // namespace v8

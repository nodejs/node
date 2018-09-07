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

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

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

JSRelativeTimeFormat::Style getStyle(const char* str) {
  if (strcmp(str, "long") == 0) return JSRelativeTimeFormat::Style::LONG;
  if (strcmp(str, "short") == 0) return JSRelativeTimeFormat::Style::SHORT;
  if (strcmp(str, "narrow") == 0) return JSRelativeTimeFormat::Style::NARROW;
  UNREACHABLE();
}

JSRelativeTimeFormat::Numeric getNumeric(const char* str) {
  if (strcmp(str, "auto") == 0) return JSRelativeTimeFormat::Numeric::AUTO;
  if (strcmp(str, "always") == 0) return JSRelativeTimeFormat::Numeric::ALWAYS;
  UNREACHABLE();
}

}  // namespace

MaybeHandle<JSRelativeTimeFormat>
JSRelativeTimeFormat::InitializeRelativeTimeFormat(
    Isolate* isolate, Handle<JSRelativeTimeFormat> relative_time_format_holder,
    Handle<Object> input_locales, Handle<Object> input_options) {
  Factory* factory = isolate->factory();

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
  if (U_FAILURE(status) || number_format == nullptr) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kRelativeDateTimeFormatterBadParameters),
        JSRelativeTimeFormat);
  }
  // 23. Perform ! CreateDataPropertyOrThrow(nfOptions, "useGrouping", false).
  number_format->setGroupingUsed(false);

  // 24. Perform ! CreateDataPropertyOrThrow(nfOptions,
  //                                         "minimumIntegerDigits", 2).
  // Ref: https://github.com/tc39/proposal-intl-relative-time/issues/80
  number_format->setMinimumIntegerDigits(2);

  // Change UDISPCTX_CAPITALIZATION_NONE to other values if
  // ECMA402 later include option to change capitalization.
  // Ref: https://github.com/tc39/proposal-intl-relative-time/issues/11
  icu::RelativeDateTimeFormatter* icu_formatter =
      new icu::RelativeDateTimeFormatter(icu_locale, number_format,
                                         getIcuStyle(style_enum),
                                         UDISPCTX_CAPITALIZATION_NONE, status);

  if (U_FAILURE(status) || (icu_formatter == nullptr)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kRelativeDateTimeFormatterBadParameters),
        JSRelativeTimeFormat);
  }
  Handle<Managed<icu::RelativeDateTimeFormatter>> managed_formatter =
      Managed<icu::RelativeDateTimeFormatter>::FromRawPtr(isolate, 0,
                                                          icu_formatter);

  // 30. Set relativeTimeFormat.[[InitializedRelativeTimeFormat]] to true.
  relative_time_format_holder->set_formatter(*managed_formatter);
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

icu::RelativeDateTimeFormatter* JSRelativeTimeFormat::UnpackFormatter(
    Isolate* isolate, Handle<JSRelativeTimeFormat> holder) {
  return Managed<icu::RelativeDateTimeFormatter>::cast(holder->formatter())
      ->raw();
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

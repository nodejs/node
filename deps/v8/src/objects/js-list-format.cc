// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-list-format.h"

#include <memory>
#include <vector>

#include "src/elements.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/managed.h"
#include "unicode/listformatter.h"

namespace v8 {
namespace internal {

namespace {
const char* kStandard = "standard";
const char* kOr = "or";
const char* kUnit = "unit";
const char* kStandardShort = "standard-short";
const char* kUnitShort = "unit-short";
const char* kUnitNarrow = "unit-narrow";

const char* GetIcuStyleString(JSListFormat::Style style,
                              JSListFormat::Type type) {
  switch (type) {
    case JSListFormat::Type::CONJUNCTION:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kStandard;
        case JSListFormat::Style::SHORT:
          return kStandardShort;
        // NARROW is now not allowed if type is not unit
        // It is impossible to reach because we've already thrown a RangeError
        // when style is "narrow" and type is not "unit".
        case JSListFormat::Style::NARROW:
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::DISJUNCTION:
      switch (style) {
        // Currently, ListFormat::createInstance on "or-short"
        // will fail so we use "or" here.
        // See https://unicode.org/cldr/trac/ticket/11254
        // TODO(ftang): change to return kOr or kOrShort depend on
        // style after the above issue fixed in CLDR/ICU.
        // CLDR bug: https://unicode.org/cldr/trac/ticket/11254
        // ICU bug: https://unicode-org.atlassian.net/browse/ICU-20014
        case JSListFormat::Style::LONG:
        case JSListFormat::Style::SHORT:
          return kOr;
        // NARROW is now not allowed if type is not unit
        // It is impossible to reach because we've already thrown a RangeError
        // when style is "narrow" and type is not "unit".
        case JSListFormat::Style::NARROW:
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::UNIT:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kUnit;
        case JSListFormat::Style::SHORT:
          return kUnitShort;
        case JSListFormat::Style::NARROW:
          return kUnitNarrow;
        case JSListFormat::Style::COUNT:
          UNREACHABLE();
      }
    case JSListFormat::Type::COUNT:
      UNREACHABLE();
  }
}

}  // namespace

JSListFormat::Style get_style(const char* str) {
  switch (str[0]) {
    case 'n':
      if (strcmp(&str[1], "arrow") == 0) return JSListFormat::Style::NARROW;
      break;
    case 'l':
      if (strcmp(&str[1], "ong") == 0) return JSListFormat::Style::LONG;
      break;
    case 's':
      if (strcmp(&str[1], "hort") == 0) return JSListFormat::Style::SHORT;
      break;
  }
  UNREACHABLE();
}

JSListFormat::Type get_type(const char* str) {
  switch (str[0]) {
    case 'c':
      if (strcmp(&str[1], "onjunction") == 0)
        return JSListFormat::Type::CONJUNCTION;
      break;
    case 'd':
      if (strcmp(&str[1], "isjunction") == 0)
        return JSListFormat::Type::DISJUNCTION;
      break;
    case 'u':
      if (strcmp(&str[1], "nit") == 0) return JSListFormat::Type::UNIT;
      break;
  }
  UNREACHABLE();
}

MaybeHandle<JSListFormat> JSListFormat::Initialize(
    Isolate* isolate, Handle<JSListFormat> list_format_holder,
    Handle<Object> locales, Handle<Object> input_options) {
  list_format_holder->set_flags(0);

  Handle<JSReceiver> options;
  // 3. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Maybe<std::vector<std::string>> maybe_requested_locales =
      Intl::CanonicalizeLocaleList(isolate, locales);
  MAYBE_RETURN(maybe_requested_locales, Handle<JSListFormat>());
  std::vector<std::string> requested_locales =
      maybe_requested_locales.FromJust();

  // 4. If options is undefined, then
  if (input_options->IsUndefined(isolate)) {
    // 4. a. Let options be ObjectCreate(null).
    options = isolate->factory()->NewJSObjectWithNullProto();
    // 5. Else
  } else {
    // 5. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                               Object::ToObject(isolate, input_options),
                               JSListFormat);
  }

  // Note: No need to create a record. It's not observable.
  // 6. Let opt be a new Record.

  // 7. Let t be GetOption(options, "type", "string", «"conjunction",
  //    "disjunction", "unit"», "conjunction").
  std::unique_ptr<char[]> type_str = nullptr;
  std::vector<const char*> type_values = {"conjunction", "disjunction", "unit"};
  Maybe<bool> maybe_found_type = Intl::GetStringOption(
      isolate, options, "type", type_values, "Intl.ListFormat", &type_str);
  Type type_enum = Type::CONJUNCTION;
  MAYBE_RETURN(maybe_found_type, MaybeHandle<JSListFormat>());
  if (maybe_found_type.FromJust()) {
    DCHECK_NOT_NULL(type_str.get());
    type_enum = get_type(type_str.get());
  }

  // 8. Set listFormat.[[Type]] to t.
  list_format_holder->set_type(type_enum);

  // 9. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  std::unique_ptr<char[]> style_str = nullptr;
  std::vector<const char*> style_values = {"long", "short", "narrow"};
  Maybe<bool> maybe_found_style = Intl::GetStringOption(
      isolate, options, "style", style_values, "Intl.ListFormat", &style_str);
  Style style_enum = Style::LONG;
  MAYBE_RETURN(maybe_found_style, MaybeHandle<JSListFormat>());
  if (maybe_found_style.FromJust()) {
    DCHECK_NOT_NULL(style_str.get());
    style_enum = get_style(style_str.get());
  }
  // 10. Set listFormat.[[Style]] to s.
  list_format_holder->set_style(style_enum);

  // 12. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.ListFormat");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSListFormat>());
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 14. If style is "narrow" and type is not "unit", throw a RangeError
  // exception.
  if (style_enum == Style::NARROW && type_enum != Type::UNIT) {
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kIllegalTypeWhileStyleNarrow),
        JSListFormat);
  }

  // 15. Let r be ResolveLocale(%ListFormat%.[[AvailableLocales]],
  // requestedLocales, opt, undefined, localeData).
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSListFormat::GetAvailableLocales(),
                          requested_locales, matcher, {});

  // 24. Set listFormat.[[Locale]] to r.[[Locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  list_format_holder->set_locale(*locale_str);

  icu::Locale icu_locale = r.icu_locale;
  UErrorCode status = U_ZERO_ERROR;
  icu::ListFormatter* formatter = icu::ListFormatter::createInstance(
      icu_locale, GetIcuStyleString(style_enum, type_enum), status);
  if (U_FAILURE(status)) {
    delete formatter;
    FATAL("Failed to create ICU list formatter, are ICU data files missing?");
  }
  CHECK_NOT_NULL(formatter);

  Handle<Managed<icu::ListFormatter>> managed_formatter =
      Managed<icu::ListFormatter>::FromRawPtr(isolate, 0, formatter);

  list_format_holder->set_icu_formatter(*managed_formatter);
  return list_format_holder;
}

// ecma402 #sec-intl.pluralrules.prototype.resolvedoptions
Handle<JSObject> JSListFormat::ResolvedOptions(
    Isolate* isolate, Handle<JSListFormat> format_holder) {
  Factory* factory = isolate->factory();
  // 4. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 5.  For each row of Table 1, except the header row, do
  //  Table 1: Resolved Options of ListFormat Instances
  //  Internal Slot    Property
  //  [[Locale]]       "locale"
  //  [[Type]]         "type"
  //  [[Style]]        "style"
  Handle<String> locale(format_holder->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->type_string(),
                        format_holder->TypeAsString(), NONE);
  JSObject::AddProperty(isolate, result, factory->style_string(),
                        format_holder->StyleAsString(), NONE);
  // 6. Return options.
  return result;
}

Handle<String> JSListFormat::StyleAsString() const {
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

Handle<String> JSListFormat::TypeAsString() const {
  switch (type()) {
    case Type::CONJUNCTION:
      return GetReadOnlyRoots().conjunction_string_handle();
    case Type::DISJUNCTION:
      return GetReadOnlyRoots().disjunction_string_handle();
    case Type::UNIT:
      return GetReadOnlyRoots().unit_string_handle();
    case Type::COUNT:
      UNREACHABLE();
  }
}

namespace {

// TODO(ftang) remove the following hack after icu::ListFormat support
// FieldPosition.
// This is a temporary workaround until icu::ListFormat support FieldPosition
// It is inefficient and won't work correctly on the edge case that the input
// contains fraction of the list pattern.
// For example the following under English will mark the "an" incorrectly
// since the formatted is "a, b, and an".
// listFormat.formatToParts(["a", "b", "an"])
// https://ssl.icu-project.org/trac/ticket/13754
MaybeHandle<JSArray> GenerateListFormatParts(
    Isolate* isolate, const icu::UnicodeString& formatted,
    const icu::UnicodeString items[], int length) {
  Factory* factory = isolate->factory();
  int estimate_size = length * 2 + 1;
  Handle<JSArray> array = factory->NewJSArray(estimate_size);
  int index = 0;
  int last_pos = 0;
  for (int i = 0; i < length; i++) {
    int found = formatted.indexOf(items[i], last_pos);
    DCHECK_GE(found, 0);
    if (found > last_pos) {
      Handle<String> substring;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, substring,
          Intl::ToString(isolate, formatted, last_pos, found), JSArray);
      Intl::AddElement(isolate, array, index++, factory->literal_string(),
                       substring);
    }
    last_pos = found + items[i].length();
    Handle<String> substring;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring, Intl::ToString(isolate, formatted, found, last_pos),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->element_string(),
                     substring);
  }
  if (last_pos < formatted.length()) {
    Handle<String> substring;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, formatted, last_pos, formatted.length()),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->literal_string(),
                     substring);
  }
  return array;
}

// Extract String from JSArray into array of UnicodeString
Maybe<bool> ToUnicodeStringArray(Isolate* isolate, Handle<JSArray> array,
                                 icu::UnicodeString items[], uint32_t length) {
  Factory* factory = isolate->factory();
  // In general, ElementsAccessor::Get actually isn't guaranteed to give us the
  // elements in order. But given that it was created by a builtin we control,
  // it shouldn't be possible for it to be problematic. Add DCHECK to ensure
  // that.
  DCHECK(array->HasFastPackedElements());
  auto* accessor = array->GetElementsAccessor();
  DCHECK(length == accessor->NumberOfElements(*array));
  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  //
  // Per spec it looks like we're supposed to throw a TypeError exception if the
  // item isn't already a string, rather than coercing to a string. Moreover,
  // the way the spec's written it looks like we're supposed to run through the
  // whole list to check that they're all strings before going further.
  for (uint32_t i = 0; i < length; i++) {
    Handle<Object> item = accessor->Get(array, i);
    DCHECK(!item.is_null());
    if (!item->IsString()) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewTypeError(MessageTemplate::kArrayItemNotType,
                       factory->NewStringFromStaticChars("list"),
                       factory->NewNumber(i),
                       factory->NewStringFromStaticChars("String")),
          Nothing<bool>());
    }
  }
  for (uint32_t i = 0; i < length; i++) {
    Handle<String> string = Handle<String>::cast(accessor->Get(array, i));
    items[i] = Intl::ToICUUnicodeString(isolate, string);
  }
  return Just(true);
}

}  // namespace

Maybe<bool> FormatListCommon(Isolate* isolate,
                             Handle<JSListFormat> format_holder,
                             Handle<JSArray> list,
                             icu::UnicodeString& formatted, uint32_t* length,
                             std::unique_ptr<icu::UnicodeString[]>& array) {
  DCHECK(!list->IsUndefined());

  icu::ListFormatter* formatter = format_holder->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  *length = list->GetElementsAccessor()->NumberOfElements(*list);
  array.reset(new icu::UnicodeString[*length]);

  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  MAYBE_RETURN(ToUnicodeStringArray(isolate, list, array.get(), *length),
               Nothing<bool>());

  UErrorCode status = U_ZERO_ERROR;
  formatter->format(array.get(), *length, formatted, status);
  DCHECK(U_SUCCESS(status));
  return Just(true);
}

// ecma402 #sec-formatlist
MaybeHandle<String> JSListFormat::FormatList(Isolate* isolate,
                                             Handle<JSListFormat> format_holder,
                                             Handle<JSArray> list) {
  icu::UnicodeString formatted;
  uint32_t length;
  std::unique_ptr<icu::UnicodeString[]> array;
  MAYBE_RETURN(
      FormatListCommon(isolate, format_holder, list, formatted, &length, array),
      Handle<String>());
  return Intl::ToString(isolate, formatted);
}

// ecma42 #sec-formatlisttoparts
MaybeHandle<JSArray> JSListFormat::FormatListToParts(
    Isolate* isolate, Handle<JSListFormat> format_holder,
    Handle<JSArray> list) {
  icu::UnicodeString formatted;
  uint32_t length;
  std::unique_ptr<icu::UnicodeString[]> array;
  MAYBE_RETURN(
      FormatListCommon(isolate, format_holder, list, formatted, &length, array),
      Handle<JSArray>());
  return GenerateListFormatParts(isolate, formatted, array.get(), length);
}

std::set<std::string> JSListFormat::GetAvailableLocales() {
  int32_t num_locales = 0;
  // TODO(ftang): for now just use
  // icu::Locale::getAvailableLocales(count) until we migrate to
  // Intl::GetAvailableLocales().
  // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20015
  const icu::Locale* icu_available_locales =
      icu::Locale::getAvailableLocales(num_locales);
  return Intl::BuildLocaleSet(icu_available_locales, num_locales);
}

}  // namespace internal
}  // namespace v8

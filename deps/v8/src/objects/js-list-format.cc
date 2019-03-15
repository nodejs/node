// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-list-format.h"

#include <memory>
#include <vector>

#include "src/elements-inl.h"
#include "src/elements.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/managed.h"
#include "unicode/fieldpos.h"
#include "unicode/fpositer.h"
#include "unicode/listformatter.h"
#include "unicode/ulistformatter.h"

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
    Isolate* isolate, Handle<JSListFormat> list_format, Handle<Object> locales,
    Handle<Object> input_options) {
  list_format->set_flags(0);

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

  // 7. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  Maybe<Intl::MatcherOption> maybe_locale_matcher =
      Intl::GetLocaleMatcher(isolate, options, "Intl.ListFormat");
  MAYBE_RETURN(maybe_locale_matcher, MaybeHandle<JSListFormat>());

  // 8. Set opt.[[localeMatcher]] to matcher.
  Intl::MatcherOption matcher = maybe_locale_matcher.FromJust();

  // 10. Let r be ResolveLocale(%ListFormat%.[[AvailableLocales]],
  // requestedLocales, opt, undefined, localeData).
  Intl::ResolvedLocale r =
      Intl::ResolveLocale(isolate, JSListFormat::GetAvailableLocales(),
                          requested_locales, matcher, {});

  // 11. Set listFormat.[[Locale]] to r.[[Locale]].
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());
  list_format->set_locale(*locale_str);

  // 12. Let t be GetOption(options, "type", "string", «"conjunction",
  //    "disjunction", "unit"», "conjunction").
  Maybe<Type> maybe_type = Intl::GetStringOption<Type>(
      isolate, options, "type", "Intl.ListFormat",
      {"conjunction", "disjunction", "unit"},
      {Type::CONJUNCTION, Type::DISJUNCTION, Type::UNIT}, Type::CONJUNCTION);
  MAYBE_RETURN(maybe_type, MaybeHandle<JSListFormat>());
  Type type_enum = maybe_type.FromJust();

  // 13. Set listFormat.[[Type]] to t.
  list_format->set_type(type_enum);

  // NOTE: Keep the old way of GetOptions on style for now. I discover a
  // disadvantage of following the lastest spec and propose to rollback that
  // part in https://github.com/tc39/proposal-intl-list-format/pull/40

  // Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  Maybe<Style> maybe_style = Intl::GetStringOption<Style>(
      isolate, options, "style", "Intl.ListFormat", {"long", "short", "narrow"},
      {Style::LONG, Style::SHORT, Style::NARROW}, Style::LONG);
  MAYBE_RETURN(maybe_style, MaybeHandle<JSListFormat>());
  Style style_enum = maybe_style.FromJust();

  // If _style_ is `"narrow"` and _type_ is not `"unit"`, throw a *RangeError*
  // exception.
  if (style_enum == Style::NARROW && type_enum != Type::UNIT) {
    THROW_NEW_ERROR(
        isolate, NewRangeError(MessageTemplate::kIllegalTypeWhileStyleNarrow),
        JSListFormat);
  }

  // 17. Set listFormat.[[Style]] to s.
  list_format->set_style(style_enum);

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

  list_format->set_icu_formatter(*managed_formatter);
  return list_format;
}

// ecma402 #sec-intl.pluralrules.prototype.resolvedoptions
Handle<JSObject> JSListFormat::ResolvedOptions(Isolate* isolate,
                                               Handle<JSListFormat> format) {
  Factory* factory = isolate->factory();
  // 4. Let options be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 5.  For each row of Table 1, except the header row, do
  //  Table 1: Resolved Options of ListFormat Instances
  //  Internal Slot    Property
  //  [[Locale]]       "locale"
  //  [[Type]]         "type"
  //  [[Style]]        "style"
  Handle<String> locale(format->locale(), isolate);
  JSObject::AddProperty(isolate, result, factory->locale_string(), locale,
                        NONE);
  JSObject::AddProperty(isolate, result, factory->type_string(),
                        format->TypeAsString(), NONE);
  JSObject::AddProperty(isolate, result, factory->style_string(),
                        format->StyleAsString(), NONE);
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

MaybeHandle<JSArray> GenerateListFormatParts(
    Isolate* isolate, const icu::UnicodeString& formatted,
    const std::vector<icu::FieldPosition>& positions) {
  Factory* factory = isolate->factory();
  Handle<JSArray> array =
      factory->NewJSArray(static_cast<int>(positions.size()));
  int index = 0;
  int prev_item_end_index = 0;
  Handle<String> substring;
  for (const icu::FieldPosition pos : positions) {
    CHECK(pos.getBeginIndex() >= prev_item_end_index);
    CHECK(pos.getField() == ULISTFMT_ELEMENT_FIELD);
    if (pos.getBeginIndex() != prev_item_end_index) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, substring,
          Intl::ToString(isolate, formatted, prev_item_end_index,
                         pos.getBeginIndex()),
          JSArray);
      Intl::AddElement(isolate, array, index++, factory->literal_string(),
                       substring);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, formatted, pos.getBeginIndex(),
                       pos.getEndIndex()),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->element_string(),
                     substring);
    prev_item_end_index = pos.getEndIndex();
  }
  if (prev_item_end_index != formatted.length()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, formatted, prev_item_end_index,
                       formatted.length()),
        JSArray);
    Intl::AddElement(isolate, array, index++, factory->literal_string(),
                     substring);
  }
  return array;
}

// Get all the FieldPosition into a vector from FieldPositionIterator and return
// them in output order.
std::vector<icu::FieldPosition> GenerateFieldPosition(
    icu::FieldPositionIterator iter) {
  std::vector<icu::FieldPosition> positions;
  icu::FieldPosition pos;
  while (iter.next(pos)) {
    // Only take the information of the ULISTFMT_ELEMENT_FIELD field.
    if (pos.getField() == ULISTFMT_ELEMENT_FIELD) {
      positions.push_back(pos);
    }
  }
  // Because the format may reoder the items, ICU FieldPositionIterator
  // keep the order for FieldPosition based on the order of the input items.
  // But the formatToParts API in ECMA402 expects in formatted output order.
  // Therefore we have to sort based on beginIndex of the FieldPosition.
  // Example of such is in the "ur" (Urdu) locale with type: "unit", where the
  // main text flows from right to left, the formatted list of unit should flow
  // from left to right and therefore in the memory the formatted result will
  // put the first item on the last in the result string according the current
  // CLDR patterns.
  // See 'listPattern' pattern in
  // third_party/icu/source/data/locales/ur_IN.txt
  std::sort(positions.begin(), positions.end(),
            [](icu::FieldPosition a, icu::FieldPosition b) {
              return a.getBeginIndex() < b.getBeginIndex();
            });
  return positions;
}

// Extract String from JSArray into array of UnicodeString
Maybe<std::vector<icu::UnicodeString>> ToUnicodeStringArray(
    Isolate* isolate, Handle<JSArray> array) {
  Factory* factory = isolate->factory();
  // In general, ElementsAccessor::Get actually isn't guaranteed to give us the
  // elements in order. But if it is a holey array, it will cause the exception
  // with the IsString check.
  auto* accessor = array->GetElementsAccessor();
  uint32_t length = accessor->NumberOfElements(*array);

  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  //
  // Per spec it looks like we're supposed to throw a TypeError exception if the
  // item isn't already a string, rather than coercing to a string.
  std::vector<icu::UnicodeString> result;
  for (uint32_t i = 0; i < length; i++) {
    DCHECK(accessor->HasElement(*array, i));
    Handle<Object> item = accessor->Get(array, i);
    DCHECK(!item.is_null());
    if (!item->IsString()) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewTypeError(MessageTemplate::kArrayItemNotType,
                       factory->list_string(),
                       // TODO(ftang): For dictionary-mode arrays, i isn't
                       // actually the index in the array but the index in the
                       // dictionary.
                       factory->NewNumber(i), factory->String_string()),
          Nothing<std::vector<icu::UnicodeString>>());
    }
    result.push_back(
        Intl::ToICUUnicodeString(isolate, Handle<String>::cast(item)));
  }
  DCHECK(!array->HasDictionaryElements());
  return Just(result);
}

}  // namespace

// ecma402 #sec-formatlist
MaybeHandle<String> JSListFormat::FormatList(Isolate* isolate,
                                             Handle<JSListFormat> format,
                                             Handle<JSArray> list) {
  DCHECK(!list->IsUndefined());
  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  Maybe<std::vector<icu::UnicodeString>> maybe_array =
      ToUnicodeStringArray(isolate, list);
  MAYBE_RETURN(maybe_array, Handle<String>());
  std::vector<icu::UnicodeString> array = maybe_array.FromJust();

  icu::ListFormatter* formatter = format->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  formatter->format(array.data(), static_cast<int32_t>(array.size()), formatted,
                    status);
  DCHECK(U_SUCCESS(status));

  return Intl::ToString(isolate, formatted);
}

const std::set<std::string>& JSListFormat::GetAvailableLocales() {
  // Since ListFormatter does not have a method to list all supported
  // locales, use the one in icu::Locale per comments in
  // ICU FR at https://unicode-org.atlassian.net/browse/ICU-20015
  return Intl::GetAvailableLocalesForLocale();
}

// ecma42 #sec-formatlisttoparts
MaybeHandle<JSArray> JSListFormat::FormatListToParts(
    Isolate* isolate, Handle<JSListFormat> format, Handle<JSArray> list) {
  DCHECK(!list->IsUndefined());
  // ecma402 #sec-createpartsfromlist
  // 2. If list contains any element value such that Type(value) is not String,
  // throw a TypeError exception.
  Maybe<std::vector<icu::UnicodeString>> maybe_array =
      ToUnicodeStringArray(isolate, list);
  MAYBE_RETURN(maybe_array, Handle<JSArray>());
  std::vector<icu::UnicodeString> array = maybe_array.FromJust();

  icu::ListFormatter* formatter = format->icu_formatter()->raw();
  CHECK_NOT_NULL(formatter);

  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  icu::FieldPositionIterator iter;
  formatter->format(array.data(), static_cast<int32_t>(array.size()), formatted,
                    &iter, status);
  DCHECK(U_SUCCESS(status));

  std::vector<icu::FieldPosition> field_positions = GenerateFieldPosition(iter);
  return GenerateListFormatParts(isolate, formatted, field_positions);
}
}  // namespace internal
}  // namespace v8

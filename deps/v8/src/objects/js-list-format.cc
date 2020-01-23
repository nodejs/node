// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-list-format.h"

#include <memory>
#include <vector>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/elements-inl.h"
#include "src/objects/elements.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/managed.h"
#include "src/objects/objects-inl.h"
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
const char* kOrShort = "or-short";
const char* kUnitShort = "unit-short";
const char* kStandardNarrow = "standard-narrow";
const char* kOrNarrow = "or-narrow";
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
        case JSListFormat::Style::NARROW:
          return kStandardNarrow;
      }
    case JSListFormat::Type::DISJUNCTION:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kOr;
        case JSListFormat::Style::SHORT:
          return kOrShort;
        case JSListFormat::Style::NARROW:
          return kOrNarrow;
      }
    case JSListFormat::Type::UNIT:
      switch (style) {
        case JSListFormat::Style::LONG:
          return kUnit;
        case JSListFormat::Style::SHORT:
          return kUnitShort;
        case JSListFormat::Style::NARROW:
          return kUnitNarrow;
      }
  }
  UNREACHABLE();
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

MaybeHandle<JSListFormat> JSListFormat::New(Isolate* isolate, Handle<Map> map,
                                            Handle<Object> locales,
                                            Handle<Object> input_options) {
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
  Handle<String> locale_str =
      isolate->factory()->NewStringFromAsciiChecked(r.locale.c_str());

  // 12. Let t be GetOption(options, "type", "string", «"conjunction",
  //    "disjunction", "unit"», "conjunction").
  Maybe<Type> maybe_type = Intl::GetStringOption<Type>(
      isolate, options, "type", "Intl.ListFormat",
      {"conjunction", "disjunction", "unit"},
      {Type::CONJUNCTION, Type::DISJUNCTION, Type::UNIT}, Type::CONJUNCTION);
  MAYBE_RETURN(maybe_type, MaybeHandle<JSListFormat>());
  Type type_enum = maybe_type.FromJust();

  // 14. Let s be ? GetOption(options, "style", "string",
  //                          «"long", "short", "narrow"», "long").
  Maybe<Style> maybe_style = Intl::GetStringOption<Style>(
      isolate, options, "style", "Intl.ListFormat", {"long", "short", "narrow"},
      {Style::LONG, Style::SHORT, Style::NARROW}, Style::LONG);
  MAYBE_RETURN(maybe_style, MaybeHandle<JSListFormat>());
  Style style_enum = maybe_style.FromJust();

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

  // Now all properties are ready, so we can allocate the result object.
  Handle<JSListFormat> list_format = Handle<JSListFormat>::cast(
      isolate->factory()->NewFastOrSlowJSObjectFromMap(map));
  DisallowHeapAllocation no_gc;
  list_format->set_flags(0);
  list_format->set_icu_formatter(*managed_formatter);

  // 11. Set listFormat.[[Locale]] to r.[[Locale]].
  list_format->set_locale(*locale_str);

  // 13. Set listFormat.[[Type]] to t.
  list_format->set_type(type_enum);

  // 15. Set listFormat.[[Style]] to s.
  list_format->set_style(style_enum);

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
  }
  UNREACHABLE();
}

Handle<String> JSListFormat::TypeAsString() const {
  switch (type()) {
    case Type::CONJUNCTION:
      return GetReadOnlyRoots().conjunction_string_handle();
    case Type::DISJUNCTION:
      return GetReadOnlyRoots().disjunction_string_handle();
    case Type::UNIT:
      return GetReadOnlyRoots().unit_string_handle();
  }
  UNREACHABLE();
}

namespace {

// Extract String from JSArray into array of UnicodeString
Maybe<std::vector<icu::UnicodeString>> ToUnicodeStringArray(
    Isolate* isolate, Handle<JSArray> array) {
  // Thanks to iterable-to-list preprocessing, we never see dictionary-mode
  // arrays here, so the loop below can construct an entry from the index.
  DCHECK(array->HasFastElements(isolate));
  auto* accessor = array->GetElementsAccessor();
  uint32_t length = accessor->NumberOfElements(*array);

  std::vector<icu::UnicodeString> result;
  for (uint32_t i = 0; i < length; i++) {
    InternalIndex entry(i);
    DCHECK(accessor->HasEntry(*array, entry));
    Handle<Object> item = accessor->Get(array, entry);
    DCHECK(item->IsString());
    Handle<String> item_str = Handle<String>::cast(item);
    if (!item_str->IsFlat()) item_str = String::Flatten(isolate, item_str);
    result.push_back(Intl::ToICUUnicodeString(isolate, item_str));
  }
  return Just(result);
}

template <typename T>
MaybeHandle<T> FormatListCommon(
    Isolate* isolate, Handle<JSListFormat> format, Handle<JSArray> list,
    MaybeHandle<T> (*formatToResult)(Isolate*, const icu::FormattedValue&)) {
  DCHECK(!list->IsUndefined());
  Maybe<std::vector<icu::UnicodeString>> maybe_array =
      ToUnicodeStringArray(isolate, list);
  MAYBE_RETURN(maybe_array, Handle<T>());
  std::vector<icu::UnicodeString> array = maybe_array.FromJust();

  icu::ListFormatter* formatter = format->icu_formatter().raw();
  CHECK_NOT_NULL(formatter);

  UErrorCode status = U_ZERO_ERROR;
  icu::FormattedList formatted = formatter->formatStringsToValue(
      array.data(), static_cast<int32_t>(array.size()), status);
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), T);
  }
  return formatToResult(isolate, formatted);
}

Handle<String> IcuFieldIdToType(Isolate* isolate, int32_t field_id) {
  switch (field_id) {
    case ULISTFMT_LITERAL_FIELD:
      return isolate->factory()->literal_string();
    case ULISTFMT_ELEMENT_FIELD:
      return isolate->factory()->element_string();
    default:
      UNREACHABLE();
      // To prevent MSVC from issuing C4715 warning.
      return Handle<String>();
  }
}

// A helper function to convert the FormattedList to a
// MaybeHandle<JSArray> for the implementation of formatToParts.
MaybeHandle<JSArray> FormattedListToJSArray(
    Isolate* isolate, const icu::FormattedValue& formatted) {
  Handle<JSArray> array = isolate->factory()->NewJSArray(0);
  icu::ConstrainedFieldPosition cfpos;
  cfpos.constrainCategory(UFIELD_CATEGORY_LIST);
  int index = 0;
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString string = formatted.toString(status);
  Handle<String> substring;
  while (formatted.nextPosition(cfpos, status) && U_SUCCESS(status)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, substring,
        Intl::ToString(isolate, string, cfpos.getStart(), cfpos.getLimit()),
        JSArray);
    Intl::AddElement(isolate, array, index++,
                     IcuFieldIdToType(isolate, cfpos.getField()), substring);
  }
  if (U_FAILURE(status)) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kIcuError), JSArray);
  }
  JSObject::ValidateElements(*array);
  return array;
}

}  // namespace

// ecma402 #sec-formatlist
MaybeHandle<String> JSListFormat::FormatList(Isolate* isolate,
                                             Handle<JSListFormat> format,
                                             Handle<JSArray> list) {
  return FormatListCommon<String>(isolate, format, list,
                                  Intl::FormattedToString);
}

// ecma42 #sec-formatlisttoparts
MaybeHandle<JSArray> JSListFormat::FormatListToParts(
    Isolate* isolate, Handle<JSListFormat> format, Handle<JSArray> list) {
  return FormatListCommon<JSArray>(isolate, format, list,
                                   FormattedListToJSArray);
}

namespace {

struct CheckListPattern {
  static const char* key() { return "listPattern"; }
  static const char* path() { return nullptr; }
};

}  // namespace

const std::set<std::string>& JSListFormat::GetAvailableLocales() {
  static base::LazyInstance<
      Intl::AvailableLocales<icu::Locale, CheckListPattern>>::type
      available_locales = LAZY_INSTANCE_INITIALIZER;
  return available_locales.Pointer()->Get();
}

}  // namespace internal
}  // namespace v8

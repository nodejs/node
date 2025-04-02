// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <cmath>
#include <list>
#include <memory>

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/date/date.h"
#include "src/logging/counters.h"
#include "src/objects/elements.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-display-names-inl.h"
#include "src/objects/js-duration-format-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/js-segments-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/option-utils.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/smi.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

BUILTIN(StringPrototypeToUpperCaseIntl) {
  HandleScope scope(isolate);
  TO_THIS_STRING(string, "String.prototype.toUpperCase");
  string = String::Flatten(isolate, string);
  RETURN_RESULT_OR_FAILURE(isolate, Intl::ConvertToUpper(isolate, string));
}

BUILTIN(StringPrototypeNormalizeIntl) {
  HandleScope handle_scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kStringNormalize);
  TO_THIS_STRING(string, "String.prototype.normalize");

  DirectHandle<Object> form_input = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Intl::Normalize(isolate, string, form_input));
}

// ecma402 #sup-properties-of-the-string-prototype-object
// ecma402 section 19.1.1.
//   String.prototype.localeCompare ( that [ , locales [ , options ] ] )
// This implementation supersedes the definition provided in ES6.
BUILTIN(StringPrototypeLocaleCompareIntl) {
  HandleScope handle_scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kStringLocaleCompare);
  static const char* const kMethod = "String.prototype.localeCompare";

  TO_THIS_STRING(str1, kMethod);
  DirectHandle<String> str2;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, str2, Object::ToString(isolate, args.atOrUndefined(isolate, 1)));
  std::optional<int> result = Intl::StringLocaleCompare(
      isolate, str1, str2, args.atOrUndefined(isolate, 2),
      args.atOrUndefined(isolate, 3), kMethod);
  if (!result.has_value()) {
    DCHECK(isolate->has_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return Smi::FromInt(result.value());
}

BUILTIN(V8BreakIteratorSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.v8BreakIterator.supportedLocalesOf",
                   JSV8BreakIterator::GetAvailableLocales(), locales, options));
}

BUILTIN(NumberFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.NumberFormat.supportedLocalesOf",
                   JSNumberFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(NumberFormatPrototypeFormatToParts) {
  const char* const method_name = "Intl.NumberFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSNumberFormat, number_format, method_name);

  Handle<Object> x;
  if (args.length() >= 2) {
    x = args.at(1);
  } else {
    x = isolate->factory()->nan_value();
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, JSNumberFormat::FormatToParts(isolate, number_format, x));
}

BUILTIN(DateTimeFormatPrototypeResolvedOptions) {
  const char* const method_name =
      "Intl.DateTimeFormat.prototype.resolvedOptions";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSReceiver, format_holder, method_name);

  // 3. Let dtf be ? UnwrapDateTimeFormat(dtf).
  DirectHandle<JSDateTimeFormat> date_time_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, date_time_format,
      JSDateTimeFormat::UnwrapDateTimeFormat(isolate, format_holder));

  RETURN_RESULT_OR_FAILURE(
      isolate, JSDateTimeFormat::ResolvedOptions(isolate, date_time_format));
}

BUILTIN(DateTimeFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.DateTimeFormat.supportedLocalesOf",
                   JSDateTimeFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(DateTimeFormatPrototypeFormatToParts) {
  const char* const method_name = "Intl.DateTimeFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSObject, date_format_holder, method_name);
  Factory* factory = isolate->factory();

  if (!IsJSDateTimeFormat(*date_format_holder)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              factory->NewStringFromAsciiChecked(method_name),
                              date_format_holder));
  }
  auto dtf = Cast<JSDateTimeFormat>(date_format_holder);

  DirectHandle<Object> x = args.atOrUndefined(isolate, 1);
  RETURN_RESULT_OR_FAILURE(isolate, JSDateTimeFormat::FormatToParts(
                                        isolate, dtf, x, false, method_name));
}

// Common code for DateTimeFormatPrototypeFormatRange(|ToParts)
template <class T,
          MaybeDirectHandle<T> (*F)(Isolate*, DirectHandle<JSDateTimeFormat>,
                                    DirectHandle<Object>, DirectHandle<Object>,
                                    const char* const)>
V8_WARN_UNUSED_RESULT Tagged<Object> DateTimeFormatRange(
    BuiltinArguments args, Isolate* isolate, const char* const method_name) {
  // 1. Let dtf be this value.
  // 2. Perform ? RequireInternalSlot(dtf, [[InitializedDateTimeFormat]]).
  CHECK_RECEIVER(JSDateTimeFormat, dtf, method_name);

  // 3. If startDate is undefined or endDate is undefined, throw a TypeError
  // exception.
  DirectHandle<Object> start_date = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> end_date = args.atOrUndefined(isolate, 2);
  if (IsUndefined(*start_date, isolate) || IsUndefined(*end_date, isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalidTimeValue));
  }

  // 4. Return ? FormatDateTimeRange(dtf, startDate, endDate)
  // OR
  // 4. Return ? FormatDateTimeRangeToParts(dtf, startDate, endDate).
  RETURN_RESULT_OR_FAILURE(isolate,
                           F(isolate, dtf, start_date, end_date, method_name));
}

BUILTIN(DateTimeFormatPrototypeFormatRange) {
  const char* const method_name = "Intl.DateTimeFormat.prototype.formatRange";
  HandleScope handle_scope(isolate);
  return DateTimeFormatRange<String, JSDateTimeFormat::FormatRange>(
      args, isolate, method_name);
}

BUILTIN(DateTimeFormatPrototypeFormatRangeToParts) {
  const char* const method_name =
      "Intl.DateTimeFormat.prototype.formatRangeToParts";
  HandleScope handle_scope(isolate);
  return DateTimeFormatRange<JSArray, JSDateTimeFormat::FormatRangeToParts>(
      args, isolate, method_name);
}

namespace {

DirectHandle<JSFunction> CreateBoundFunction(Isolate* isolate,
                                             DirectHandle<JSObject> object,
                                             Builtin builtin, int len) {
  DirectHandle<NativeContext> native_context(
      isolate->context()->native_context(), isolate);
  DirectHandle<Context> context = isolate->factory()->NewBuiltinContext(
      native_context,
      static_cast<int>(Intl::BoundFunctionContextSlot::kLength));

  context->set(static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction),
               *object);

  DirectHandle<SharedFunctionInfo> info =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), builtin, len, kAdapt);

  return Factory::JSFunctionBuilder{isolate, info, context}
      .set_map(isolate->strict_function_without_prototype_map())
      .Build();
}

/**
 * Common code shared between DateTimeFormatConstructor and
 * NumberFormatConstructor
 */
template <class T>
Tagged<Object> LegacyFormatConstructor(BuiltinArguments args, Isolate* isolate,
                                       v8::Isolate::UseCounterFeature feature,
                                       DirectHandle<JSAny> constructor,
                                       const char* method_name) {
  isolate->CountUsage(feature);
  DirectHandle<JSReceiver> new_target;
  // 1. If NewTarget is undefined, let newTarget be the active
  // function object, else let newTarget be NewTarget.
  if (IsUndefined(*args.new_target(), isolate)) {
    new_target = args.target();
  } else {
    new_target = Cast<JSReceiver>(args.new_target());
  }

  // [[Construct]]
  DirectHandle<JSFunction> target = args.target();
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  // 2. Let format be ? OrdinaryCreateFromConstructor(newTarget,
  // "%<T>Prototype%", ...).
  DirectHandle<Map> map;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, map, JSFunction::GetDerivedMap(isolate, target, new_target));

  // 3. Perform ? Initialize<T>(Format, locales, options).
  DirectHandle<T> format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, format, T::New(isolate, map, locales, options, method_name));
  // 4. Let this be the this value.
  if (IsUndefined(*args.new_target(), isolate)) {
    DirectHandle<JSAny> receiver = args.receiver();
    // 5. If NewTarget is undefined and ? OrdinaryHasInstance(%<T>%, this)
    // is true, then Look up the intrinsic value that has been stored on
    // the context.
    DirectHandle<Object> ordinary_has_instance_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, ordinary_has_instance_obj,
        Object::OrdinaryHasInstance(isolate, constructor, receiver));
    if (Object::BooleanValue(*ordinary_has_instance_obj, isolate)) {
      if (!IsJSReceiver(*receiver)) {
        THROW_NEW_ERROR_RETURN_FAILURE(
            isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                  isolate->factory()->NewStringFromAsciiChecked(
                                      method_name),
                                  receiver));
      }
      DirectHandle<JSReceiver> rec = Cast<JSReceiver>(receiver);
      // a. Perform ? DefinePropertyOrThrow(this,
      // %Intl%.[[FallbackSymbol]], PropertyDescriptor{ [[Value]]: format,
      // [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }).
      PropertyDescriptor desc;
      desc.set_value(format);
      desc.set_writable(false);
      desc.set_enumerable(false);
      desc.set_configurable(false);
      Maybe<bool> success = JSReceiver::DefineOwnProperty(
          isolate, rec, isolate->factory()->intl_fallback_symbol(), &desc,
          Just(kThrowOnError));
      MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
      CHECK(success.FromJust());
      // b. b. Return this.
      return *receiver;
    }
  }
  // 6. Return format.
  return *format;
}

/**
 * Common code shared by ListFormat, RelativeTimeFormat, PluralRules, and
 * Segmenter
 */
template <class T>
Tagged<Object> DisallowCallConstructor(BuiltinArguments args, Isolate* isolate,
                                       v8::Isolate::UseCounterFeature feature,
                                       const char* method_name) {
  isolate->CountUsage(feature);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (IsUndefined(*args.new_target(), isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  // [[Construct]]
  DirectHandle<JSFunction> target = args.target();
  DirectHandle<JSReceiver> new_target = Cast<JSReceiver>(args.new_target());

  DirectHandle<Map> map;
  // 2. Let result be OrdinaryCreateFromConstructor(NewTarget,
  //    "%<T>Prototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, map, JSFunction::GetDerivedMap(isolate, target, new_target));

  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  // 3. Return New<T>(t, locales, options).
  RETURN_RESULT_OR_FAILURE(isolate, T::New(isolate, map, locales, options));
}

/**
 * Common code shared by Collator and V8BreakIterator
 */
template <class T>
Tagged<Object> CallOrConstructConstructor(BuiltinArguments args,
                                          Isolate* isolate,
                                          const char* method_name) {
  DirectHandle<JSReceiver> new_target;

  if (IsUndefined(*args.new_target(), isolate)) {
    new_target = args.target();
  } else {
    new_target = Cast<JSReceiver>(args.new_target());
  }

  // [[Construct]]
  DirectHandle<JSFunction> target = args.target();

  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  DirectHandle<Map> map;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, map, JSFunction::GetDerivedMap(isolate, target, new_target));

  RETURN_RESULT_OR_FAILURE(isolate,
                           T::New(isolate, map, locales, options, method_name));
}

}  // namespace

// Intl.DisplayNames

BUILTIN(DisplayNamesConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSDisplayNames>(
      args, isolate, v8::Isolate::UseCounterFeature::kDisplayNames,
      "Intl.DisplayNames");
}

BUILTIN(DisplayNamesPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDisplayNames, holder,
                 "Intl.DisplayNames.prototype.resolvedOptions");
  return *JSDisplayNames::ResolvedOptions(isolate, holder);
}

BUILTIN(DisplayNamesSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.DisplayNames.supportedLocalesOf",
                   JSDisplayNames::GetAvailableLocales(), locales, options));
}

BUILTIN(DisplayNamesPrototypeOf) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDisplayNames, holder, "Intl.DisplayNames.prototype.of");
  Handle<Object> code_obj = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSDisplayNames::Of(isolate, holder, code_obj));
}

// Intl.DurationFormat
BUILTIN(DurationFormatConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSDurationFormat>(
      args, isolate, v8::Isolate::UseCounterFeature::kDurationFormat,
      "Intl.DurationFormat");
}

BUILTIN(DurationFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDurationFormat, holder,
                 "Intl.DurationFormat.prototype.resolvedOptions");
  return *JSDurationFormat::ResolvedOptions(isolate, holder);
}

BUILTIN(DurationFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.DurationFormat.supportedLocalesOf",
                   JSDurationFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(DurationFormatPrototypeFormat) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDurationFormat, holder,
                 "Intl.DurationFormat.prototype.format");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSDurationFormat::Format(isolate, holder, value));
}

BUILTIN(DurationFormatPrototypeFormatToParts) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSDurationFormat, holder,
                 "Intl.DurationFormat.prototype.formatToParts");
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSDurationFormat::FormatToParts(isolate, holder, value));
}

// Intl.NumberFormat

BUILTIN(NumberFormatConstructor) {
  HandleScope scope(isolate);

  return LegacyFormatConstructor<JSNumberFormat>(
      args, isolate, v8::Isolate::UseCounterFeature::kNumberFormat,
      isolate->intl_number_format_function(), "Intl.NumberFormat");
}

BUILTIN(NumberFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  const char* const method_name = "Intl.NumberFormat.prototype.resolvedOptions";

  // 1. Let nf be the this value.
  // 2. If Type(nf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, number_format_holder, method_name);

  // 3. Let nf be ? UnwrapNumberFormat(nf)
  DirectHandle<JSNumberFormat> number_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, number_format,
      JSNumberFormat::UnwrapNumberFormat(isolate, number_format_holder));

  return *JSNumberFormat::ResolvedOptions(isolate, number_format);
}

BUILTIN(NumberFormatPrototypeFormatNumber) {
  const char* const method_name = "get Intl.NumberFormat.prototype.format";
  HandleScope scope(isolate);

  // 1. Let nf be the this value.
  // 2. If Type(nf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, receiver, method_name);

  // 3. Let nf be ? UnwrapNumberFormat(nf).
  DirectHandle<JSNumberFormat> number_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, number_format,
      JSNumberFormat::UnwrapNumberFormat(isolate, receiver));

  DirectHandle<Object> bound_format(number_format->bound_format(), isolate);

  // 4. If nf.[[BoundFormat]] is undefined, then
  if (!IsUndefined(*bound_format, isolate)) {
    DCHECK(IsJSFunction(*bound_format));
    // 5. Return nf.[[BoundFormat]].
    return *bound_format;
  }

  DirectHandle<JSFunction> new_bound_format_function = CreateBoundFunction(
      isolate, number_format, Builtin::kNumberFormatInternalFormatNumber, 1);

  // 4. c. Set nf.[[BoundFormat]] to F.
  number_format->set_bound_format(*new_bound_format_function);

  // 5. Return nf.[[BoundFormat]].
  return *new_bound_format_function;
}

BUILTIN(NumberFormatInternalFormatNumber) {
  HandleScope scope(isolate);

  DirectHandle<Context> context(isolate->context(), isolate);

  // 1. Let nf be F.[[NumberFormat]].
  // 2. Assert: Type(nf) is Object and nf has an
  //    [[InitializedNumberFormat]] internal slot.
  DirectHandle<JSNumberFormat> number_format(
      Cast<JSNumberFormat>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  // 3. If value is not provided, let value be undefined.
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate, JSNumberFormat::NumberFormatFunction(
                                        isolate, number_format, value));
}

// Common code for NumberFormatPrototypeFormatRange(|ToParts)
template <class T,
          MaybeDirectHandle<T> (*F)(Isolate*, DirectHandle<JSNumberFormat>,
                                    Handle<Object>, Handle<Object>)>
V8_WARN_UNUSED_RESULT Tagged<Object> NumberFormatRange(
    BuiltinArguments args, Isolate* isolate, const char* const method_name) {
  // 1. Let nf be this value.
  // 2. Perform ? RequireInternalSlot(nf, [[InitializedNumberFormat]]).
  CHECK_RECEIVER(JSNumberFormat, nf, method_name);

  Handle<Object> start = args.atOrUndefined(isolate, 1);
  Handle<Object> end = args.atOrUndefined(isolate, 2);

  Factory* factory = isolate->factory();
  // 3. If start is undefined or end is undefined, throw a TypeError exception.
  if (IsUndefined(*start, isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kInvalid,
                     factory->NewStringFromStaticChars("start"), start));
  }
  if (IsUndefined(*end, isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalid,
                              factory->NewStringFromStaticChars("end"), end));
  }

  RETURN_RESULT_OR_FAILURE(isolate, F(isolate, nf, start, end));
}

BUILTIN(NumberFormatPrototypeFormatRange) {
  const char* const method_name = "Intl.NumberFormat.prototype.formatRange";
  HandleScope handle_scope(isolate);
  return NumberFormatRange<String, JSNumberFormat::FormatNumericRange>(
      args, isolate, method_name);
}

BUILTIN(NumberFormatPrototypeFormatRangeToParts) {
  const char* const method_name =
      "Intl.NumberFormat.prototype.formatRangeToParts";
  HandleScope handle_scope(isolate);
  return NumberFormatRange<JSArray, JSNumberFormat::FormatNumericRangeToParts>(
      args, isolate, method_name);
}

BUILTIN(DateTimeFormatConstructor) {
  HandleScope scope(isolate);

  return LegacyFormatConstructor<JSDateTimeFormat>(
      args, isolate, v8::Isolate::UseCounterFeature::kDateTimeFormat,
      isolate->intl_date_time_format_function(), "Intl.DateTimeFormat");
}

BUILTIN(DateTimeFormatPrototypeFormat) {
  const char* const method_name = "get Intl.DateTimeFormat.prototype.format";
  HandleScope scope(isolate);

  // 1. Let dtf be this value.
  // 2. If Type(dtf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, receiver, method_name);

  // 3. Let dtf be ? UnwrapDateTimeFormat(dtf).
  DirectHandle<JSDateTimeFormat> format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, format,
      JSDateTimeFormat::UnwrapDateTimeFormat(isolate, receiver));

  DirectHandle<Object> bound_format =
      DirectHandle<Object>(format->bound_format(), isolate);

  // 4. If dtf.[[BoundFormat]] is undefined, then
  if (!IsUndefined(*bound_format, isolate)) {
    DCHECK(IsJSFunction(*bound_format));
    // 5. Return dtf.[[BoundFormat]].
    return *bound_format;
  }

  DirectHandle<JSFunction> new_bound_format_function = CreateBoundFunction(
      isolate, format, Builtin::kDateTimeFormatInternalFormat, 1);

  // 4.c. Set dtf.[[BoundFormat]] to F.
  format->set_bound_format(*new_bound_format_function);

  // 5. Return dtf.[[BoundFormat]].
  return *new_bound_format_function;
}

BUILTIN(DateTimeFormatInternalFormat) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  // 1. Let dtf be F.[[DateTimeFormat]].
  // 2. Assert: Type(dtf) is Object and dtf has an [[InitializedDateTimeFormat]]
  // internal slot.
  DirectHandle<JSDateTimeFormat> date_format_holder(
      Cast<JSDateTimeFormat>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  DirectHandle<Object> date = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate, JSDateTimeFormat::DateTimeFormat(
                                        isolate, date_format_holder, date,
                                        "DateTime Format Functions"));
}

BUILTIN(IntlGetCanonicalLocales) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Intl::GetCanonicalLocales(isolate, locales));
}

BUILTIN(IntlSupportedValuesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate, Intl::SupportedValuesOf(isolate, locales));
}

BUILTIN(ListFormatConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSListFormat>(
      args, isolate, v8::Isolate::UseCounterFeature::kListFormat,
      "Intl.ListFormat");
}

BUILTIN(ListFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSListFormat, format_holder,
                 "Intl.ListFormat.prototype.resolvedOptions");
  return *JSListFormat::ResolvedOptions(isolate, format_holder);
}

BUILTIN(ListFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.ListFormat.supportedLocalesOf",
                   JSListFormat::GetAvailableLocales(), locales, options));
}

// Intl.Locale implementation
BUILTIN(LocaleConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocale);

  const char* method_name = "Intl.Locale";
  if (IsUndefined(*args.new_target(), isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  // [[Construct]]
  DirectHandle<JSFunction> target = args.target();
  DirectHandle<JSReceiver> new_target = Cast<JSReceiver>(args.new_target());

  DirectHandle<Object> tag = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  DirectHandle<Map> map;
  // 6. Let locale be ? OrdinaryCreateFromConstructor(NewTarget,
  // %LocalePrototype%, internalSlotsList).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, map, JSFunction::GetDerivedMap(isolate, target, new_target));

  // 7. If Type(tag) is not String or Object, throw a TypeError exception.
  if (!IsString(*tag) && !IsJSReceiver(*tag)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kLocaleNotEmpty));
  }

  DirectHandle<String> locale_string;
  // 8. If Type(tag) is Object and tag has an [[InitializedLocale]] internal
  // slot, then
  if (IsJSLocale(*tag)) {
    // a. Let tag be tag.[[Locale]].
    locale_string = JSLocale::ToString(isolate, Cast<JSLocale>(tag));
  } else {  // 9. Else,
    // a. Let tag be ? ToString(tag).
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, locale_string,
                                       Object::ToString(isolate, tag));
  }

  // 10. Set options to ? CoerceOptionsToObject(options).
  DirectHandle<JSReceiver> options_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, options_object,
      CoerceOptionsToObject(isolate, options, method_name));

  RETURN_RESULT_OR_FAILURE(
      isolate, JSLocale::New(isolate, map, locale_string, options_object));
}

BUILTIN(LocalePrototypeMaximize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.maximize");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::Maximize(isolate, locale));
}

BUILTIN(LocalePrototypeMinimize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.minimize");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::Minimize(isolate, locale));
}

BUILTIN(LocalePrototypeGetCalendars) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getCalendars");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetCalendars(isolate, locale));
}

BUILTIN(LocalePrototypeGetCollations) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getCollations");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetCollations(isolate, locale));
}

BUILTIN(LocalePrototypeGetHourCycles) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getHourCycles");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetHourCycles(isolate, locale));
}

BUILTIN(LocalePrototypeGetNumberingSystems) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getNumberingSystems");
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSLocale::GetNumberingSystems(isolate, locale));
}

BUILTIN(LocalePrototypeGetTextInfo) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getTextInfo");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetTextInfo(isolate, locale));
}

BUILTIN(LocalePrototypeGetTimeZones) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getTimeZones");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetTimeZones(isolate, locale));
}

BUILTIN(LocalePrototypeGetWeekInfo) {
  HandleScope scope(isolate);
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocaleInfoFunctions);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.getWeekInfo");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetWeekInfo(isolate, locale));
}

BUILTIN(LocalePrototypeCalendars) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.calendars");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetCalendars(isolate, locale));
}

BUILTIN(LocalePrototypeCollations) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.collations");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetCollations(isolate, locale));
}

BUILTIN(LocalePrototypeHourCycles) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.hourCycles");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetHourCycles(isolate, locale));
}

BUILTIN(LocalePrototypeNumberingSystems) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.numberingSystems");
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSLocale::GetNumberingSystems(isolate, locale));
}

BUILTIN(LocalePrototypeTextInfo) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.textInfo");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetTextInfo(isolate, locale));
}

BUILTIN(LocalePrototypeTimeZones) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.timeZones");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetTimeZones(isolate, locale));
}

BUILTIN(LocalePrototypeWeekInfo) {
  HandleScope scope(isolate);
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kLocaleInfoObsoletedGetters);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.weekInfo");
  RETURN_RESULT_OR_FAILURE(isolate, JSLocale::GetWeekInfo(isolate, locale));
}

BUILTIN(RelativeTimeFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      Intl::SupportedLocalesOf(
          isolate, "Intl.RelativeTimeFormat.supportedLocalesOf",
          JSRelativeTimeFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(RelativeTimeFormatPrototypeFormat) {
  HandleScope scope(isolate);
  // 1. Let relativeTimeFormat be the this value.
  // 2. If Type(relativeTimeFormat) is not Object or relativeTimeFormat does not
  //    have an [[InitializedRelativeTimeFormat]] internal slot whose value is
  //    true, throw a TypeError exception.
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.format");
  Handle<Object> value_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> unit_obj = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSRelativeTimeFormat::Format(isolate, value_obj, unit_obj,
                                            format_holder));
}

BUILTIN(RelativeTimeFormatPrototypeFormatToParts) {
  HandleScope scope(isolate);
  // 1. Let relativeTimeFormat be the this value.
  // 2. If Type(relativeTimeFormat) is not Object or relativeTimeFormat does not
  //    have an [[InitializedRelativeTimeFormat]] internal slot whose value is
  //    true, throw a TypeError exception.
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.formatToParts");
  Handle<Object> value_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> unit_obj = args.atOrUndefined(isolate, 2);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSRelativeTimeFormat::FormatToParts(isolate, value_obj, unit_obj,
                                                   format_holder));
}

// Locale getters.
BUILTIN(LocalePrototypeLanguage) {
  HandleScope scope(isolate);
  // CHECK_RECEIVER will case locale_holder to JSLocale.
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.language");

  return *JSLocale::Language(isolate, locale);
}

BUILTIN(LocalePrototypeScript) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.script");

  return *JSLocale::Script(isolate, locale);
}

BUILTIN(LocalePrototypeRegion) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.region");

  return *JSLocale::Region(isolate, locale);
}

BUILTIN(LocalePrototypeBaseName) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.baseName");

  return *JSLocale::BaseName(isolate, locale);
}

BUILTIN(LocalePrototypeCalendar) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.calendar");

  return *JSLocale::Calendar(isolate, locale);
}

BUILTIN(LocalePrototypeCaseFirst) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.caseFirst");

  return *JSLocale::CaseFirst(isolate, locale);
}

BUILTIN(LocalePrototypeCollation) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.collation");

  return *JSLocale::Collation(isolate, locale);
}

BUILTIN(LocalePrototypeFirstDayOfWeek) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.firstDayOfWeek");

  return *JSLocale::FirstDayOfWeek(isolate, locale);
}

BUILTIN(LocalePrototypeHourCycle) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.hourCycle");

  return *JSLocale::HourCycle(isolate, locale);
}

BUILTIN(LocalePrototypeNumeric) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.numeric");

  return *JSLocale::Numeric(isolate, locale);
}

BUILTIN(LocalePrototypeNumberingSystem) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.numberingSystem");

  return *JSLocale::NumberingSystem(isolate, locale);
}

BUILTIN(LocalePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale, "Intl.Locale.prototype.toString");

  return *JSLocale::ToString(isolate, locale);
}

BUILTIN(RelativeTimeFormatConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSRelativeTimeFormat>(
      args, isolate, v8::Isolate::UseCounterFeature::kRelativeTimeFormat,
      "Intl.RelativeTimeFormat");
}

BUILTIN(RelativeTimeFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.resolvedOptions");
  return *JSRelativeTimeFormat::ResolvedOptions(isolate, format_holder);
}

bool IsFastLocale(Tagged<Object> maybe_locale) {
  DisallowGarbageCollection no_gc;
  if (!IsSeqOneByteString(maybe_locale)) {
    return false;
  }
  auto locale = Cast<SeqOneByteString>(maybe_locale);
  uint8_t* chars = locale->GetChars(no_gc);
  if (locale->length() < 2 || !std::isalpha(chars[0]) ||
      !std::isalpha(chars[1])) {
    return false;
  }
  if (locale->length() != 2 &&
      (locale->length() != 5 || chars[2] != '-' || !std::isalpha(chars[3]) ||
       !std::isalpha(chars[4]))) {
    return false;
  }
  char first = chars[0] | 0x20;
  char second = chars[1] | 0x20;
  return (first != 'a' || second != 'z') && (first != 'e' || second != 'l') &&
         (first != 'l' || second != 't') && (first != 't' || second != 'r');
}

BUILTIN(StringPrototypeToLocaleUpperCase) {
  HandleScope scope(isolate);
  DirectHandle<Object> maybe_locale = args.atOrUndefined(isolate, 1);
  TO_THIS_STRING(string, "String.prototype.toLocaleUpperCase");
  if (IsUndefined(*maybe_locale) || IsFastLocale(*maybe_locale)) {
    string = String::Flatten(isolate, string);
    RETURN_RESULT_OR_FAILURE(isolate, Intl::ConvertToUpper(isolate, string));
  } else {
    RETURN_RESULT_OR_FAILURE(isolate, Intl::StringLocaleConvertCase(
                                          isolate, string, true, maybe_locale));
  }
}

BUILTIN(PluralRulesConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSPluralRules>(
      args, isolate, v8::Isolate::UseCounterFeature::kPluralRules,
      "Intl.PluralRules");
}

BUILTIN(PluralRulesPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSPluralRules, plural_rules_holder,
                 "Intl.PluralRules.prototype.resolvedOptions");
  return *JSPluralRules::ResolvedOptions(isolate, plural_rules_holder);
}

BUILTIN(PluralRulesPrototypeSelect) {
  HandleScope scope(isolate);

  // 1. 1. Let pr be the this value.
  // 2. Perform ? RequireInternalSlot(pr, [[InitializedPluralRules]]).
  CHECK_RECEIVER(JSPluralRules, plural_rules,
                 "Intl.PluralRules.prototype.select");

  // 3. Let n be ? ToNumber(value).
  DirectHandle<Object> number = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number,
                                     Object::ToNumber(isolate, number));
  double number_double = Object::NumberValue(*number);

  // 4. Return ! ResolvePlural(pr, n).
  RETURN_RESULT_OR_FAILURE(isolate, JSPluralRules::ResolvePlural(
                                        isolate, plural_rules, number_double));
}

BUILTIN(PluralRulesPrototypeSelectRange) {
  HandleScope scope(isolate);

  // 1. Let pr be the this value.
  // 2. Perform ? RequireInternalSlot(pr, [[InitializedPluralRules]]).
  CHECK_RECEIVER(JSPluralRules, plural_rules,
                 "Intl.PluralRules.prototype.selectRange");

  // 3. If start is undefined or end is undefined, throw a TypeError exception.
  DirectHandle<Object> start = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> end = args.atOrUndefined(isolate, 2);
  if (IsUndefined(*start)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalid,
                              isolate->factory()->startRange_string(), start));
  }
  if (IsUndefined(*end)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kInvalid,
                              isolate->factory()->endRange_string(), end));
  }

  // 4. Let x be ? ToNumber(start).
  DirectHandle<Object> x;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                     Object::ToNumber(isolate, start));

  // 5. Let y be ? ToNumber(end).
  DirectHandle<Object> y;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, y,
                                     Object::ToNumber(isolate, end));

  // 6. Return ! ResolvePluralRange(pr, x, y).
  if (IsNaN(*x)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalid,
                               isolate->factory()->startRange_string(), x));
  }
  if (IsNaN(*y)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalid,
                               isolate->factory()->endRange_string(), y));
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, JSPluralRules::ResolvePluralRange(isolate, plural_rules,
                                                 Object::NumberValue(*x),
                                                 Object::NumberValue(*y)));
}

BUILTIN(PluralRulesSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.PluralRules.supportedLocalesOf",
                   JSPluralRules::GetAvailableLocales(), locales, options));
}

BUILTIN(CollatorConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kCollator);

  return CallOrConstructConstructor<JSCollator>(args, isolate, "Intl.Collator");
}

BUILTIN(CollatorPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSCollator, collator_holder,
                 "Intl.Collator.prototype.resolvedOptions");
  return *JSCollator::ResolvedOptions(isolate, collator_holder);
}

BUILTIN(CollatorSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.Collator.supportedLocalesOf",
                   JSCollator::GetAvailableLocales(), locales, options));
}

BUILTIN(CollatorPrototypeCompare) {
  const char* const method_name = "get Intl.Collator.prototype.compare";
  HandleScope scope(isolate);

  // 1. Let collator be this value.
  // 2. If Type(collator) is not Object, throw a TypeError exception.
  // 3. If collator does not have an [[InitializedCollator]] internal slot,
  // throw a TypeError exception.
  CHECK_RECEIVER(JSCollator, collator, method_name);

  // 4. If collator.[[BoundCompare]] is undefined, then
  DirectHandle<Object> bound_compare(collator->bound_compare(), isolate);
  if (!IsUndefined(*bound_compare, isolate)) {
    DCHECK(IsJSFunction(*bound_compare));
    // 5. Return collator.[[BoundCompare]].
    return *bound_compare;
  }

  DirectHandle<JSFunction> new_bound_compare_function = CreateBoundFunction(
      isolate, collator, Builtin::kCollatorInternalCompare, 2);

  // 4.c. Set collator.[[BoundCompare]] to F.
  collator->set_bound_compare(*new_bound_compare_function);

  // 5. Return collator.[[BoundCompare]].
  return *new_bound_compare_function;
}

BUILTIN(CollatorInternalCompare) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  // 1. Let collator be F.[[Collator]].
  // 2. Assert: Type(collator) is Object and collator has an
  // [[InitializedCollator]] internal slot.
  DirectHandle<JSCollator> collator(
      Cast<JSCollator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  // 3. If x is not provided, let x be undefined.
  DirectHandle<Object> x = args.atOrUndefined(isolate, 1);
  // 4. If y is not provided, let y be undefined.
  DirectHandle<Object> y = args.atOrUndefined(isolate, 2);

  // 5. Let X be ? ToString(x).
  DirectHandle<String> string_x;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, string_x,
                                     Object::ToString(isolate, x));
  // 6. Let Y be ? ToString(y).
  DirectHandle<String> string_y;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, string_y,
                                     Object::ToString(isolate, y));

  // 7. Return CompareStrings(collator, X, Y).
  icu::Collator* icu_collator = collator->icu_collator()->raw();
  CHECK_NOT_NULL(icu_collator);
  return Smi::FromInt(
      Intl::CompareStrings(isolate, *icu_collator, string_x, string_y));
}

// ecma402 #sec-%segmentiteratorprototype%.next
BUILTIN(SegmentIteratorPrototypeNext) {
  const char* const method_name = "%SegmentIterator.prototype%.next";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method_name);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSSegmentIterator::Next(isolate, segment_iterator));
}

// ecma402 #sec-intl.segmenter
BUILTIN(SegmenterConstructor) {
  HandleScope scope(isolate);

  return DisallowCallConstructor<JSSegmenter>(
      args, isolate, v8::Isolate::UseCounterFeature::kSegmenter,
      "Intl.Segmenter");
}

// ecma402 #sec-intl.segmenter.supportedlocalesof
BUILTIN(SegmenterSupportedLocalesOf) {
  HandleScope scope(isolate);
  DirectHandle<Object> locales = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.Segmenter.supportedLocalesOf",
                   JSSegmenter::GetAvailableLocales(), locales, options));
}

// ecma402 #sec-intl.segmenter.prototype.resolvedoptions
BUILTIN(SegmenterPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmenter, segmenter,
                 "Intl.Segmenter.prototype.resolvedOptions");
  return *JSSegmenter::ResolvedOptions(isolate, segmenter);
}

// ecma402 #sec-intl.segmenter.prototype.segment
BUILTIN(SegmenterPrototypeSegment) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmenter, segmenter, "Intl.Segmenter.prototype.segment");
  Handle<Object> input_text = args.atOrUndefined(isolate, 1);
  // 3. Let string be ? ToString(string).
  DirectHandle<String> string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, string,
                                     Object::ToString(isolate, input_text));

  // 4. Return ? CreateSegmentsObject(segmenter, string).
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSSegments::Create(isolate, segmenter, string));
}

// ecma402 #sec-%segmentsprototype%.containing
BUILTIN(SegmentsPrototypeContaining) {
  const char* const method_name = "%Segments.prototype%.containing";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegments, segments, method_name);
  DirectHandle<Object> index = args.atOrUndefined(isolate, 1);

  // 6. Let n be ? ToInteger(index).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, index,
                                     Object::ToInteger(isolate, index));
  double const n = Object::NumberValue(*index);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSSegments::Containing(isolate, segments, n));
}

// ecma402 #sec-%segmentsprototype%-@@iterator
BUILTIN(SegmentsPrototypeIterator) {
  const char* const method_name = "%SegmentIsPrototype%[@@iterator]";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegments, segments, method_name);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSSegmentIterator::Create(
          isolate, direct_handle(segments->raw_string(), isolate),
          segments->icu_break_iterator()->raw(), segments->granularity()));
}

BUILTIN(V8BreakIteratorConstructor) {
  HandleScope scope(isolate);

  return CallOrConstructConstructor<JSV8BreakIterator>(args, isolate,
                                                       "Intl.v8BreakIterator");
}

BUILTIN(V8BreakIteratorPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSV8BreakIterator, break_iterator,
                 "Intl.v8BreakIterator.prototype.resolvedOptions");
  return *JSV8BreakIterator::ResolvedOptions(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeAdoptText) {
  const char* const method_name =
      "get Intl.v8BreakIterator.prototype.adoptText";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method_name);

  DirectHandle<Object> bound_adopt_text(break_iterator->bound_adopt_text(),
                                        isolate);
  if (!IsUndefined(*bound_adopt_text, isolate)) {
    DCHECK(IsJSFunction(*bound_adopt_text));
    return *bound_adopt_text;
  }

  DirectHandle<JSFunction> new_bound_adopt_text_function = CreateBoundFunction(
      isolate, break_iterator, Builtin::kV8BreakIteratorInternalAdoptText, 1);
  break_iterator->set_bound_adopt_text(*new_bound_adopt_text_function);
  return *new_bound_adopt_text_function;
}

BUILTIN(V8BreakIteratorInternalAdoptText) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  DirectHandle<JSV8BreakIterator> break_iterator(
      Cast<JSV8BreakIterator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  Handle<Object> input_text = args.atOrUndefined(isolate, 1);
  DirectHandle<String> text;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, text,
                                     Object::ToString(isolate, input_text));

  JSV8BreakIterator::AdoptText(isolate, break_iterator, text);
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(V8BreakIteratorPrototypeFirst) {
  const char* const method_name = "get Intl.v8BreakIterator.prototype.first";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method_name);

  DirectHandle<Object> bound_first(break_iterator->bound_first(), isolate);
  if (!IsUndefined(*bound_first, isolate)) {
    DCHECK(IsJSFunction(*bound_first));
    return *bound_first;
  }

  DirectHandle<JSFunction> new_bound_first_function = CreateBoundFunction(
      isolate, break_iterator, Builtin::kV8BreakIteratorInternalFirst, 0);
  break_iterator->set_bound_first(*new_bound_first_function);
  return *new_bound_first_function;
}

BUILTIN(V8BreakIteratorInternalFirst) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  DirectHandle<JSV8BreakIterator> break_iterator(
      Cast<JSV8BreakIterator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  return *JSV8BreakIterator::First(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeNext) {
  const char* const method_name = "get Intl.v8BreakIterator.prototype.next";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method_name);

  DirectHandle<Object> bound_next(break_iterator->bound_next(), isolate);
  if (!IsUndefined(*bound_next, isolate)) {
    DCHECK(IsJSFunction(*bound_next));
    return *bound_next;
  }

  DirectHandle<JSFunction> new_bound_next_function = CreateBoundFunction(
      isolate, break_iterator, Builtin::kV8BreakIteratorInternalNext, 0);
  break_iterator->set_bound_next(*new_bound_next_function);
  return *new_bound_next_function;
}

BUILTIN(V8BreakIteratorInternalNext) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  DirectHandle<JSV8BreakIterator> break_iterator(
      Cast<JSV8BreakIterator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return *JSV8BreakIterator::Next(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeCurrent) {
  const char* const method_name = "get Intl.v8BreakIterator.prototype.current";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method_name);

  DirectHandle<Object> bound_current(break_iterator->bound_current(), isolate);
  if (!IsUndefined(*bound_current, isolate)) {
    DCHECK(IsJSFunction(*bound_current));
    return *bound_current;
  }

  DirectHandle<JSFunction> new_bound_current_function = CreateBoundFunction(
      isolate, break_iterator, Builtin::kV8BreakIteratorInternalCurrent, 0);
  break_iterator->set_bound_current(*new_bound_current_function);
  return *new_bound_current_function;
}

BUILTIN(V8BreakIteratorInternalCurrent) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  DirectHandle<JSV8BreakIterator> break_iterator(
      Cast<JSV8BreakIterator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return *JSV8BreakIterator::Current(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeBreakType) {
  const char* const method_name =
      "get Intl.v8BreakIterator.prototype.breakType";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method_name);

  DirectHandle<Object> bound_break_type(break_iterator->bound_break_type(),
                                        isolate);
  if (!IsUndefined(*bound_break_type, isolate)) {
    DCHECK(IsJSFunction(*bound_break_type));
    return *bound_break_type;
  }

  DirectHandle<JSFunction> new_bound_break_type_function = CreateBoundFunction(
      isolate, break_iterator, Builtin::kV8BreakIteratorInternalBreakType, 0);
  break_iterator->set_bound_break_type(*new_bound_break_type_function);
  return *new_bound_break_type_function;
}

BUILTIN(V8BreakIteratorInternalBreakType) {
  HandleScope scope(isolate);
  DirectHandle<Context> context(isolate->context(), isolate);

  DirectHandle<JSV8BreakIterator> break_iterator(
      Cast<JSV8BreakIterator>(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return JSV8BreakIterator::BreakType(isolate, break_iterator);
}

}  // namespace internal
}  // namespace v8

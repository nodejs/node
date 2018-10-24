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
#include "src/date.h"
#include "src/elements.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/property-descriptor.h"

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
  TO_THIS_STRING(string, "String.prototype.normalize");

  Handle<Object> form_input = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Intl::Normalize(isolate, string, form_input));
}

BUILTIN(V8BreakIteratorSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.v8BreakIterator.supportedLocalesOf",
                   JSV8BreakIterator::GetAvailableLocales(), locales, options));
}

BUILTIN(NumberFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.NumberFormat.supportedLocalesOf",
                   JSNumberFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(NumberFormatPrototypeFormatToParts) {
  const char* const method = "Intl.NumberFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSNumberFormat, number_format, method);

  Handle<Object> x;
  if (args.length() >= 2) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                       Object::ToNumber(isolate, args.at(1)));
  } else {
    x = isolate->factory()->nan_value();
  }

  RETURN_RESULT_OR_FAILURE(isolate, JSNumberFormat::FormatToParts(
                                        isolate, number_format, x->Number()));
}

BUILTIN(DateTimeFormatPrototypeResolvedOptions) {
  const char* const method = "Intl.DateTimeFormat.prototype.resolvedOptions";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSReceiver, format_holder, method);

  // 3. Let dtf be ? UnwrapDateTimeFormat(dtf).
  Handle<JSDateTimeFormat> date_time_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, date_time_format,
      JSDateTimeFormat::UnwrapDateTimeFormat(isolate, format_holder));

  RETURN_RESULT_OR_FAILURE(
      isolate, JSDateTimeFormat::ResolvedOptions(isolate, date_time_format));
}

BUILTIN(DateTimeFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.DateTimeFormat.supportedLocalesOf",
                   JSDateTimeFormat::GetAvailableLocales(), locales, options));
}

BUILTIN(DateTimeFormatPrototypeFormatToParts) {
  const char* const method = "Intl.DateTimeFormat.prototype.formatToParts";
  HandleScope handle_scope(isolate);
  CHECK_RECEIVER(JSObject, date_format_holder, method);
  Factory* factory = isolate->factory();

  if (!date_format_holder->IsJSDateTimeFormat()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              factory->NewStringFromAsciiChecked(method),
                              date_format_holder));
  }
  Handle<JSDateTimeFormat> dtf =
      Handle<JSDateTimeFormat>::cast(date_format_holder);

  Handle<Object> x = args.atOrUndefined(isolate, 1);
  if (x->IsUndefined(isolate)) {
    x = factory->NewNumber(JSDate::CurrentTimeValue(isolate));
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, x,
                                       Object::ToNumber(isolate, args.at(1)));
  }

  double date_value = DateCache::TimeClip(x->Number());
  if (std::isnan(date_value)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidTimeValue));
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, JSDateTimeFormat::FormatToParts(isolate, dtf, date_value));
}

namespace {
Handle<JSFunction> CreateBoundFunction(Isolate* isolate,
                                       Handle<JSObject> object,
                                       Builtins::Name builtin_id, int len) {
  Handle<NativeContext> native_context(isolate->context()->native_context(),
                                       isolate);
  Handle<Context> context = isolate->factory()->NewBuiltinContext(
      native_context,
      static_cast<int>(Intl::BoundFunctionContextSlot::kLength));

  context->set(static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction),
               *object);

  Handle<SharedFunctionInfo> info =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), builtin_id, kNormalFunction);
  info->set_internal_formal_parameter_count(len);
  info->set_length(len);

  Handle<Map> map = isolate->strict_function_without_prototype_map();

  Handle<JSFunction> new_bound_function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(map, info, context);
  return new_bound_function;
}

/**
 * Common code shared between DateTimeFormatConstructor and
 * NumberFormatConstrutor
 */
template <class T>
Object* FormatConstructor(BuiltinArguments args, Isolate* isolate,
                          Handle<Object> constructor, const char* method) {
  Handle<JSReceiver> new_target;
  // 1. If NewTarget is undefined, let newTarget be the active
  // function object, else let newTarget be NewTarget.
  if (args.new_target()->IsUndefined(isolate)) {
    new_target = args.target();
  } else {
    new_target = Handle<JSReceiver>::cast(args.new_target());
  }

  // [[Construct]]
  Handle<JSFunction> target = args.target();

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 2. Let format be ? OrdinaryCreateFromConstructor(newTarget,
  // "%<T>Prototype%", ...).

  Handle<JSObject> format_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, format_obj,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<T> format = Handle<T>::cast(format_obj);

  // 3. Perform ? Initialize<T>(Format, locales, options).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, format, T::Initialize(isolate, format, locales, options));
  // 4. Let this be the this value.
  Handle<Object> receiver = args.receiver();

  // 5. If NewTarget is undefined and ? InstanceofOperator(this, %<T>%)
  // is true, then
  //
  // Look up the intrinsic value that has been stored on the context.
  // Call the instanceof function
  Handle<Object> is_instance_of_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, is_instance_of_obj,
      Object::InstanceOf(isolate, receiver, constructor));

  // Get the boolean value of the result
  bool is_instance_of = is_instance_of_obj->BooleanValue(isolate);

  if (args.new_target()->IsUndefined(isolate) && is_instance_of) {
    if (!receiver->IsJSReceiver()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                       isolate->factory()->NewStringFromAsciiChecked(method),
                       receiver));
    }
    Handle<JSReceiver> rec = Handle<JSReceiver>::cast(receiver);
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
        kThrowOnError);
    MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
    CHECK(success.FromJust());
    // b. b. Return this.
    return *receiver;
  }
  // 6. Return format.
  return *format;
}

}  // namespace

BUILTIN(NumberFormatConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kNumberFormat);

  return FormatConstructor<JSNumberFormat>(
      args, isolate, isolate->intl_number_format_function(),
      "Intl.NumberFormat");
}

BUILTIN(NumberFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  const char* const method = "Intl.NumberFormat.prototype.resolvedOptions";

  // 1. Let nf be the this value.
  // 2. If Type(nf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, number_format_holder, method);

  // 3. Let nf be ? UnwrapNumberFormat(nf)
  Handle<JSNumberFormat> number_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, number_format,
      JSNumberFormat::UnwrapNumberFormat(isolate, number_format_holder));

  return *JSNumberFormat::ResolvedOptions(isolate, number_format);
}

BUILTIN(NumberFormatPrototypeFormatNumber) {
  const char* const method = "get Intl.NumberFormat.prototype.format";
  HandleScope scope(isolate);

  // 1. Let nf be the this value.
  // 2. If Type(nf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, receiver, method);

  // 3. Let nf be ? UnwrapNumberFormat(nf).
  Handle<JSNumberFormat> number_format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, number_format,
      JSNumberFormat::UnwrapNumberFormat(isolate, receiver));

  Handle<Object> bound_format(number_format->bound_format(), isolate);

  // 4. If nf.[[BoundFormat]] is undefined, then
  if (!bound_format->IsUndefined(isolate)) {
    DCHECK(bound_format->IsJSFunction());
    // 5. Return nf.[[BoundFormat]].
    return *bound_format;
  }

  Handle<JSFunction> new_bound_format_function = CreateBoundFunction(
      isolate, number_format, Builtins::kNumberFormatInternalFormatNumber, 1);

  // 4. c. Set nf.[[BoundFormat]] to F.
  number_format->set_bound_format(*new_bound_format_function);

  // 5. Return nf.[[BoundFormat]].
  return *new_bound_format_function;
}

BUILTIN(NumberFormatInternalFormatNumber) {
  HandleScope scope(isolate);

  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  // 1. Let nf be F.[[NumberFormat]].
  // 2. Assert: Type(nf) is Object and nf has an
  //    [[InitializedNumberFormat]] internal slot.
  Handle<JSNumberFormat> number_format = Handle<JSNumberFormat>(
      JSNumberFormat::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  // 3. If value is not provided, let value be undefined.
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  // 4. Let x be ? ToNumber(value).
  Handle<Object> number_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_obj,
                                     Object::ToNumber(isolate, value));

  // Spec treats -0 as 0.
  if (number_obj->IsMinusZero()) {
    number_obj = Handle<Smi>(Smi::kZero, isolate);
  }

  double number = number_obj->Number();
  // Return FormatNumber(nf, x).
  RETURN_RESULT_OR_FAILURE(
      isolate, JSNumberFormat::FormatNumber(isolate, number_format, number));
}

BUILTIN(DateTimeFormatConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kDateTimeFormat);

  return FormatConstructor<JSDateTimeFormat>(
      args, isolate, isolate->intl_date_time_format_function(),
      "Intl.DateTimeFormat");
}

BUILTIN(DateTimeFormatPrototypeFormat) {
  const char* const method = "get Intl.DateTimeFormat.prototype.format";
  HandleScope scope(isolate);

  // 1. Let dtf be this value.
  // 2. If Type(dtf) is not Object, throw a TypeError exception.
  CHECK_RECEIVER(JSReceiver, receiver, method);

  // 3. Let dtf be ? UnwrapDateTimeFormat(dtf).
  Handle<JSDateTimeFormat> format;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, format,
      JSDateTimeFormat::UnwrapDateTimeFormat(isolate, receiver));

  Handle<Object> bound_format = Handle<Object>(format->bound_format(), isolate);

  // 4. If dtf.[[BoundFormat]] is undefined, then
  if (!bound_format->IsUndefined(isolate)) {
    DCHECK(bound_format->IsJSFunction());
    // 5. Return dtf.[[BoundFormat]].
    return *bound_format;
  }

  Handle<JSFunction> new_bound_format_function = CreateBoundFunction(
      isolate, format, Builtins::kDateTimeFormatInternalFormat, 1);

  // 4.c. Set dtf.[[BoundFormat]] to F.
  format->set_bound_format(*new_bound_format_function);

  // 5. Return dtf.[[BoundFormat]].
  return *new_bound_format_function;
}

BUILTIN(DateTimeFormatInternalFormat) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  // 1. Let dtf be F.[[DateTimeFormat]].
  // 2. Assert: Type(dtf) is Object and dtf has an [[InitializedDateTimeFormat]]
  // internal slot.
  Handle<JSDateTimeFormat> date_format_holder = Handle<JSDateTimeFormat>(
      JSDateTimeFormat::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  Handle<Object> date = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate, JSDateTimeFormat::DateTimeFormat(
                                        isolate, date_format_holder, date));
}

BUILTIN(IntlGetCanonicalLocales) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);

  RETURN_RESULT_OR_FAILURE(isolate,
                           Intl::GetCanonicalLocales(isolate, locales));
}

BUILTIN(ListFormatConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kListFormat);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.ListFormat")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<JSObject> result;
  // 2. Let listFormat be OrdinaryCreateFromConstructor(NewTarget,
  //    "%ListFormatPrototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSListFormat> format = Handle<JSListFormat>::cast(result);
  format->set_flags(0);

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 3. Return InitializeListFormat(listFormat, locales, options).
  RETURN_RESULT_OR_FAILURE(
      isolate, JSListFormat::Initialize(isolate, format, locales, options));
}

BUILTIN(ListFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSListFormat, format_holder,
                 "Intl.ListFormat.prototype.resolvedOptions");
  return *JSListFormat::ResolvedOptions(isolate, format_holder);
}

BUILTIN(ListFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.ListFormat.supportedLocalesOf",
                   JSListFormat::GetAvailableLocales(), locales, options));
}

namespace {

MaybeHandle<JSLocale> CreateLocale(Isolate* isolate,
                                   Handle<JSFunction> constructor,
                                   Handle<JSReceiver> new_target,
                                   Handle<Object> tag, Handle<Object> options) {
  Handle<JSObject> locale;
  // 6. Let locale be ? OrdinaryCreateFromConstructor(NewTarget,
  // %LocalePrototype%, internalSlotsList).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, locale,
      JSObject::New(constructor, new_target, Handle<AllocationSite>::null()),
      JSLocale);

  // 7. If Type(tag) is not String or Object, throw a TypeError exception.
  if (!tag->IsString() && !tag->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kLocaleNotEmpty),
                    JSLocale);
  }

  Handle<String> locale_string;
  // 8. If Type(tag) is Object and tag has an [[InitializedLocale]] internal
  // slot, then
  if (tag->IsJSLocale() && Handle<JSLocale>::cast(tag)->locale()->IsString()) {
    // a. Let tag be tag.[[Locale]].
    locale_string =
        Handle<String>(Handle<JSLocale>::cast(tag)->locale(), isolate);
  } else {  // 9. Else,
    // a. Let tag be ? ToString(tag).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, locale_string,
                               Object::ToString(isolate, tag), JSLocale);
  }

  Handle<JSReceiver> options_object;
  // 10. If options is undefined, then
  if (options->IsUndefined(isolate)) {
    // a. Let options be ! ObjectCreate(null).
    options_object = isolate->factory()->NewJSObjectWithNullProto();
  } else {  // 11. Else
    // a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, options_object,
                               Object::ToObject(isolate, options), JSLocale);
  }

  return JSLocale::Initialize(isolate, Handle<JSLocale>::cast(locale),
                              locale_string, options_object);
}

}  // namespace

// Intl.Locale implementation
BUILTIN(LocaleConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kLocale);

  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Intl.Locale")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<Object> tag = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLocale(isolate, target, new_target, tag, options));
}

BUILTIN(LocalePrototypeMaximize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.maximize");
  Handle<JSFunction> constructor(
      isolate->native_context()->intl_locale_function(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      CreateLocale(isolate, constructor, constructor,
                   JSLocale::Maximize(isolate, locale_holder->locale()),
                   isolate->factory()->NewJSObjectWithNullProto()));
}

BUILTIN(LocalePrototypeMinimize) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.minimize");
  Handle<JSFunction> constructor(
      isolate->native_context()->intl_locale_function(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      CreateLocale(isolate, constructor, constructor,
                   JSLocale::Minimize(isolate, locale_holder->locale()),
                   isolate->factory()->NewJSObjectWithNullProto()));
}

BUILTIN(RelativeTimeFormatSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

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
                                            format_holder, "format", false));
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
  RETURN_RESULT_OR_FAILURE(isolate, JSRelativeTimeFormat::Format(
                                        isolate, value_obj, unit_obj,
                                        format_holder, "formatToParts", true));
}

// Locale getters.
BUILTIN(LocalePrototypeLanguage) {
  HandleScope scope(isolate);
  // CHECK_RECEIVER will case locale_holder to JSLocale.
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.language");

  return locale_holder->language();
}

BUILTIN(LocalePrototypeScript) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.script");

  return locale_holder->script();
}

BUILTIN(LocalePrototypeRegion) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.region");

  return locale_holder->region();
}

BUILTIN(LocalePrototypeBaseName) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.baseName");

  return locale_holder->base_name();
}

BUILTIN(LocalePrototypeCalendar) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.calendar");

  return locale_holder->calendar();
}

BUILTIN(LocalePrototypeCaseFirst) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.caseFirst");

  return *(locale_holder->CaseFirstAsString());
}

BUILTIN(LocalePrototypeCollation) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.collation");

  return locale_holder->collation();
}

BUILTIN(LocalePrototypeHourCycle) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.hourCycle");

  return *(locale_holder->HourCycleAsString());
}

BUILTIN(LocalePrototypeNumeric) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.numeric");

  switch (locale_holder->numeric()) {
    case JSLocale::Numeric::TRUE_VALUE:
      return *(isolate->factory()->true_value());
    case JSLocale::Numeric::FALSE_VALUE:
      return *(isolate->factory()->false_value());
    case JSLocale::Numeric::NOTSET:
      return *(isolate->factory()->undefined_value());
    case JSLocale::Numeric::COUNT:
      UNREACHABLE();
  }
}

BUILTIN(LocalePrototypeNumberingSystem) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder,
                 "Intl.Locale.prototype.numberingSystem");

  return locale_holder->numbering_system();
}

BUILTIN(LocalePrototypeToString) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSLocale, locale_holder, "Intl.Locale.prototype.toString");

  return locale_holder->locale();
}

BUILTIN(RelativeTimeFormatConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kRelativeTimeFormat);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.RelativeTimeFormat")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<JSObject> result;
  // 2. Let relativeTimeFormat be
  //    ! OrdinaryCreateFromConstructor(NewTarget,
  //                                    "%RelativeTimeFormatPrototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSRelativeTimeFormat> format =
      Handle<JSRelativeTimeFormat>::cast(result);
  format->set_flags(0);

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 3. Return ? InitializeRelativeTimeFormat(relativeTimeFormat, locales,
  //                                          options).
  RETURN_RESULT_OR_FAILURE(isolate, JSRelativeTimeFormat::Initialize(
                                        isolate, format, locales, options));
}

BUILTIN(RelativeTimeFormatPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSRelativeTimeFormat, format_holder,
                 "Intl.RelativeTimeFormat.prototype.resolvedOptions");
  return *JSRelativeTimeFormat::ResolvedOptions(isolate, format_holder);
}

BUILTIN(StringPrototypeToLocaleLowerCase) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kStringToLocaleLowerCase);

  TO_THIS_STRING(string, "String.prototype.toLocaleLowerCase");

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::StringLocaleConvertCase(isolate, string, false,
                                             args.atOrUndefined(isolate, 1)));
}

BUILTIN(StringPrototypeToLocaleUpperCase) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kStringToLocaleUpperCase);

  TO_THIS_STRING(string, "String.prototype.toLocaleUpperCase");

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::StringLocaleConvertCase(isolate, string, true,
                                             args.atOrUndefined(isolate, 1)));
}

BUILTIN(PluralRulesConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kPluralRules);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.PluralRules")));
  }

  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 2. Let pluralRules be ? OrdinaryCreateFromConstructor(newTarget,
  // "%PluralRulesPrototype%", « [[InitializedPluralRules]],
  // [[Locale]], [[Type]], [[MinimumIntegerDigits]],
  // [[MinimumFractionDigits]], [[MaximumFractionDigits]],
  // [[MinimumSignificantDigits]], [[MaximumSignificantDigits]] »).
  Handle<JSObject> plural_rules_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, plural_rules_obj,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSPluralRules> plural_rules =
      Handle<JSPluralRules>::cast(plural_rules_obj);
  plural_rules->set_flags(0);

  // 3. Return ? InitializePluralRules(pluralRules, locales, options).
  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSPluralRules::Initialize(isolate, plural_rules, locales, options));
}

BUILTIN(PluralRulesPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSPluralRules, plural_rules_holder,
                 "Intl.PluralRules.prototype.resolvedOptions");
  return *JSPluralRules::ResolvedOptions(isolate, plural_rules_holder);
}

BUILTIN(PluralRulesPrototypeSelect) {
  HandleScope scope(isolate);

  // 1. Let pr be the this value.
  // 2. If Type(pr) is not Object, throw a TypeError exception.
  // 3. If pr does not have an [[InitializedPluralRules]] internal slot, throw a
  // TypeError exception.
  CHECK_RECEIVER(JSPluralRules, plural_rules,
                 "Intl.PluralRules.prototype.select");

  // 4. Let n be ? ToNumber(value).
  Handle<Object> number = args.atOrUndefined(isolate, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number,
                                     Object::ToNumber(isolate, number));
  double number_double = number->Number();

  // 5. Return ? ResolvePlural(pr, n).
  RETURN_RESULT_OR_FAILURE(isolate, JSPluralRules::ResolvePlural(
                                        isolate, plural_rules, number_double));
}

BUILTIN(PluralRulesSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.PluralRules.supportedLocalesOf",
                   JSPluralRules::GetAvailableLocales(), locales, options));
}

BUILTIN(CollatorConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kCollator);

  Handle<JSReceiver> new_target;
  // 1. If NewTarget is undefined, let newTarget be the active
  // function object, else let newTarget be NewTarget.
  if (args.new_target()->IsUndefined(isolate)) {
    new_target = args.target();
  } else {
    new_target = Handle<JSReceiver>::cast(args.new_target());
  }

  // [[Construct]]
  Handle<JSFunction> target = args.target();

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  // 5. Let collator be ? OrdinaryCreateFromConstructor(newTarget,
  // "%CollatorPrototype%", internalSlotsList).
  Handle<JSObject> collator_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, collator_obj,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSCollator> collator = Handle<JSCollator>::cast(collator_obj);

  // 6. Return ? InitializeCollator(collator, locales, options).
  RETURN_RESULT_OR_FAILURE(
      isolate, JSCollator::Initialize(isolate, collator, locales, options));
}

BUILTIN(CollatorPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSCollator, collator_holder,
                 "Intl.Collator.prototype.resolvedOptions");
  return *JSCollator::ResolvedOptions(isolate, collator_holder);
}

BUILTIN(CollatorSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.Collator.supportedLocalesOf",
                   JSCollator::GetAvailableLocales(), locales, options));
}

BUILTIN(CollatorPrototypeCompare) {
  const char* const method = "get Intl.Collator.prototype.compare";
  HandleScope scope(isolate);

  // 1. Let collator be this value.
  // 2. If Type(collator) is not Object, throw a TypeError exception.
  // 3. If collator does not have an [[InitializedCollator]] internal slot,
  // throw a TypeError exception.
  CHECK_RECEIVER(JSCollator, collator, method);

  // 4. If collator.[[BoundCompare]] is undefined, then
  Handle<Object> bound_compare(collator->bound_compare(), isolate);
  if (!bound_compare->IsUndefined(isolate)) {
    DCHECK(bound_compare->IsJSFunction());
    // 5. Return collator.[[BoundCompare]].
    return *bound_compare;
  }

  Handle<JSFunction> new_bound_compare_function = CreateBoundFunction(
      isolate, collator, Builtins::kCollatorInternalCompare, 2);

  // 4.c. Set collator.[[BoundCompare]] to F.
  collator->set_bound_compare(*new_bound_compare_function);

  // 5. Return collator.[[BoundCompare]].
  return *new_bound_compare_function;
}

BUILTIN(CollatorInternalCompare) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  // 1. Let collator be F.[[Collator]].
  // 2. Assert: Type(collator) is Object and collator has an
  // [[InitializedCollator]] internal slot.
  Handle<JSCollator> collator_holder = Handle<JSCollator>(
      JSCollator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  // 3. If x is not provided, let x be undefined.
  Handle<Object> x = args.atOrUndefined(isolate, 1);
  // 4. If y is not provided, let y be undefined.
  Handle<Object> y = args.atOrUndefined(isolate, 2);

  // 5. Let X be ? ToString(x).
  Handle<String> string_x;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, string_x,
                                     Object::ToString(isolate, x));
  // 6. Let Y be ? ToString(y).
  Handle<String> string_y;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, string_y,
                                     Object::ToString(isolate, y));

  // 7. Return CompareStrings(collator, X, Y).
  return *Intl::CompareStrings(isolate, collator_holder, string_x, string_y);
}

// ecma402 #sec-segment-iterator-prototype-breakType
BUILTIN(SegmentIteratorPrototypeBreakType) {
  const char* const method = "get %SegmentIteratorPrototype%.breakType";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method);
  return *segment_iterator->BreakType();
}

// ecma402 #sec-segment-iterator-prototype-following
BUILTIN(SegmentIteratorPrototypeFollowing) {
  const char* const method = "%SegmentIteratorPrototype%.following";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method);

  Handle<Object> from = args.atOrUndefined(isolate, 1);

  Maybe<bool> success =
      JSSegmentIterator::Following(isolate, segment_iterator, from);
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(success.FromJust());
}

// ecma402 #sec-segment-iterator-prototype-next
BUILTIN(SegmentIteratorPrototypeNext) {
  const char* const method = "%SegmentIteratorPrototype%.next";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method);

  RETURN_RESULT_OR_FAILURE(isolate,
                           JSSegmentIterator::Next(isolate, segment_iterator));
}

// ecma402 #sec-segment-iterator-prototype-preceding
BUILTIN(SegmentIteratorPrototypePreceding) {
  const char* const method = "%SegmentIteratorPrototype%.preceding";
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method);

  Handle<Object> from = args.atOrUndefined(isolate, 1);

  Maybe<bool> success =
      JSSegmentIterator::Preceding(isolate, segment_iterator, from);
  MAYBE_RETURN(success, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(success.FromJust());
}

// ecma402 #sec-segment-iterator-prototype-position
BUILTIN(SegmentIteratorPrototypePosition) {
  const char* const method = "get %SegmentIteratorPrototype%.position";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSSegmentIterator, segment_iterator, method);
  return *JSSegmentIterator::Position(isolate, segment_iterator);
}

BUILTIN(SegmenterConstructor) {
  HandleScope scope(isolate);

  isolate->CountUsage(v8::Isolate::UseCounterFeature::kSegmenter);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              isolate->factory()->NewStringFromStaticChars(
                                  "Intl.Segmenter")));
  }
  // [[Construct]]
  Handle<JSFunction> target = args.target();
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());

  Handle<JSObject> result;
  // 2. Let segmenter be OrdinaryCreateFromConstructor(NewTarget,
  //    "%SegmenterPrototype%").
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSSegmenter> segmenter = Handle<JSSegmenter>::cast(result);
  segmenter->set_flags(0);

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSSegmenter::Initialize(isolate, segmenter, locales, options));
}

BUILTIN(SegmenterSupportedLocalesOf) {
  HandleScope scope(isolate);
  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, Intl::SupportedLocalesOf(
                   isolate, "Intl.Segmenter.supportedLocalesOf",
                   JSSegmenter::GetAvailableLocales(), locales, options));
}

BUILTIN(SegmenterPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmenter, segmenter_holder,
                 "Intl.Segmenter.prototype.resolvedOptions");
  return *JSSegmenter::ResolvedOptions(isolate, segmenter_holder);
}

// ecma402 #sec-Intl.Segmenter.prototype.segment
BUILTIN(SegmenterPrototypeSegment) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSSegmenter, segmenter_holder,
                 "Intl.Segmenter.prototype.segment");
  Handle<Object> input_text = args.atOrUndefined(isolate, 1);
  // 3. Let string be ? ToString(string).
  Handle<String> text;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, text,
                                     Object::ToString(isolate, input_text));

  // 4. Return ? CreateSegmentIterator(segment, string).
  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSSegmentIterator::Create(
          isolate, segmenter_holder->icu_break_iterator()->raw()->clone(),
          segmenter_holder->granularity(), text));
}

BUILTIN(V8BreakIteratorConstructor) {
  HandleScope scope(isolate);
  Handle<JSReceiver> new_target;

  if (args.new_target()->IsUndefined(isolate)) {
    new_target = args.target();
  } else {
    new_target = Handle<JSReceiver>::cast(args.new_target());
  }

  // [[Construct]]
  Handle<JSFunction> target = args.target();

  Handle<Object> locales = args.atOrUndefined(isolate, 1);
  Handle<Object> options = args.atOrUndefined(isolate, 2);

  Handle<JSObject> break_iterator_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, break_iterator_obj,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  Handle<JSV8BreakIterator> break_iterator =
      Handle<JSV8BreakIterator>::cast(break_iterator_obj);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      JSV8BreakIterator::Initialize(isolate, break_iterator, locales, options));
}

BUILTIN(V8BreakIteratorPrototypeResolvedOptions) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSV8BreakIterator, break_iterator,
                 "Intl.v8BreakIterator.prototype.resolvedOptions");
  return *JSV8BreakIterator::ResolvedOptions(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeAdoptText) {
  const char* const method = "get Intl.v8BreakIterator.prototype.adoptText";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method);

  Handle<Object> bound_adopt_text(break_iterator->bound_adopt_text(), isolate);
  if (!bound_adopt_text->IsUndefined(isolate)) {
    DCHECK(bound_adopt_text->IsJSFunction());
    return *bound_adopt_text;
  }

  Handle<JSFunction> new_bound_adopt_text_function = CreateBoundFunction(
      isolate, break_iterator, Builtins::kV8BreakIteratorInternalAdoptText, 1);
  break_iterator->set_bound_adopt_text(*new_bound_adopt_text_function);
  return *new_bound_adopt_text_function;
}

BUILTIN(V8BreakIteratorInternalAdoptText) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  Handle<JSV8BreakIterator> break_iterator = Handle<JSV8BreakIterator>(
      JSV8BreakIterator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  Handle<Object> input_text = args.atOrUndefined(isolate, 1);
  Handle<String> text;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, text,
                                     Object::ToString(isolate, input_text));

  JSV8BreakIterator::AdoptText(isolate, break_iterator, text);
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(V8BreakIteratorPrototypeFirst) {
  const char* const method = "get Intl.v8BreakIterator.prototype.first";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method);

  Handle<Object> bound_first(break_iterator->bound_first(), isolate);
  if (!bound_first->IsUndefined(isolate)) {
    DCHECK(bound_first->IsJSFunction());
    return *bound_first;
  }

  Handle<JSFunction> new_bound_first_function = CreateBoundFunction(
      isolate, break_iterator, Builtins::kV8BreakIteratorInternalFirst, 0);
  break_iterator->set_bound_first(*new_bound_first_function);
  return *new_bound_first_function;
}

BUILTIN(V8BreakIteratorInternalFirst) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  Handle<JSV8BreakIterator> break_iterator = Handle<JSV8BreakIterator>(
      JSV8BreakIterator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);

  return *JSV8BreakIterator::First(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeNext) {
  const char* const method = "get Intl.v8BreakIterator.prototype.next";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method);

  Handle<Object> bound_next(break_iterator->bound_next(), isolate);
  if (!bound_next->IsUndefined(isolate)) {
    DCHECK(bound_next->IsJSFunction());
    return *bound_next;
  }

  Handle<JSFunction> new_bound_next_function = CreateBoundFunction(
      isolate, break_iterator, Builtins::kV8BreakIteratorInternalNext, 0);
  break_iterator->set_bound_next(*new_bound_next_function);
  return *new_bound_next_function;
}

BUILTIN(V8BreakIteratorInternalNext) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  Handle<JSV8BreakIterator> break_iterator = Handle<JSV8BreakIterator>(
      JSV8BreakIterator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return *JSV8BreakIterator::Next(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeCurrent) {
  const char* const method = "get Intl.v8BreakIterator.prototype.current";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method);

  Handle<Object> bound_current(break_iterator->bound_current(), isolate);
  if (!bound_current->IsUndefined(isolate)) {
    DCHECK(bound_current->IsJSFunction());
    return *bound_current;
  }

  Handle<JSFunction> new_bound_current_function = CreateBoundFunction(
      isolate, break_iterator, Builtins::kV8BreakIteratorInternalCurrent, 0);
  break_iterator->set_bound_current(*new_bound_current_function);
  return *new_bound_current_function;
}

BUILTIN(V8BreakIteratorInternalCurrent) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  Handle<JSV8BreakIterator> break_iterator = Handle<JSV8BreakIterator>(
      JSV8BreakIterator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return *JSV8BreakIterator::Current(isolate, break_iterator);
}

BUILTIN(V8BreakIteratorPrototypeBreakType) {
  const char* const method = "get Intl.v8BreakIterator.prototype.breakType";
  HandleScope scope(isolate);

  CHECK_RECEIVER(JSV8BreakIterator, break_iterator, method);

  Handle<Object> bound_break_type(break_iterator->bound_break_type(), isolate);
  if (!bound_break_type->IsUndefined(isolate)) {
    DCHECK(bound_break_type->IsJSFunction());
    return *bound_break_type;
  }

  Handle<JSFunction> new_bound_break_type_function = CreateBoundFunction(
      isolate, break_iterator, Builtins::kV8BreakIteratorInternalBreakType, 0);
  break_iterator->set_bound_break_type(*new_bound_break_type_function);
  return *new_bound_break_type_function;
}

BUILTIN(V8BreakIteratorInternalBreakType) {
  HandleScope scope(isolate);
  Handle<Context> context = Handle<Context>(isolate->context(), isolate);

  Handle<JSV8BreakIterator> break_iterator = Handle<JSV8BreakIterator>(
      JSV8BreakIterator::cast(context->get(
          static_cast<int>(Intl::BoundFunctionContextSlot::kBoundFunction))),
      isolate);
  return JSV8BreakIterator::BreakType(isolate, break_iterator);
}

}  // namespace internal
}  // namespace v8

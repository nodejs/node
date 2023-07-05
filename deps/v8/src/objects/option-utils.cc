// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/option-utils.h"

#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// ecma402/#sec-getoptionsobject
MaybeHandle<JSReceiver> GetOptionsObject(Isolate* isolate,
                                         Handle<Object> options,
                                         const char* method_name) {
  // 1. If options is undefined, then
  if (options->IsUndefined(isolate)) {
    // a. Return ! ObjectCreate(null).
    return isolate->factory()->NewJSObjectWithNullProto();
  }
  // 2. If Type(options) is Object, then
  if (options->IsJSReceiver()) {
    // a. Return options.
    return Handle<JSReceiver>::cast(options);
  }
  // 3. Throw a TypeError exception.
  THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kInvalidArgument),
                  JSReceiver);
}

// ecma402/#sec-coerceoptionstoobject
MaybeHandle<JSReceiver> CoerceOptionsToObject(Isolate* isolate,
                                              Handle<Object> options,
                                              const char* method_name) {
  // 1. If options is undefined, then
  if (options->IsUndefined(isolate)) {
    // a. Return ! ObjectCreate(null).
    return isolate->factory()->NewJSObjectWithNullProto();
  }
  // 2. Return ? ToObject(options).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                             Object::ToObject(isolate, options, method_name),
                             JSReceiver);
  return Handle<JSReceiver>::cast(options);
}

Maybe<bool> GetStringOption(Isolate* isolate, Handle<JSReceiver> options,
                            const char* property,
                            const std::vector<const char*>& values,
                            const char* method_name,
                            std::unique_ptr<char[]>* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  if (value->IsUndefined(isolate)) {
    return Just(false);
  }

  // 2. c. Let value be ? ToString(value).
  Handle<String> value_str;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_str, Object::ToString(isolate, value), Nothing<bool>());
  std::unique_ptr<char[]> value_cstr = value_str->ToCString();

  // 2. d. if values is not undefined, then
  if (values.size() > 0) {
    // 2. d. i. If values does not contain an element equal to value,
    // throw a RangeError exception.
    for (size_t i = 0; i < values.size(); i++) {
      if (strcmp(values.at(i), value_cstr.get()) == 0) {
        // 2. e. return value
        *result = std::move(value_cstr);
        return Just(true);
      }
    }

    Handle<String> method_str =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kValueOutOfRange, value, method_str,
                      property_str),
        Nothing<bool>());
  }

  // 2. e. return value
  *result = std::move(value_cstr);
  return Just(true);
}

V8_WARN_UNUSED_RESULT Maybe<bool> GetBoolOption(Isolate* isolate,
                                                Handle<JSReceiver> options,
                                                const char* property,
                                                const char* method_name,
                                                bool* result) {
  Handle<String> property_str =
      isolate->factory()->NewStringFromAsciiChecked(property);

  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Object::GetPropertyOrElement(isolate, options, property_str),
      Nothing<bool>());

  // 2. If value is not undefined, then
  if (!value->IsUndefined(isolate)) {
    // 2. b. i. Let value be ToBoolean(value).
    *result = value->BooleanValue(isolate);

    // 2. e. return value
    return Just(true);
  }

  return Just(false);
}

// ecma402/#sec-defaultnumberoption
Maybe<int> DefaultNumberOption(Isolate* isolate, Handle<Object> value, int min,
                               int max, int fallback, Handle<String> property) {
  // 2. Else, return fallback.
  if (value->IsUndefined()) return Just(fallback);

  // 1. If value is not undefined, then
  // a. Let value be ? ToNumber(value).
  Handle<Object> value_num;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value_num, Object::ToNumber(isolate, value), Nothing<int>());
  DCHECK(value_num->IsNumber());

  // b. If value is NaN or less than minimum or greater than maximum, throw a
  // RangeError exception.
  if (value_num->IsNaN() || value_num->Number() < min ||
      value_num->Number() > max) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property),
        Nothing<int>());
  }

  // The max and min arguments are integers and the above check makes
  // sure that we are within the integer range making this double to
  // int conversion safe.
  //
  // c. Return floor(value).
  return Just(FastD2I(floor(value_num->Number())));
}

// ecma402/#sec-getnumberoption
Maybe<int> GetNumberOption(Isolate* isolate, Handle<JSReceiver> options,
                           Handle<String> property, int min, int max,
                           int fallback) {
  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, JSReceiver::GetProperty(isolate, options, property),
      Nothing<int>());

  // Return ? DefaultNumberOption(value, minimum, maximum, fallback).
  return DefaultNumberOption(isolate, value, min, max, fallback, property);
}

// #sec-getoption while type is "number"
Maybe<double> GetNumberOptionAsDouble(Isolate* isolate,
                                      Handle<JSReceiver> options,
                                      Handle<String> property,
                                      double default_value) {
  // 1. Let value be ? Get(options, property).
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, JSReceiver::GetProperty(isolate, options, property),
      Nothing<double>());
  // 2. If value is undefined, then
  if (value->IsUndefined()) {
    // b. Return default.
    return Just(default_value);
  }
  // 4. Else if type is "number", then
  // a. Set value to ? ToNumber(value).
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, Object::ToNumber(isolate, value), Nothing<double>());
  // b. If value is NaN, throw a RangeError exception.
  if (value->IsNaN()) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property),
        Nothing<double>());
  }

  // 7. Return value.
  return Just(value->Number());
}

}  // namespace internal
}  // namespace v8

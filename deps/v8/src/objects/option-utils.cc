// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/option-utils.h"

#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// ecma402/#sec-getoptionsobject
MaybeDirectHandle<JSReceiver> GetOptionsObject(Isolate* isolate,
                                               DirectHandle<Object> options,
                                               const char* method_name) {
  // 1. If options is undefined, then
  if (IsUndefined(*options, isolate)) {
    // a. Return ! ObjectCreate(null).
    return isolate->factory()->NewJSObjectWithNullProto();
  }
  // 2. If Type(options) is Object, then
  if (IsJSReceiver(*options)) {
    // a. Return options.
    return Cast<JSReceiver>(options);
  }
  // 3. Throw a TypeError exception.
  THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kInvalidArgument));
}

// ecma402/#sec-coerceoptionstoobject
MaybeDirectHandle<JSReceiver> CoerceOptionsToObject(
    Isolate* isolate, DirectHandle<Object> options, const char* method_name) {
  // 1. If options is undefined, then
  if (IsUndefined(*options, isolate)) {
    // a. Return ! ObjectCreate(null).
    return isolate->factory()->NewJSObjectWithNullProto();
  }
  // 2. Return ? ToObject(options).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, options,
                             Object::ToObject(isolate, options, method_name));
  return Cast<JSReceiver>(options);
}

Maybe<bool> GetStringOption(Isolate* isolate, DirectHandle<JSReceiver> options,
                            DirectHandle<String> property,
                            const char* method_name,
                            DirectHandle<String>* result) {
  // 1. Let value be ? Get(options, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, Object::GetPropertyOrElement(isolate, options, property));

  if (IsUndefined(*value, isolate)) {
    return Just(false);
  }

  // 2. c. Let value be ? ToString(value).
  DirectHandle<String> value_str;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value_str,
                             Object::ToString(isolate, value));

  // 2. d. if values is not undefined, then
  // Skip: this overload is only for when values is undefined

  // 2. e. return value
  *result = value_str;
  return Just(true);
}

V8_WARN_UNUSED_RESULT Maybe<bool> GetBoolOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    DirectHandle<String> property, const char* method_name, bool* result) {
  // 1. Let value be ? Get(options, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, Object::GetPropertyOrElement(isolate, options, property));

  // 2. If value is not undefined, then
  if (!IsUndefined(*value, isolate)) {
    // 2. b. i. Let value be ToBoolean(value).
    *result = Object::BooleanValue(*value, isolate);

    // 2. e. return value
    return Just(true);
  }

  return Just(false);
}

// ecma402/#sec-defaultnumberoption
Maybe<int> DefaultNumberOption(Isolate* isolate, DirectHandle<Object> value,
                               int min, int max, int fallback,
                               DirectHandle<String> property) {
  // 2. Else, return fallback.
  if (IsUndefined(*value)) return Just(fallback);

  // 1. If value is not undefined, then
  // a. Let value be ? ToNumber(value).
  DirectHandle<Number> value_num;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value_num,
                             Object::ToNumber(isolate, value));
  DCHECK(IsNumber(*value_num));

  // b. If value is NaN or less than minimum or greater than maximum, throw a
  // RangeError exception.
  if (IsNaN(*value_num) || Object::NumberValue(*value_num) < min ||
      Object::NumberValue(*value_num) > max) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property));
  }

  // The max and min arguments are integers and the above check makes
  // sure that we are within the integer range making this double to
  // int conversion safe.
  //
  // c. Return floor(value).
  return Just(FastD2I(floor(Object::NumberValue(*value_num))));
}

// ecma402/#sec-getnumberoption
Maybe<int> GetNumberOption(Isolate* isolate, DirectHandle<JSReceiver> options,
                           DirectHandle<String> property, int min, int max,
                           int fallback) {
  // 1. Let value be ? Get(options, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, JSReceiver::GetProperty(isolate, options, property));

  // Return ? DefaultNumberOption(value, minimum, maximum, fallback).
  return DefaultNumberOption(isolate, value, min, max, fallback, property);
}

// #sec-getoption while type is "number"
Maybe<double> GetNumberOptionAsDouble(Isolate* isolate,
                                      DirectHandle<JSReceiver> options,
                                      DirectHandle<String> property,
                                      double default_value) {
  // 1. Let value be ? Get(options, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, JSReceiver::GetProperty(isolate, options, property));
  // 2. If value is undefined, then
  if (IsUndefined(*value)) {
    // b. Return default.
    return Just(default_value);
  }
  // 4. Else if type is "number", then
  // a. Set value to ? ToNumber(value).
  DirectHandle<Number> value_num;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value_num,
                             Object::ToNumber(isolate, value));
  // b. If value is NaN, throw a RangeError exception.
  if (IsNaN(*value_num)) {
    THROW_NEW_ERROR(
        isolate,
        NewRangeError(MessageTemplate::kPropertyValueOutOfRange, property));
  }

  // 7. Return value.
  return Just(Object::NumberValue(*value_num));
}

}  // namespace internal
}  // namespace v8

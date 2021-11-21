// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OPTION_UTILS_H_
#define V8_OBJECTS_OPTION_UTILS_H_

#include "src/execution/isolate.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

// ecma402/#sec-getoptionsobject and temporal/#sec-getoptionsobject
V8_WARN_UNUSED_RESULT MaybeHandle<JSReceiver> GetOptionsObject(
    Isolate* isolate, Handle<Object> options, const char* method_name);

// ecma402/#sec-coerceoptionstoobject
V8_WARN_UNUSED_RESULT MaybeHandle<JSReceiver> CoerceOptionsToObject(
    Isolate* isolate, Handle<Object> options, const char* method_name);

// ECMA402 9.2.10. GetOption( options, property, type, values, fallback)
// ecma402/#sec-getoption and temporal/#sec-getoption
//
// This is specialized for the case when type is string.
//
// Instead of passing undefined for the values argument as the spec
// defines, pass in an empty vector.
//
// Returns true if options object has the property and stores the
// result in value. Returns false if the value is not found. The
// caller is required to use fallback value appropriately in this
// case.
//
// method_name is a string denoting the method the call from; used when
// printing the error message.
V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT Maybe<bool> GetStringOption(
    Isolate* isolate, Handle<JSReceiver> options, const char* property,
    std::vector<const char*> values, const char* method_name,
    std::unique_ptr<char[]>* result);

// A helper template to get string from option into a enum.
// The enum in the enum_values is the corresponding value to the strings
// in the str_values. If the option does not contains name,
// default_value will be return.
template <typename T>
V8_WARN_UNUSED_RESULT static Maybe<T> GetStringOption(
    Isolate* isolate, Handle<JSReceiver> options, const char* name,
    const char* method_name, const std::vector<const char*>& str_values,
    const std::vector<T>& enum_values, T default_value) {
  DCHECK_EQ(str_values.size(), enum_values.size());
  std::unique_ptr<char[]> cstr;
  Maybe<bool> found =
      GetStringOption(isolate, options, name, str_values, method_name, &cstr);
  MAYBE_RETURN(found, Nothing<T>());
  if (found.FromJust()) {
    DCHECK_NOT_NULL(cstr.get());
    for (size_t i = 0; i < str_values.size(); i++) {
      if (strcmp(cstr.get(), str_values[i]) == 0) {
        return Just(enum_values[i]);
      }
    }
    UNREACHABLE();
  }
  return Just(default_value);
}

// ECMA402 9.2.10. GetOption( options, property, type, values, fallback)
// ecma402/#sec-getoption
//
// This is specialized for the case when type is boolean.
//
// Returns true if options object has the property and stores the
// result in value. Returns false if the value is not found. The
// caller is required to use fallback value appropriately in this
// case.
//
// method_name is a string denoting the method it called from; used when
// printing the error message.
V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT Maybe<bool> GetBoolOption(
    Isolate* isolate, Handle<JSReceiver> options, const char* property,
    const char* method_name, bool* result);

V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT Maybe<int> GetNumberOption(
    Isolate* isolate, Handle<JSReceiver> options, Handle<String> property,
    int min, int max, int fallback);

// ecma402/#sec-defaultnumberoption
V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT Maybe<int> DefaultNumberOption(
    Isolate* isolate, Handle<Object> value, int min, int max, int fallback,
    Handle<String> property);

}  // namespace internal
}  // namespace v8
#endif  // V8_OBJECTS_OPTION_UTILS_H_

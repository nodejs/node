// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-value-utils.h"

#include "include/v8-container.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"

namespace v8_inspector {

v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> object,
                                   v8::Local<v8::Name> key,
                                   v8::Local<v8::Value> value) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::TryCatch tryCatch(isolate);
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return object->CreateDataProperty(context, key, value);
}

v8::Maybe<bool> createDataProperty(v8::Local<v8::Context> context,
                                   v8::Local<v8::Array> array, int index,
                                   v8::Local<v8::Value> value) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::TryCatch tryCatch(isolate);
  v8::Isolate::DisallowJavascriptExecutionScope throwJs(
      isolate, v8::Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);
  return array->CreateDataProperty(context, index, value);
}
}  // namespace v8_inspector

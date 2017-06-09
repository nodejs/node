// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

static void CheckObject(Isolate* isolate, Handle<Object> obj,
                        const char* string) {
  Object* print_string = *Object::NoSideEffectsToString(isolate, obj);
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}

static void CheckSmi(Isolate* isolate, int value, const char* string) {
  Handle<Object> handle(Smi::FromInt(value), isolate);
  CheckObject(isolate, handle, string);
}

static void CheckString(Isolate* isolate, const char* value,
                        const char* string) {
  Handle<String> handle(isolate->factory()->NewStringFromAsciiChecked(value));
  CheckObject(isolate, handle, string);
}

static void CheckNumber(Isolate* isolate, double value, const char* string) {
  Handle<Object> number = isolate->factory()->NewNumber(value);
  CHECK(number->IsNumber());
  CheckObject(isolate, number, string);
}

static void CheckBoolean(Isolate* isolate, bool value, const char* string) {
  CheckObject(isolate, value ? isolate->factory()->true_value()
                             : isolate->factory()->false_value(),
              string);
}

TEST(NoSideEffectsToString) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  HandleScope scope(isolate);

  CheckString(isolate, "fisk hest", "fisk hest");
  CheckNumber(isolate, 42.3, "42.3");
  CheckSmi(isolate, 42, "42");
  CheckBoolean(isolate, true, "true");
  CheckBoolean(isolate, false, "false");
  CheckBoolean(isolate, false, "false");
  CheckObject(isolate, factory->undefined_value(), "undefined");
  CheckObject(isolate, factory->null_value(), "null");

  CheckObject(isolate, factory->error_to_string(), "[object Error]");
  CheckObject(isolate, factory->stack_trace_symbol(),
              "Symbol(stack_trace_symbol)");
  CheckObject(isolate, factory->NewError(isolate->error_function(),
                                         factory->empty_string()),
              "Error");
  CheckObject(isolate, factory->NewError(
                           isolate->error_function(),
                           factory->NewStringFromAsciiChecked("fisk hest")),
              "Error: fisk hest");
  CheckObject(isolate, factory->NewJSObject(isolate->object_function()),
              "#<Object>");
}

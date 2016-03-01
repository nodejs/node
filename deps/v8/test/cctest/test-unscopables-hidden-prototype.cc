// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"
#include "test/cctest/cctest.h"

namespace {


static void Cleanup() {
  CompileRun(
      "delete object.x;"
      "delete hidden_prototype.x;"
      "delete object[Symbol.unscopables];"
      "delete hidden_prototype[Symbol.unscopables];");
}


TEST(Unscopables) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  v8::Local<v8::FunctionTemplate> t0 = v8::FunctionTemplate::New(isolate);
  v8::Local<v8::FunctionTemplate> t1 = v8::FunctionTemplate::New(isolate);

  t1->SetHiddenPrototype(true);

  v8::Local<v8::Object> object = t0->GetFunction(current_context)
                                     .ToLocalChecked()
                                     ->NewInstance(current_context)
                                     .ToLocalChecked();
  v8::Local<v8::Object> hidden_prototype = t1->GetFunction(current_context)
                                               .ToLocalChecked()
                                               ->NewInstance(current_context)
                                               .ToLocalChecked();

  CHECK(object->SetPrototype(current_context, hidden_prototype).FromJust());

  context->Global()
      ->Set(current_context, v8_str("object"), object)
      .FromMaybe(false);
  context->Global()
      ->Set(current_context, v8_str("hidden_prototype"), hidden_prototype)
      .FromMaybe(false);

  CHECK_EQ(1, CompileRun("var result;"
                         "var x = 0;"
                         "object.x = 1;"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(2, CompileRun("var result;"
                         "var x = 0;"
                         "hidden_prototype.x = 2;"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, CompileRun("var result;"
                         "var x = 0;"
                         "object.x = 3;"
                         "object[Symbol.unscopables] = {x: true};"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, CompileRun("var result;"
                         "var x = 0;"
                         "hidden_prototype.x = 4;"
                         "hidden_prototype[Symbol.unscopables] = {x: true};"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, CompileRun("var result;"
                         "var x = 0;"
                         "object.x = 5;"
                         "hidden_prototype[Symbol.unscopables] = {x: true};"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result;")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, CompileRun("var result;"
                         "var x = 0;"
                         "hidden_prototype.x = 6;"
                         "object[Symbol.unscopables] = {x: true};"
                         "with (object) {"
                         "  result = x;"
                         "}"
                         "result")
                  ->Int32Value(current_context)
                  .FromJust());
}

}  // namespace

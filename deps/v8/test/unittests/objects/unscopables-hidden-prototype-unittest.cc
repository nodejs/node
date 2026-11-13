// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "include/v8-function.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace unscopables_hidden_prototype_unittest {

class UnscopablesHiddenPrototypeTest : public TestWithContext {
 protected:
  void Cleanup() {
    RunJS(
        "delete object.x;"
        "delete prototype.x;"
        "delete object[Symbol.unscopables];"
        "delete prototype[Symbol.unscopables];");
  }
};

TEST_F(UnscopablesHiddenPrototypeTest, Unscopables) {
  v8::Isolate* isolate = v8_isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> current_context = v8_context();

  v8::Local<v8::FunctionTemplate> t0 = v8::FunctionTemplate::New(isolate);
  v8::Local<v8::FunctionTemplate> t1 = v8::FunctionTemplate::New(isolate);

  v8::Local<v8::Object> object = t0->GetFunction(current_context)
                                     .ToLocalChecked()
                                     ->NewInstance(current_context)
                                     .ToLocalChecked();
  v8::Local<v8::Object> prototype = t1->GetFunction(current_context)
                                        .ToLocalChecked()
                                        ->NewInstance(current_context)
                                        .ToLocalChecked();

  CHECK(object->SetPrototype(current_context, prototype).FromJust());

  current_context->Global()
      ->Set(current_context, NewString("object"), object)
      .FromMaybe(false);
  current_context->Global()
      ->Set(current_context, NewString("prototype"), prototype)
      .FromMaybe(false);

  CHECK_EQ(1, RunJS("var result;"
                    "var x = 0;"
                    "object.x = 1;"
                    "with (object) {"
                    "  result = x;"
                    "}"
                    "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(2, RunJS("var result;"
                    "var x = 0;"
                    "prototype.x = 2;"
                    "with (object) {"
                    "  result = x;"
                    "}"
                    "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, RunJS("var result;"
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
  CHECK_EQ(0, RunJS("var result;"
                    "var x = 0;"
                    "prototype.x = 4;"
                    "prototype[Symbol.unscopables] = {x: true};"
                    "with (object) {"
                    "  result = x;"
                    "}"
                    "result")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, RunJS("var result;"
                    "var x = 0;"
                    "object.x = 5;"
                    "prototype[Symbol.unscopables] = {x: true};"
                    "with (object) {"
                    "  result = x;"
                    "}"
                    "result;")
                  ->Int32Value(current_context)
                  .FromJust());

  Cleanup();
  CHECK_EQ(0, RunJS("var result;"
                    "var x = 0;"
                    "prototype.x = 6;"
                    "object[Symbol.unscopables] = {x: true};"
                    "with (object) {"
                    "  result = x;"
                    "}"
                    "result")
                  ->Int32Value(current_context)
                  .FromJust());
}

}  // namespace unscopables_hidden_prototype_unittest
}  // namespace internal
}  // namespace v8

// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-exception.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-template.h"
#include "src/api/api.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace {

using RuntimeTest = TestWithContext;

TEST_F(RuntimeTest, ReturnsPrototype) {
  TryCatch try_catch(isolate());

  Local<v8::Object> object = v8::Object::New(isolate());
  Handle<JSArray> i_result =
      Runtime::GetInternalProperties(i_isolate(), Utils::OpenHandle(*object))
          .ToHandleChecked();
  Local<Array> result = Utils::ToLocal(i_result);
  EXPECT_GE(result->Length(), 1u);

  char name_buffer[100];
  result->Get(context(), 0)
      .ToLocalChecked()
      .As<v8::String>()
      ->WriteUtf8(isolate(), name_buffer);
  EXPECT_EQ("[[Prototype]]", std::string(name_buffer));
}

bool AccessCheck(Local<v8::Context> accessing_context,
                 Local<v8::Object> accessed_object, Local<Value> data) {
  return false;
}

TEST_F(RuntimeTest, DoesNotReturnPrototypeWhenInacessible) {
  TryCatch try_catch(isolate());

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessCheckCallback(AccessCheck);

  Local<v8::Object> object =
      object_template->NewInstance(context()).ToLocalChecked();
  Handle<JSArray> i_result =
      Runtime::GetInternalProperties(i_isolate(), Utils::OpenHandle(*object))
          .ToHandleChecked();
  Local<Array> result = Utils::ToLocal(i_result);
  EXPECT_EQ(0u, result->Length());
}

}  // namespace
}  // namespace internal
}  // namespace v8

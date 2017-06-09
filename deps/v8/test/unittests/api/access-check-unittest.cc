// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using AccessCheckTest = TestWithIsolate;

namespace {

bool AccessCheck(Local<Context> accessing_context,
                 Local<Object> accessed_object, Local<Value> data) {
  return false;
}

MaybeLocal<Value> CompileRun(Isolate* isolate, const char* source) {
  Local<String> source_string =
      String::NewFromUtf8(isolate, source, NewStringType::kNormal)
          .ToLocalChecked();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Script> script =
      Script::Compile(context, source_string).ToLocalChecked();
  return script->Run(context);
}

}  // namespace

TEST_F(AccessCheckTest, GetOwnPropertyDescriptor) {
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](v8::Local<v8::Object> host, v8::AccessType type,
         v8::Local<v8::Value> data) {});
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallback(AccessCheck);

  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) {
        FAIL() << "This should never be called.";
        info.GetReturnValue().Set(42);
      });
  getter_template->SetAcceptAnyReceiver(false);
  Local<FunctionTemplate> setter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>& info) {
        FAIL() << "This should never be called.";
      });
  setter_template->SetAcceptAnyReceiver(false);
  global_template->SetAccessorProperty(
      String::NewFromUtf8(isolate(), "property", NewStringType::kNormal)
          .ToLocalChecked(),
      getter_template, setter_template);

  Local<Context> target_context =
      Context::New(isolate(), nullptr, global_template);
  Local<Context> accessing_context =
      Context::New(isolate(), nullptr, global_template);

  accessing_context->Global()
      ->Set(accessing_context,
            String::NewFromUtf8(isolate(), "other", NewStringType::kNormal)
                .ToLocalChecked(),
            target_context->Global())
      .FromJust();

  Context::Scope context_scope(accessing_context);
  Local<Value> result =
      CompileRun(isolate(),
                 "Object.getOwnPropertyDescriptor(this, 'property')"
                 "    .get.call(other);")
          .ToLocalChecked();
  EXPECT_TRUE(result->IsUndefined());
  CompileRun(isolate(),
             "Object.getOwnPropertyDescriptor(this, 'property')"
             "    .set.call(other, 42);");
}

}  // namespace v8

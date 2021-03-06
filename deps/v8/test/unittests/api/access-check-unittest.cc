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
      String::NewFromUtf8(isolate, source).ToLocalChecked();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Script> script =
      Script::Compile(context, source_string).ToLocalChecked();
  return script->Run(context);
}

v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x).ToLocalChecked();
}

}  // namespace

TEST_F(AccessCheckTest, GetOwnPropertyDescriptor) {
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](v8::Local<v8::Object> host, v8::AccessType type,
         v8::Local<v8::Value> data) {});
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallback(AccessCheck);

  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) { FAIL(); });
  getter_template->SetAcceptAnyReceiver(false);
  Local<FunctionTemplate> setter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>& info) { FAIL(); });
  setter_template->SetAcceptAnyReceiver(false);
  global_template->SetAccessorProperty(v8_str("property"), getter_template,
                                       setter_template);

  Local<Context> target_context =
      Context::New(isolate(), nullptr, global_template);
  Local<Context> accessing_context =
      Context::New(isolate(), nullptr, global_template);

  accessing_context->Global()
      ->Set(accessing_context, v8_str("other"), target_context->Global())
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

class AccessRegressionTest : public AccessCheckTest {
 protected:
  i::Handle<i::JSFunction> RetrieveFunctionFrom(Local<Context> context,
                                                const char* script) {
    Context::Scope context_scope(context);
    Local<Value> getter = CompileRun(isolate(), script).ToLocalChecked();
    EXPECT_TRUE(getter->IsFunction());

    i::Handle<i::JSReceiver> r =
        Utils::OpenHandle(*Local<Function>::Cast(getter));
    EXPECT_TRUE(r->IsJSFunction());
    return i::Handle<i::JSFunction>::cast(r);
  }
};

TEST_F(AccessRegressionTest,
       InstantiatedLazyAccessorPairsHaveCorrectNativeContext) {
  // The setup creates two contexts and sets an object created
  // in context 1 on the global of context 2.
  // The object has an accessor pair {property}. Accessing the
  // property descriptor of {property} causes instantiation of the
  // accessor pair. The test checks that the access pair has the
  // correct native context.
  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>&) { FAIL(); });
  Local<FunctionTemplate> setter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>&) { FAIL(); });

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessorProperty(v8_str("property"), getter_template,
                                       setter_template);

  Local<Context> context1 = Context::New(isolate(), nullptr);
  Local<Context> context2 = Context::New(isolate(), nullptr);

  Local<Object> object =
      object_template->NewInstance(context1).ToLocalChecked();
  context2->Global()
      ->Set(context2, v8_str("object_from_context1"), object)
      .Check();

  i::Handle<i::JSFunction> getter = RetrieveFunctionFrom(
      context2,
      "Object.getOwnPropertyDescriptor(object_from_context1, 'property').get");

  ASSERT_EQ(getter->native_context(), *Utils::OpenHandle(*context1));
}

// Regression test for https://crbug.com/986063.
TEST_F(AccessRegressionTest,
       InstantiatedLazyAccessorPairsHaveCorrectNativeContextDebug) {
  // The setup creates two contexts and installs an object "object"
  // on the global this for each context.
  // The object consists of:
  //    - an accessor pair "property".
  //    - a normal function "breakfn".
  //
  // The test sets a break point on {object.breakfn} in the first context.
  // This forces instantation of the JSFunction for the {object.property}
  // accessor pair. The test verifies afterwards that the respective
  // JSFunction of the getter have the correct native context.

  Local<FunctionTemplate> getter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>&) { FAIL(); });
  Local<FunctionTemplate> setter_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>&) { FAIL(); });
  Local<FunctionTemplate> break_template = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<v8::Value>&) { FAIL(); });

  Local<Context> context1 = Context::New(isolate(), nullptr);
  Local<Context> context2 = Context::New(isolate(), nullptr);

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->Set(isolate(), "breakfn", break_template);
  object_template->SetAccessorProperty(v8_str("property"), getter_template,
                                       setter_template);

  Local<Object> object1 =
      object_template->NewInstance(context1).ToLocalChecked();
  EXPECT_TRUE(
      context1->Global()->Set(context1, v8_str("object"), object1).IsJust());

  Local<Object> object2 =
      object_template->NewInstance(context2).ToLocalChecked();
  EXPECT_TRUE(
      context2->Global()->Set(context2, v8_str("object"), object2).IsJust());

  // Force instantiation of the JSFunction for the getter and setter
  // of {object.property} by setting a break point on {object.breakfn}
  {
    Context::Scope context_scope(context1);
    i::Isolate* iso = reinterpret_cast<i::Isolate*>(isolate());
    i::Handle<i::JSFunction> break_fn =
        RetrieveFunctionFrom(context1, "object.breakfn");

    int id;
    iso->debug()->SetBreakpointForFunction(i::handle(break_fn->shared(), iso),
                                           iso->factory()->empty_string(), &id);
  }

  i::Handle<i::JSFunction> getter_c1 = RetrieveFunctionFrom(
      context1, "Object.getOwnPropertyDescriptor(object, 'property').get");
  i::Handle<i::JSFunction> getter_c2 = RetrieveFunctionFrom(
      context2, "Object.getOwnPropertyDescriptor(object, 'property').get");

  ASSERT_EQ(getter_c1->native_context(), *Utils::OpenHandle(*context1));
  ASSERT_EQ(getter_c2->native_context(), *Utils::OpenHandle(*context2));
}

namespace {
bool failed_access_check_callback_called;

class AccessCheckTestConsoleDelegate : public debug::ConsoleDelegate {
 public:
  void Log(const debug::ConsoleCallArguments& args,
           const debug::ConsoleContext& context) {
    FAIL();
  }
};

}  // namespace

// Ensure that {console.log} does an access check for its arguments.
TEST_F(AccessCheckTest, ConsoleLog) {
  isolate()->SetFailedAccessCheckCallbackFunction(
      [](v8::Local<v8::Object> host, v8::AccessType type,
         v8::Local<v8::Value> data) {
        failed_access_check_callback_called = true;
      });
  AccessCheckTestConsoleDelegate console{};
  debug::SetConsoleDelegate(isolate(), &console);

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate());
  object_template->SetAccessCheckCallback(AccessCheck);

  Local<Context> context1 = Context::New(isolate(), nullptr);
  Local<Context> context2 = Context::New(isolate(), nullptr);

  Local<Object> object1 =
      object_template->NewInstance(context1).ToLocalChecked();
  EXPECT_TRUE(context2->Global()
                  ->Set(context2, v8_str("object_from_context1"), object1)
                  .IsJust());

  Context::Scope context_scope(context2);
  failed_access_check_callback_called = false;
  CompileRun(isolate(), "console.log(object_from_context1);").ToLocalChecked();

  ASSERT_TRUE(failed_access_check_callback_called);
}

}  // namespace v8

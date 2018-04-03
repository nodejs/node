// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "include/v8.h"
#include "src/api.h"
#include "src/objects-inl.h"

namespace i = v8::internal;

// The goal is to avoid the callback.
static void UnreachableCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  UNREACHABLE();
}

TEST(CachedAccessor) {
  // Crankshaft support for fast accessors is not implemented; crankshafted
  // code uses the slow accessor which breaks this test's expectations.
  v8::internal::FLAG_always_opt = false;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Create 'foo' class, with a hidden property.
  v8::Local<v8::ObjectTemplate> foo = v8::ObjectTemplate::New(isolate);

  v8::Local<v8::Private> priv =
      v8::Private::ForApi(isolate, v8_str("Foo#draft"));

  foo->SetAccessorProperty(v8_str("draft"), v8::FunctionTemplate::NewWithCache(
                                                isolate, UnreachableCallback,
                                                priv, v8::Local<v8::Value>()));

  // Create 'obj', instance of 'foo'.
  v8::Local<v8::Object> obj = foo->NewInstance(env.local()).ToLocalChecked();

  // Install the private property on the instance.
  CHECK(obj->SetPrivate(isolate->GetCurrentContext(), priv,
                        v8::Undefined(isolate))
            .FromJust());

  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  // Access cached accessor.
  ExpectUndefined("obj.draft");

  // Set hidden property.
  CHECK(obj->SetPrivate(isolate->GetCurrentContext(), priv,
                        v8_str("Shhh, I'm private!"))
            .FromJust());

  ExpectString("obj.draft", "Shhh, I'm private!");

  // Stress the accessor to use the IC.
  ExpectString(
      "var result = '';"
      "for (var i = 0; i < 10; ++i) { "
      "  result = obj.draft; "
      "} "
      "result; ",
      "Shhh, I'm private!");
}

TEST(CachedAccessorCrankshaft) {
  i::FLAG_allow_natives_syntax = true;
  // v8::internal::FLAG_always_opt = false;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Create 'foo' class, with a hidden property.
  v8::Local<v8::ObjectTemplate> foo = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Private> priv =
      v8::Private::ForApi(isolate, v8_str("Foo#draft"));

  // Install the private property on the template.
  // foo->SetPrivate(priv, v8::Undefined(isolate));

  foo->SetAccessorProperty(v8_str("draft"), v8::FunctionTemplate::NewWithCache(
                                                isolate, UnreachableCallback,
                                                priv, v8::Local<v8::Value>()));

  // Create 'obj', instance of 'foo'.
  v8::Local<v8::Object> obj = foo->NewInstance(env.local()).ToLocalChecked();

  // Install the private property on the instance.
  CHECK(obj->SetPrivate(isolate->GetCurrentContext(), priv,
                        v8::Undefined(isolate))
            .FromJust());

  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  // Access surrogate accessor.
  ExpectUndefined("obj.draft");

  // Set hidden property.
  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 123))
            .FromJust());

  // Test ICs.
  CompileRun(
      "function f() {"
      "  var x;"
      "  for (var i = 0; i < 100; i++) {"
      "    x = obj.draft;"
      "  }"
      "  return x;"
      "}");

  ExpectInt32("f()", 123);

  // Reset hidden property.
  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
            .FromJust());

  // Test Crankshaft.
  CompileRun("%OptimizeFunctionOnNextCall(f);");

  ExpectInt32("f()", 456);

  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
            .FromJust());
  // Test non-global ICs.
  CompileRun(
      "function g() {"
      "  var x = obj;"
      "  var r = 0;"
      "  for (var i = 0; i < 100; i++) {"
      "    r = x.draft;"
      "  }"
      "  return r;"
      "}");

  ExpectInt32("g()", 456);

  // Reset hidden property.
  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 789))
            .FromJust());

  // Test non-global access in Crankshaft.
  CompileRun("%OptimizeFunctionOnNextCall(g);");

  ExpectInt32("g()", 789);
}

TEST(CachedAccessorOnGlobalObject) {
  i::FLAG_allow_natives_syntax = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::FunctionTemplate> templ =
      v8::FunctionTemplate::New(CcTest::isolate());
  v8::Local<v8::ObjectTemplate> object_template = templ->InstanceTemplate();
  v8::Local<v8::Private> priv =
      v8::Private::ForApi(isolate, v8_str("Foo#draft"));

  object_template->SetAccessorProperty(
      v8_str("draft"),
      v8::FunctionTemplate::NewWithCache(isolate, UnreachableCallback, priv,
                                         v8::Local<v8::Value>()));

  v8::Local<v8::Context> ctx =
      v8::Context::New(CcTest::isolate(), nullptr, object_template);
  v8::Local<v8::Object> obj = ctx->Global();

  // Install the private property on the instance.
  CHECK(obj->SetPrivate(isolate->GetCurrentContext(), priv,
                        v8::Undefined(isolate))
            .FromJust());

  {
    v8::Context::Scope context_scope(ctx);

    // Access surrogate accessor.
    ExpectUndefined("draft");

    // Set hidden property.
    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 123))
              .FromJust());

    // Test ICs.
    CompileRun(
        "function f() {"
        "  var x;"
        "  for (var i = 0; i < 100; i++) {"
        "    x = draft;"
        "  }"
        "  return x;"
        "}");

    ExpectInt32("f()", 123);

    // Reset hidden property.
    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
              .FromJust());

    // Test Crankshaft.
    CompileRun("%OptimizeFunctionOnNextCall(f);");

    ExpectInt32("f()", 456);

    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
              .FromJust());
    // Test non-global ICs.
    CompileRun(
        "var x = this;"
        "function g() {"
        "  var r = 0;"
        "  for (var i = 0; i < 100; i++) {"
        "    r = x.draft;"
        "  }"
        "  return r;"
        "}");

    ExpectInt32("g()", 456);

    // Reset hidden property.
    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 789))
              .FromJust());

    // Test non-global access in Crankshaft.
    CompileRun("%OptimizeFunctionOnNextCall(g);");

    ExpectInt32("g()", 789);
  }
}

namespace {

static void Setter(v8::Local<v8::String> name, v8::Local<v8::Value> value,
                   const v8::PropertyCallbackInfo<void>& info) {}
}

// Re-declaration of non-configurable accessors should throw.
TEST(RedeclareAccessor) {
  v8::HandleScope scope(CcTest::isolate());
  LocalContext env;

  v8::Local<v8::FunctionTemplate> templ =
      v8::FunctionTemplate::New(CcTest::isolate());

  v8::Local<v8::ObjectTemplate> object_template = templ->InstanceTemplate();
  object_template->SetAccessor(
      v8_str("foo"), nullptr, Setter, v8::Local<v8::Value>(),
      v8::AccessControl::DEFAULT, v8::PropertyAttribute::DontDelete);

  v8::Local<v8::Context> ctx =
      v8::Context::New(CcTest::isolate(), nullptr, object_template);

  // Declare function.
  v8::Local<v8::String> code = v8_str("function foo() {};");

  v8::TryCatch try_catch(CcTest::isolate());
  v8::Script::Compile(ctx, code).ToLocalChecked()->Run(ctx).IsEmpty();
  CHECK(try_catch.HasCaught());
}

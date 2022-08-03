// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-context.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace i = v8::internal;

// The goal is to avoid the callback.
static void UnreachableCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  UNREACHABLE();
}

TEST(CachedAccessor) {
  // TurboFan support for fast accessors is not implemented; turbofanned
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

TEST(CachedAccessorTurboFan) {
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
      "};"
      "%PrepareFunctionForOptimization(f);");

  ExpectInt32("f()", 123);

  // Reset hidden property.
  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
            .FromJust());

  // Test TurboFan.
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
      "};"
      "%PrepareFunctionForOptimization(g);");

  ExpectInt32("g()", 456);

  // Reset hidden property.
  CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 789))
            .FromJust());

  // Test non-global access in TurboFan.
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
        "}"
        "%PrepareFunctionForOptimization(f);");

    ExpectInt32("f()", 123);

    // Reset hidden property.
    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 456))
              .FromJust());

    // Test TurboFan.
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
        "}"
        "%PrepareFunctionForOptimization(g);");

    ExpectInt32("g()", 456);

    // Reset hidden property.
    CHECK(obj->SetPrivate(env.local(), priv, v8::Integer::New(isolate, 789))
              .FromJust());

    // Test non-global access in TurboFan.
    CompileRun("%OptimizeFunctionOnNextCall(g);");

    ExpectInt32("g()", 789);
  }
}

namespace {

// Getter return value should be non-null to trigger lazy property paths.
static void Getter(v8::Local<v8::Name> name,
                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(v8_str("return value"));
}

static void StringGetter(v8::Local<v8::String> name,
                         const v8::PropertyCallbackInfo<v8::Value>& info) {}

static int set_accessor_call_count = 0;

static void Setter(v8::Local<v8::Name> name, v8::Local<v8::Value> value,
                   const v8::PropertyCallbackInfo<void>& info) {
  set_accessor_call_count++;
}

static void EmptyCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {}
}  // namespace

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

class NoopDelegate : public v8::debug::DebugDelegate {};

static void CheckSideEffectFreeAccesses(v8::Isolate* isolate,
                                        v8::Local<v8::String> call_getter,
                                        v8::Local<v8::String> call_setter) {
  const int kIterationsCountForICProgression = 20;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);
  v8::Local<v8::Script> func = v8_compile(call_getter);
  // Check getter. Run enough number of times to ensure IC creates data handler.
  for (int i = 0; i < kIterationsCountForICProgression; i++) {
    v8::TryCatch try_catch(isolate);
    CHECK(EvaluateGlobalForTesting(
              isolate, func,
              v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect,
              true)
              .IsEmpty());

    CHECK(try_catch.HasCaught());

    // Ensure that IC state progresses.
    CHECK(!func->Run(context).IsEmpty());
  }

  func = v8_compile(call_setter);
  // Check setter. Run enough number of times to ensure IC creates data handler.
  for (int i = 0; i < kIterationsCountForICProgression; i++) {
    v8::TryCatch try_catch(isolate);
    CHECK(EvaluateGlobalForTesting(
              isolate, func,
              v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect,
              true)
              .IsEmpty());

    CHECK(try_catch.HasCaught());

    // Ensure that IC state progresses.
    CHECK(!func->Run(context).IsEmpty());
  }
}

TEST(AccessorsWithSideEffects) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  v8::Local<v8::FunctionTemplate> templ_with_sideffect =
      v8::FunctionTemplate::New(isolate, EmptyCallback, v8::Local<v8::Value>(),
                                v8::Local<v8::Signature>(), 0,
                                v8::ConstructorBehavior::kAllow,
                                v8::SideEffectType::kHasSideEffect);
  v8::Local<v8::FunctionTemplate> templ_no_sideffect =
      v8::FunctionTemplate::New(isolate, EmptyCallback, v8::Local<v8::Value>(),
                                v8::Local<v8::Signature>(), 0,
                                v8::ConstructorBehavior::kAllow,
                                v8::SideEffectType::kHasNoSideEffect);

  // Install non-native properties with side effects
  obj->SetAccessorProperty(
      v8_str("get"),
      templ_with_sideffect->GetFunction(context).ToLocalChecked(), {},
      v8::PropertyAttribute::None, v8::AccessControl::DEFAULT);

  obj->SetAccessorProperty(
      v8_str("set"), templ_no_sideffect->GetFunction(context).ToLocalChecked(),
      templ_with_sideffect->GetFunction(context).ToLocalChecked(),
      v8::PropertyAttribute::None, v8::AccessControl::DEFAULT);

  CheckSideEffectFreeAccesses(isolate, v8_str("obj.get"),
                              v8_str("obj.set = 123;"));
}

TEST(TemplateAccessorsWithSideEffects) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::FunctionTemplate> templ_with_sideffect =
      v8::FunctionTemplate::New(isolate, EmptyCallback, v8::Local<v8::Value>(),
                                v8::Local<v8::Signature>(), 0,
                                v8::ConstructorBehavior::kAllow,
                                v8::SideEffectType::kHasSideEffect);
  v8::Local<v8::FunctionTemplate> templ_no_sideffect =
      v8::FunctionTemplate::New(isolate, EmptyCallback, v8::Local<v8::Value>(),
                                v8::Local<v8::Signature>(), 0,
                                v8::ConstructorBehavior::kAllow,
                                v8::SideEffectType::kHasNoSideEffect);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetAccessorProperty(v8_str("get"), templ_with_sideffect);
  templ->SetAccessorProperty(v8_str("set"), templ_no_sideffect,
                             templ_with_sideffect);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  CheckSideEffectFreeAccesses(isolate, v8_str("obj.get"),
                              v8_str("obj.set = 123;"));
}

TEST(NativeTemplateAccessorWithSideEffects) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("get"), Getter, nullptr, v8::Local<v8::Value>(),
                     v8::AccessControl::DEFAULT, v8::PropertyAttribute::None,
                     v8::SideEffectType::kHasSideEffect);
  templ->SetAccessor(v8_str("set"), Getter, Setter, v8::Local<v8::Value>(),
                     v8::AccessControl::DEFAULT, v8::PropertyAttribute::None,
                     v8::SideEffectType::kHasNoSideEffect,
                     v8::SideEffectType::kHasSideEffect);

  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  CheckSideEffectFreeAccesses(isolate, v8_str("obj.get"),
                              v8_str("obj.set = 123;"));
}

TEST(NativeAccessorsWithSideEffects) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  // Install native data property with side effects.
  obj->SetAccessor(context, v8_str("get"), Getter, nullptr,
                   v8::MaybeLocal<v8::Value>(), v8::AccessControl::DEFAULT,
                   v8::PropertyAttribute::None,
                   v8::SideEffectType::kHasSideEffect)
      .ToChecked();
  obj->SetAccessor(context, v8_str("set"), Getter, Setter,
                   v8::MaybeLocal<v8::Value>(), v8::AccessControl::DEFAULT,
                   v8::PropertyAttribute::None,
                   v8::SideEffectType::kHasNoSideEffect,
                   v8::SideEffectType::kHasSideEffect)
      .ToChecked();

  CheckSideEffectFreeAccesses(isolate, v8_str("obj.get"),
                              v8_str("obj.set = 123;"));
}

// Accessors can be allowlisted as side-effect-free via SetAccessor.
TEST(AccessorSetHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());
  obj->SetAccessor(context, v8_str("foo"), Getter).ToChecked();
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());

  obj->SetAccessor(context, v8_str("foo"), Getter, nullptr,
                   v8::MaybeLocal<v8::Value>(), v8::AccessControl::DEFAULT,
                   v8::PropertyAttribute::None,
                   v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
  CHECK_EQ(0, set_accessor_call_count);
}

// Set accessors can be allowlisted as side-effect-free via SetAccessor.
TEST(SetAccessorSetSideEffectReceiverCheck1) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());
  obj->SetAccessor(env.local(), v8_str("foo"), Getter, Setter,
                   v8::MaybeLocal<v8::Value>(), v8::AccessControl::DEFAULT,
                   v8::PropertyAttribute::None,
                   v8::SideEffectType::kHasNoSideEffect,
                   v8::SideEffectType::kHasSideEffectToReceiver)
      .ToChecked();
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .ToLocalChecked()
            ->Equals(env.local(), v8_str("return value"))
            .FromJust());
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_EQ(0, set_accessor_call_count);
}

static void ConstructCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
}

TEST(SetAccessorSetSideEffectReceiverCheck2) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(
      isolate, ConstructCallback, v8::Local<v8::Value>(),
      v8::Local<v8::Signature>(), 0, v8::ConstructorBehavior::kAllow,
      v8::SideEffectType::kHasNoSideEffect);
  templ->InstanceTemplate()->SetAccessor(
      v8_str("bar"), Getter, Setter, v8::Local<v8::Value>(),
      v8::AccessControl::DEFAULT, v8::PropertyAttribute::None,
      v8::SideEffectType::kHasSideEffectToReceiver,
      v8::SideEffectType::kHasSideEffectToReceiver);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("f"),
                  templ->GetFunction(env.local()).ToLocalChecked())
            .FromJust());
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("new f().bar"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .ToLocalChecked()
            ->Equals(env.local(), v8_str("return value"))
            .FromJust());
  v8::debug::EvaluateGlobal(
      isolate, v8_str("new f().bar = 1"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();
  CHECK_EQ(1, set_accessor_call_count);
}

// Accessors can be allowlisted as side-effect-free via SetNativeDataProperty.
TEST(AccessorSetNativeDataPropertyHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());
  obj->SetNativeDataProperty(context, v8_str("foo"), Getter).ToChecked();
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());

  obj->SetNativeDataProperty(
         context, v8_str("foo"), Getter, nullptr, v8::Local<v8::Value>(),
         v8::PropertyAttribute::None, v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
}

// Accessors can be allowlisted as side-effect-free via SetLazyDataProperty.
TEST(AccessorSetLazyDataPropertyHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());
  obj->SetLazyDataProperty(context, v8_str("foo"), Getter).ToChecked();
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());

  obj->SetLazyDataProperty(context, v8_str("foo"), Getter,
                           v8::Local<v8::Value>(), v8::PropertyAttribute::None,
                           v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
}

TEST(ObjectTemplateSetAccessorHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetAccessor(v8_str("foo"), StringGetter);
  templ->SetAccessor(v8_str("foo2"), StringGetter, nullptr,
                     v8::Local<v8::Value>(), v8::AccessControl::DEFAULT,
                     v8::PropertyAttribute::None,
                     v8::SideEffectType::kHasNoSideEffect);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo2"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo2 = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo2"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
}

TEST(ObjectTemplateSetNativePropertyHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetNativeDataProperty(v8_str("foo"), Getter);
  templ->SetNativeDataProperty(
      v8_str("foo2"), Getter, nullptr, v8::Local<v8::Value>(),
      v8::PropertyAttribute::None, v8::AccessControl::DEFAULT,
      v8::SideEffectType::kHasNoSideEffect);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo2"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo2 = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo2"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
}

TEST(ObjectTemplateSetLazyPropertyHasNoSideEffect) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  NoopDelegate delegate;
  i_isolate->debug()->SetDebugDelegate(&delegate);

  v8::Local<v8::ObjectTemplate> templ = v8::ObjectTemplate::New(isolate);
  templ->SetLazyDataProperty(v8_str("foo"), Getter);
  templ->SetLazyDataProperty(v8_str("foo2"), Getter, v8::Local<v8::Value>(),
                             v8::PropertyAttribute::None,
                             v8::SideEffectType::kHasNoSideEffect);
  v8::Local<v8::Object> obj = templ->NewInstance(env.local()).ToLocalChecked();
  CHECK(env->Global()->Set(env.local(), v8_str("obj"), obj).FromJust());

  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  v8::debug::EvaluateGlobal(
      isolate, v8_str("obj.foo2"),
      v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
      .ToLocalChecked();

  // Check that setter is not allowlisted.
  v8::TryCatch try_catch(isolate);
  CHECK(v8::debug::EvaluateGlobal(
            isolate, v8_str("obj.foo2 = 1"),
            v8::debug::EvaluateGlobalMode::kDisableBreaksAndThrowOnSideEffect)
            .IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK_NE(1, v8::debug::EvaluateGlobal(isolate, v8_str("obj.foo2"),
                                        v8::debug::EvaluateGlobalMode::kDefault)
                  .ToLocalChecked()
                  ->Int32Value(env.local())
                  .FromJust());
}

namespace {
void FunctionNativeGetter(v8::Local<v8::String> property,
                          const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetIsolate()->ThrowError(v8_str("side effect in getter"));
}
}  // namespace

TEST(BindFunctionTemplateSetNativeDataProperty) {
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope scope(isolate);

  // Check that getter is called on Function.prototype.bind.
  {
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
    templ->SetNativeDataProperty(v8_str("name"), FunctionNativeGetter);
    v8::Local<v8::Function> func =
        templ->GetFunction(env.local()).ToLocalChecked();
    CHECK(env->Global()->Set(env.local(), v8_str("func"), func).FromJust());

    v8::TryCatch try_catch(isolate);
    CHECK(CompileRun("func.bind()").IsEmpty());
    CHECK(try_catch.HasCaught());
  }

  // Check that getter is called on Function.prototype.bind.
  {
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(isolate);
    templ->SetNativeDataProperty(v8_str("length"), FunctionNativeGetter);
    v8::Local<v8::Function> func =
        templ->GetFunction(env.local()).ToLocalChecked();
    CHECK(env->Global()->Set(env.local(), v8_str("func"), func).FromJust());

    v8::TryCatch try_catch(isolate);
    CHECK(CompileRun("func.bind()").IsEmpty());
    CHECK(try_catch.HasCaught());
  }
}

namespace {
v8::MaybeLocal<v8::Context> TestHostCreateShadowRealmContextCallback(
    v8::Local<v8::Context> initiator_context) {
  v8::Isolate* isolate = initiator_context->GetIsolate();
  v8::Local<v8::FunctionTemplate> global_constructor =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      global_constructor->InstanceTemplate();

  // Check that getter is called on Function.prototype.bind.
  global_template->SetNativeDataProperty(
      v8_str("func1"), [](v8::Local<v8::String> property,
                          const v8::PropertyCallbackInfo<v8::Value>& info) {
        v8::Isolate* isolate = info.GetIsolate();
        v8::Local<v8::FunctionTemplate> templ =
            v8::FunctionTemplate::New(isolate);
        templ->SetNativeDataProperty(v8_str("name"), FunctionNativeGetter);
        info.GetReturnValue().Set(
            templ->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
      });

  // Check that getter is called on Function.prototype.bind.
  global_template->SetNativeDataProperty(
      v8_str("func2"), [](v8::Local<v8::String> property,
                          const v8::PropertyCallbackInfo<v8::Value>& info) {
        v8::Isolate* isolate = info.GetIsolate();
        v8::Local<v8::FunctionTemplate> templ =
            v8::FunctionTemplate::New(isolate);
        templ->SetNativeDataProperty(v8_str("length"), FunctionNativeGetter);
        info.GetReturnValue().Set(
            templ->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
      });

  return v8::Context::New(isolate, nullptr, global_template);
}
}  // namespace

TEST(WrapFunctionTemplateSetNativeDataProperty) {
  i::FLAG_harmony_shadow_realm = true;
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  isolate->SetHostCreateShadowRealmContextCallback(
      TestHostCreateShadowRealmContextCallback);

  v8::HandleScope scope(isolate);
  // Check that getter is called on WrappedFunctionCreate.
  {
    v8::TryCatch try_catch(isolate);
    CHECK(
        CompileRun("new ShadowRealm().evaluate('globalThis.func1')").IsEmpty());
    CHECK(try_catch.HasCaught());
  }
  // Check that getter is called on WrappedFunctionCreate.
  {
    v8::TryCatch try_catch(isolate);
    CHECK(
        CompileRun("new ShadowRealm().evaluate('globalThis.func2')").IsEmpty());
    CHECK(try_catch.HasCaught());
  }
}

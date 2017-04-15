// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "include/v8-experimental.h"
#include "include/v8.h"
#include "src/api.h"
#include "src/objects-inl.h"

namespace i = v8::internal;

static void CppAccessor42(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(42);
}


static void CppAccessor41(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(41);
}


v8::experimental::FastAccessorBuilder* FastAccessor(v8::Isolate* isolate) {
  auto builder = v8::experimental::FastAccessorBuilder::New(isolate);
  builder->ReturnValue(builder->IntegerConstant(41));
  return builder;
}


TEST(FastAccessors) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;

  // We emulate Embedder-created DOM Node instances. Specifically:
  // - 'parent': FunctionTemplate ~= DOM Node superclass
  // - 'child':  FunctionTemplate ~= a specific DOM node type, like a <div />
  //
  // We'll install both a C++-based and a JS-based accessor on the parent,
  // and expect it to be callable on the child.

  // Setup the parent template ( =~ DOM Node w/ accessors).
  v8::Local<v8::FunctionTemplate> parent = v8::FunctionTemplate::New(isolate);
  {
    auto signature = v8::Signature::New(isolate, parent);

    // cpp accessor as "firstChild":
    parent->PrototypeTemplate()->SetAccessorProperty(
        v8_str("firstChild"),
        v8::FunctionTemplate::New(isolate, CppAccessor42,
                                  v8::Local<v8::Value>(), signature));

    // JS accessor as "firstChildRaw":
    parent->PrototypeTemplate()->SetAccessorProperty(
        v8_str("firstChildRaw"),
        v8::FunctionTemplate::NewWithFastHandler(
            isolate, CppAccessor41, FastAccessor(isolate),
            v8::Local<v8::Value>(), signature));
  }

  // Setup child object ( =~ a specific DOM Node, e.g. a <div> ).
  // Also, make a creation function on the global object, so we can access it
  // in a test.
  v8::Local<v8::FunctionTemplate> child = v8::FunctionTemplate::New(isolate);
  child->Inherit(parent);
  CHECK(env->Global()
            ->Set(env.local(), v8_str("Node"),
                  child->GetFunction(env.local()).ToLocalChecked())
            .IsJust());

  // Setup done: Let's test it:

  // The simple case: Run it once.
  ExpectInt32("var n = new Node(); n.firstChild", 42);
  ExpectInt32("var n = new Node(); n.firstChildRaw", 41);

  // Run them in a loop. This will likely trigger the optimizing compiler:
  ExpectInt32(
      "var m = new Node(); "
      "var sum = 0; "
      "for (var i = 0; i < 10; ++i) { "
      "  sum += m.firstChild; "
      "  sum += m.firstChildRaw; "
      "}; "
      "sum;",
      10 * (42 + 41));

  // Obtain the accessor and call it via apply on the Node:
  ExpectInt32(
      "var n = new Node(); "
      "var g = Object.getOwnPropertyDescriptor("
      "    n.__proto__.__proto__, 'firstChild')['get']; "
      "g.apply(n);",
      42);
  ExpectInt32(
      "var n = new Node(); "
      "var g = Object.getOwnPropertyDescriptor("
      "    n.__proto__.__proto__, 'firstChildRaw')['get']; "
      "g.apply(n);",
      41);

  ExpectInt32(
      "var n = new Node();"
      "var g = Object.getOwnPropertyDescriptor("
      "    n.__proto__.__proto__, 'firstChildRaw')['get'];"
      "try {"
      "  var f = { firstChildRaw: '51' };"
      "  g.apply(f);"
      "} catch(e) {"
      "  31415;"
      "}",
      31415);
}

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
      v8_str("foo"), NULL, Setter, v8::Local<v8::Value>(),
      v8::AccessControl::DEFAULT, v8::PropertyAttribute::DontDelete);

  v8::Local<v8::Context> ctx =
      v8::Context::New(CcTest::isolate(), nullptr, object_template);

  // Declare function.
  v8::Local<v8::String> code = v8_str("function foo() {};");

  v8::TryCatch try_catch(CcTest::isolate());
  v8::Script::Compile(ctx, code).ToLocalChecked()->Run(ctx).IsEmpty();
  CHECK(try_catch.HasCaught());
}

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "include/v8.h"
#include "include/v8-experimental.h"


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

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jochen): Remove this after the setting is turned on globally.
#define V8_IMMINENT_DEPRECATION_WARNINGS

#include "test/cctest/cctest.h"

#include "include/v8.h"


#ifdef V8_JS_ACCESSORS
static void CppAccessor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(42);
}

static const char* JsAccessor =
    "function firstChildJS(value) { return 41; }; firstChildJS";

TEST(JavascriptAccessors) {
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
        v8::FunctionTemplate::New(isolate, CppAccessor, v8::Local<v8::Value>(),
                                  signature));

    // JS accessor as "firstChildJS":
    auto js_accessor = v8::Local<v8::Function>::Cast(CompileRun(JsAccessor));
    parent->PrototypeTemplate()->SetAccessorProperty(v8_str("firstChildJS"),
                                                     js_accessor);
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
  ExpectInt32("var n = new Node(); n.firstChildJS", 41);

  // Run them in a loop. This will likely trigger the optimizing compiler:
  ExpectInt32(
      "var m = new Node(); "
      "var sum = 0; "
      "for (var i = 0; i < 3; ++i) { "
      "  sum += m.firstChild; "
      "  sum += m.firstChildJS; "
      "}; "
      "sum;",
      3 * (42 + 41));

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
      "    n.__proto__.__proto__, 'firstChildJS')['get']; "
      "g.apply(n);",
      41);

  // TODO(vogelheim): Verify compatible receiver check works.
}
#endif  // V8_JS_ACCESSORS

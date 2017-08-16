// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/api.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

using ::v8::Array;
using ::v8::Context;
using ::v8::Local;
using ::v8::Value;

// This test fails if properties on the prototype of the global object appear
// as declared globals.
TEST(StrictUndeclaredGlobalVariable) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::String> var_name = v8_str("x");
  LocalContext context;
  v8::TryCatch try_catch(CcTest::isolate());
  v8::Local<v8::Script> script = v8_compile("\"use strict\"; x = 42;");
  v8::Local<v8::Object> proto = v8::Object::New(CcTest::isolate());
  v8::Local<v8::Object> global =
      context->Global()->GetPrototype().As<v8::Object>();
  proto->Set(context.local(), var_name, v8_num(100)).FromJust();
  global->SetPrototype(context.local(), proto).FromJust();
  CHECK(script->Run(context.local()).IsEmpty());
  CHECK(try_catch.HasCaught());
  v8::String::Utf8Value exception(try_catch.Exception());
  CHECK_EQ(0, strcmp("ReferenceError: x is not defined", *exception));
}


TEST(KeysGlobalObject_Regress2764) {
  LocalContext env1;
  v8::HandleScope scope(env1->GetIsolate());

  // Create second environment.
  v8::Local<Context> env2 = Context::New(env1->GetIsolate());

  Local<Value> token = v8_str("foo");

  // Set same security token for env1 and env2.
  env1->SetSecurityToken(token);
  env2->SetSecurityToken(token);

  // Create a reference to env2 global from env1 global.
  env1->Global()
      ->Set(env1.local(), v8_str("global2"), env2->Global())
      .FromJust();
  // Set some global variables in global2
  env2->Global()->Set(env2, v8_str("a"), v8_str("a")).FromJust();
  env2->Global()->Set(env2, v8_str("42"), v8_str("42")).FromJust();

  // List all entries from global2.
  Local<Array> result;
  result = Local<Array>::Cast(CompileRun("Object.keys(global2)"));
  CHECK_EQ(2u, result->Length());
  CHECK(
      v8_str("42")
          ->Equals(env1.local(), result->Get(env1.local(), 0).ToLocalChecked())
          .FromJust());
  CHECK(
      v8_str("a")
          ->Equals(env1.local(), result->Get(env1.local(), 1).ToLocalChecked())
          .FromJust());

  result =
      Local<Array>::Cast(CompileRun("Object.getOwnPropertyNames(global2)"));
  CHECK_LT(2u, result->Length());
  // Check that all elements are in the property names
  ExpectTrue("-1 < Object.getOwnPropertyNames(global2).indexOf('42')");
  ExpectTrue("-1 < Object.getOwnPropertyNames(global2).indexOf('a')");

  // Hold on to global from env2 and detach global from env2.
  env2->DetachGlobal();

  // List again all entries from the detached global2.
  result = Local<Array>::Cast(CompileRun("Object.keys(global2)"));
  CHECK_EQ(0u, result->Length());
  result =
      Local<Array>::Cast(CompileRun("Object.getOwnPropertyNames(global2)"));
  CHECK_EQ(0u, result->Length());
}

TEST(KeysGlobalObject_SetPrototype) {
  LocalContext env1;
  v8::HandleScope scope(env1->GetIsolate());

  // Create second environment.
  v8::Local<Context> env2 = Context::New(env1->GetIsolate());

  Local<Value> token = v8_str("foo");

  // Set same security token for env1 and env2.
  env1->SetSecurityToken(token);
  env2->SetSecurityToken(token);

  // Create a reference to env2 global from env1 global.
  env1->Global()
      ->GetPrototype()
      .As<v8::Object>()
      ->SetPrototype(env1.local(), env2->Global()->GetPrototype())
      .FromJust();
  // Set some global variables in global2
  env2->Global()->Set(env2, v8_str("a"), v8_str("a")).FromJust();
  env2->Global()->Set(env2, v8_str("42"), v8_str("42")).FromJust();

  // List all entries from global2.
  ExpectTrue("a == 'a'");
}

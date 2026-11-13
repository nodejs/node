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

#include "src/api/api-inl.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using GlobalObjectTest = TestWithContext;

// This test fails if properties on the prototype of the global object appear
// as declared globals.
TEST_F(GlobalObjectTest, StrictUndeclaredGlobalVariable) {
  Local<String> var_name = NewString("x");
  TryCatch try_catch(isolate());
  Local<Object> proto = Object::New(isolate());
  Local<Object> global = context()->Global();
  proto->Set(context(), var_name, Number::New(isolate(), 100)).FromJust();
  global->SetPrototype(context(), proto).FromJust();
  CHECK_EQ(global->GetPrototype(), proto);
  CHECK(TryRunJS("\"use strict\"; x = 42;").IsEmpty());
  CHECK(try_catch.HasCaught());
  String::Utf8Value exception(isolate(), try_catch.Exception());
  CHECK_EQ(0, strcmp("ReferenceError: x is not defined", *exception));
}

TEST_F(GlobalObjectTest, KeysGlobalObject_Regress2764) {
  Local<Context> env1 = context();
  // Create second environment.
  Local<Context> env2 = Context::New(isolate());

  Local<Value> token = NewString("foo");

  // Set same security token for env1 and env2.
  env1->SetSecurityToken(token);
  env2->SetSecurityToken(token);

  // Create a reference to env2 global from env1 global.
  env1->Global()->Set(env1, NewString("global2"), env2->Global()).FromJust();
  // Set some global variables in global2
  env2->Global()->Set(env2, NewString("a"), NewString("a")).FromJust();
  env2->Global()->Set(env2, NewString("42"), NewString("42")).FromJust();

  // List all entries from global2.
  Local<Array> result;
  result = Local<Array>::Cast(RunJS("Object.keys(global2)"));
  CHECK_EQ(2u, result->Length());
  CHECK(NewString("42")
            ->Equals(env1, result->Get(env1, 0).ToLocalChecked())
            .FromJust());
  CHECK(NewString("a")
            ->Equals(env1, result->Get(env1, 1).ToLocalChecked())
            .FromJust());

  result = Local<Array>::Cast(RunJS("Object.getOwnPropertyNames(global2)"));
  CHECK_LT(2u, result->Length());
  // Check that all elements are in the property names
  CHECK(RunJS("-1 < Object.getOwnPropertyNames(global2).indexOf('42')")
            ->IsTrue());
  CHECK(
      RunJS("-1 < Object.getOwnPropertyNames(global2).indexOf('a')")->IsTrue());

  // Hold on to global from env2 and detach global from env2.
  env2->DetachGlobal();

  // List again all entries from the detached global2.
  result = Local<Array>::Cast(RunJS("Object.keys(global2)"));
  CHECK_EQ(0u, result->Length());
  result = Local<Array>::Cast(RunJS("Object.getOwnPropertyNames(global2)"));
  CHECK_EQ(0u, result->Length());
}

TEST_F(GlobalObjectTest, KeysGlobalObject_SetPrototype) {
  Local<Context> env1 = context();
  // Create second environment.
  Local<Context> env2 = Context::New(isolate());

  Local<Value> token = NewString("foo");

  // Set same security token for env1 and env2.
  env1->SetSecurityToken(token);
  env2->SetSecurityToken(token);

  // Create a reference to env2 global from env1 global.
  env1->Global()->SetPrototype(env1, env2->Global()).FromJust();
  CHECK_EQ(env1->Global()->GetPrototype(), env2->Global());

  // Set some global variables in global2
  env2->Global()->Set(env2, NewString("a"), NewString("a")).FromJust();
  env2->Global()->Set(env2, NewString("42"), NewString("42")).FromJust();

  // List all entries from global2.
  CHECK(RunJS("a == 'a'")->IsTrue());
}

TEST_F(GlobalObjectTest, ObjectCreateGlobalProxyAcrossReinitializeAndRemote) {
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->SetAccessCheckCallbackAndHandler(
      [](Local<Context>, Local<Object>, Local<Value>) { return true; },
      NamedPropertyHandlerConfiguration(
          [](Local<Name>, const PropertyCallbackInfo<Value>&) {
            return v8::Intercepted::kNo;
          }),
      IndexedPropertyHandlerConfiguration());

  // 1. Reinitialize global proxy in a new context without prior DetachGlobal.
  Local<Object> proxy;
  i::DirectHandle<i::Map> env1_map;
  Local<Context> env1 = Context::New(isolate(), nullptr, global_template);
  {
    Context::Scope scope(env1);
    proxy = env1->Global();
    Local<Value> v =
        Script::Compile(env1, NewString("globalThis.tag = 1; "
                                        "Object.create(globalThis)"))
            .ToLocalChecked()
            ->Run(env1)
            .ToLocalChecked();
    ASSERT_TRUE(v->IsObject());
    EXPECT_EQ(env1, v.As<Object>()->GetCreationContextChecked(isolate()));
    env1_map =
        i::direct_handle(Utils::OpenDirectHandle(*proxy)->map(), i_isolate());
  }

  i::DirectHandle<i::Map> env2_map;
  Local<Context> env2 =
      Context::New(isolate(), nullptr, global_template, proxy);
  {
#ifdef VERIFY_HEAP
    env1_map->HeapObjectVerify(i_isolate());
    Utils::OpenDirectHandle(*proxy)->map()->HeapObjectVerify(i_isolate());
#endif  // VERIFY_HEAP
    Context::Scope scope(env2);
    Local<Value> v =
        Script::Compile(env2, NewString("globalThis.tag = 2; "
                                        "const o1 = Object.create(globalThis); "
                                        "const o2 = Object.create(globalThis); "
                                        "[o1, o2.tag]"))
            .ToLocalChecked()
            ->Run(env2)
            .ToLocalChecked();
    ASSERT_TRUE(v->IsArray());
    Local<Array> arr = v.As<Array>();
    Local<Object> o1 = arr->Get(env2, 0).ToLocalChecked().As<Object>();
    EXPECT_EQ(env2, o1->GetCreationContextChecked(isolate()));
    EXPECT_EQ(2,
              arr->Get(env2, 1).ToLocalChecked()->Int32Value(env2).FromJust());
    env2_map =
        i::direct_handle(Utils::OpenDirectHandle(*proxy)->map(), i_isolate());
  }

  // 2. Transition global proxy through NewRemoteContext and back to
  // Context::New.
  Local<Object> remote_proxy =
      Context::NewRemoteContext(isolate(), global_template, proxy)
          .ToLocalChecked();
  EXPECT_EQ(proxy, remote_proxy);
#ifdef VERIFY_HEAP
  env2_map->HeapObjectVerify(i_isolate());
  Utils::OpenDirectHandle(*remote_proxy)->map()->HeapObjectVerify(i_isolate());
#endif  // VERIFY_HEAP
  {
    Context::Scope scope(context());
    context()
        ->Global()
        ->Set(context(), NewString("remoteProxy"), remote_proxy)
        .Check();
    Local<Value> v =
        Script::Compile(context(), NewString("Object.create(remoteProxy)"))
            .ToLocalChecked()
            ->Run(context())
            .ToLocalChecked();
    ASSERT_TRUE(v->IsObject());
    EXPECT_EQ(context(), v.As<Object>()->GetCreationContextChecked(isolate()));
  }

  i::DirectHandle<i::Map> env3_map;
  Local<Context> env3 =
      Context::New(isolate(), nullptr, global_template, remote_proxy);
  {
    Context::Scope scope(env3);
    Local<Value> v =
        Script::Compile(env3, NewString("globalThis.tag = 3; "
                                        "Object.create(globalThis)"))
            .ToLocalChecked()
            ->Run(env3)
            .ToLocalChecked();
    ASSERT_TRUE(v->IsObject());
    Local<Object> o3 = v.As<Object>();
    EXPECT_EQ(env3, o3->GetCreationContextChecked(isolate()));
    EXPECT_EQ(3, o3->Get(env3, NewString("tag"))
                     .ToLocalChecked()
                     ->Int32Value(env3)
                     .FromJust());
    env3_map = i::direct_handle(Utils::OpenDirectHandle(*remote_proxy)->map(),
                                i_isolate());
  }

  env3->DetachGlobal();
#ifdef VERIFY_HEAP
  env3_map->HeapObjectVerify(i_isolate());
  Utils::OpenDirectHandle(*remote_proxy)->map()->HeapObjectVerify(i_isolate());
#endif  // VERIFY_HEAP
}

}  // namespace v8

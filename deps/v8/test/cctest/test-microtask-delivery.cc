// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "src/v8.h"

#include "test/cctest/cctest.h"

namespace i = v8::internal;

namespace {
class HarmonyIsolate {
 public:
  HarmonyIsolate() {
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    isolate_ = v8::Isolate::New(create_params);
    isolate_->Enter();
  }

  ~HarmonyIsolate() {
    isolate_->Exit();
    isolate_->Dispose();
  }

  v8::Isolate* GetIsolate() const { return isolate_; }

 private:
  v8::Isolate* isolate_;
};
}  // namespace


TEST(MicrotaskDeliverySimple) {
  i::FLAG_harmony_object_observe = true;
  HarmonyIsolate isolate;
  v8::HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
  CompileRun(
      "var ordering = [];"
      "var resolver = {};"
      "function handler(resolve) { resolver.resolve = resolve; }"
      "var obj = {};"
      "var observeOrders = [1, 4];"
      "function observer() {"
        "ordering.push(observeOrders.shift());"
        "resolver.resolve();"
      "}"
      "var p = new Promise(handler);"
      "p.then(function() {"
        "ordering.push(2);"
      "}).then(function() {"
        "ordering.push(3);"
        "obj.id++;"
        "return new Promise(handler);"
      "}).then(function() {"
        "ordering.push(5);"
      "}).then(function() {"
        "ordering.push(6);"
      "});"
      "Object.observe(obj, observer);"
      "obj.id = 1;");
  CHECK_EQ(
      6, CompileRun("ordering.length")->Int32Value(context.local()).FromJust());
  CHECK_EQ(1,
           CompileRun("ordering[0]")->Int32Value(context.local()).FromJust());
  CHECK_EQ(2,
           CompileRun("ordering[1]")->Int32Value(context.local()).FromJust());
  CHECK_EQ(3,
           CompileRun("ordering[2]")->Int32Value(context.local()).FromJust());
  CHECK_EQ(4,
           CompileRun("ordering[3]")->Int32Value(context.local()).FromJust());
  CHECK_EQ(5,
           CompileRun("ordering[4]")->Int32Value(context.local()).FromJust());
  CHECK_EQ(6,
           CompileRun("ordering[5]")->Int32Value(context.local()).FromJust());
}


TEST(MicrotaskPerIsolateState) {
  i::FLAG_harmony_object_observe = true;
  HarmonyIsolate isolate;
  v8::HandleScope scope(isolate.GetIsolate());
  LocalContext context1(isolate.GetIsolate());
  isolate.GetIsolate()->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
  CompileRun(
      "var obj = { calls: 0 };");
  v8::Local<v8::Value> obj = CompileRun("obj");
  {
    LocalContext context2(isolate.GetIsolate());
    context2->Global()
        ->Set(context2.local(),
              v8::String::NewFromUtf8(isolate.GetIsolate(), "obj",
                                      v8::NewStringType::kNormal)
                  .ToLocalChecked(),
              obj)
        .FromJust();
    CompileRun(
        "var resolver = {};"
        "new Promise(function(resolve) {"
          "resolver.resolve = resolve;"
        "}).then(function() {"
          "obj.calls++;"
        "});"
        "(function() {"
          "resolver.resolve();"
        "})();");
  }
  {
    LocalContext context3(isolate.GetIsolate());
    context3->Global()
        ->Set(context3.local(),
              v8::String::NewFromUtf8(isolate.GetIsolate(), "obj",
                                      v8::NewStringType::kNormal)
                  .ToLocalChecked(),
              obj)
        .FromJust();
    CompileRun(
        "var foo = { id: 1 };"
        "Object.observe(foo, function() {"
          "obj.calls++;"
        "});"
        "foo.id++;");
  }
  {
    LocalContext context4(isolate.GetIsolate());
    context4->Global()
        ->Set(context4.local(),
              v8::String::NewFromUtf8(isolate.GetIsolate(), "obj",
                                      v8::NewStringType::kNormal)
                  .ToLocalChecked(),
              obj)
        .FromJust();
    isolate.GetIsolate()->RunMicrotasks();
    CHECK_EQ(2,
             CompileRun("obj.calls")->Int32Value(context4.local()).FromJust());
  }
}

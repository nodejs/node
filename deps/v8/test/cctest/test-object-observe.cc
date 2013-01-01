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

#include "v8.h"

#include "cctest.h"

using namespace v8;

namespace {
// Need to create a new isolate when FLAG_harmony_observation is on.
class HarmonyIsolate {
 public:
  HarmonyIsolate() {
    i::FLAG_harmony_observation = true;
    isolate_ = Isolate::New();
    isolate_->Enter();
  }

  ~HarmonyIsolate() {
    isolate_->Exit();
    isolate_->Dispose();
  }

 private:
  Isolate* isolate_;
};
}

TEST(PerIsolateState) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context1;
  CompileRun(
      "var count = 0;"
      "var calls = 0;"
      "var observer = function(records) { count = records.length; calls++ };"
      "var obj = {};"
      "Object.observe(obj, observer);");
  Handle<Value> observer = CompileRun("observer");
  Handle<Value> obj = CompileRun("obj");
  Handle<Value> notify_fun1 = CompileRun(
      "(function() { obj.foo = 'bar'; })");
  Handle<Value> notify_fun2;
  {
    LocalContext context2;
    context2->Global()->Set(String::New("obj"), obj);
    notify_fun2 = CompileRun(
        "(function() { obj.foo = 'baz'; })");
  }
  Handle<Value> notify_fun3;
  {
    LocalContext context3;
    context3->Global()->Set(String::New("obj"), obj);
    notify_fun3 = CompileRun(
        "(function() { obj.foo = 'bat'; })");
  }
  {
    LocalContext context4;
    context4->Global()->Set(String::New("observer"), observer);
    context4->Global()->Set(String::New("fun1"), notify_fun1);
    context4->Global()->Set(String::New("fun2"), notify_fun2);
    context4->Global()->Set(String::New("fun3"), notify_fun3);
    CompileRun("fun1(); fun2(); fun3(); Object.deliverChangeRecords(observer)");
  }
  CHECK_EQ(1, CompileRun("calls")->Int32Value());
  CHECK_EQ(3, CompileRun("count")->Int32Value());
}

TEST(EndOfMicrotaskDelivery) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context;
  CompileRun(
      "var obj = {};"
      "var count = 0;"
      "var observer = function(records) { count = records.length };"
      "Object.observe(obj, observer);"
      "obj.foo = 'bar';");
  CHECK_EQ(1, CompileRun("count")->Int32Value());
}

TEST(DeliveryOrdering) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context;
  CompileRun(
      "var obj1 = {};"
      "var obj2 = {};"
      "var ordering = [];"
      "function observer2() { ordering.push(2); };"
      "function observer1() { ordering.push(1); };"
      "function observer3() { ordering.push(3); };"
      "Object.observe(obj1, observer1);"
      "Object.observe(obj1, observer2);"
      "Object.observe(obj1, observer3);"
      "obj1.foo = 'bar';");
  CHECK_EQ(3, CompileRun("ordering.length")->Int32Value());
  CHECK_EQ(1, CompileRun("ordering[0]")->Int32Value());
  CHECK_EQ(2, CompileRun("ordering[1]")->Int32Value());
  CHECK_EQ(3, CompileRun("ordering[2]")->Int32Value());
  CompileRun(
      "ordering = [];"
      "Object.observe(obj2, observer3);"
      "Object.observe(obj2, observer2);"
      "Object.observe(obj2, observer1);"
      "obj2.foo = 'baz'");
  CHECK_EQ(3, CompileRun("ordering.length")->Int32Value());
  CHECK_EQ(1, CompileRun("ordering[0]")->Int32Value());
  CHECK_EQ(2, CompileRun("ordering[1]")->Int32Value());
  CHECK_EQ(3, CompileRun("ordering[2]")->Int32Value());
}

TEST(DeliveryOrderingReentrant) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context;
  CompileRun(
      "var obj = {};"
      "var reentered = false;"
      "var ordering = [];"
      "function observer1() { ordering.push(1); };"
      "function observer2() {"
      "  if (!reentered) {"
      "    obj.foo = 'baz';"
      "    reentered = true;"
      "  }"
      "  ordering.push(2);"
      "};"
      "function observer3() { ordering.push(3); };"
      "Object.observe(obj, observer1);"
      "Object.observe(obj, observer2);"
      "Object.observe(obj, observer3);"
      "obj.foo = 'bar';");
  CHECK_EQ(5, CompileRun("ordering.length")->Int32Value());
  CHECK_EQ(1, CompileRun("ordering[0]")->Int32Value());
  CHECK_EQ(2, CompileRun("ordering[1]")->Int32Value());
  CHECK_EQ(3, CompileRun("ordering[2]")->Int32Value());
  // Note that we re-deliver to observers 1 and 2, while observer3
  // already received the second record during the first round.
  CHECK_EQ(1, CompileRun("ordering[3]")->Int32Value());
  CHECK_EQ(2, CompileRun("ordering[1]")->Int32Value());
}

TEST(DeliveryOrderingDeliverChangeRecords) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context;
  CompileRun(
      "var obj = {};"
      "var ordering = [];"
      "function observer1() { ordering.push(1); if (!obj.b) obj.b = true };"
      "function observer2() { ordering.push(2); };"
      "Object.observe(obj, observer1);"
      "Object.observe(obj, observer2);"
      "obj.a = 1;"
      "Object.deliverChangeRecords(observer2);");
  CHECK_EQ(4, CompileRun("ordering.length")->Int32Value());
  // First, observer2 is called due to deliverChangeRecords
  CHECK_EQ(2, CompileRun("ordering[0]")->Int32Value());
  // Then, observer1 is called when the stack unwinds
  CHECK_EQ(1, CompileRun("ordering[1]")->Int32Value());
  // observer1's mutation causes both 1 and 2 to be reactivated,
  // with 1 having priority.
  CHECK_EQ(1, CompileRun("ordering[2]")->Int32Value());
  CHECK_EQ(2, CompileRun("ordering[3]")->Int32Value());
}

TEST(ObjectHashTableGrowth) {
  HarmonyIsolate isolate;
  HandleScope scope;
  // Initializing this context sets up initial hash tables.
  LocalContext context;
  Handle<Value> obj = CompileRun("obj = {};");
  Handle<Value> observer = CompileRun(
      "var ran = false;"
      "(function() { ran = true })");
  {
    // As does initializing this context.
    LocalContext context2;
    context2->Global()->Set(String::New("obj"), obj);
    context2->Global()->Set(String::New("observer"), observer);
    CompileRun(
        "var objArr = [];"
        // 100 objects should be enough to make the hash table grow
        // (and thus relocate).
        "for (var i = 0; i < 100; ++i) {"
        "  objArr.push({});"
        "  Object.observe(objArr[objArr.length-1], function(){});"
        "}"
        "Object.observe(obj, observer);");
  }
  // obj is now marked "is_observed", but our map has moved.
  CompileRun("obj.foo = 'bar'");
  CHECK(CompileRun("ran")->BooleanValue());
}

TEST(GlobalObjectObservation) {
  HarmonyIsolate isolate;
  HandleScope scope;
  LocalContext context;
  Handle<Object> global_proxy = context->Global();
  Handle<Object> inner_global = global_proxy->GetPrototype().As<Object>();
  CompileRun(
      "var records = [];"
      "var global = this;"
      "Object.observe(global, function(r) { [].push.apply(records, r) });"
      "global.foo = 'hello';");
  CHECK_EQ(1, CompileRun("records.length")->Int32Value());
  CHECK(global_proxy->StrictEquals(CompileRun("records[0].object")));

  // Detached, mutating the proxy has no effect.
  context->DetachGlobal();
  CompileRun("global.bar = 'goodbye';");
  CHECK_EQ(1, CompileRun("records.length")->Int32Value());

  // Mutating the global object directly still has an effect...
  CompileRun("this.bar = 'goodbye';");
  CHECK_EQ(2, CompileRun("records.length")->Int32Value());
  CHECK(inner_global->StrictEquals(CompileRun("records[1].object")));

  // Reattached, back to global proxy.
  context->ReattachGlobal(global_proxy);
  CompileRun("global.baz = 'again';");
  CHECK_EQ(3, CompileRun("records.length")->Int32Value());
  CHECK(global_proxy->StrictEquals(CompileRun("records[2].object")));

  // Attached to a different context, should not leak mutations
  // to the old context.
  context->DetachGlobal();
  {
    LocalContext context2;
    context2->DetachGlobal();
    context2->ReattachGlobal(global_proxy);
    CompileRun(
        "var records2 = [];"
        "Object.observe(this, function(r) { [].push.apply(records2, r) });"
        "this.bat = 'context2';");
    CHECK_EQ(1, CompileRun("records2.length")->Int32Value());
    CHECK(global_proxy->StrictEquals(CompileRun("records2[0].object")));
  }
  CHECK_EQ(3, CompileRun("records.length")->Int32Value());

  // Attaching by passing to Context::New
  {
    // Delegates to Context::New
    LocalContext context3(NULL, Handle<ObjectTemplate>(), global_proxy);
    CompileRun(
        "var records3 = [];"
        "Object.observe(this, function(r) { [].push.apply(records3, r) });"
        "this.qux = 'context3';");
    CHECK_EQ(1, CompileRun("records3.length")->Int32Value());
    CHECK(global_proxy->StrictEquals(CompileRun("records3[0].object")));
  }
  CHECK_EQ(3, CompileRun("records.length")->Int32Value());
}

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
namespace i = v8::internal;

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

  Isolate* GetIsolate() const { return isolate_; }

 private:
  Isolate* isolate_;
};
}

TEST(PerIsolateState) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
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
  HandleScope scope(isolate.GetIsolate());
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
  HandleScope scope(isolate.GetIsolate());
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
  HandleScope scope(isolate.GetIsolate());
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
  HandleScope scope(isolate.GetIsolate());
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
  HandleScope scope(isolate.GetIsolate());
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
  LocalContext context;
  HandleScope scope(isolate.GetIsolate());
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


struct RecordExpectation {
  Handle<Value> object;
  const char* type;
  const char* name;
  Handle<Value> old_value;
};

// TODO(adamk): Use this helper elsewhere in this file.
static void ExpectRecords(Handle<Value> records,
                          const RecordExpectation expectations[],
                          int num) {
  CHECK(records->IsArray());
  Handle<Array> recordArray = records.As<Array>();
  CHECK_EQ(num, static_cast<int>(recordArray->Length()));
  for (int i = 0; i < num; ++i) {
    Handle<Value> record = recordArray->Get(i);
    CHECK(record->IsObject());
    Handle<Object> recordObj = record.As<Object>();
    CHECK(expectations[i].object->StrictEquals(
        recordObj->Get(String::New("object"))));
    CHECK(String::New(expectations[i].type)->Equals(
        recordObj->Get(String::New("type"))));
    CHECK(String::New(expectations[i].name)->Equals(
        recordObj->Get(String::New("name"))));
    if (!expectations[i].old_value.IsEmpty()) {
      CHECK(expectations[i].old_value->Equals(
          recordObj->Get(String::New("oldValue"))));
    }
  }
}

#define EXPECT_RECORDS(records, expectations) \
    ExpectRecords(records, expectations, ARRAY_SIZE(expectations))

TEST(APITestBasicMutation) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context;
  Handle<Object> obj = Handle<Object>::Cast(CompileRun(
      "var records = [];"
      "var obj = {};"
      "function observer(r) { [].push.apply(records, r); };"
      "Object.observe(obj, observer);"
      "obj"));
  obj->Set(String::New("foo"), Number::New(7));
  obj->Set(1, Number::New(2));
  // ForceSet should work just as well as Set
  obj->ForceSet(String::New("foo"), Number::New(3));
  obj->ForceSet(Number::New(1), Number::New(4));
  // Setting an indexed element via the property setting method
  obj->Set(Number::New(1), Number::New(5));
  // Setting with a non-String, non-uint32 key
  obj->Set(Number::New(1.1), Number::New(6), DontDelete);
  obj->Delete(String::New("foo"));
  obj->Delete(1);
  obj->ForceDelete(Number::New(1.1));

  // Force delivery
  // TODO(adamk): Should the above set methods trigger delivery themselves?
  CompileRun("void 0");
  CHECK_EQ(9, CompileRun("records.length")->Int32Value());
  const RecordExpectation expected_records[] = {
    { obj, "new", "foo", Handle<Value>() },
    { obj, "new", "1", Handle<Value>() },
    // Note: use 7 not 1 below, as the latter triggers a nifty VS10 compiler bug
    // where instead of 1.0, a garbage value would be passed into Number::New.
    { obj, "updated", "foo", Number::New(7) },
    { obj, "updated", "1", Number::New(2) },
    { obj, "updated", "1", Number::New(4) },
    { obj, "new", "1.1", Handle<Value>() },
    { obj, "deleted", "foo", Number::New(3) },
    { obj, "deleted", "1", Number::New(5) },
    { obj, "deleted", "1.1", Number::New(6) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
}

TEST(HiddenPrototypeObservation) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context;
  Handle<FunctionTemplate> tmpl = FunctionTemplate::New();
  tmpl->SetHiddenPrototype(true);
  tmpl->InstanceTemplate()->Set(String::New("foo"), Number::New(75));
  Handle<Object> proto = tmpl->GetFunction()->NewInstance();
  Handle<Object> obj = Object::New();
  obj->SetPrototype(proto);
  context->Global()->Set(String::New("obj"), obj);
  context->Global()->Set(String::New("proto"), proto);
  CompileRun(
      "var records;"
      "function observer(r) { records = r; };"
      "Object.observe(obj, observer);"
      "obj.foo = 41;"  // triggers a notification
      "proto.foo = 42;");  // does not trigger a notification
  const RecordExpectation expected_records[] = {
    { obj, "updated", "foo", Number::New(75) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
  obj->SetPrototype(Null());
  CompileRun("obj.foo = 43");
  const RecordExpectation expected_records2[] = {
    { obj, "new", "foo", Handle<Value>() }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records2);
  obj->SetPrototype(proto);
  CompileRun(
      "Object.observe(proto, observer);"
      "proto.bar = 1;"
      "Object.unobserve(obj, observer);"
      "obj.foo = 44;");
  const RecordExpectation expected_records3[] = {
    { proto, "new", "bar", Handle<Value>() }
    // TODO(adamk): The below record should be emitted since proto is observed
    // and has been modified. Not clear if this happens in practice.
    // { proto, "updated", "foo", Number::New(43) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records3);
}


static int NumberOfElements(i::Handle<i::JSWeakMap> map) {
  return i::ObjectHashTable::cast(map->table())->NumberOfElements();
}


TEST(ObservationWeakMap) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context;
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, function(){});"
      "Object.getNotifier(obj);"
      "obj = null;");
  i::Handle<i::JSObject> observation_state = FACTORY->observation_state();
  i::Handle<i::JSWeakMap> observerInfoMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "observerInfoMap"));
  i::Handle<i::JSWeakMap> objectInfoMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "objectInfoMap"));
  i::Handle<i::JSWeakMap> notifierTargetMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "notifierTargetMap"));
  CHECK_EQ(1, NumberOfElements(observerInfoMap));
  CHECK_EQ(1, NumberOfElements(objectInfoMap));
  CHECK_EQ(1, NumberOfElements(notifierTargetMap));
  HEAP->CollectAllGarbage(i::Heap::kAbortIncrementalMarkingMask);
  CHECK_EQ(0, NumberOfElements(observerInfoMap));
  CHECK_EQ(0, NumberOfElements(objectInfoMap));
  CHECK_EQ(0, NumberOfElements(notifierTargetMap));
}

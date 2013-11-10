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
  LocalContext context1(isolate.GetIsolate());
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
    LocalContext context2(isolate.GetIsolate());
    context2->Global()->Set(String::New("obj"), obj);
    notify_fun2 = CompileRun(
        "(function() { obj.foo = 'baz'; })");
  }
  Handle<Value> notify_fun3;
  {
    LocalContext context3(isolate.GetIsolate());
    context3->Global()->Set(String::New("obj"), obj);
    notify_fun3 = CompileRun(
        "(function() { obj.foo = 'bat'; })");
  }
  {
    LocalContext context4(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
  Handle<Value> obj = CompileRun("obj = {};");
  Handle<Value> observer = CompileRun(
      "var ran = false;"
      "(function() { ran = true })");
  {
    // As does initializing this context.
    LocalContext context2(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
    LocalContext context2(isolate.GetIsolate());
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
    LocalContext context3(
        isolate.GetIsolate(), NULL, Handle<ObjectTemplate>(), global_proxy);
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
    if (strcmp("splice", expectations[i].type) != 0) {
      CHECK(String::New(expectations[i].name)->Equals(
          recordObj->Get(String::New("name"))));
      if (!expectations[i].old_value.IsEmpty()) {
        CHECK(expectations[i].old_value->Equals(
            recordObj->Get(String::New("oldValue"))));
      }
    }
  }
}

#define EXPECT_RECORDS(records, expectations) \
    ExpectRecords(records, expectations, ARRAY_SIZE(expectations))

TEST(APITestBasicMutation) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
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
  LocalContext context(isolate.GetIsolate());
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
  obj->SetPrototype(Null(isolate.GetIsolate()));
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
  LocalContext context(isolate.GetIsolate());
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, function(){});"
      "Object.getNotifier(obj);"
      "obj = null;");
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate.GetIsolate());
  i::Handle<i::JSObject> observation_state =
      i_isolate->factory()->observation_state();
  i::Handle<i::JSWeakMap> callbackInfoMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "callbackInfoMap"));
  i::Handle<i::JSWeakMap> objectInfoMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "objectInfoMap"));
  i::Handle<i::JSWeakMap> notifierObjectInfoMap =
      i::Handle<i::JSWeakMap>::cast(
          i::GetProperty(observation_state, "notifierObjectInfoMap"));
  CHECK_EQ(1, NumberOfElements(callbackInfoMap));
  CHECK_EQ(1, NumberOfElements(objectInfoMap));
  CHECK_EQ(1, NumberOfElements(notifierObjectInfoMap));
  i_isolate->heap()->CollectAllGarbage(i::Heap::kAbortIncrementalMarkingMask);
  CHECK_EQ(0, NumberOfElements(callbackInfoMap));
  CHECK_EQ(0, NumberOfElements(objectInfoMap));
  CHECK_EQ(0, NumberOfElements(notifierObjectInfoMap));
}


static bool NamedAccessAlwaysAllowed(Local<Object>, Local<Value>, AccessType,
                                     Local<Value>) {
  return true;
}


static bool IndexedAccessAlwaysAllowed(Local<Object>, uint32_t, AccessType,
                                       Local<Value>) {
  return true;
}


static AccessType g_access_block_type = ACCESS_GET;
static const uint32_t kBlockedContextIndex = 1337;


static bool NamedAccessAllowUnlessBlocked(Local<Object> host,
                                          Local<Value> key,
                                          AccessType type,
                                          Local<Value> data) {
  if (type != g_access_block_type) return true;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(
      Utils::OpenHandle(*host)->GetIsolate());
  Handle<Object> global = isolate->GetCurrentContext()->Global();
  if (!global->Has(kBlockedContextIndex)) return true;
  return !key->IsString() || !key->Equals(data);
}


static bool IndexedAccessAllowUnlessBlocked(Local<Object> host,
                                            uint32_t index,
                                            AccessType type,
                                            Local<Value> data) {
  if (type != g_access_block_type) return true;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(
      Utils::OpenHandle(*host)->GetIsolate());
  Handle<Object> global = isolate->GetCurrentContext()->Global();
  if (!global->Has(kBlockedContextIndex)) return true;
  return index != data->Uint32Value();
}


static bool BlockAccessKeys(Local<Object> host, Local<Value> key,
                            AccessType type, Local<Value>) {
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(
      Utils::OpenHandle(*host)->GetIsolate());
  Handle<Object> global = isolate->GetCurrentContext()->Global();
  return type != ACCESS_KEYS || !global->Has(kBlockedContextIndex);
}


static Handle<Object> CreateAccessCheckedObject(
    NamedSecurityCallback namedCallback,
    IndexedSecurityCallback indexedCallback,
    Handle<Value> data = Handle<Value>()) {
  Handle<ObjectTemplate> tmpl = ObjectTemplate::New();
  tmpl->SetAccessCheckCallbacks(namedCallback, indexedCallback, data);
  Handle<Object> instance = tmpl->NewInstance();
  Handle<Object> global = instance->CreationContext()->Global();
  global->Set(String::New("obj"), instance);
  global->Set(kBlockedContextIndex, v8::True());
  return instance;
}


TEST(NamedAccessCheck) {
  HarmonyIsolate isolate;
  const AccessType types[] = { ACCESS_GET, ACCESS_HAS };
  for (size_t i = 0; i < ARRAY_SIZE(types); ++i) {
    HandleScope scope(isolate.GetIsolate());
    LocalContext context(isolate.GetIsolate());
    g_access_block_type = types[i];
    Handle<Object> instance = CreateAccessCheckedObject(
        NamedAccessAllowUnlessBlocked,
        IndexedAccessAlwaysAllowed,
        String::New("foo"));
    CompileRun("var records = null;"
               "var objNoCheck = {};"
               "var observer = function(r) { records = r };"
               "Object.observe(obj, observer);"
               "Object.observe(objNoCheck, observer);");
    Handle<Value> obj_no_check = CompileRun("objNoCheck");
    {
      LocalContext context2(isolate.GetIsolate());
      context2->Global()->Set(String::New("obj"), instance);
      context2->Global()->Set(String::New("objNoCheck"), obj_no_check);
      CompileRun("var records2 = null;"
                 "var observer2 = function(r) { records2 = r };"
                 "Object.observe(obj, observer2);"
                 "Object.observe(objNoCheck, observer2);"
                 "obj.foo = 'bar';"
                 "Object.defineProperty(obj, 'foo', {value: 5});"
                 "Object.defineProperty(obj, 'foo', {get: function(){}});"
                 "obj.bar = 'baz';"
                 "objNoCheck.baz = 'quux'");
      const RecordExpectation expected_records2[] = {
        { instance, "new", "foo", Handle<Value>() },
        { instance, "updated", "foo", String::New("bar") },
        { instance, "reconfigured", "foo", Number::New(5) },
        { instance, "new", "bar", Handle<Value>() },
        { obj_no_check, "new", "baz", Handle<Value>() },
      };
      EXPECT_RECORDS(CompileRun("records2"), expected_records2);
    }
    const RecordExpectation expected_records[] = {
      { instance, "new", "bar", Handle<Value>() },
      { obj_no_check, "new", "baz", Handle<Value>() }
    };
    EXPECT_RECORDS(CompileRun("records"), expected_records);
  }
}


TEST(IndexedAccessCheck) {
  HarmonyIsolate isolate;
  const AccessType types[] = { ACCESS_GET, ACCESS_HAS };
  for (size_t i = 0; i < ARRAY_SIZE(types); ++i) {
    HandleScope scope(isolate.GetIsolate());
    LocalContext context(isolate.GetIsolate());
    g_access_block_type = types[i];
    Handle<Object> instance = CreateAccessCheckedObject(
        NamedAccessAlwaysAllowed, IndexedAccessAllowUnlessBlocked,
        Number::New(7));
    CompileRun("var records = null;"
               "var objNoCheck = {};"
               "var observer = function(r) { records = r };"
               "Object.observe(obj, observer);"
               "Object.observe(objNoCheck, observer);");
    Handle<Value> obj_no_check = CompileRun("objNoCheck");
    {
      LocalContext context2(isolate.GetIsolate());
      context2->Global()->Set(String::New("obj"), instance);
      context2->Global()->Set(String::New("objNoCheck"), obj_no_check);
      CompileRun("var records2 = null;"
                 "var observer2 = function(r) { records2 = r };"
                 "Object.observe(obj, observer2);"
                 "Object.observe(objNoCheck, observer2);"
                 "obj[7] = 'foo';"
                 "Object.defineProperty(obj, '7', {value: 5});"
                 "Object.defineProperty(obj, '7', {get: function(){}});"
                 "obj[8] = 'bar';"
                 "objNoCheck[42] = 'quux'");
      const RecordExpectation expected_records2[] = {
        { instance, "new", "7", Handle<Value>() },
        { instance, "updated", "7", String::New("foo") },
        { instance, "reconfigured", "7", Number::New(5) },
        { instance, "new", "8", Handle<Value>() },
        { obj_no_check, "new", "42", Handle<Value>() }
      };
      EXPECT_RECORDS(CompileRun("records2"), expected_records2);
    }
    const RecordExpectation expected_records[] = {
      { instance, "new", "8", Handle<Value>() },
      { obj_no_check, "new", "42", Handle<Value>() }
    };
    EXPECT_RECORDS(CompileRun("records"), expected_records);
  }
}


TEST(SpliceAccessCheck) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
  g_access_block_type = ACCESS_GET;
  Handle<Object> instance = CreateAccessCheckedObject(
      NamedAccessAlwaysAllowed, IndexedAccessAllowUnlessBlocked,
      Number::New(1));
  CompileRun("var records = null;"
             "obj[1] = 'foo';"
             "obj.length = 2;"
             "var objNoCheck = {1: 'bar', length: 2};"
             "observer = function(r) { records = r };"
             "Array.observe(obj, observer);"
             "Array.observe(objNoCheck, observer);");
  Handle<Value> obj_no_check = CompileRun("objNoCheck");
  {
    LocalContext context2(isolate.GetIsolate());
    context2->Global()->Set(String::New("obj"), instance);
    context2->Global()->Set(String::New("objNoCheck"), obj_no_check);
    CompileRun("var records2 = null;"
               "var observer2 = function(r) { records2 = r };"
               "Array.observe(obj, observer2);"
               "Array.observe(objNoCheck, observer2);"
               // No one should hear about this: no splice records are emitted
               // for access-checked objects
               "[].push.call(obj, 5);"
               "[].splice.call(obj, 1, 1);"
               "[].pop.call(obj);"
               "[].pop.call(objNoCheck);");
    // TODO(adamk): Extend EXPECT_RECORDS to be able to assert more things
    // about splice records. For this test it's not so important since
    // we just want to guarantee the machinery is in operation at all.
    const RecordExpectation expected_records2[] = {
      { obj_no_check, "splice", "", Handle<Value>() }
    };
    EXPECT_RECORDS(CompileRun("records2"), expected_records2);
  }
  const RecordExpectation expected_records[] = {
    { obj_no_check, "splice", "", Handle<Value>() }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
}


TEST(DisallowAllForAccessKeys) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
  Handle<Object> instance = CreateAccessCheckedObject(
      BlockAccessKeys, IndexedAccessAlwaysAllowed);
  CompileRun("var records = null;"
             "var objNoCheck = {};"
             "var observer = function(r) { records = r };"
             "Object.observe(obj, observer);"
             "Object.observe(objNoCheck, observer);");
  Handle<Value> obj_no_check = CompileRun("objNoCheck");
  {
    LocalContext context2(isolate.GetIsolate());
    context2->Global()->Set(String::New("obj"), instance);
    context2->Global()->Set(String::New("objNoCheck"), obj_no_check);
    CompileRun("var records2 = null;"
               "var observer2 = function(r) { records2 = r };"
               "Object.observe(obj, observer2);"
               "Object.observe(objNoCheck, observer2);"
               "obj.foo = 'bar';"
               "obj[5] = 'baz';"
               "objNoCheck.baz = 'quux'");
    const RecordExpectation expected_records2[] = {
      { instance, "new", "foo", Handle<Value>() },
      { instance, "new", "5", Handle<Value>() },
      { obj_no_check, "new", "baz", Handle<Value>() },
    };
    EXPECT_RECORDS(CompileRun("records2"), expected_records2);
  }
  const RecordExpectation expected_records[] = {
    { obj_no_check, "new", "baz", Handle<Value>() }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
}


TEST(AccessCheckDisallowApiModifications) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
  Handle<Object> instance = CreateAccessCheckedObject(
      BlockAccessKeys, IndexedAccessAlwaysAllowed);
  CompileRun("var records = null;"
             "var observer = function(r) { records = r };"
             "Object.observe(obj, observer);");
  {
    LocalContext context2(isolate.GetIsolate());
    context2->Global()->Set(String::New("obj"), instance);
    CompileRun("var records2 = null;"
               "var observer2 = function(r) { records2 = r };"
               "Object.observe(obj, observer2);");
    instance->Set(5, String::New("bar"));
    instance->Set(String::New("foo"), String::New("bar"));
    CompileRun("");  // trigger delivery
    const RecordExpectation expected_records2[] = {
      { instance, "new", "5", Handle<Value>() },
      { instance, "new", "foo", Handle<Value>() }
    };
    EXPECT_RECORDS(CompileRun("records2"), expected_records2);
  }
  CHECK(CompileRun("records")->IsNull());
}


TEST(HiddenPropertiesLeakage) {
  HarmonyIsolate isolate;
  HandleScope scope(isolate.GetIsolate());
  LocalContext context(isolate.GetIsolate());
  CompileRun("var obj = {};"
             "var records = null;"
             "var observer = function(r) { records = r };"
             "Object.observe(obj, observer);");
  Handle<Value> obj = context->Global()->Get(String::New("obj"));
  Handle<Object>::Cast(obj)->SetHiddenValue(String::New("foo"), Null());
  CompileRun("");  // trigger delivery
  CHECK(CompileRun("records")->IsNull());
}

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

using namespace v8;
namespace i = v8::internal;


TEST(PerIsolateState) {
  HandleScope scope(CcTest::isolate());
  LocalContext context1(CcTest::isolate());

  Local<Value> foo = v8_str("foo");
  context1->SetSecurityToken(foo);

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
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            obj);
    notify_fun2 = CompileRun(
        "(function() { obj.foo = 'baz'; })");
  }
  Handle<Value> notify_fun3;
  {
    LocalContext context3(CcTest::isolate());
    context3->SetSecurityToken(foo);
    context3->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            obj);
    notify_fun3 = CompileRun(
        "(function() { obj.foo = 'bat'; })");
  }
  {
    LocalContext context4(CcTest::isolate());
    context4->SetSecurityToken(foo);
    context4->Global()->Set(
        String::NewFromUtf8(CcTest::isolate(), "observer"), observer);
    context4->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "fun1"),
                            notify_fun1);
    context4->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "fun2"),
                            notify_fun2);
    context4->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "fun3"),
                            notify_fun3);
    CompileRun("fun1(); fun2(); fun3(); Object.deliverChangeRecords(observer)");
  }
  CHECK_EQ(1, CompileRun("calls")->Int32Value());
  CHECK_EQ(3, CompileRun("count")->Int32Value());
}


TEST(EndOfMicrotaskDelivery) {
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun(
      "var obj = {};"
      "var count = 0;"
      "var observer = function(records) { count = records.length };"
      "Object.observe(obj, observer);"
      "obj.foo = 'bar';");
  CHECK_EQ(1, CompileRun("count")->Int32Value());
}


TEST(DeliveryOrdering) {
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
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
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
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
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
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
  HandleScope scope(CcTest::isolate());
  // Initializing this context sets up initial hash tables.
  LocalContext context(CcTest::isolate());
  Handle<Value> obj = CompileRun("obj = {};");
  Handle<Value> observer = CompileRun(
      "var ran = false;"
      "(function() { ran = true })");
  {
    // As does initializing this context.
    LocalContext context2(CcTest::isolate());
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            obj);
    context2->Global()->Set(
        String::NewFromUtf8(CcTest::isolate(), "observer"), observer);
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


struct RecordExpectation {
  Handle<Value> object;
  const char* type;
  const char* name;
  Handle<Value> old_value;
};


// TODO(adamk): Use this helper elsewhere in this file.
static void ExpectRecords(v8::Isolate* isolate,
                          Handle<Value> records,
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
        recordObj->Get(String::NewFromUtf8(isolate, "object"))));
    CHECK(String::NewFromUtf8(isolate, expectations[i].type)->Equals(
        recordObj->Get(String::NewFromUtf8(isolate, "type"))));
    if (strcmp("splice", expectations[i].type) != 0) {
      CHECK(String::NewFromUtf8(isolate, expectations[i].name)->Equals(
          recordObj->Get(String::NewFromUtf8(isolate, "name"))));
      if (!expectations[i].old_value.IsEmpty()) {
        CHECK(expectations[i].old_value->Equals(
            recordObj->Get(String::NewFromUtf8(isolate, "oldValue"))));
      }
    }
  }
}

#define EXPECT_RECORDS(records, expectations)                \
  ExpectRecords(CcTest::isolate(), records, expectations, \
                arraysize(expectations))

TEST(APITestBasicMutation) {
  v8::Isolate* v8_isolate = CcTest::isolate();
  HandleScope scope(v8_isolate);
  LocalContext context(v8_isolate);
  Handle<Object> obj = Handle<Object>::Cast(CompileRun(
      "var records = [];"
      "var obj = {};"
      "function observer(r) { [].push.apply(records, r); };"
      "Object.observe(obj, observer);"
      "obj"));
  obj->Set(String::NewFromUtf8(v8_isolate, "foo"),
           Number::New(v8_isolate, 7));
  obj->Set(1, Number::New(v8_isolate, 2));
  // ForceSet should work just as well as Set
  obj->ForceSet(String::NewFromUtf8(v8_isolate, "foo"),
                Number::New(v8_isolate, 3));
  obj->ForceSet(Number::New(v8_isolate, 1), Number::New(v8_isolate, 4));
  // Setting an indexed element via the property setting method
  obj->Set(Number::New(v8_isolate, 1), Number::New(v8_isolate, 5));
  // Setting with a non-String, non-uint32 key
  obj->ForceSet(Number::New(v8_isolate, 1.1), Number::New(v8_isolate, 6),
                DontDelete);
  obj->Delete(String::NewFromUtf8(v8_isolate, "foo"));
  obj->Delete(1);
  obj->ForceDelete(Number::New(v8_isolate, 1.1));

  // Force delivery
  // TODO(adamk): Should the above set methods trigger delivery themselves?
  CompileRun("void 0");
  CHECK_EQ(9, CompileRun("records.length")->Int32Value());
  const RecordExpectation expected_records[] = {
    { obj, "add", "foo", Handle<Value>() },
    { obj, "add", "1", Handle<Value>() },
    // Note: use 7 not 1 below, as the latter triggers a nifty VS10 compiler bug
    // where instead of 1.0, a garbage value would be passed into Number::New.
    { obj, "update", "foo", Number::New(v8_isolate, 7) },
    { obj, "update", "1", Number::New(v8_isolate, 2) },
    { obj, "update", "1", Number::New(v8_isolate, 4) },
    { obj, "add", "1.1", Handle<Value>() },
    { obj, "delete", "foo", Number::New(v8_isolate, 3) },
    { obj, "delete", "1", Number::New(v8_isolate, 5) },
    { obj, "delete", "1.1", Number::New(v8_isolate, 6) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
}


TEST(HiddenPrototypeObservation) {
  v8::Isolate* v8_isolate = CcTest::isolate();
  HandleScope scope(v8_isolate);
  LocalContext context(v8_isolate);
  Handle<FunctionTemplate> tmpl = FunctionTemplate::New(v8_isolate);
  tmpl->SetHiddenPrototype(true);
  tmpl->InstanceTemplate()->Set(
      String::NewFromUtf8(v8_isolate, "foo"), Number::New(v8_isolate, 75));
  Handle<Object> proto = tmpl->GetFunction()->NewInstance();
  Handle<Object> obj = Object::New(v8_isolate);
  obj->SetPrototype(proto);
  context->Global()->Set(String::NewFromUtf8(v8_isolate, "obj"), obj);
  context->Global()->Set(String::NewFromUtf8(v8_isolate, "proto"),
                         proto);
  CompileRun(
      "var records;"
      "function observer(r) { records = r; };"
      "Object.observe(obj, observer);"
      "obj.foo = 41;"  // triggers a notification
      "proto.foo = 42;");  // does not trigger a notification
  const RecordExpectation expected_records[] = {
    { obj, "update", "foo", Number::New(v8_isolate, 75) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records);
  obj->SetPrototype(Null(v8_isolate));
  CompileRun("obj.foo = 43");
  const RecordExpectation expected_records2[] = {
    { obj, "add", "foo", Handle<Value>() }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records2);
  obj->SetPrototype(proto);
  CompileRun(
      "Object.observe(proto, observer);"
      "proto.bar = 1;"
      "Object.unobserve(obj, observer);"
      "obj.foo = 44;");
  const RecordExpectation expected_records3[] = {
    { proto, "add", "bar", Handle<Value>() }
    // TODO(adamk): The below record should be emitted since proto is observed
    // and has been modified. Not clear if this happens in practice.
    // { proto, "update", "foo", Number::New(43) }
  };
  EXPECT_RECORDS(CompileRun("records"), expected_records3);
}


static int NumberOfElements(i::Handle<i::JSWeakMap> map) {
  return i::ObjectHashTable::cast(map->table())->NumberOfElements();
}


TEST(ObservationWeakMap) {
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, function(){});"
      "Object.getNotifier(obj);"
      "obj = null;");
  i::Isolate* i_isolate = CcTest::i_isolate();
  i::Handle<i::JSObject> observation_state =
      i_isolate->factory()->observation_state();
  i::Handle<i::JSWeakMap> callbackInfoMap =
      i::Handle<i::JSWeakMap>::cast(i::Object::GetProperty(
          i_isolate, observation_state, "callbackInfoMap").ToHandleChecked());
  i::Handle<i::JSWeakMap> objectInfoMap =
      i::Handle<i::JSWeakMap>::cast(i::Object::GetProperty(
          i_isolate, observation_state, "objectInfoMap").ToHandleChecked());
  i::Handle<i::JSWeakMap> notifierObjectInfoMap =
      i::Handle<i::JSWeakMap>::cast(i::Object::GetProperty(
          i_isolate, observation_state, "notifierObjectInfoMap")
              .ToHandleChecked());
  CHECK_EQ(1, NumberOfElements(callbackInfoMap));
  CHECK_EQ(1, NumberOfElements(objectInfoMap));
  CHECK_EQ(1, NumberOfElements(notifierObjectInfoMap));
  i_isolate->heap()->CollectAllGarbage(i::Heap::kAbortIncrementalMarkingMask);
  CHECK_EQ(0, NumberOfElements(callbackInfoMap));
  CHECK_EQ(0, NumberOfElements(objectInfoMap));
  CHECK_EQ(0, NumberOfElements(notifierObjectInfoMap));
}


static int TestObserveSecurity(Handle<Context> observer_context,
                               Handle<Context> object_context,
                               Handle<Context> mutation_context) {
  Context::Scope observer_scope(observer_context);
  CompileRun("var records = null;"
             "var observer = function(r) { records = r };");
  Handle<Value> observer = CompileRun("observer");
  {
    Context::Scope object_scope(object_context);
    object_context->Global()->Set(
        String::NewFromUtf8(CcTest::isolate(), "observer"), observer);
    CompileRun("var obj = {};"
               "obj.length = 0;"
               "Object.observe(obj, observer,"
                   "['add', 'update', 'delete','reconfigure','splice']"
               ");");
    Handle<Value> obj = CompileRun("obj");
    {
      Context::Scope mutation_scope(mutation_context);
      mutation_context->Global()->Set(
          String::NewFromUtf8(CcTest::isolate(), "obj"), obj);
      CompileRun("obj.foo = 'bar';"
                 "obj.foo = 'baz';"
                 "delete obj.foo;"
                 "Object.defineProperty(obj, 'bar', {value: 'bot'});"
                 "Array.prototype.push.call(obj, 1, 2, 3);"
                 "Array.prototype.splice.call(obj, 1, 2, 2, 4);"
                 "Array.prototype.pop.call(obj);"
                 "Array.prototype.shift.call(obj);");
    }
  }
  return CompileRun("records ? records.length : 0")->Int32Value();
}


TEST(ObserverSecurityAAA) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  CHECK_EQ(8, TestObserveSecurity(contextA, contextA, contextA));
}


TEST(ObserverSecurityA1A2A3) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  v8::Local<Context> contextA1 = Context::New(isolate);
  v8::Local<Context> contextA2 = Context::New(isolate);
  v8::Local<Context> contextA3 = Context::New(isolate);

  Local<Value> foo = v8_str("foo");
  contextA1->SetSecurityToken(foo);
  contextA2->SetSecurityToken(foo);
  contextA3->SetSecurityToken(foo);

  CHECK_EQ(8, TestObserveSecurity(contextA1, contextA2, contextA3));
}


TEST(ObserverSecurityAAB) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextA, contextA, contextB));
}


TEST(ObserverSecurityA1A2B) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);

  v8::Local<Context> contextA1 = Context::New(isolate);
  v8::Local<Context> contextA2 = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);

  Local<Value> foo = v8_str("foo");
  contextA1->SetSecurityToken(foo);
  contextA2->SetSecurityToken(foo);

  CHECK_EQ(0, TestObserveSecurity(contextA1, contextA2, contextB));
}


TEST(ObserverSecurityABA) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextA, contextB, contextA));
}


TEST(ObserverSecurityA1BA2) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA1 = Context::New(isolate);
  v8::Local<Context> contextA2 = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);

  Local<Value> foo = v8_str("foo");
  contextA1->SetSecurityToken(foo);
  contextA2->SetSecurityToken(foo);

  CHECK_EQ(0, TestObserveSecurity(contextA1, contextB, contextA2));
}


TEST(ObserverSecurityBAA) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextB, contextA, contextA));
}


TEST(ObserverSecurityBA1A2) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA1 = Context::New(isolate);
  v8::Local<Context> contextA2 = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);

  Local<Value> foo = v8_str("foo");
  contextA1->SetSecurityToken(foo);
  contextA2->SetSecurityToken(foo);

  CHECK_EQ(0, TestObserveSecurity(contextB, contextA1, contextA2));
}


TEST(ObserverSecurityNotify) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);

  Context::Scope scopeA(contextA);
  CompileRun("var obj = {};"
             "var recordsA = null;"
             "var observerA = function(r) { recordsA = r };"
             "Object.observe(obj, observerA);");
  Handle<Value> obj = CompileRun("obj");

  {
    Context::Scope scopeB(contextB);
    contextB->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"), obj);
    CompileRun("var recordsB = null;"
               "var observerB = function(r) { recordsB = r };"
               "Object.observe(obj, observerB);");
  }

  CompileRun("var notifier = Object.getNotifier(obj);"
             "notifier.notify({ type: 'update' });");
  CHECK_EQ(1, CompileRun("recordsA ? recordsA.length : 0")->Int32Value());

  {
    Context::Scope scopeB(contextB);
    CHECK_EQ(0, CompileRun("recordsB ? recordsB.length : 0")->Int32Value());
  }
}


TEST(HiddenPropertiesLeakage) {
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun("var obj = {};"
             "var records = null;"
             "var observer = function(r) { records = r };"
             "Object.observe(obj, observer);");
  Handle<Value> obj =
      context->Global()->Get(String::NewFromUtf8(CcTest::isolate(), "obj"));
  Handle<Object>::Cast(obj)
      ->SetHiddenValue(String::NewFromUtf8(CcTest::isolate(), "foo"),
                       Null(CcTest::isolate()));
  CompileRun("");  // trigger delivery
  CHECK(CompileRun("records")->IsNull());
}


TEST(GetNotifierFromOtherContext) {
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun("var obj = {};");
  Handle<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            instance);
    CHECK(CompileRun("Object.getNotifier(obj)")->IsNull());
  }
}


TEST(GetNotifierFromOtherOrigin) {
  HandleScope scope(CcTest::isolate());
  Handle<Value> foo = String::NewFromUtf8(CcTest::isolate(), "foo");
  Handle<Value> bar = String::NewFromUtf8(CcTest::isolate(), "bar");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Handle<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(bar);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            instance);
    CHECK(CompileRun("Object.getNotifier(obj)")->IsNull());
  }
}


TEST(GetNotifierFromSameOrigin) {
  HandleScope scope(CcTest::isolate());
  Handle<Value> foo = String::NewFromUtf8(CcTest::isolate(), "foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Handle<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            instance);
    CHECK(CompileRun("Object.getNotifier(obj)")->IsObject());
  }
}


static int GetGlobalObjectsCount() {
  int count = 0;
  i::HeapIterator it(CcTest::heap());
  for (i::HeapObject* object = it.next(); object != NULL; object = it.next())
    if (object->IsJSGlobalObject()) count++;
  return count;
}


static void CheckSurvivingGlobalObjectsCount(int expected) {
  // We need to collect all garbage twice to be sure that everything
  // has been collected.  This is because inline caches are cleared in
  // the first garbage collection but some of the maps have already
  // been marked at that point.  Therefore some of the maps are not
  // collected until the second garbage collection.
  CcTest::heap()->CollectAllGarbage(i::Heap::kNoGCFlags);
  CcTest::heap()->CollectAllGarbage(i::Heap::kMakeHeapIterableMask);
  int count = GetGlobalObjectsCount();
#ifdef DEBUG
  if (count != expected) CcTest::heap()->TracePathToGlobal();
#endif
  CHECK_EQ(expected, count);
}


TEST(DontLeakContextOnObserve) {
  HandleScope scope(CcTest::isolate());
  Handle<Value> foo = String::NewFromUtf8(CcTest::isolate(), "foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Handle<Value> object = CompileRun("obj");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            object);
    CompileRun("function observer() {};"
               "Object.observe(obj, observer, ['foo', 'bar', 'baz']);"
               "Object.unobserve(obj, observer);");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(1);
}


TEST(DontLeakContextOnGetNotifier) {
  HandleScope scope(CcTest::isolate());
  Handle<Value> foo = String::NewFromUtf8(CcTest::isolate(), "foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Handle<Value> object = CompileRun("obj");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            object);
    CompileRun("Object.getNotifier(obj);");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(1);
}


TEST(DontLeakContextOnNotifierPerformChange) {
  HandleScope scope(CcTest::isolate());
  Handle<Value> foo = String::NewFromUtf8(CcTest::isolate(), "foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Handle<Value> object = CompileRun("obj");
  Handle<Value> notifier = CompileRun("Object.getNotifier(obj)");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "obj"),
                            object);
    context2->Global()->Set(String::NewFromUtf8(CcTest::isolate(), "notifier"),
                            notifier);
    CompileRun("var obj2 = {};"
               "var notifier2 = Object.getNotifier(obj2);"
               "notifier2.performChange.call("
                   "notifier, 'foo', function(){})");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(1);
}

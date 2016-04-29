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

inline int32_t ToInt32(v8::Local<v8::Value> value) {
  return value->Int32Value(v8::Isolate::GetCurrent()->GetCurrentContext())
      .FromJust();
}


TEST(PerIsolateState) {
  i::FLAG_harmony_object_observe = true;
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
  Local<Value> observer = CompileRun("observer");
  Local<Value> obj = CompileRun("obj");
  Local<Value> notify_fun1 = CompileRun("(function() { obj.foo = 'bar'; })");
  Local<Value> notify_fun2;
  {
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              obj)
        .FromJust();
    notify_fun2 = CompileRun(
        "(function() { obj.foo = 'baz'; })");
  }
  Local<Value> notify_fun3;
  {
    LocalContext context3(CcTest::isolate());
    context3->SetSecurityToken(foo);
    context3->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              obj)
        .FromJust();
    notify_fun3 = CompileRun("(function() { obj.foo = 'bat'; })");
  }
  {
    LocalContext context4(CcTest::isolate());
    context4->SetSecurityToken(foo);
    context4->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
              v8_str("observer"), observer)
        .FromJust();
    context4->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("fun1"),
              notify_fun1)
        .FromJust();
    context4->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("fun2"),
              notify_fun2)
        .FromJust();
    context4->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("fun3"),
              notify_fun3)
        .FromJust();
    CompileRun("fun1(); fun2(); fun3(); Object.deliverChangeRecords(observer)");
  }
  CHECK_EQ(1, ToInt32(CompileRun("calls")));
  CHECK_EQ(3, ToInt32(CompileRun("count")));
}


TEST(EndOfMicrotaskDelivery) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun(
      "var obj = {};"
      "var count = 0;"
      "var observer = function(records) { count = records.length };"
      "Object.observe(obj, observer);"
      "obj.foo = 'bar';");
  CHECK_EQ(1, ToInt32(CompileRun("count")));
}


TEST(DeliveryOrdering) {
  i::FLAG_harmony_object_observe = true;
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
  CHECK_EQ(3, ToInt32(CompileRun("ordering.length")));
  CHECK_EQ(1, ToInt32(CompileRun("ordering[0]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[1]")));
  CHECK_EQ(3, ToInt32(CompileRun("ordering[2]")));
  CompileRun(
      "ordering = [];"
      "Object.observe(obj2, observer3);"
      "Object.observe(obj2, observer2);"
      "Object.observe(obj2, observer1);"
      "obj2.foo = 'baz'");
  CHECK_EQ(3, ToInt32(CompileRun("ordering.length")));
  CHECK_EQ(1, ToInt32(CompileRun("ordering[0]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[1]")));
  CHECK_EQ(3, ToInt32(CompileRun("ordering[2]")));
}


TEST(DeliveryCallbackThrows) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun(
      "var obj = {};"
      "var ordering = [];"
      "function observer1() { ordering.push(1); };"
      "function observer2() { ordering.push(2); };"
      "function observer_throws() {"
      "  ordering.push(0);"
      "  throw new Error();"
      "  ordering.push(-1);"
      "};"
      "Object.observe(obj, observer_throws.bind());"
      "Object.observe(obj, observer1);"
      "Object.observe(obj, observer_throws.bind());"
      "Object.observe(obj, observer2);"
      "Object.observe(obj, observer_throws.bind());"
      "obj.foo = 'bar';");
  CHECK_EQ(5, ToInt32(CompileRun("ordering.length")));
  CHECK_EQ(0, ToInt32(CompileRun("ordering[0]")));
  CHECK_EQ(1, ToInt32(CompileRun("ordering[1]")));
  CHECK_EQ(0, ToInt32(CompileRun("ordering[2]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[3]")));
  CHECK_EQ(0, ToInt32(CompileRun("ordering[4]")));
}


TEST(DeliveryChangesMutationInCallback) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun(
      "var obj = {};"
      "var ordering = [];"
      "function observer1(records) {"
      "  ordering.push(100 + records.length);"
      "  records.push(11);"
      "  records.push(22);"
      "};"
      "function observer2(records) {"
      "  ordering.push(200 + records.length);"
      "  records.push(33);"
      "  records.push(44);"
      "};"
      "Object.observe(obj, observer1);"
      "Object.observe(obj, observer2);"
      "obj.foo = 'bar';");
  CHECK_EQ(2, ToInt32(CompileRun("ordering.length")));
  CHECK_EQ(101, ToInt32(CompileRun("ordering[0]")));
  CHECK_EQ(201, ToInt32(CompileRun("ordering[1]")));
}


TEST(DeliveryOrderingReentrant) {
  i::FLAG_harmony_object_observe = true;
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
  CHECK_EQ(5, ToInt32(CompileRun("ordering.length")));
  CHECK_EQ(1, ToInt32(CompileRun("ordering[0]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[1]")));
  CHECK_EQ(3, ToInt32(CompileRun("ordering[2]")));
  // Note that we re-deliver to observers 1 and 2, while observer3
  // already received the second record during the first round.
  CHECK_EQ(1, ToInt32(CompileRun("ordering[3]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[1]")));
}


TEST(DeliveryOrderingDeliverChangeRecords) {
  i::FLAG_harmony_object_observe = true;
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
  CHECK_EQ(4, ToInt32(CompileRun("ordering.length")));
  // First, observer2 is called due to deliverChangeRecords
  CHECK_EQ(2, ToInt32(CompileRun("ordering[0]")));
  // Then, observer1 is called when the stack unwinds
  CHECK_EQ(1, ToInt32(CompileRun("ordering[1]")));
  // observer1's mutation causes both 1 and 2 to be reactivated,
  // with 1 having priority.
  CHECK_EQ(1, ToInt32(CompileRun("ordering[2]")));
  CHECK_EQ(2, ToInt32(CompileRun("ordering[3]")));
}


TEST(ObjectHashTableGrowth) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  // Initializing this context sets up initial hash tables.
  LocalContext context(CcTest::isolate());
  Local<Value> obj = CompileRun("obj = {};");
  Local<Value> observer = CompileRun(
      "var ran = false;"
      "(function() { ran = true })");
  {
    // As does initializing this context.
    LocalContext context2(CcTest::isolate());
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              obj)
        .FromJust();
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
              v8_str("observer"), observer)
        .FromJust();
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
  CHECK(CompileRun("ran")
            ->BooleanValue(v8::Isolate::GetCurrent()->GetCurrentContext())
            .FromJust());
}


struct RecordExpectation {
  Local<Value> object;
  const char* type;
  const char* name;
  Local<Value> old_value;
};


// TODO(adamk): Use this helper elsewhere in this file.
static void ExpectRecords(v8::Isolate* isolate, Local<Value> records,
                          const RecordExpectation expectations[], int num) {
  CHECK(records->IsArray());
  Local<Array> recordArray = records.As<Array>();
  CHECK_EQ(num, static_cast<int>(recordArray->Length()));
  for (int i = 0; i < num; ++i) {
    Local<Value> record =
        recordArray->Get(v8::Isolate::GetCurrent()->GetCurrentContext(), i)
            .ToLocalChecked();
    CHECK(record->IsObject());
    Local<Object> recordObj = record.As<Object>();
    Local<Value> value =
        recordObj->Get(v8::Isolate::GetCurrent()->GetCurrentContext(),
                       v8_str("object"))
            .ToLocalChecked();
    CHECK(expectations[i].object->StrictEquals(value));
    value = recordObj->Get(v8::Isolate::GetCurrent()->GetCurrentContext(),
                           v8_str("type"))
                .ToLocalChecked();
    CHECK(v8_str(expectations[i].type)
              ->Equals(v8::Isolate::GetCurrent()->GetCurrentContext(), value)
              .FromJust());
    if (strcmp("splice", expectations[i].type) != 0) {
      Local<Value> name =
          recordObj->Get(v8::Isolate::GetCurrent()->GetCurrentContext(),
                         v8_str("name"))
              .ToLocalChecked();
      CHECK(v8_str(expectations[i].name)
                ->Equals(v8::Isolate::GetCurrent()->GetCurrentContext(), name)
                .FromJust());
      if (!expectations[i].old_value.IsEmpty()) {
        Local<Value> old_value =
            recordObj->Get(v8::Isolate::GetCurrent()->GetCurrentContext(),
                           v8_str("oldValue"))
                .ToLocalChecked();
        CHECK(expectations[i]
                  .old_value->Equals(
                                v8::Isolate::GetCurrent()->GetCurrentContext(),
                                old_value)
                  .FromJust());
      }
    }
  }
}

#define EXPECT_RECORDS(records, expectations)                \
  ExpectRecords(CcTest::isolate(), records, expectations, \
                arraysize(expectations))

TEST(APITestBasicMutation) {
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* v8_isolate = CcTest::isolate();
  HandleScope scope(v8_isolate);
  LocalContext context(v8_isolate);
  Local<Object> obj = Local<Object>::Cast(
      CompileRun("var records = [];"
                 "var obj = {};"
                 "function observer(r) { [].push.apply(records, r); };"
                 "Object.observe(obj, observer);"
                 "obj"));
  obj->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("foo"),
           Number::New(v8_isolate, 7))
      .FromJust();
  obj->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), 1,
           Number::New(v8_isolate, 2))
      .FromJust();
  // CreateDataProperty should work just as well as Set
  obj->CreateDataProperty(v8::Isolate::GetCurrent()->GetCurrentContext(),
                          v8_str("foo"), Number::New(v8_isolate, 3))
      .FromJust();
  obj->CreateDataProperty(v8::Isolate::GetCurrent()->GetCurrentContext(), 1,
                          Number::New(v8_isolate, 4))
      .FromJust();
  // Setting an indexed element via the property setting method
  obj->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
           Number::New(v8_isolate, 1), Number::New(v8_isolate, 5))
      .FromJust();
  // Setting with a non-String, non-uint32 key
  obj->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
           Number::New(v8_isolate, 1.1), Number::New(v8_isolate, 6))
      .FromJust();
  obj->Delete(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("foo"))
      .FromJust();
  obj->Delete(v8::Isolate::GetCurrent()->GetCurrentContext(), 1).FromJust();
  obj->Delete(v8::Isolate::GetCurrent()->GetCurrentContext(),
              Number::New(v8_isolate, 1.1))
      .FromJust();

  // Force delivery
  // TODO(adamk): Should the above set methods trigger delivery themselves?
  CompileRun("void 0");
  CHECK_EQ(9, ToInt32(CompileRun("records.length")));
  const RecordExpectation expected_records[] = {
      {obj, "add", "foo", Local<Value>()},
      {obj, "add", "1", Local<Value>()},
      // Note: use 7 not 1 below, as the latter triggers a nifty VS10 compiler
      // bug
      // where instead of 1.0, a garbage value would be passed into Number::New.
      {obj, "update", "foo", Number::New(v8_isolate, 7)},
      {obj, "update", "1", Number::New(v8_isolate, 2)},
      {obj, "update", "1", Number::New(v8_isolate, 4)},
      {obj, "add", "1.1", Local<Value>()},
      {obj, "delete", "foo", Number::New(v8_isolate, 3)},
      {obj, "delete", "1", Number::New(v8_isolate, 5)},
      {obj, "delete", "1.1", Number::New(v8_isolate, 6)}};
  EXPECT_RECORDS(CompileRun("records"), expected_records);
}


TEST(HiddenPrototypeObservation) {
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* v8_isolate = CcTest::isolate();
  HandleScope scope(v8_isolate);
  LocalContext context(v8_isolate);
  Local<FunctionTemplate> tmpl = FunctionTemplate::New(v8_isolate);
  tmpl->SetHiddenPrototype(true);
  tmpl->InstanceTemplate()->Set(v8_str("foo"), Number::New(v8_isolate, 75));
  Local<Function> function =
      tmpl->GetFunction(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  Local<Object> proto =
      function->NewInstance(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  Local<Object> obj = Object::New(v8_isolate);
  obj->SetPrototype(v8::Isolate::GetCurrent()->GetCurrentContext(), proto)
      .FromJust();
  context->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"), obj)
      .FromJust();
  context->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("proto"),
            proto)
      .FromJust();
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
  obj->SetPrototype(v8::Isolate::GetCurrent()->GetCurrentContext(),
                    Null(v8_isolate))
      .FromJust();
  CompileRun("obj.foo = 43");
  const RecordExpectation expected_records2[] = {
      {obj, "add", "foo", Local<Value>()}};
  EXPECT_RECORDS(CompileRun("records"), expected_records2);
  obj->SetPrototype(v8::Isolate::GetCurrent()->GetCurrentContext(), proto)
      .FromJust();
  CompileRun(
      "Object.observe(proto, observer);"
      "proto.bar = 1;"
      "Object.unobserve(obj, observer);"
      "obj.foo = 44;");
  const RecordExpectation expected_records3[] = {
      {proto, "add", "bar", Local<Value>()}
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
  i::FLAG_harmony_object_observe = true;
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
  i::Handle<i::JSWeakMap> callbackInfoMap = i::Handle<i::JSWeakMap>::cast(
      i::JSReceiver::GetProperty(i_isolate, observation_state,
                                 "callbackInfoMap")
          .ToHandleChecked());
  i::Handle<i::JSWeakMap> objectInfoMap = i::Handle<i::JSWeakMap>::cast(
      i::JSReceiver::GetProperty(i_isolate, observation_state, "objectInfoMap")
          .ToHandleChecked());
  i::Handle<i::JSWeakMap> notifierObjectInfoMap = i::Handle<i::JSWeakMap>::cast(
      i::JSReceiver::GetProperty(i_isolate, observation_state,
                                 "notifierObjectInfoMap")
          .ToHandleChecked());
  CHECK_EQ(1, NumberOfElements(callbackInfoMap));
  CHECK_EQ(1, NumberOfElements(objectInfoMap));
  CHECK_EQ(1, NumberOfElements(notifierObjectInfoMap));
  i_isolate->heap()->CollectAllGarbage();
  CHECK_EQ(0, NumberOfElements(callbackInfoMap));
  CHECK_EQ(0, NumberOfElements(objectInfoMap));
  CHECK_EQ(0, NumberOfElements(notifierObjectInfoMap));
}


static int TestObserveSecurity(Local<Context> observer_context,
                               Local<Context> object_context,
                               Local<Context> mutation_context) {
  Context::Scope observer_scope(observer_context);
  CompileRun("var records = null;"
             "var observer = function(r) { records = r };");
  Local<Value> observer = CompileRun("observer");
  {
    Context::Scope object_scope(object_context);
    object_context->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
              v8_str("observer"), observer)
        .FromJust();
    CompileRun("var obj = {};"
               "obj.length = 0;"
               "Object.observe(obj, observer,"
                   "['add', 'update', 'delete','reconfigure','splice']"
               ");");
    Local<Value> obj = CompileRun("obj");
    {
      Context::Scope mutation_scope(mutation_context);
      mutation_context->Global()
          ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
                obj)
          .FromJust();
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
  return ToInt32(CompileRun("records ? records.length : 0"));
}


TEST(ObserverSecurityAAA) {
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  CHECK_EQ(8, TestObserveSecurity(contextA, contextA, contextA));
}


TEST(ObserverSecurityA1A2A3) {
  i::FLAG_harmony_object_observe = true;
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
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextA, contextA, contextB));
}


TEST(ObserverSecurityA1A2B) {
  i::FLAG_harmony_object_observe = true;
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
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextA, contextB, contextA));
}


TEST(ObserverSecurityA1BA2) {
  i::FLAG_harmony_object_observe = true;
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
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);
  CHECK_EQ(0, TestObserveSecurity(contextB, contextA, contextA));
}


TEST(ObserverSecurityBA1A2) {
  i::FLAG_harmony_object_observe = true;
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
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<Context> contextA = Context::New(isolate);
  v8::Local<Context> contextB = Context::New(isolate);

  Context::Scope scopeA(contextA);
  CompileRun("var obj = {};"
             "var recordsA = null;"
             "var observerA = function(r) { recordsA = r };"
             "Object.observe(obj, observerA);");
  Local<Value> obj = CompileRun("obj");

  {
    Context::Scope scopeB(contextB);
    contextB->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              obj)
        .FromJust();
    CompileRun("var recordsB = null;"
               "var observerB = function(r) { recordsB = r };"
               "Object.observe(obj, observerB);");
  }

  CompileRun("var notifier = Object.getNotifier(obj);"
             "notifier.notify({ type: 'update' });");
  CHECK_EQ(1, ToInt32(CompileRun("recordsA ? recordsA.length : 0")));

  {
    Context::Scope scopeB(contextB);
    CHECK_EQ(0, ToInt32(CompileRun("recordsB ? recordsB.length : 0")));
  }
}


TEST(HiddenPropertiesLeakage) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun("var obj = {};"
             "var records = null;"
             "var observer = function(r) { records = r };"
             "Object.observe(obj, observer);");
  Local<Value> obj =
      context->Global()
          ->Get(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"))
          .ToLocalChecked();
  Local<Object>::Cast(obj)
      ->SetPrivate(v8::Isolate::GetCurrent()->GetCurrentContext(),
                   v8::Private::New(CcTest::isolate(), v8_str("foo")),
                   Null(CcTest::isolate()))
      .FromJust();
  CompileRun("");  // trigger delivery
  CHECK(CompileRun("records")->IsNull());
}


TEST(GetNotifierFromOtherContext) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  LocalContext context(CcTest::isolate());
  CompileRun("var obj = {};");
  Local<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              instance)
        .FromJust();
    CHECK(CompileRun("Object.getNotifier(obj)")->IsNull());
  }
}


TEST(GetNotifierFromOtherOrigin) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  Local<Value> foo = v8_str("foo");
  Local<Value> bar = v8_str("bar");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Local<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(bar);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              instance)
        .FromJust();
    CHECK(CompileRun("Object.getNotifier(obj)")->IsNull());
  }
}


TEST(GetNotifierFromSameOrigin) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  Local<Value> foo = v8_str("foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Local<Value> instance = CompileRun("obj");
  {
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              instance)
        .FromJust();
    CHECK(CompileRun("Object.getNotifier(obj)")->IsObject());
  }
}


static int GetGlobalObjectsCount() {
  int count = 0;
  i::HeapIterator it(CcTest::heap());
  for (i::HeapObject* object = it.next(); object != NULL; object = it.next())
    if (object->IsJSGlobalObject()) {
      i::JSGlobalObject* g = i::JSGlobalObject::cast(object);
      // Skip dummy global object.
      if (i::GlobalDictionary::cast(g->properties())->NumberOfElements() != 0) {
        count++;
      }
    }
  // Subtract one to compensate for the code stub context that is always present
  return count - 1;
}


static void CheckSurvivingGlobalObjectsCount(int expected) {
  // We need to collect all garbage twice to be sure that everything
  // has been collected.  This is because inline caches are cleared in
  // the first garbage collection but some of the maps have already
  // been marked at that point.  Therefore some of the maps are not
  // collected until the second garbage collection.
  CcTest::heap()->CollectAllGarbage();
  CcTest::heap()->CollectAllGarbage(i::Heap::kMakeHeapIterableMask);
  int count = GetGlobalObjectsCount();
#ifdef DEBUG
  if (count != expected) CcTest::heap()->TracePathToGlobal();
#endif
  CHECK_EQ(expected, count);
}


TEST(DontLeakContextOnObserve) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  Local<Value> foo = v8_str("foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Local<Value> object = CompileRun("obj");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              object)
        .FromJust();
    CompileRun("function observer() {};"
               "Object.observe(obj, observer, ['foo', 'bar', 'baz']);"
               "Object.unobserve(obj, observer);");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(0);
}


TEST(DontLeakContextOnGetNotifier) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  Local<Value> foo = v8_str("foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Local<Value> object = CompileRun("obj");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              object)
        .FromJust();
    CompileRun("Object.getNotifier(obj);");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(0);
}


TEST(DontLeakContextOnNotifierPerformChange) {
  i::FLAG_harmony_object_observe = true;
  HandleScope scope(CcTest::isolate());
  Local<Value> foo = v8_str("foo");
  LocalContext context(CcTest::isolate());
  context->SetSecurityToken(foo);
  CompileRun("var obj = {};");
  Local<Value> object = CompileRun("obj");
  Local<Value> notifier = CompileRun("Object.getNotifier(obj)");
  {
    HandleScope scope(CcTest::isolate());
    LocalContext context2(CcTest::isolate());
    context2->SetSecurityToken(foo);
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
              object)
        .FromJust();
    context2->Global()
        ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(),
              v8_str("notifier"), notifier)
        .FromJust();
    CompileRun("var obj2 = {};"
               "var notifier2 = Object.getNotifier(obj2);"
               "notifier2.performChange.call("
                   "notifier, 'foo', function(){})");
  }

  CcTest::isolate()->ContextDisposedNotification();
  CheckSurvivingGlobalObjectsCount(0);
}


static void ObserverCallback(const FunctionCallbackInfo<Value>& args) {
  *static_cast<int*>(Local<External>::Cast(args.Data())->Value()) =
      Local<Array>::Cast(args[0])->Length();
}


TEST(ObjectObserveCallsCppFunction) {
  i::FLAG_harmony_object_observe = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext context(isolate);
  int numRecordsSent = 0;
  Local<Function> observer =
      Function::New(CcTest::isolate()->GetCurrentContext(), ObserverCallback,
                    External::New(isolate, &numRecordsSent))
          .ToLocalChecked();
  context->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("observer"),
            observer)
      .FromJust();
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, observer);"
      "obj.foo = 1;"
      "obj.bar = 2;");
  CHECK_EQ(2, numRecordsSent);
}


TEST(ObjectObserveCallsFunctionTemplateInstance) {
  i::FLAG_harmony_object_observe = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope scope(isolate);
  LocalContext context(isolate);
  int numRecordsSent = 0;
  Local<FunctionTemplate> tmpl = FunctionTemplate::New(
      isolate, ObserverCallback, External::New(isolate, &numRecordsSent));
  Local<Function> function =
      tmpl->GetFunction(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  context->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("observer"),
            function)
      .FromJust();
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, observer);"
      "obj.foo = 1;"
      "obj.bar = 2;");
  CHECK_EQ(2, numRecordsSent);
}


static void AccessorGetter(Local<Name> property,
                           const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(Integer::New(info.GetIsolate(), 42));
}


static void AccessorSetter(Local<Name> property, Local<Value> value,
                           const PropertyCallbackInfo<void>& info) {
  info.GetReturnValue().SetUndefined();
}


TEST(APIAccessorsShouldNotNotify) {
  i::FLAG_harmony_object_observe = true;
  Isolate* isolate = CcTest::isolate();
  HandleScope handle_scope(isolate);
  LocalContext context(isolate);
  Local<Object> object = Object::New(isolate);
  object->SetAccessor(v8::Isolate::GetCurrent()->GetCurrentContext(),
                      v8_str("accessor"), &AccessorGetter, &AccessorSetter)
      .FromJust();
  context->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
            object)
      .FromJust();
  CompileRun(
      "var records = null;"
      "Object.observe(obj, function(r) { records = r });"
      "obj.accessor = 43;");
  CHECK(CompileRun("records")->IsNull());
  CompileRun("Object.defineProperty(obj, 'accessor', { value: 44 });");
  CHECK(CompileRun("records")->IsNull());
}


namespace {

int* global_use_counts = NULL;

void MockUseCounterCallback(v8::Isolate* isolate,
                            v8::Isolate::UseCounterFeature feature) {
  ++global_use_counts[feature];
}
}


TEST(UseCountObjectObserve) {
  i::FLAG_harmony_object_observe = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);
  CompileRun(
      "var obj = {};"
      "Object.observe(obj, function(){})");
  CHECK_EQ(1, use_counts[v8::Isolate::kObjectObserve]);
  CompileRun(
      "var obj2 = {};"
      "Object.observe(obj2, function(){})");
  // Only counts the first use of observe in a given context.
  CHECK_EQ(1, use_counts[v8::Isolate::kObjectObserve]);
  {
    LocalContext env2;
    CompileRun(
        "var obj = {};"
        "Object.observe(obj, function(){})");
  }
  // Counts different contexts separately.
  CHECK_EQ(2, use_counts[v8::Isolate::kObjectObserve]);
}


TEST(UseCountObjectGetNotifier) {
  i::FLAG_harmony_object_observe = true;
  i::Isolate* isolate = CcTest::i_isolate();
  i::HandleScope scope(isolate);
  LocalContext env;
  int use_counts[v8::Isolate::kUseCounterFeatureCount] = {};
  global_use_counts = use_counts;
  CcTest::isolate()->SetUseCounterCallback(MockUseCounterCallback);
  CompileRun("var obj = {}");
  CompileRun("Object.getNotifier(obj)");
  CHECK_EQ(1, use_counts[v8::Isolate::kObjectObserve]);
}

static bool NamedAccessCheckAlwaysAllow(Local<v8::Context> accessing_context,
                                        Local<v8::Object> accessed_object,
                                        Local<v8::Value> data) {
  return true;
}


TEST(DisallowObserveAccessCheckedObject) {
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);
  object_template->SetAccessCheckCallback(NamedAccessCheckAlwaysAllow);
  Local<Object> new_instance =
      object_template->NewInstance(
                         v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  env->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
            new_instance)
      .FromJust();
  v8::TryCatch try_catch(isolate);
  CompileRun("Object.observe(obj, function(){})");
  CHECK(try_catch.HasCaught());
}


TEST(DisallowGetNotifierAccessCheckedObject) {
  i::FLAG_harmony_object_observe = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  LocalContext env;
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);
  object_template->SetAccessCheckCallback(NamedAccessCheckAlwaysAllow);
  Local<Object> new_instance =
      object_template->NewInstance(
                         v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  env->Global()
      ->Set(v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str("obj"),
            new_instance)
      .FromJust();
  v8::TryCatch try_catch(isolate);
  CompileRun("Object.getNotifier(obj)");
  CHECK(try_catch.HasCaught());
}

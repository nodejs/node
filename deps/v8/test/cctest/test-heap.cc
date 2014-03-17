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

#include <stdlib.h>
#include <utility>

#include "v8.h"

#include "compilation-cache.h"
#include "execution.h"
#include "factory.h"
#include "macro-assembler.h"
#include "global-handles.h"
#include "stub-cache.h"
#include "cctest.h"

using namespace v8::internal;


// Go through all incremental marking steps in one swoop.
static void SimulateIncrementalMarking() {
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  if (collector->IsConcurrentSweepingInProgress()) {
    collector->WaitUntilSweepingCompleted();
  }
  CHECK(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    marking->Start();
  }
  CHECK(marking->IsMarking());
  while (!marking->IsComplete()) {
    marking->Step(MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  }
  CHECK(marking->IsComplete());
}


static void CheckMap(Map* map, int type, int instance_size) {
  CHECK(map->IsHeapObject());
#ifdef DEBUG
  CHECK(CcTest::heap()->Contains(map));
#endif
  CHECK_EQ(CcTest::heap()->meta_map(), map->map());
  CHECK_EQ(type, map->instance_type());
  CHECK_EQ(instance_size, map->instance_size());
}


TEST(HeapMaps) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CheckMap(heap->meta_map(), MAP_TYPE, Map::kSize);
  CheckMap(heap->heap_number_map(), HEAP_NUMBER_TYPE, HeapNumber::kSize);
  CheckMap(heap->fixed_array_map(), FIXED_ARRAY_TYPE, kVariableSizeSentinel);
  CheckMap(heap->string_map(), STRING_TYPE, kVariableSizeSentinel);
}


static void CheckOddball(Isolate* isolate, Object* obj, const char* string) {
  CHECK(obj->IsOddball());
  bool exc;
  Handle<Object> handle(obj, isolate);
  Object* print_string =
      *Execution::ToString(isolate, handle, &exc);
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckSmi(Isolate* isolate, int value, const char* string) {
  bool exc;
  Handle<Object> handle(Smi::FromInt(value), isolate);
  Object* print_string =
      *Execution::ToString(isolate, handle, &exc);
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckNumber(Isolate* isolate, double value, const char* string) {
  Object* obj = CcTest::heap()->NumberFromDouble(value)->ToObjectChecked();
  CHECK(obj->IsNumber());
  bool exc;
  Handle<Object> handle(obj, isolate);
  Object* print_string =
      *Execution::ToString(isolate, handle, &exc);
  CHECK(String::cast(print_string)->IsUtf8EqualTo(CStrVector(string)));
}


static void CheckFindCodeObject(Isolate* isolate) {
  // Test FindCodeObject
#define __ assm.

  Assembler assm(isolate, NULL, 0);

  __ nop();  // supported on all architectures

  CodeDesc desc;
  assm.GetCode(&desc);
  Heap* heap = isolate->heap();
  Object* code = heap->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());

  HeapObject* obj = HeapObject::cast(code);
  Address obj_addr = obj->address();

  for (int i = 0; i < obj->Size(); i += kPointerSize) {
    Object* found = isolate->FindCodeObject(obj_addr + i);
    CHECK_EQ(code, found);
  }

  Object* copy = heap->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(copy->IsCode());
  HeapObject* obj_copy = HeapObject::cast(copy);
  Object* not_right = isolate->FindCodeObject(obj_copy->address() +
                                              obj_copy->Size() / 2);
  CHECK(not_right != code);
}


TEST(HeapObjects) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope sc(isolate);
  Object* value = heap->NumberFromDouble(1.000123)->ToObjectChecked();
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(1.000123, value->Number());

  value = heap->NumberFromDouble(1.0)->ToObjectChecked();
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1.0, value->Number());

  value = heap->NumberFromInt32(1024)->ToObjectChecked();
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(1024.0, value->Number());

  value = heap->NumberFromInt32(Smi::kMinValue)->ToObjectChecked();
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMinValue, Smi::cast(value)->value());

  value = heap->NumberFromInt32(Smi::kMaxValue)->ToObjectChecked();
  CHECK(value->IsSmi());
  CHECK(value->IsNumber());
  CHECK_EQ(Smi::kMaxValue, Smi::cast(value)->value());

#ifndef V8_TARGET_ARCH_X64
  // TODO(lrn): We need a NumberFromIntptr function in order to test this.
  value = heap->NumberFromInt32(Smi::kMinValue - 1)->ToObjectChecked();
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(Smi::kMinValue - 1), value->Number());
#endif

  MaybeObject* maybe_value =
      heap->NumberFromUint32(static_cast<uint32_t>(Smi::kMaxValue) + 1);
  value = maybe_value->ToObjectChecked();
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(static_cast<uint32_t>(Smi::kMaxValue) + 1),
           value->Number());

  maybe_value = heap->NumberFromUint32(static_cast<uint32_t>(1) << 31);
  value = maybe_value->ToObjectChecked();
  CHECK(value->IsHeapNumber());
  CHECK(value->IsNumber());
  CHECK_EQ(static_cast<double>(static_cast<uint32_t>(1) << 31),
           value->Number());

  // nan oddball checks
  CHECK(heap->nan_value()->IsNumber());
  CHECK(std::isnan(heap->nan_value()->Number()));

  Handle<String> s = factory->NewStringFromAscii(CStrVector("fisk hest "));
  CHECK(s->IsString());
  CHECK_EQ(10, s->length());

  Handle<String> object_string = Handle<String>::cast(factory->Object_string());
  Handle<GlobalObject> global(CcTest::i_isolate()->context()->global_object());
  CHECK(JSReceiver::HasLocalProperty(global, object_string));

  // Check ToString for oddballs
  CheckOddball(isolate, heap->true_value(), "true");
  CheckOddball(isolate, heap->false_value(), "false");
  CheckOddball(isolate, heap->null_value(), "null");
  CheckOddball(isolate, heap->undefined_value(), "undefined");

  // Check ToString for Smis
  CheckSmi(isolate, 0, "0");
  CheckSmi(isolate, 42, "42");
  CheckSmi(isolate, -42, "-42");

  // Check ToString for Numbers
  CheckNumber(isolate, 1.1, "1.1");

  CheckFindCodeObject(isolate);
}


TEST(Tagging) {
  CcTest::InitializeVM();
  int request = 24;
  CHECK_EQ(request, static_cast<int>(OBJECT_POINTER_ALIGN(request)));
  CHECK(Smi::FromInt(42)->IsSmi());
  CHECK(Failure::RetryAfterGC(NEW_SPACE)->IsFailure());
  CHECK_EQ(NEW_SPACE,
           Failure::RetryAfterGC(NEW_SPACE)->allocation_space());
  CHECK_EQ(OLD_POINTER_SPACE,
           Failure::RetryAfterGC(OLD_POINTER_SPACE)->allocation_space());
  CHECK(Failure::Exception()->IsFailure());
  CHECK(Smi::FromInt(Smi::kMinValue)->IsSmi());
  CHECK(Smi::FromInt(Smi::kMaxValue)->IsSmi());
}


TEST(GarbageCollection) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  HandleScope sc(isolate);
  // Check GC.
  heap->CollectGarbage(NEW_SPACE);

  Handle<GlobalObject> global(CcTest::i_isolate()->context()->global_object());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<String> prop_namex = factory->InternalizeUtf8String("theSlotx");
  Handle<String> obj_name = factory->InternalizeUtf8String("theObject");
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  {
    HandleScope inner_scope(isolate);
    // Allocate a function and keep it in global object's property.
    Handle<JSFunction> function =
        factory->NewFunction(name, factory->undefined_value());
    Handle<Map> initial_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    function->set_initial_map(*initial_map);
    JSReceiver::SetProperty(global, name, function, NONE, kNonStrictMode);
    // Allocate an object.  Unrooted after leaving the scope.
    Handle<JSObject> obj = factory->NewJSObject(function);
    JSReceiver::SetProperty(obj, prop_name, twenty_three, NONE, kNonStrictMode);
    JSReceiver::SetProperty(obj, prop_namex, twenty_four, NONE, kNonStrictMode);

    CHECK_EQ(Smi::FromInt(23), obj->GetProperty(*prop_name));
    CHECK_EQ(Smi::FromInt(24), obj->GetProperty(*prop_namex));
  }

  heap->CollectGarbage(NEW_SPACE);

  // Function should be alive.
  CHECK(JSReceiver::HasLocalProperty(global, name));
  // Check function is retained.
  Object* func_value = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));

  {
    HandleScope inner_scope(isolate);
    // Allocate another object, make it reachable from global.
    Handle<JSObject> obj = factory->NewJSObject(function);
    JSReceiver::SetProperty(global, obj_name, obj, NONE, kNonStrictMode);
    JSReceiver::SetProperty(obj, prop_name, twenty_three, NONE, kNonStrictMode);
  }

  // After gc, it should survive.
  heap->CollectGarbage(NEW_SPACE);

  CHECK(JSReceiver::HasLocalProperty(global, obj_name));
  CHECK(CcTest::i_isolate()->context()->global_object()->
        GetProperty(*obj_name)->ToObjectChecked()->IsJSObject());
  Object* obj = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*obj_name)->ToObjectChecked();
  JSObject* js_obj = JSObject::cast(obj);
  CHECK_EQ(Smi::FromInt(23), js_obj->GetProperty(*prop_name));
}


static void VerifyStringAllocation(Isolate* isolate, const char* string) {
  HandleScope scope(isolate);
  Handle<String> s = isolate->factory()->NewStringFromUtf8(CStrVector(string));
  CHECK_EQ(StrLength(string), s->length());
  for (int index = 0; index < s->length(); index++) {
    CHECK_EQ(static_cast<uint16_t>(string[index]), s->Get(index));
  }
}


TEST(String) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());

  VerifyStringAllocation(isolate, "a");
  VerifyStringAllocation(isolate, "ab");
  VerifyStringAllocation(isolate, "abc");
  VerifyStringAllocation(isolate, "abcd");
  VerifyStringAllocation(isolate, "fiskerdrengen er paa havet");
}


TEST(LocalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope scope(CcTest::isolate());
  const char* name = "Kasper the spunky";
  Handle<String> string = factory->NewStringFromAscii(CStrVector(name));
  CHECK_EQ(StrLength(name), string->length());
}


TEST(GlobalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  Handle<Object> h1;
  Handle<Object> h2;
  Handle<Object> h3;
  Handle<Object> h4;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromAscii(CStrVector("fisk"));
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*i);
    h2 = global_handles->Create(*u);
    h3 = global_handles->Create(*i);
    h4 = global_handles->Create(*u);
  }

  // after gc, it should survive
  heap->CollectGarbage(NEW_SPACE);

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());
  CHECK((*h3)->IsString());
  CHECK((*h4)->IsHeapNumber());

  CHECK_EQ(*h3, *h1);
  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h3.location());

  CHECK_EQ(*h4, *h2);
  GlobalHandles::Destroy(h2.location());
  GlobalHandles::Destroy(h4.location());
}


static bool WeakPointerCleared = false;

static void TestWeakGlobalHandleCallback(
    const v8::WeakCallbackData<v8::Value, void>& data) {
  std::pair<v8::Persistent<v8::Value>*, int>* p =
      reinterpret_cast<std::pair<v8::Persistent<v8::Value>*, int>*>(
          data.GetParameter());
  if (p->second == 1234) WeakPointerCleared = true;
  p->first->Reset();
}


TEST(WeakGlobalHandlesScavenge) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;

  Handle<Object> h1;
  Handle<Object> h2;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromAscii(CStrVector("fisk"));
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*i);
    h2 = global_handles->Create(*u);
  }

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);

  // Scavenge treats weak pointers as normal roots.
  heap->PerformScavenge();

  CHECK((*h1)->IsString());
  CHECK((*h2)->IsHeapNumber());

  CHECK(!WeakPointerCleared);
  CHECK(!global_handles->IsNearDeath(h2.location()));
  CHECK(!global_handles->IsNearDeath(h1.location()));

  GlobalHandles::Destroy(h1.location());
  GlobalHandles::Destroy(h2.location());
}


TEST(WeakGlobalHandlesMark) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;

  Handle<Object> h1;
  Handle<Object> h2;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromAscii(CStrVector("fisk"));
    Handle<Object> u = factory->NewNumber(1.12344);

    h1 = global_handles->Create(*i);
    h2 = global_handles->Create(*u);
  }

  // Make sure the objects are promoted.
  heap->CollectGarbage(OLD_POINTER_SPACE);
  heap->CollectGarbage(NEW_SPACE);
  CHECK(!heap->InNewSpace(*h1) && !heap->InNewSpace(*h2));

  std::pair<Handle<Object>*, int> handle_and_id(&h2, 1234);
  GlobalHandles::MakeWeak(h2.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));
  CHECK(!GlobalHandles::IsNearDeath(h2.location()));

  // Incremental marking potentially marked handles before they turned weak.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  CHECK((*h1)->IsString());

  CHECK(WeakPointerCleared);
  CHECK(!GlobalHandles::IsNearDeath(h1.location()));

  GlobalHandles::Destroy(h1.location());
}


TEST(DeleteWeakGlobalHandle) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  GlobalHandles* global_handles = isolate->global_handles();

  WeakPointerCleared = false;

  Handle<Object> h;

  {
    HandleScope scope(isolate);

    Handle<Object> i = factory->NewStringFromAscii(CStrVector("fisk"));
    h = global_handles->Create(*i);
  }

  std::pair<Handle<Object>*, int> handle_and_id(&h, 1234);
  GlobalHandles::MakeWeak(h.location(),
                          reinterpret_cast<void*>(&handle_and_id),
                          &TestWeakGlobalHandleCallback);

  // Scanvenge does not recognize weak reference.
  heap->PerformScavenge();

  CHECK(!WeakPointerCleared);

  // Mark-compact treats weak reference properly.
  heap->CollectGarbage(OLD_POINTER_SPACE);

  CHECK(WeakPointerCleared);
}


static const char* not_so_random_string_table[] = {
  "abstract",
  "boolean",
  "break",
  "byte",
  "case",
  "catch",
  "char",
  "class",
  "const",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "double",
  "else",
  "enum",
  "export",
  "extends",
  "false",
  "final",
  "finally",
  "float",
  "for",
  "function",
  "goto",
  "if",
  "implements",
  "import",
  "in",
  "instanceof",
  "int",
  "interface",
  "long",
  "native",
  "new",
  "null",
  "package",
  "private",
  "protected",
  "public",
  "return",
  "short",
  "static",
  "super",
  "switch",
  "synchronized",
  "this",
  "throw",
  "throws",
  "transient",
  "true",
  "try",
  "typeof",
  "var",
  "void",
  "volatile",
  "while",
  "with",
  0
};


static void CheckInternalizedStrings(const char** strings) {
  for (const char* string = *strings; *strings != 0; string = *strings++) {
    Object* a;
    MaybeObject* maybe_a = CcTest::heap()->InternalizeUtf8String(string);
    // InternalizeUtf8String may return a failure if a GC is needed.
    if (!maybe_a->ToObject(&a)) continue;
    CHECK(a->IsInternalizedString());
    Object* b;
    MaybeObject* maybe_b = CcTest::heap()->InternalizeUtf8String(string);
    if (!maybe_b->ToObject(&b)) continue;
    CHECK_EQ(b, a);
    CHECK(String::cast(b)->IsUtf8EqualTo(CStrVector(string)));
  }
}


TEST(StringTable) {
  CcTest::InitializeVM();

  CheckInternalizedStrings(not_so_random_string_table);
  CheckInternalizedStrings(not_so_random_string_table);
}


TEST(FunctionAllocation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<JSFunction> function =
      factory->NewFunction(name, factory->undefined_value());
  Handle<Map> initial_map =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  function->set_initial_map(*initial_map);

  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  Handle<Smi> twenty_four(Smi::FromInt(24), isolate);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);
  JSReceiver::SetProperty(obj, prop_name, twenty_three, NONE, kNonStrictMode);
  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(*prop_name));
  // Check that we can add properties to function objects.
  JSReceiver::SetProperty(function, prop_name, twenty_four, NONE,
                          kNonStrictMode);
  CHECK_EQ(Smi::FromInt(24), function->GetProperty(*prop_name));
}


TEST(ObjectProperties) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  String* object_string = String::cast(CcTest::heap()->Object_string());
  Object* raw_object = CcTest::i_isolate()->context()->global_object()->
      GetProperty(object_string)->ToObjectChecked();
  JSFunction* object_function = JSFunction::cast(raw_object);
  Handle<JSFunction> constructor(object_function);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  // check for empty
  CHECK(!JSReceiver::HasLocalProperty(obj, first));

  // add first
  JSReceiver::SetProperty(obj, first, one, NONE, kNonStrictMode);
  CHECK(JSReceiver::HasLocalProperty(obj, first));

  // delete first
  JSReceiver::DeleteProperty(obj, first, JSReceiver::NORMAL_DELETION);
  CHECK(!JSReceiver::HasLocalProperty(obj, first));

  // add first and then second
  JSReceiver::SetProperty(obj, first, one, NONE, kNonStrictMode);
  JSReceiver::SetProperty(obj, second, two, NONE, kNonStrictMode);
  CHECK(JSReceiver::HasLocalProperty(obj, first));
  CHECK(JSReceiver::HasLocalProperty(obj, second));

  // delete first and then second
  JSReceiver::DeleteProperty(obj, first, JSReceiver::NORMAL_DELETION);
  CHECK(JSReceiver::HasLocalProperty(obj, second));
  JSReceiver::DeleteProperty(obj, second, JSReceiver::NORMAL_DELETION);
  CHECK(!JSReceiver::HasLocalProperty(obj, first));
  CHECK(!JSReceiver::HasLocalProperty(obj, second));

  // add first and then second
  JSReceiver::SetProperty(obj, first, one, NONE, kNonStrictMode);
  JSReceiver::SetProperty(obj, second, two, NONE, kNonStrictMode);
  CHECK(JSReceiver::HasLocalProperty(obj, first));
  CHECK(JSReceiver::HasLocalProperty(obj, second));

  // delete second and then first
  JSReceiver::DeleteProperty(obj, second, JSReceiver::NORMAL_DELETION);
  CHECK(JSReceiver::HasLocalProperty(obj, first));
  JSReceiver::DeleteProperty(obj, first, JSReceiver::NORMAL_DELETION);
  CHECK(!JSReceiver::HasLocalProperty(obj, first));
  CHECK(!JSReceiver::HasLocalProperty(obj, second));

  // check string and internalized string match
  const char* string1 = "fisk";
  Handle<String> s1 = factory->NewStringFromAscii(CStrVector(string1));
  JSReceiver::SetProperty(obj, s1, one, NONE, kNonStrictMode);
  Handle<String> s1_string = factory->InternalizeUtf8String(string1);
  CHECK(JSReceiver::HasLocalProperty(obj, s1_string));

  // check internalized string and string match
  const char* string2 = "fugl";
  Handle<String> s2_string = factory->InternalizeUtf8String(string2);
  JSReceiver::SetProperty(obj, s2_string, one, NONE, kNonStrictMode);
  Handle<String> s2 = factory->NewStringFromAscii(CStrVector(string2));
  CHECK(JSReceiver::HasLocalProperty(obj, s2));
}


TEST(JSObjectMaps) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("theFunction");
  Handle<JSFunction> function =
      factory->NewFunction(name, factory->undefined_value());
  Handle<Map> initial_map =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  function->set_initial_map(*initial_map);

  Handle<String> prop_name = factory->InternalizeUtf8String("theSlot");
  Handle<JSObject> obj = factory->NewJSObject(function);

  // Set a propery
  Handle<Smi> twenty_three(Smi::FromInt(23), isolate);
  JSReceiver::SetProperty(obj, prop_name, twenty_three, NONE, kNonStrictMode);
  CHECK_EQ(Smi::FromInt(23), obj->GetProperty(*prop_name));

  // Check the map has changed
  CHECK(*initial_map != obj->map());
}


TEST(JSArray) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  Handle<String> name = factory->InternalizeUtf8String("Array");
  Object* raw_object = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*name)->ToObjectChecked();
  Handle<JSFunction> function = Handle<JSFunction>(
      JSFunction::cast(raw_object));

  // Allocate the object.
  Handle<JSObject> object = factory->NewJSObject(function);
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  // We just initialized the VM, no heap allocation failure yet.
  array->Initialize(0)->ToObjectChecked();

  // Set array length to 0.
  array->SetElementsLength(Smi::FromInt(0))->ToObjectChecked();
  CHECK_EQ(Smi::FromInt(0), array->length());
  // Must be in fast mode.
  CHECK(array->HasFastSmiOrObjectElements());

  // array[length] = name.
  JSReceiver::SetElement(array, 0, name, NONE, kNonStrictMode);
  CHECK_EQ(Smi::FromInt(1), array->length());
  CHECK_EQ(array->GetElement(isolate, 0), *name);

  // Set array length with larger than smi value.
  Handle<Object> length =
      factory->NewNumberFromUint(static_cast<uint32_t>(Smi::kMaxValue) + 1);
  array->SetElementsLength(*length)->ToObjectChecked();

  uint32_t int_length = 0;
  CHECK(length->ToArrayIndex(&int_length));
  CHECK_EQ(*length, array->length());
  CHECK(array->HasDictionaryElements());  // Must be in slow mode.

  // array[length] = name.
  JSReceiver::SetElement(array, int_length, name, NONE, kNonStrictMode);
  uint32_t new_int_length = 0;
  CHECK(array->length()->ToArrayIndex(&new_int_length));
  CHECK_EQ(static_cast<double>(int_length), new_int_length - 1);
  CHECK_EQ(array->GetElement(isolate, int_length), *name);
  CHECK_EQ(array->GetElement(isolate, 0), *name);
}


TEST(JSObjectCopy) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(CcTest::isolate());
  String* object_string = String::cast(CcTest::heap()->Object_string());
  Object* raw_object = CcTest::i_isolate()->context()->global_object()->
      GetProperty(object_string)->ToObjectChecked();
  JSFunction* object_function = JSFunction::cast(raw_object);
  Handle<JSFunction> constructor(object_function);
  Handle<JSObject> obj = factory->NewJSObject(constructor);
  Handle<String> first = factory->InternalizeUtf8String("first");
  Handle<String> second = factory->InternalizeUtf8String("second");

  Handle<Smi> one(Smi::FromInt(1), isolate);
  Handle<Smi> two(Smi::FromInt(2), isolate);

  JSReceiver::SetProperty(obj, first, one, NONE, kNonStrictMode);
  JSReceiver::SetProperty(obj, second, two, NONE, kNonStrictMode);

  JSReceiver::SetElement(obj, 0, first, NONE, kNonStrictMode);
  JSReceiver::SetElement(obj, 1, second, NONE, kNonStrictMode);

  // Make the clone.
  Handle<JSObject> clone = JSObject::Copy(obj);
  CHECK(!clone.is_identical_to(obj));

  CHECK_EQ(obj->GetElement(isolate, 0), clone->GetElement(isolate, 0));
  CHECK_EQ(obj->GetElement(isolate, 1), clone->GetElement(isolate, 1));

  CHECK_EQ(obj->GetProperty(*first), clone->GetProperty(*first));
  CHECK_EQ(obj->GetProperty(*second), clone->GetProperty(*second));

  // Flip the values.
  JSReceiver::SetProperty(clone, first, two, NONE, kNonStrictMode);
  JSReceiver::SetProperty(clone, second, one, NONE, kNonStrictMode);

  JSReceiver::SetElement(clone, 0, second, NONE, kNonStrictMode);
  JSReceiver::SetElement(clone, 1, first, NONE, kNonStrictMode);

  CHECK_EQ(obj->GetElement(isolate, 1), clone->GetElement(isolate, 0));
  CHECK_EQ(obj->GetElement(isolate, 0), clone->GetElement(isolate, 1));

  CHECK_EQ(obj->GetProperty(*second), clone->GetProperty(*first));
  CHECK_EQ(obj->GetProperty(*first), clone->GetProperty(*second));
}


TEST(StringAllocation) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();

  const unsigned char chars[] = { 0xe5, 0xa4, 0xa7 };
  for (int length = 0; length < 100; length++) {
    v8::HandleScope scope(CcTest::isolate());
    char* non_ascii = NewArray<char>(3 * length + 1);
    char* ascii = NewArray<char>(length + 1);
    non_ascii[3 * length] = 0;
    ascii[length] = 0;
    for (int i = 0; i < length; i++) {
      ascii[i] = 'a';
      non_ascii[3 * i] = chars[0];
      non_ascii[3 * i + 1] = chars[1];
      non_ascii[3 * i + 2] = chars[2];
    }
    Handle<String> non_ascii_sym =
        factory->InternalizeUtf8String(
            Vector<const char>(non_ascii, 3 * length));
    CHECK_EQ(length, non_ascii_sym->length());
    Handle<String> ascii_sym =
        factory->InternalizeOneByteString(OneByteVector(ascii, length));
    CHECK_EQ(length, ascii_sym->length());
    Handle<String> non_ascii_str =
        factory->NewStringFromUtf8(Vector<const char>(non_ascii, 3 * length));
    non_ascii_str->Hash();
    CHECK_EQ(length, non_ascii_str->length());
    Handle<String> ascii_str =
        factory->NewStringFromUtf8(Vector<const char>(ascii, length));
    ascii_str->Hash();
    CHECK_EQ(length, ascii_str->length());
    DeleteArray(non_ascii);
    DeleteArray(ascii);
  }
}


static int ObjectsFoundInHeap(Heap* heap, Handle<Object> objs[], int size) {
  // Count the number of objects found in the heap.
  int found_count = 0;
  heap->EnsureHeapIsIterable();
  HeapIterator iterator(heap);
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    for (int i = 0; i < size; i++) {
      if (*objs[i] == obj) {
        found_count++;
      }
    }
  }
  return found_count;
}


TEST(Iteration) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Array of objects to scan haep for.
  const int objs_count = 6;
  Handle<Object> objs[objs_count];
  int next_objs_index = 0;

  // Allocate a JS array to OLD_POINTER_SPACE and NEW_SPACE
  objs[next_objs_index++] = factory->NewJSArray(10);
  objs[next_objs_index++] = factory->NewJSArray(10,
                                                FAST_HOLEY_ELEMENTS,
                                                TENURED);

  // Allocate a small string to OLD_DATA_SPACE and NEW_SPACE
  objs[next_objs_index++] =
      factory->NewStringFromAscii(CStrVector("abcdefghij"));
  objs[next_objs_index++] =
      factory->NewStringFromAscii(CStrVector("abcdefghij"), TENURED);

  // Allocate a large string (for large object space).
  int large_size = Page::kMaxRegularHeapObjectSize + 1;
  char* str = new char[large_size];
  for (int i = 0; i < large_size - 1; ++i) str[i] = 'a';
  str[large_size - 1] = '\0';
  objs[next_objs_index++] =
      factory->NewStringFromAscii(CStrVector(str), TENURED);
  delete[] str;

  // Add a Map object to look for.
  objs[next_objs_index++] = Handle<Map>(HeapObject::cast(*objs[0])->map());

  CHECK_EQ(objs_count, next_objs_index);
  CHECK_EQ(objs_count, ObjectsFoundInHeap(CcTest::heap(), objs, objs_count));
}


TEST(EmptyHandleEscapeFrom) {
  CcTest::InitializeVM();

  v8::HandleScope scope(CcTest::isolate());
  Handle<JSObject> runaway;

  {
      v8::EscapableHandleScope nested(CcTest::isolate());
      Handle<JSObject> empty;
      runaway = empty.EscapeFrom(&nested);
  }

  CHECK(runaway.is_null());
}


static int LenFromSize(int size) {
  return (size - FixedArray::kHeaderSize) / kPointerSize;
}


TEST(Regression39128) {
  // Test case for crbug.com/39128.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  // Increase the chance of 'bump-the-pointer' allocation in old space.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  v8::HandleScope scope(CcTest::isolate());

  // The plan: create JSObject which references objects in new space.
  // Then clone this object (forcing it to go into old space) and check
  // that region dirty marks are updated correctly.

  // Step 1: prepare a map for the object.  We add 1 inobject property to it.
  Handle<JSFunction> object_ctor(
      CcTest::i_isolate()->native_context()->object_function());
  CHECK(object_ctor->has_initial_map());
  Handle<Map> object_map(object_ctor->initial_map());
  // Create a map with single inobject property.
  Handle<Map> my_map = factory->CopyMap(object_map, 1);
  int n_properties = my_map->inobject_properties();
  CHECK_GT(n_properties, 0);

  int object_size = my_map->instance_size();

  // Step 2: allocate a lot of objects so to almost fill new space: we need
  // just enough room to allocate JSObject and thus fill the newspace.

  int allocation_amount = Min(FixedArray::kMaxSize,
                              Page::kMaxRegularHeapObjectSize + kPointerSize);
  int allocation_len = LenFromSize(allocation_amount);
  NewSpace* new_space = heap->new_space();
  Address* top_addr = new_space->allocation_top_address();
  Address* limit_addr = new_space->allocation_limit_address();
  while ((*limit_addr - *top_addr) > allocation_amount) {
    CHECK(!heap->always_allocate());
    Object* array = heap->AllocateFixedArray(allocation_len)->ToObjectChecked();
    CHECK(!array->IsFailure());
    CHECK(new_space->Contains(array));
  }

  // Step 3: now allocate fixed array and JSObject to fill the whole new space.
  int to_fill = static_cast<int>(*limit_addr - *top_addr - object_size);
  int fixed_array_len = LenFromSize(to_fill);
  CHECK(fixed_array_len < FixedArray::kMaxLength);

  CHECK(!heap->always_allocate());
  Object* array = heap->AllocateFixedArray(fixed_array_len)->ToObjectChecked();
  CHECK(!array->IsFailure());
  CHECK(new_space->Contains(array));

  Object* object = heap->AllocateJSObjectFromMap(*my_map)->ToObjectChecked();
  CHECK(new_space->Contains(object));
  JSObject* jsobject = JSObject::cast(object);
  CHECK_EQ(0, FixedArray::cast(jsobject->elements())->length());
  CHECK_EQ(0, jsobject->properties()->length());
  // Create a reference to object in new space in jsobject.
  jsobject->FastPropertyAtPut(-1, array);

  CHECK_EQ(0, static_cast<int>(*limit_addr - *top_addr));

  // Step 4: clone jsobject, but force always allocate first to create a clone
  // in old pointer space.
  Address old_pointer_space_top = heap->old_pointer_space()->top();
  AlwaysAllocateScope aa_scope;
  Object* clone_obj = heap->CopyJSObject(jsobject)->ToObjectChecked();
  JSObject* clone = JSObject::cast(clone_obj);
  if (clone->address() != old_pointer_space_top) {
    // Alas, got allocated from free list, we cannot do checks.
    return;
  }
  CHECK(heap->old_pointer_space()->Contains(clone->address()));
}


TEST(TestCodeFlushing) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Object* func_value = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*foo_name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));
  CHECK(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(function->shared()->is_compiled());

  // Simulate several GCs that use full marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  }

  // foo should no longer be in the compilation cache
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());
  // Call foo to get it recompiled.
  CompileRun("foo()");
  CHECK(function->shared()->is_compiled());
  CHECK(function->is_compiled());
}


TEST(TestCodeFlushingPreAged) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = true;
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // Compile foo, but don't run it.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Object* func_value = Isolate::Current()->context()->global_object()->
      GetProperty(*foo_name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));
  CHECK(function->shared()->is_compiled());

  // The code has been run so will survive at least one GC.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(function->shared()->is_compiled());

  // The code was only run once, so it should be pre-aged and collected on the
  // next GC.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());

  // Execute the function again twice, and ensure it is reset to the young age.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo();"
               "foo();");
  }

  // The code will survive at least two GC now that it is young again.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(function->shared()->is_compiled());

  // Simulate several GCs that use full marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  }

  // foo should no longer be in the compilation cache
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());
  // Call foo to get it recompiled.
  CompileRun("foo()");
  CHECK(function->shared()->is_compiled());
  CHECK(function->is_compiled());
}


TEST(TestCodeFlushingIncremental) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code || !FLAG_flush_code_incrementally) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Object* func_value = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*foo_name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));
  CHECK(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(function->shared()->is_compiled());

  // Simulate several GCs that use incremental marking.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    SimulateIncrementalMarking();
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  }
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());

  // This compile will compile the function again.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo();");
  }

  // Simulate several GCs that use incremental marking but make sure
  // the loop breaks once the function is enqueued as a candidate.
  for (int i = 0; i < kAgingThreshold; i++) {
    SimulateIncrementalMarking();
    if (!function->next_function_link()->IsUndefined()) break;
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  }

  // Force optimization while incremental marking is active and while
  // the function is enqueued as a candidate.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC to make sure the candidate queue is sane.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(function->shared()->is_compiled() || !function->IsOptimized());
  CHECK(function->is_compiled() || !function->IsOptimized());
}


TEST(TestCodeFlushingIncrementalScavenge) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code || !FLAG_flush_code_incrementally) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "var foo = function() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo();"
                       "var bar = function() {"
                       "  var x = 23;"
                       "};"
                       "bar();";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");
  Handle<String> bar_name = factory->InternalizeUtf8String("bar");

  // Perfrom one initial GC to enable code flushing.
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check functions are compiled.
  Object* func_value = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*foo_name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));
  CHECK(function->shared()->is_compiled());
  Object* func_value2 = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*bar_name)->ToObjectChecked();
  CHECK(func_value2->IsJSFunction());
  Handle<JSFunction> function2(JSFunction::cast(func_value2));
  CHECK(function2->shared()->is_compiled());

  // Clear references to functions so that one of them can die.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("foo = 0; bar = 0;");
  }

  // Bump the code age so that flushing is triggered while the function
  // object is still located in new-space.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    function->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    function2->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
  }

  // Simulate incremental marking so that the functions are enqueued as
  // code flushing candidates. Then kill one of the functions. Finally
  // perform a scavenge while incremental marking is still running.
  SimulateIncrementalMarking();
  *function2.location() = NULL;
  CcTest::heap()->CollectGarbage(NEW_SPACE, "test scavenge while marking");

  // Simulate one final GC to make sure the candidate queue is sane.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(!function->shared()->is_compiled() || function->IsOptimized());
  CHECK(!function->is_compiled() || function->IsOptimized());
}


TEST(TestCodeFlushingIncrementalAbort) {
  // If we do not flush code this test is invalid.
  if (!FLAG_flush_code || !FLAG_flush_code_incrementally) return;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_optimize_for_size = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "function foo() {"
                       "  var x = 42;"
                       "  var y = 42;"
                       "  var z = x + y;"
                       "};"
                       "foo()";
  Handle<String> foo_name = factory->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun(source);
  }

  // Check function is compiled.
  Object* func_value = CcTest::i_isolate()->context()->global_object()->
      GetProperty(*foo_name)->ToObjectChecked();
  CHECK(func_value->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(func_value));
  CHECK(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK(function->shared()->is_compiled());

  // Bump the code age so that flushing is triggered.
  const int kAgingThreshold = 6;
  for (int i = 0; i < kAgingThreshold; i++) {
    function->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
  }

  // Simulate incremental marking so that the function is enqueued as
  // code flushing candidate.
  SimulateIncrementalMarking();

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Enable the debugger and add a breakpoint while incremental marking
  // is running so that incremental marking aborts and code flushing is
  // disabled.
  int position = 0;
  Handle<Object> breakpoint_object(Smi::FromInt(0), isolate);
  isolate->debug()->SetBreakPoint(function, breakpoint_object, &position);
  isolate->debug()->ClearAllBreakPoints();
#endif  // ENABLE_DEBUGGER_SUPPORT

  // Force optimization now that code flushing is disabled.
  { v8::HandleScope scope(CcTest::isolate());
    CompileRun("%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC to make sure the candidate queue is sane.
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(function->shared()->is_compiled() || !function->IsOptimized());
  CHECK(function->is_compiled() || !function->IsOptimized());
}


// Count the number of native contexts in the weak list of native contexts.
int CountNativeContexts() {
  int count = 0;
  Object* object = CcTest::heap()->native_contexts_list();
  while (!object->IsUndefined()) {
    count++;
    object = Context::cast(object)->get(Context::NEXT_CONTEXT_LINK);
  }
  return count;
}


// Count the number of user functions in the weak list of optimized
// functions attached to a native context.
static int CountOptimizedUserFunctions(v8::Handle<v8::Context> context) {
  int count = 0;
  Handle<Context> icontext = v8::Utils::OpenHandle(*context);
  Object* object = icontext->get(Context::OPTIMIZED_FUNCTIONS_LIST);
  while (object->IsJSFunction() && !JSFunction::cast(object)->IsBuiltin()) {
    count++;
    object = JSFunction::cast(object)->next_function_link();
  }
  return count;
}


TEST(TestInternalWeakLists) {
  v8::V8::Initialize();

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction) return;

  static const int kNumTestContexts = 10;

  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);
  v8::Handle<v8::Context> ctx[kNumTestContexts];

  CHECK_EQ(0, CountNativeContexts());

  // Create a number of global contests which gets linked together.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i] = v8::Context::New(CcTest::isolate());

    // Collect garbage that might have been created by one of the
    // installed extensions.
    isolate->compilation_cache()->Clear();
    heap->CollectAllGarbage(Heap::kNoGCFlags);

    bool opt = (FLAG_always_opt && isolate->use_crankshaft());

    CHECK_EQ(i + 1, CountNativeContexts());

    ctx[i]->Enter();

    // Create a handle scope so no function objects get stuch in the outer
    // handle scope
    HandleScope scope(isolate);
    const char* source = "function f1() { };"
                         "function f2() { };"
                         "function f3() { };"
                         "function f4() { };"
                         "function f5() { };";
    CompileRun(source);
    CHECK_EQ(0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f1()");
    CHECK_EQ(opt ? 1 : 0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f2()");
    CHECK_EQ(opt ? 2 : 0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f3()");
    CHECK_EQ(opt ? 3 : 0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f4()");
    CHECK_EQ(opt ? 4 : 0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f5()");
    CHECK_EQ(opt ? 5 : 0, CountOptimizedUserFunctions(ctx[i]));

    // Remove function f1, and
    CompileRun("f1=null");

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->PerformScavenge();
      CHECK_EQ(opt ? 5 : 0, CountOptimizedUserFunctions(ctx[i]));
    }

    // Mark compact handles the weak references.
    isolate->compilation_cache()->Clear();
    heap->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(opt ? 4 : 0, CountOptimizedUserFunctions(ctx[i]));

    // Get rid of f3 and f5 in the same way.
    CompileRun("f3=null");
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->PerformScavenge();
      CHECK_EQ(opt ? 4 : 0, CountOptimizedUserFunctions(ctx[i]));
    }
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(opt ? 3 : 0, CountOptimizedUserFunctions(ctx[i]));
    CompileRun("f5=null");
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->PerformScavenge();
      CHECK_EQ(opt ? 3 : 0, CountOptimizedUserFunctions(ctx[i]));
    }
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(opt ? 2 : 0, CountOptimizedUserFunctions(ctx[i]));

    ctx[i]->Exit();
  }

  // Force compilation cache cleanup.
  CcTest::heap()->NotifyContextDisposed();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  // Dispose the native contexts one by one.
  for (int i = 0; i < kNumTestContexts; i++) {
    // TODO(dcarney): is there a better way to do this?
    i::Object** unsafe = reinterpret_cast<i::Object**>(*ctx[i]);
    *unsafe = CcTest::heap()->undefined_value();
    ctx[i].Clear();

    // Scavenge treats these references as strong.
    for (int j = 0; j < 10; j++) {
      CcTest::heap()->PerformScavenge();
      CHECK_EQ(kNumTestContexts - i, CountNativeContexts());
    }

    // Mark compact handles the weak references.
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(kNumTestContexts - i - 1, CountNativeContexts());
  }

  CHECK_EQ(0, CountNativeContexts());
}


// Count the number of native contexts in the weak list of native contexts
// causing a GC after the specified number of elements.
static int CountNativeContextsWithGC(Isolate* isolate, int n) {
  Heap* heap = isolate->heap();
  int count = 0;
  Handle<Object> object(heap->native_contexts_list(), isolate);
  while (!object->IsUndefined()) {
    count++;
    if (count == n) heap->CollectAllGarbage(Heap::kNoGCFlags);
    object =
        Handle<Object>(Context::cast(*object)->get(Context::NEXT_CONTEXT_LINK),
                       isolate);
  }
  return count;
}


// Count the number of user functions in the weak list of optimized
// functions attached to a native context causing a GC after the
// specified number of elements.
static int CountOptimizedUserFunctionsWithGC(v8::Handle<v8::Context> context,
                                             int n) {
  int count = 0;
  Handle<Context> icontext = v8::Utils::OpenHandle(*context);
  Isolate* isolate = icontext->GetIsolate();
  Handle<Object> object(icontext->get(Context::OPTIMIZED_FUNCTIONS_LIST),
                        isolate);
  while (object->IsJSFunction() &&
         !Handle<JSFunction>::cast(object)->IsBuiltin()) {
    count++;
    if (count == n) isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    object = Handle<Object>(
        Object::cast(JSFunction::cast(*object)->next_function_link()),
        isolate);
  }
  return count;
}


TEST(TestInternalWeakListsTraverseWithGC) {
  v8::V8::Initialize();
  Isolate* isolate = CcTest::i_isolate();

  static const int kNumTestContexts = 10;

  HandleScope scope(isolate);
  v8::Handle<v8::Context> ctx[kNumTestContexts];

  CHECK_EQ(0, CountNativeContexts());

  // Create an number of contexts and check the length of the weak list both
  // with and without GCs while iterating the list.
  for (int i = 0; i < kNumTestContexts; i++) {
    ctx[i] = v8::Context::New(CcTest::isolate());
    CHECK_EQ(i + 1, CountNativeContexts());
    CHECK_EQ(i + 1, CountNativeContextsWithGC(isolate, i / 2 + 1));
  }

  bool opt = (FLAG_always_opt && isolate->use_crankshaft());

  // Compile a number of functions the length of the weak list of optimized
  // functions both with and without GCs while iterating the list.
  ctx[0]->Enter();
  const char* source = "function f1() { };"
                       "function f2() { };"
                       "function f3() { };"
                       "function f4() { };"
                       "function f5() { };";
  CompileRun(source);
  CHECK_EQ(0, CountOptimizedUserFunctions(ctx[0]));
  CompileRun("f1()");
  CHECK_EQ(opt ? 1 : 0, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(opt ? 1 : 0, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  CompileRun("f2()");
  CHECK_EQ(opt ? 2 : 0, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(opt ? 2 : 0, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  CompileRun("f3()");
  CHECK_EQ(opt ? 3 : 0, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(opt ? 3 : 0, CountOptimizedUserFunctionsWithGC(ctx[0], 1));
  CompileRun("f4()");
  CHECK_EQ(opt ? 4 : 0, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(opt ? 4 : 0, CountOptimizedUserFunctionsWithGC(ctx[0], 2));
  CompileRun("f5()");
  CHECK_EQ(opt ? 5 : 0, CountOptimizedUserFunctions(ctx[0]));
  CHECK_EQ(opt ? 5 : 0, CountOptimizedUserFunctionsWithGC(ctx[0], 4));

  ctx[0]->Exit();
}


TEST(TestSizeOfObjects) {
  v8::V8::Initialize();

  // Get initial heap size after several full GCs, which will stabilize
  // the heap size and return with sweeping finished completely.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(CcTest::heap()->old_pointer_space()->IsLazySweepingComplete());
  int initial_size = static_cast<int>(CcTest::heap()->SizeOfObjects());

  {
    // Allocate objects on several different old-space pages so that
    // lazy sweeping kicks in for subsequent GC runs.
    AlwaysAllocateScope always_allocate;
    int filler_size = static_cast<int>(FixedArray::SizeFor(8192));
    for (int i = 1; i <= 100; i++) {
      CcTest::heap()->AllocateFixedArray(8192, TENURED)->ToObjectChecked();
      CHECK_EQ(initial_size + i * filler_size,
               static_cast<int>(CcTest::heap()->SizeOfObjects()));
    }
  }

  // The heap size should go back to initial size after a full GC, even
  // though sweeping didn't finish yet.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  // Normally sweeping would not be complete here, but no guarantees.

  CHECK_EQ(initial_size, static_cast<int>(CcTest::heap()->SizeOfObjects()));

  // Advancing the sweeper step-wise should not change the heap size.
  while (!CcTest::heap()->old_pointer_space()->IsLazySweepingComplete()) {
    CcTest::heap()->old_pointer_space()->AdvanceSweeper(KB);
    CHECK_EQ(initial_size, static_cast<int>(CcTest::heap()->SizeOfObjects()));
  }
}


TEST(TestSizeOfObjectsVsHeapIteratorPrecision) {
  CcTest::InitializeVM();
  CcTest::heap()->EnsureHeapIsIterable();
  intptr_t size_of_objects_1 = CcTest::heap()->SizeOfObjects();
  HeapIterator iterator(CcTest::heap());
  intptr_t size_of_objects_2 = 0;
  for (HeapObject* obj = iterator.next();
       obj != NULL;
       obj = iterator.next()) {
    if (!obj->IsFreeSpace()) {
      size_of_objects_2 += obj->Size();
    }
  }
  // Delta must be within 5% of the larger result.
  // TODO(gc): Tighten this up by distinguishing between byte
  // arrays that are real and those that merely mark free space
  // on the heap.
  if (size_of_objects_1 > size_of_objects_2) {
    intptr_t delta = size_of_objects_1 - size_of_objects_2;
    PrintF("Heap::SizeOfObjects: %" V8_PTR_PREFIX "d, "
           "Iterator: %" V8_PTR_PREFIX "d, "
           "delta: %" V8_PTR_PREFIX "d\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_1 / 20, delta);
  } else {
    intptr_t delta = size_of_objects_2 - size_of_objects_1;
    PrintF("Heap::SizeOfObjects: %" V8_PTR_PREFIX "d, "
           "Iterator: %" V8_PTR_PREFIX "d, "
           "delta: %" V8_PTR_PREFIX "d\n",
           size_of_objects_1, size_of_objects_2, delta);
    CHECK_GT(size_of_objects_2 / 20, delta);
  }
}


static void FillUpNewSpace(NewSpace* new_space) {
  // Fill up new space to the point that it is completely full. Make sure
  // that the scavenger does not undo the filling.
  Heap* heap = new_space->heap();
  Isolate* isolate = heap->isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);
  AlwaysAllocateScope always_allocate;
  intptr_t available = new_space->EffectiveCapacity() - new_space->Size();
  intptr_t number_of_fillers = (available / FixedArray::SizeFor(32)) - 1;
  for (intptr_t i = 0; i < number_of_fillers; i++) {
    CHECK(heap->InNewSpace(*factory->NewFixedArray(32, NOT_TENURED)));
  }
}


TEST(GrowAndShrinkNewSpace) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  NewSpace* new_space = heap->new_space();

  if (heap->ReservedSemiSpaceSize() == heap->InitialSemiSpaceSize() ||
      heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    // The max size cannot exceed the reserved size, since semispaces must be
    // always within the reserved space.  We can't test new space growing and
    // shrinking if the reserved size is the same as the minimum (initial) size.
    return;
  }

  // Explicitly growing should double the space capacity.
  intptr_t old_capacity, new_capacity;
  old_capacity = new_space->Capacity();
  new_space->Grow();
  new_capacity = new_space->Capacity();
  CHECK(2 * old_capacity == new_capacity);

  old_capacity = new_space->Capacity();
  FillUpNewSpace(new_space);
  new_capacity = new_space->Capacity();
  CHECK(old_capacity == new_capacity);

  // Explicitly shrinking should not affect space capacity.
  old_capacity = new_space->Capacity();
  new_space->Shrink();
  new_capacity = new_space->Capacity();
  CHECK(old_capacity == new_capacity);

  // Let the scavenger empty the new space.
  heap->CollectGarbage(NEW_SPACE);
  CHECK_LE(new_space->Size(), old_capacity);

  // Explicitly shrinking should halve the space capacity.
  old_capacity = new_space->Capacity();
  new_space->Shrink();
  new_capacity = new_space->Capacity();
  CHECK(old_capacity == 2 * new_capacity);

  // Consecutive shrinking should not affect space capacity.
  old_capacity = new_space->Capacity();
  new_space->Shrink();
  new_space->Shrink();
  new_space->Shrink();
  new_capacity = new_space->Capacity();
  CHECK(old_capacity == new_capacity);
}


TEST(CollectingAllAvailableGarbageShrinksNewSpace) {
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  if (heap->ReservedSemiSpaceSize() == heap->InitialSemiSpaceSize() ||
      heap->MaxSemiSpaceSize() == heap->InitialSemiSpaceSize()) {
    // The max size cannot exceed the reserved size, since semispaces must be
    // always within the reserved space.  We can't test new space growing and
    // shrinking if the reserved size is the same as the minimum (initial) size.
    return;
  }

  v8::HandleScope scope(CcTest::isolate());
  NewSpace* new_space = heap->new_space();
  intptr_t old_capacity, new_capacity;
  old_capacity = new_space->Capacity();
  new_space->Grow();
  new_capacity = new_space->Capacity();
  CHECK(2 * old_capacity == new_capacity);
  FillUpNewSpace(new_space);
  heap->CollectAllAvailableGarbage();
  new_capacity = new_space->Capacity();
  CHECK(old_capacity == new_capacity);
}


static int NumberOfGlobalObjects() {
  int count = 0;
  HeapIterator iterator(CcTest::heap());
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsGlobalObject()) count++;
  }
  return count;
}


// Test that we don't embed maps from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaMap) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = {x: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o.x; }"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    v8::V8::ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


// Test that we don't embed functions from foreign contexts into
// optimized code.
TEST(LeakNativeContextViaFunction) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = function() { return 42; }");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f(x) { return x(); }"
        "for (var i = 0; i < 10; ++i) f(o);"
        "%OptimizeFunctionOnNextCall(f);"
        "f(o);");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    v8::V8::ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapKeyed) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = [42, 43]");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() { return o[0]; }"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    v8::V8::ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(LeakNativeContextViaMapProto) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(4, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    CompileRun("var v = { y: 42}");
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    v8::Local<v8::Value> v = ctx1->Global()->Get(v8_str("v"));
    ctx2->Enter();
    ctx2->Global()->Set(v8_str("o"), v);
    v8::Local<v8::Value> res = CompileRun(
        "function f() {"
        "  var p = {x: 42};"
        "  p.__proto__ = o;"
        "  return p.x;"
        "}"
        "for (var i = 0; i < 10; ++i) f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
    CHECK_EQ(42, res->Int32Value());
    ctx2->Global()->Set(v8_str("o"), v8::Int32::New(isolate, 0));
    ctx2->Exit();
    ctx1->Exit();
    ctx1p.Reset();
    v8::V8::ContextDisposedNotification();
  }
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  CcTest::heap()->CollectAllAvailableGarbage();
  CHECK_EQ(0, NumberOfGlobalObjects());
}


TEST(InstanceOfStubWriteBarrier) {
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  if (i::FLAG_force_marking_deque_overflows) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function foo () { }"
        "function mkbar () { return new (new Function(\"\")) (); }"
        "function f (x) { return (x instanceof foo); }"
        "function g () { f(mkbar()); }"
        "f(new foo()); f(new foo());"
        "%OptimizeFunctionOnNextCall(f);"
        "f(new foo()); g();");
  }

  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Abort();
  marking->Start();

  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  CHECK(f->IsOptimized());

  while (!Marking::IsBlack(Marking::MarkBitFrom(f->code())) &&
         !marking->IsStopped()) {
    // Discard any pending GC requests otherwise we will get GC when we enter
    // code below.
    marking->Step(MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  }

  CHECK(marking->IsMarking());

  {
    v8::HandleScope scope(CcTest::isolate());
    v8::Handle<v8::Object> global = CcTest::global();
    v8::Handle<v8::Function> g =
        v8::Handle<v8::Function>::Cast(global->Get(v8_str("g")));
    g->Call(global, 0, NULL);
  }

  CcTest::heap()->incremental_marking()->set_should_hurry(true);
  CcTest::heap()->CollectGarbage(OLD_POINTER_SPACE);
}


TEST(PrototypeTransitionClearing) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "var base = {};"
      "var live = [];"
      "for (var i = 0; i < 10; i++) {"
      "  var object = {};"
      "  var prototype = {};"
      "  object.__proto__ = prototype;"
      "  if (i >= 3) live.push(object, prototype);"
      "}");

  Handle<JSObject> baseObject =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("base"))));

  // Verify that only dead prototype transitions are cleared.
  CHECK_EQ(10, baseObject->map()->NumberOfProtoTransitions());
  CcTest::heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  const int transitions = 10 - 3;
  CHECK_EQ(transitions, baseObject->map()->NumberOfProtoTransitions());

  // Verify that prototype transitions array was compacted.
  FixedArray* trans = baseObject->map()->GetPrototypeTransitions();
  for (int i = 0; i < transitions; i++) {
    int j = Map::kProtoTransitionHeaderSize +
        i * Map::kProtoTransitionElementsPerEntry;
    CHECK(trans->get(j + Map::kProtoTransitionMapOffset)->IsMap());
    Object* proto = trans->get(j + Map::kProtoTransitionPrototypeOffset);
    CHECK(proto->IsTheHole() || proto->IsJSObject());
  }

  // Make sure next prototype is placed on an old-space evacuation candidate.
  Handle<JSObject> prototype;
  PagedSpace* space = CcTest::heap()->old_pointer_space();
  {
    AlwaysAllocateScope always_allocate;
    SimulateFullSpace(space);
    prototype = factory->NewJSArray(32 * KB, FAST_HOLEY_ELEMENTS, TENURED);
  }

  // Add a prototype on an evacuation candidate and verify that transition
  // clearing correctly records slots in prototype transition array.
  i::FLAG_always_compact = true;
  Handle<Map> map(baseObject->map());
  CHECK(!space->LastPage()->Contains(
      map->GetPrototypeTransitions()->address()));
  CHECK(space->LastPage()->Contains(prototype->address()));
  JSObject::SetPrototype(baseObject, prototype, false);
  CHECK(Map::GetPrototypeTransition(map, prototype)->IsMap());
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(Map::GetPrototypeTransition(map, prototype)->IsMap());
}


TEST(ResetSharedFunctionInfoCountersDuringIncrementalMarking) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function f () {"
        "  var s = 0;"
        "  for (var i = 0; i < 100; i++)  s += i;"
        "  return s;"
        "}"
        "f(); f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
  }
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));
  CHECK(f->IsOptimized());

  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Abort();
  marking->Start();

  // The following two calls will increment CcTest::heap()->global_ic_age().
  const int kLongIdlePauseInMs = 1000;
  v8::V8::ContextDisposedNotification();
  v8::V8::IdleNotification(kLongIdlePauseInMs);

  while (!marking->IsStopped() && !marking->IsComplete()) {
    marking->Step(1 * MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  }
  if (!marking->IsStopped() || marking->should_hurry()) {
    // We don't normally finish a GC via Step(), we normally finish by
    // setting the stack guard and then do the final steps in the stack
    // guard interrupt.  But here we didn't ask for that, and there is no
    // JS code running to trigger the interrupt, so we explicitly finalize
    // here.
    CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags,
                            "Test finalizing incremental mark-sweep");
  }

  CHECK_EQ(CcTest::heap()->global_ic_age(), f->shared()->ic_age());
  CHECK_EQ(0, f->shared()->opt_count());
  CHECK_EQ(0, f->shared()->code()->profiler_ticks());
}


TEST(ResetSharedFunctionInfoCountersDuringMarkSweep) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
#ifdef VERIFY_HEAP
  i::FLAG_verify_heap = true;
#endif

  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft()) return;
  v8::HandleScope outer_scope(CcTest::isolate());

  {
    v8::HandleScope scope(CcTest::isolate());
    CompileRun(
        "function f () {"
        "  var s = 0;"
        "  for (var i = 0; i < 100; i++)  s += i;"
        "  return s;"
        "}"
        "f(); f();"
        "%OptimizeFunctionOnNextCall(f);"
        "f();");
  }
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));
  CHECK(f->IsOptimized());

  CcTest::heap()->incremental_marking()->Abort();

  // The following two calls will increment CcTest::heap()->global_ic_age().
  // Since incremental marking is off, IdleNotification will do full GC.
  const int kLongIdlePauseInMs = 1000;
  v8::V8::ContextDisposedNotification();
  v8::V8::IdleNotification(kLongIdlePauseInMs);

  CHECK_EQ(CcTest::heap()->global_ic_age(), f->shared()->ic_age());
  CHECK_EQ(0, f->shared()->opt_count());
  CHECK_EQ(0, f->shared()->code()->profiler_ticks());
}


// Test that HAllocateObject will always return an object in new-space.
TEST(OptimizedAllocationAlwaysInNewSpace) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  SimulateFullSpace(CcTest::heap()->new_space());
  AlwaysAllocateScope always_allocate;
  v8::Local<v8::Value> res = CompileRun(
      "function c(x) {"
      "  this.x = x;"
      "  for (var i = 0; i < 32; i++) {"
      "    this['x' + i] = x;"
      "  }"
      "}"
      "function f(x) { return new c(x); };"
      "f(1); f(2); f(3);"
      "%OptimizeFunctionOnNextCall(f);"
      "f(4);");
  CHECK_EQ(4, res->ToObject()->GetRealNamedProperty(v8_str("x"))->Int32Value());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InNewSpace(*o));
}


TEST(OptimizedPretenuringAllocationFolding) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  i::FLAG_allocation_site_pretenuring = false;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());
  CcTest::heap()->SetNewSpaceHighPromotionModeActive(true);

  v8::Local<v8::Value> res = CompileRun(
      "function DataObject() {"
      "  this.a = 1.1;"
      "  this.b = [{}];"
      "  this.c = 1.2;"
      "  this.d = [{}];"
      "  this.e = 1.3;"
      "  this.f = [{}];"
      "}"
      "var number_elements = 20000;"
      "var elements = new Array();"
      "function f() {"
      "  for (var i = 0; i < 20000-1; i++) {"
      "    elements[i] = new DataObject();"
      "  }"
      "  return new DataObject()"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(0)));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(1)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(2)));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(3)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(4)));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(5)));
}


TEST(OptimizedPretenuringAllocationFoldingBlocks) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  i::FLAG_allocation_site_pretenuring = false;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());
  CcTest::heap()->SetNewSpaceHighPromotionModeActive(true);

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 30000;"
      "var elements = new Array(number_elements);"
      "function DataObject() {"
      "  this.a = [{}];"
      "  this.b = [{}];"
      "  this.c = 1.1;"
      "  this.d = 1.2;"
      "  this.e = [{}];"
      "  this.f = 1.3;"
      "}"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = new DataObject();"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(0)));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(1)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(2)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(3)));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(4)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(5)));
}


TEST(OptimizedPretenuringObjectArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 20000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [{}, {}, {}];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldPointerSpace(o->elements()));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
}


TEST(OptimizedPretenuringMixedInObjectProperties) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 20000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = {a: {c: 2.2, d: {}}, b: 1.1};"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldPointerSpace(*o));
  CHECK(CcTest::heap()->InOldPointerSpace(o->RawFastPropertyAt(0)));
  CHECK(CcTest::heap()->InOldDataSpace(o->RawFastPropertyAt(1)));

  JSObject* inner_object = reinterpret_cast<JSObject*>(o->RawFastPropertyAt(0));
  CHECK(CcTest::heap()->InOldPointerSpace(inner_object));
  CHECK(CcTest::heap()->InOldDataSpace(inner_object->RawFastPropertyAt(0)));
  CHECK(CcTest::heap()->InOldPointerSpace(inner_object->RawFastPropertyAt(1)));
}


TEST(OptimizedPretenuringDoubleArrayProperties) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 30000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = {a: 1.1, b: 2.2};"
      "  }"
      "  return elements[i - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldPointerSpace(*o));
  CHECK(CcTest::heap()->InOldDataSpace(o->properties()));
}


TEST(OptimizedPretenuringdoubleArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 30000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [1.1, 2.2, 3.3];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InOldDataSpace(o->elements()));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
}


TEST(OptimizedPretenuringNestedMixedArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 20000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[{}, {}, {}], [1.1, 2.2, 3.3]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  v8::Local<v8::Value> int_array = v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> int_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array));
  v8::Local<v8::Value> double_array = v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> double_array_handle =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
  CHECK(CcTest::heap()->InOldPointerSpace(*int_array_handle));
  CHECK(CcTest::heap()->InOldPointerSpace(int_array_handle->elements()));
  CHECK(CcTest::heap()->InOldPointerSpace(*double_array_handle));
  CHECK(CcTest::heap()->InOldDataSpace(double_array_handle->elements()));
}


TEST(OptimizedPretenuringNestedObjectLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 20000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[{}, {}, {}],[{}, {}, {}]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  v8::Local<v8::Value> int_array_1 = v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> int_array_handle_1 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array_1));
  v8::Local<v8::Value> int_array_2 = v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> int_array_handle_2 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(int_array_2));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
  CHECK(CcTest::heap()->InOldPointerSpace(*int_array_handle_1));
  CHECK(CcTest::heap()->InOldPointerSpace(int_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldPointerSpace(*int_array_handle_2));
  CHECK(CcTest::heap()->InOldPointerSpace(int_array_handle_2->elements()));
}


TEST(OptimizedPretenuringNestedDoubleLiterals) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_max_new_space_size = 2048;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "var number_elements = 20000;"
      "var elements = new Array(number_elements);"
      "function f() {"
      "  for (var i = 0; i < number_elements; i++) {"
      "    elements[i] = [[1.1, 1.2, 1.3],[2.1, 2.2, 2.3]];"
      "  }"
      "  return elements[number_elements - 1];"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  v8::Local<v8::Value> double_array_1 =
      v8::Object::Cast(*res)->Get(v8_str("0"));
  Handle<JSObject> double_array_handle_1 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array_1));
  v8::Local<v8::Value> double_array_2 =
      v8::Object::Cast(*res)->Get(v8_str("1"));
  Handle<JSObject> double_array_handle_2 =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(double_array_2));

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
  CHECK(CcTest::heap()->InOldPointerSpace(*double_array_handle_1));
  CHECK(CcTest::heap()->InOldDataSpace(double_array_handle_1->elements()));
  CHECK(CcTest::heap()->InOldPointerSpace(*double_array_handle_2));
  CHECK(CcTest::heap()->InOldDataSpace(double_array_handle_2->elements()));
}


// Test regular array literals allocation.
TEST(OptimizedAllocationArrayLiterals) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());

  v8::Local<v8::Value> res = CompileRun(
      "function f() {"
      "  var numbers = new Array(1, 2, 3);"
      "  numbers[0] = 3.14;"
      "  return numbers;"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");
  CHECK_EQ(static_cast<int>(3.14),
           v8::Object::Cast(*res)->Get(v8_str("0"))->Int32Value());

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  CHECK(CcTest::heap()->InNewSpace(o->elements()));
}


TEST(OptimizedPretenuringCallNew) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_allocation_site_pretenuring = false;
  i::FLAG_pretenuring_call_new = true;
  CcTest::InitializeVM();
  if (!CcTest::i_isolate()->use_crankshaft() || i::FLAG_always_opt) return;
  if (i::FLAG_gc_global || i::FLAG_stress_compaction) return;
  v8::HandleScope scope(CcTest::isolate());
  CcTest::heap()->SetNewSpaceHighPromotionModeActive(true);

  AlwaysAllocateScope always_allocate;
  v8::Local<v8::Value> res = CompileRun(
      "function g() { this.a = 0; }"
      "function f() {"
      "  return new g();"
      "};"
      "f(); f(); f();"
      "%OptimizeFunctionOnNextCall(f);"
      "f();");

  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));
  CHECK(CcTest::heap()->InOldPointerSpace(*o));
}


static int CountMapTransitions(Map* map) {
  return map->transitions()->number_of_transitions();
}


// Test that map transitions are cleared and maps are collected with
// incremental marking as well.
TEST(Regress1465) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_trace_incremental_marking = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  static const int transitions_count = 256;

  {
    AlwaysAllocateScope always_allocate;
    for (int i = 0; i < transitions_count; i++) {
      EmbeddedVector<char, 64> buffer;
      OS::SNPrintF(buffer, "var o = new Object; o.prop%d = %d;", i, i);
      CompileRun(buffer.start());
    }
    CompileRun("var root = new Object;");
  }

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // Count number of live transitions before marking.
  int transitions_before = CountMapTransitions(root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(transitions_count, transitions_before);

  SimulateIncrementalMarking();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  // Count number of live transitions after marking.  Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after = CountMapTransitions(root->map());
  CompileRun("%DebugPrint(root);");
  CHECK_EQ(1, transitions_after);
}


TEST(Regress2143a) {
  i::FLAG_collect_maps = true;
  i::FLAG_incremental_marking = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare a map transition from the root object together with a yet
  // untransitioned root object.
  CompileRun("var root = new Object;"
             "root.foo = 0;"
             "root = new Object;");

  SimulateIncrementalMarking();

  // Compile a StoreIC that performs the prepared map transition. This
  // will restart incremental marking and should make sure the root is
  // marked grey again.
  CompileRun("function f(o) {"
             "  o.foo = 0;"
             "}"
             "f(new Object);"
             "f(root);");

  // This bug only triggers with aggressive IC clearing.
  CcTest::heap()->AgeInlineCaches();

  // Explicitly request GC to perform final marking step and sweeping.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // The root object should be in a sane state.
  CHECK(root->IsJSObject());
  CHECK(root->map()->IsMap());
}


TEST(Regress2143b) {
  i::FLAG_collect_maps = true;
  i::FLAG_incremental_marking = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare a map transition from the root object together with a yet
  // untransitioned root object.
  CompileRun("var root = new Object;"
             "root.foo = 0;"
             "root = new Object;");

  SimulateIncrementalMarking();

  // Compile an optimized LStoreNamedField that performs the prepared
  // map transition. This will restart incremental marking and should
  // make sure the root is marked grey again.
  CompileRun("function f(o) {"
             "  o.foo = 0;"
             "}"
             "f(new Object);"
             "f(new Object);"
             "%OptimizeFunctionOnNextCall(f);"
             "f(root);"
             "%DeoptimizeFunction(f);");

  // This bug only triggers with aggressive IC clearing.
  CcTest::heap()->AgeInlineCaches();

  // Explicitly request GC to perform final marking step and sweeping.
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  Handle<JSObject> root =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Object>::Cast(
              CcTest::global()->Get(v8_str("root"))));

  // The root object should be in a sane state.
  CHECK(root->IsJSObject());
  CHECK(root->map()->IsMap());
}


TEST(ReleaseOverReservedPages) {
  i::FLAG_trace_gc = true;
  // The optimizer can allocate stuff, messing up the test.
  i::FLAG_crankshaft = false;
  i::FLAG_always_opt = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(CcTest::isolate());
  static const int number_of_test_pages = 20;

  // Prepare many pages with low live-bytes count.
  PagedSpace* old_pointer_space = heap->old_pointer_space();
  CHECK_EQ(1, old_pointer_space->CountTotalPages());
  for (int i = 0; i < number_of_test_pages; i++) {
    AlwaysAllocateScope always_allocate;
    SimulateFullSpace(old_pointer_space);
    factory->NewFixedArray(1, TENURED);
  }
  CHECK_EQ(number_of_test_pages + 1, old_pointer_space->CountTotalPages());

  // Triggering one GC will cause a lot of garbage to be discovered but
  // even spread across all allocated pages.
  heap->CollectAllGarbage(Heap::kNoGCFlags, "triggered for preparation");
  CHECK_GE(number_of_test_pages + 1, old_pointer_space->CountTotalPages());

  // Triggering subsequent GCs should cause at least half of the pages
  // to be released to the OS after at most two cycles.
  heap->CollectAllGarbage(Heap::kNoGCFlags, "triggered by test 1");
  CHECK_GE(number_of_test_pages + 1, old_pointer_space->CountTotalPages());
  heap->CollectAllGarbage(Heap::kNoGCFlags, "triggered by test 2");
  CHECK_GE(number_of_test_pages + 1, old_pointer_space->CountTotalPages() * 2);

  // Triggering a last-resort GC should cause all pages to be released to the
  // OS so that other processes can seize the memory.  If we get a failure here
  // where there are 2 pages left instead of 1, then we should increase the
  // size of the first page a little in SizeOfFirstPage in spaces.cc.  The
  // first page should be small in order to reduce memory used when the VM
  // boots, but if the 20 small arrays don't fit on the first page then that's
  // an indication that it is too small.
  heap->CollectAllAvailableGarbage("triggered really hard");
  CHECK_EQ(1, old_pointer_space->CountTotalPages());
}


TEST(Regress2237) {
  i::FLAG_stress_compaction = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());
  Handle<String> slice(CcTest::heap()->empty_string());

  {
    // Generate a parent that lives in new-space.
    v8::HandleScope inner_scope(CcTest::isolate());
    const char* c = "This text is long enough to trigger sliced strings.";
    Handle<String> s = factory->NewStringFromAscii(CStrVector(c));
    CHECK(s->IsSeqOneByteString());
    CHECK(CcTest::heap()->InNewSpace(*s));

    // Generate a sliced string that is based on the above parent and
    // lives in old-space.
    SimulateFullSpace(CcTest::heap()->new_space());
    AlwaysAllocateScope always_allocate;
    Handle<String> t = factory->NewProperSubString(s, 5, 35);
    CHECK(t->IsSlicedString());
    CHECK(!CcTest::heap()->InNewSpace(*t));
    *slice.location() = *t.location();
  }

  CHECK(SlicedString::cast(*slice)->parent()->IsSeqOneByteString());
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(SlicedString::cast(*slice)->parent()->IsSeqOneByteString());
}


#ifdef OBJECT_PRINT
TEST(PrintSharedFunctionInfo) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  const char* source = "f = function() { return 987654321; }\n"
                       "g = function() { return 123456789; }\n";
  CompileRun(source);
  Handle<JSFunction> g =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("g"))));

  DisallowHeapAllocation no_allocation;
  g->shared()->PrintLn();
}
#endif  // OBJECT_PRINT


TEST(Regress2211) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  v8::Handle<v8::String> value = v8_str("val string");
  Smi* hash = Smi::FromInt(321);
  Heap* heap = CcTest::heap();

  for (int i = 0; i < 2; i++) {
    // Store identity hash first and common hidden property second.
    v8::Handle<v8::Object> obj = v8::Object::New(CcTest::isolate());
    Handle<JSObject> internal_obj = v8::Utils::OpenHandle(*obj);
    CHECK(internal_obj->HasFastProperties());

    // In the first iteration, set hidden value first and identity hash second.
    // In the second iteration, reverse the order.
    if (i == 0) obj->SetHiddenValue(v8_str("key string"), value);
    JSObject::SetIdentityHash(internal_obj, handle(hash, CcTest::i_isolate()));
    if (i == 1) obj->SetHiddenValue(v8_str("key string"), value);

    // Check values.
    CHECK_EQ(hash,
             internal_obj->GetHiddenProperty(heap->identity_hash_string()));
    CHECK(value->Equals(obj->GetHiddenValue(v8_str("key string"))));

    // Check size.
    DescriptorArray* descriptors = internal_obj->map()->instance_descriptors();
    ObjectHashTable* hashtable = ObjectHashTable::cast(
        internal_obj->RawFastPropertyAt(descriptors->GetFieldIndex(0)));
    // HashTable header (5) and 4 initial entries (8).
    CHECK_LE(hashtable->SizeFor(hashtable->length()), 13 * kPointerSize);
  }
}


TEST(IncrementalMarkingClearsTypeFeedbackCells) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> fun1, fun2;

  {
    LocalContext env;
    CompileRun("function fun() {};");
    fun1 = env->Global()->Get(v8_str("fun"));
  }

  {
    LocalContext env;
    CompileRun("function fun() {};");
    fun2 = env->Global()->Get(v8_str("fun"));
  }

  // Prepare function f that contains type feedback for closures
  // originating from two different native contexts.
  CcTest::global()->Set(v8_str("fun1"), fun1);
  CcTest::global()->Set(v8_str("fun2"), fun2);
  CompileRun("function f(a, b) { a(); b(); } f(fun1, fun2);");
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));
  Handle<TypeFeedbackCells> cells(TypeFeedbackInfo::cast(
      f->shared()->code()->type_feedback_info())->type_feedback_cells());

  CHECK_EQ(2, cells->CellCount());
  CHECK(cells->GetCell(0)->value()->IsJSFunction());
  CHECK(cells->GetCell(1)->value()->IsJSFunction());

  SimulateIncrementalMarking();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  CHECK_EQ(2, cells->CellCount());
  CHECK(cells->GetCell(0)->value()->IsTheHole());
  CHECK(cells->GetCell(1)->value()->IsTheHole());
}


static Code* FindFirstIC(Code* code, Code::Kind kind) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CONSTRUCT_CALL) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target = Code::GetCodeFromTargetAddress(info->target_address());
    if (target->is_inline_cache_stub() && target->kind() == kind) {
      return target;
    }
  }
  return NULL;
}


TEST(IncrementalMarkingPreservesMonomorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  CompileRun("function fun() { this.x = 1; }; var obj = new fun();"
             "function f(o) { return o.x; } f(obj); f(obj);");
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(ic_before->ic_state() == MONOMORPHIC);

  SimulateIncrementalMarking();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(ic_after->ic_state() == MONOMORPHIC);
}


TEST(IncrementalMarkingClearsMonomorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(v8_str("obj"));
  }

  // Prepare function f that contains a monomorphic IC for object
  // originating from a different native context.
  CcTest::global()->Set(v8_str("obj1"), obj1);
  CompileRun("function f(o) { return o.x; } f(obj1); f(obj1);");
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(ic_before->ic_state() == MONOMORPHIC);

  // Fire context dispose notification.
  v8::V8::ContextDisposedNotification();
  SimulateIncrementalMarking();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(IC::IsCleared(ic_after));
}


TEST(IncrementalMarkingClearsPolymorphicIC) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Value> obj1, obj2;

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 1; }; var obj = new fun();");
    obj1 = env->Global()->Get(v8_str("obj"));
  }

  {
    LocalContext env;
    CompileRun("function fun() { this.x = 2; }; var obj = new fun();");
    obj2 = env->Global()->Get(v8_str("obj"));
  }

  // Prepare function f that contains a polymorphic IC for objects
  // originating from two different native contexts.
  CcTest::global()->Set(v8_str("obj1"), obj1);
  CcTest::global()->Set(v8_str("obj2"), obj2);
  CompileRun("function f(o) { return o.x; } f(obj1); f(obj1); f(obj2);");
  Handle<JSFunction> f =
      v8::Utils::OpenHandle(
          *v8::Handle<v8::Function>::Cast(
              CcTest::global()->Get(v8_str("f"))));

  Code* ic_before = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(ic_before->ic_state() == POLYMORPHIC);

  // Fire context dispose notification.
  v8::V8::ContextDisposedNotification();
  SimulateIncrementalMarking();
  CcTest::heap()->CollectAllGarbage(Heap::kNoGCFlags);

  Code* ic_after = FindFirstIC(f->shared()->code(), Code::LOAD_IC);
  CHECK(IC::IsCleared(ic_after));
}


class SourceResource: public v8::String::ExternalAsciiStringResource {
 public:
  explicit SourceResource(const char* data)
    : data_(data), length_(strlen(data)) { }

  virtual void Dispose() {
    i::DeleteArray(data_);
    data_ = NULL;
  }

  const char* data() const { return data_; }

  size_t length() const { return length_; }

  bool IsDisposed() { return data_ == NULL; }

 private:
  const char* data_;
  size_t length_;
};


void ReleaseStackTraceDataTest(const char* source, const char* accessor) {
  // Test that the data retained by the Error.stack accessor is released
  // after the first time the accessor is fired.  We use external string
  // to check whether the data is being released since the external string
  // resource's callback is fired when the external string is GC'ed.
  v8::HandleScope scope(CcTest::isolate());
  SourceResource* resource = new SourceResource(i::StrDup(source));
  {
    v8::HandleScope scope(CcTest::isolate());
    v8::Handle<v8::String> source_string =
        v8::String::NewExternal(CcTest::isolate(), resource);
    CcTest::heap()->CollectAllAvailableGarbage();
    v8::Script::Compile(source_string)->Run();
    CHECK(!resource->IsDisposed());
  }
  // CcTest::heap()->CollectAllAvailableGarbage();
  CHECK(!resource->IsDisposed());

  CompileRun(accessor);
  CcTest::heap()->CollectAllAvailableGarbage();

  // External source has been released.
  CHECK(resource->IsDisposed());
  delete resource;
}


TEST(ReleaseStackTraceData) {
  FLAG_use_ic = false;  // ICs retain objects.
  FLAG_concurrent_recompilation = false;
  CcTest::InitializeVM();
  static const char* source1 = "var error = null;            "
  /* Normal Error */           "try {                        "
                               "  throw new Error();         "
                               "} catch (e) {                "
                               "  error = e;                 "
                               "}                            ";
  static const char* source2 = "var error = null;            "
  /* Stack overflow */         "try {                        "
                               "  (function f() { f(); })(); "
                               "} catch (e) {                "
                               "  error = e;                 "
                               "}                            ";
  static const char* source3 = "var error = null;            "
  /* Normal Error */           "try {                        "
  /* as prototype */           "  throw new Error();         "
                               "} catch (e) {                "
                               "  error = {};                "
                               "  error.__proto__ = e;       "
                               "}                            ";
  static const char* source4 = "var error = null;            "
  /* Stack overflow */         "try {                        "
  /* as prototype   */         "  (function f() { f(); })(); "
                               "} catch (e) {                "
                               "  error = {};                "
                               "  error.__proto__ = e;       "
                               "}                            ";
  static const char* getter = "error.stack";
  static const char* setter = "error.stack = 0";

  ReleaseStackTraceDataTest(source1, setter);
  ReleaseStackTraceDataTest(source2, setter);
  // We do not test source3 and source4 with setter, since the setter is
  // supposed to (untypically) write to the receiver, not the holder.  This is
  // to emulate the behavior of a data property.

  ReleaseStackTraceDataTest(source1, getter);
  ReleaseStackTraceDataTest(source2, getter);
  ReleaseStackTraceDataTest(source3, getter);
  ReleaseStackTraceDataTest(source4, getter);
}


TEST(Regress159140) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_flush_code_incrementally = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Prepare several closures that are all eligible for code flushing
  // because all reachable ones are not optimized. Make sure that the
  // optimized code object is directly reachable through a handle so
  // that it is marked black during incremental marking.
  Handle<Code> code;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function h(x) {}"
               "function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "var g = mkClosure();"
               "f(1); f(2);"
               "g(1); g(2);"
               "h(1); h(2);"
               "%OptimizeFunctionOnNextCall(f); f(3);"
               "%OptimizeFunctionOnNextCall(h); h(3);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    CompileRun("f = null;");

    Handle<JSFunction> g =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("g"))));
    CHECK(g->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      g->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    code = inner_scope.CloseAndEscape(Handle<Code>(f->code()));
  }

  // Simulate incremental marking so that the functions are enqueued as
  // code flushing candidates. Then optimize one function. Finally
  // finish the GC to complete code flushing.
  SimulateIncrementalMarking();
  CompileRun("%OptimizeFunctionOnNextCall(g); g(3);");
  heap->CollectAllGarbage(Heap::kNoGCFlags);

  // Unoptimized code is missing and the deoptimizer will go ballistic.
  CompileRun("g('bozo');");
}


TEST(Regress165495) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_flush_code_incrementally = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Prepare an optimized closure that the optimized code map will get
  // populated. Then age the unoptimized code to trigger code flushing
  // but make sure the optimized code is unreachable.
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);"
               "%OptimizeFunctionOnNextCall(f); f(3);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    CompileRun("f = null;");
  }

  // Simulate incremental marking so that unoptimized code is flushed
  // even though it still is cached in the optimized code map.
  SimulateIncrementalMarking();
  heap->CollectAllGarbage(Heap::kNoGCFlags);

  // Make a new closure that will get code installed from the code map.
  // Unoptimized code is missing and the deoptimizer will go ballistic.
  CompileRun("var g = mkClosure(); g('bozo');");
}


TEST(Regress169209) {
  i::FLAG_stress_compaction = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_flush_code_incrementally = true;

  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Prepare a shared function info eligible for code flushing for which
  // the unoptimized code will be replaced during optimization.
  Handle<SharedFunctionInfo> shared1;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function f() { return 'foobar'; }"
               "function g(x) { if (x) f(); }"
               "f();"
               "g(false);"
               "g(false);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    shared1 = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
  }

  // Prepare a shared function info eligible for code flushing that will
  // represent the dangling tail of the candidate list.
  Handle<SharedFunctionInfo> shared2;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function flushMe() { return 0; }"
               "flushMe(1);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("flushMe"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    shared2 = inner_scope.CloseAndEscape(handle(f->shared(), isolate));
  }

  // Simulate incremental marking and collect code flushing candidates.
  SimulateIncrementalMarking();
  CHECK(shared1->code()->gc_metadata() != NULL);

  // Optimize function and make sure the unoptimized code is replaced.
#ifdef DEBUG
  FLAG_stop_at = "f";
#endif
  CompileRun("%OptimizeFunctionOnNextCall(g);"
             "g(false);");

  // Finish garbage collection cycle.
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK(shared1->code()->gc_metadata() == NULL);
}


// Helper function that simulates a fill new-space in the heap.
static inline void AllocateAllButNBytes(v8::internal::NewSpace* space,
                                        int extra_bytes) {
  int space_remaining = static_cast<int>(
      *space->allocation_limit_address() - *space->allocation_top_address());
  CHECK(space_remaining >= extra_bytes);
  int new_linear_size = space_remaining - extra_bytes;
  v8::internal::MaybeObject* maybe = space->AllocateRaw(new_linear_size);
  v8::internal::FreeListNode* node = v8::internal::FreeListNode::cast(maybe);
  node->set_size(space->heap(), new_linear_size);
}


TEST(Regress169928) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_crankshaft = false;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  v8::HandleScope scope(CcTest::isolate());

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (FLAG_gc_global || FLAG_stress_compaction) return;

  // Prepare the environment
  CompileRun("function fastliteralcase(literal, value) {"
             "    literal[0] = value;"
             "    return literal;"
             "}"
             "function get_standard_literal() {"
             "    var literal = [1, 2, 3];"
             "    return literal;"
             "}"
             "obj = fastliteralcase(get_standard_literal(), 1);"
             "obj = fastliteralcase(get_standard_literal(), 1.5);"
             "obj = fastliteralcase(get_standard_literal(), 2);");

  // prepare the heap
  v8::Local<v8::String> mote_code_string =
      v8_str("fastliteralcase(mote, 2.5);");

  v8::Local<v8::String> array_name = v8_str("mote");
  CcTest::global()->Set(array_name, v8::Int32::New(CcTest::isolate(), 0));

  // First make sure we flip spaces
  CcTest::heap()->CollectGarbage(NEW_SPACE);

  // Allocate the object.
  Handle<FixedArray> array_data = factory->NewFixedArray(2, NOT_TENURED);
  array_data->set(0, Smi::FromInt(1));
  array_data->set(1, Smi::FromInt(2));

  AllocateAllButNBytes(CcTest::heap()->new_space(),
                       JSArray::kSize + AllocationMemento::kSize +
                       kPointerSize);

  Handle<JSArray> array = factory->NewJSArrayWithElements(array_data,
                                                          FAST_SMI_ELEMENTS,
                                                          NOT_TENURED);

  CHECK_EQ(Smi::FromInt(2), array->length());
  CHECK(array->HasFastSmiOrObjectElements());

  // We need filler the size of AllocationMemento object, plus an extra
  // fill pointer value.
  MaybeObject* maybe_object = CcTest::heap()->AllocateRaw(
      AllocationMemento::kSize + kPointerSize, NEW_SPACE, OLD_POINTER_SPACE);
  Object* obj = NULL;
  CHECK(maybe_object->ToObject(&obj));
  Address addr_obj = reinterpret_cast<Address>(
      reinterpret_cast<byte*>(obj - kHeapObjectTag));
  CcTest::heap()->CreateFillerObjectAt(addr_obj,
                             AllocationMemento::kSize + kPointerSize);

  // Give the array a name, making sure not to allocate strings.
  v8::Handle<v8::Object> array_obj = v8::Utils::ToLocal(array);
  CcTest::global()->Set(array_name, array_obj);

  // This should crash with a protection violation if we are running a build
  // with the bug.
  AlwaysAllocateScope aa_scope;
  v8::Script::Compile(mote_code_string)->Run();
}


TEST(Regress168801) {
  i::FLAG_always_compact = true;
  i::FLAG_cache_optimized_code = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_flush_code_incrementally = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Ensure the code ends up on an evacuation candidate.
  SimulateFullSpace(heap->code_space());

  // Prepare an unoptimized function that is eligible for code flushing.
  Handle<JSFunction> function;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    function = inner_scope.CloseAndEscape(handle(*f, isolate));
  }

  // Simulate incremental marking so that unoptimized function is enqueued as a
  // candidate for code flushing. The shared function info however will not be
  // explicitly enqueued.
  SimulateIncrementalMarking();

  // Now optimize the function so that it is taken off the candidate list.
  {
    HandleScope inner_scope(isolate);
    CompileRun("%OptimizeFunctionOnNextCall(f); f(3);");
  }

  // This cycle will bust the heap and subsequent cycles will go ballistic.
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
}


TEST(Regress173458) {
  i::FLAG_always_compact = true;
  i::FLAG_cache_optimized_code = false;
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_flush_code_incrementally = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope scope(isolate);

  // Perform one initial GC to enable code flushing.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Ensure the code ends up on an evacuation candidate.
  SimulateFullSpace(heap->code_space());

  // Prepare an unoptimized function that is eligible for code flushing.
  Handle<JSFunction> function;
  {
    HandleScope inner_scope(isolate);
    CompileRun("function mkClosure() {"
               "  return function(x) { return x + 1; };"
               "}"
               "var f = mkClosure();"
               "f(1); f(2);");

    Handle<JSFunction> f =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("f"))));
    CHECK(f->is_compiled());
    const int kAgingThreshold = 6;
    for (int i = 0; i < kAgingThreshold; i++) {
      f->shared()->code()->MakeOlder(static_cast<MarkingParity>(i % 2));
    }

    function = inner_scope.CloseAndEscape(handle(*f, isolate));
  }

  // Simulate incremental marking so that unoptimized function is enqueued as a
  // candidate for code flushing. The shared function info however will not be
  // explicitly enqueued.
  SimulateIncrementalMarking();

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Now enable the debugger which in turn will disable code flushing.
  CHECK(isolate->debug()->Load());
#endif  // ENABLE_DEBUGGER_SUPPORT

  // This cycle will bust the heap and subsequent cycles will go ballistic.
  heap->CollectAllGarbage(Heap::kNoGCFlags);
  heap->CollectAllGarbage(Heap::kNoGCFlags);
}


class DummyVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) { }
};


TEST(DeferredHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));
  HandleScopeData* data = isolate->handle_scope_data();
  Handle<Object> init(heap->empty_string(), isolate);
  while (data->next < data->limit) {
    Handle<Object> obj(heap->empty_string(), isolate);
  }
  // An entire block of handles has been filled.
  // Next handle would require a new block.
  ASSERT(data->next == data->limit);

  DeferredHandleScope deferred(isolate);
  DummyVisitor visitor;
  isolate->handle_scope_implementer()->Iterate(&visitor);
  delete deferred.Detach();
}


TEST(IncrementalMarkingStepMakesBigProgressWithLargeObjects) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun("function f(n) {"
             "    var a = new Array(n);"
             "    for (var i = 0; i < n; i += 100) a[i] = i;"
             "};"
             "f(10 * 1024 * 1024);");
  IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  if (marking->IsStopped()) marking->Start();
  // This big step should be sufficient to mark the whole array.
  marking->Step(100 * MB, IncrementalMarking::NO_GC_VIA_STACK_GUARD);
  ASSERT(marking->IsComplete());
}


TEST(DisableInlineAllocation) {
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  CompileRun("function test() {"
             "  var x = [];"
             "  for (var i = 0; i < 10; i++) {"
             "    x[i] = [ {}, [1,2,3], [1,x,3] ];"
             "  }"
             "}"
             "function run() {"
             "  %OptimizeFunctionOnNextCall(test);"
             "  test();"
             "  %DeoptimizeFunction(test);"
             "}");

  // Warm-up with inline allocation enabled.
  CompileRun("test(); test(); run();");

  // Run test with inline allocation disabled.
  CcTest::heap()->DisableInlineAllocation();
  CompileRun("run()");

  // Run test with inline allocation disabled and pretenuring.
  CcTest::heap()->SetNewSpaceHighPromotionModeActive(true);
  CompileRun("run()");

  // Run test with inline allocation re-enabled.
  CcTest::heap()->EnableInlineAllocation();
  CompileRun("run()");
}


static int AllocationSitesCount(Heap* heap) {
  int count = 0;
  for (Object* site = heap->allocation_sites_list();
       !(site->IsUndefined());
       site = AllocationSite::cast(site)->weak_next()) {
    count++;
  }
  return count;
}


TEST(EnsureAllocationSiteDependentCodesProcessed) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();
  GlobalHandles* global_handles = isolate->global_handles();

  if (!isolate->use_crankshaft()) return;

  // The allocation site at the head of the list is ours.
  Handle<AllocationSite> site;
  {
    LocalContext context;
    v8::HandleScope scope(context->GetIsolate());

    int count = AllocationSitesCount(heap);
    CompileRun("var bar = function() { return (new Array()); };"
               "var a = bar();"
               "bar();"
               "bar();");

    // One allocation site should have been created.
    int new_count = AllocationSitesCount(heap);
    CHECK_EQ(new_count, (count + 1));
    site = Handle<AllocationSite>::cast(
        global_handles->Create(
            AllocationSite::cast(heap->allocation_sites_list())));

    CompileRun("%OptimizeFunctionOnNextCall(bar); bar();");

    DependentCode::GroupStartIndexes starts(site->dependent_code());
    CHECK_GE(starts.number_of_entries(), 1);
    int index = starts.at(DependentCode::kAllocationSiteTransitionChangedGroup);
    CHECK(site->dependent_code()->is_code_at(index));
    Code* function_bar = site->dependent_code()->code_at(index);
    Handle<JSFunction> bar_handle =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    CHECK_EQ(bar_handle->code(), function_bar);
  }

  // Now make sure that a gc should get rid of the function, even though we
  // still have the allocation site alive.
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage(false);
  }

  // The site still exists because of our global handle, but the code is no
  // longer referred to by dependent_code().
  DependentCode::GroupStartIndexes starts(site->dependent_code());
  int index = starts.at(DependentCode::kAllocationSiteTransitionChangedGroup);
  CHECK(!(site->dependent_code()->is_code_at(index)));
}


TEST(CellsInOptimizedCodeAreWeak) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_weak_embedded_objects_in_optimized_code = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun("bar = (function() {"
               "  function bar() {"
               "    return foo(1);"
               "  };"
               "  var foo = function(x) { with (x) { return 1 + x; } };"
               "  bar(foo);"
               "  bar(foo);"
               "  bar(foo);"
               "  %OptimizeFunctionOnNextCall(bar);"
               "  bar(foo);"
               "  return bar;})();");

    Handle<JSFunction> bar =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    code = scope.CloseAndEscape(Handle<Code>(bar->code()));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  }

  ASSERT(code->marked_for_deoptimization());
}


TEST(ObjectsInOptimizedCodeAreWeak) {
  if (i::FLAG_always_opt || !i::FLAG_crankshaft) return;
  i::FLAG_weak_embedded_objects_in_optimized_code = true;
  i::FLAG_allow_natives_syntax = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::internal::Heap* heap = CcTest::heap();

  if (!isolate->use_crankshaft()) return;
  HandleScope outer_scope(heap->isolate());
  Handle<Code> code;
  {
    LocalContext context;
    HandleScope scope(heap->isolate());

    CompileRun("function bar() {"
               "  return foo(1);"
               "};"
               "function foo(x) { with (x) { return 1 + x; } };"
               "bar();"
               "bar();"
               "bar();"
               "%OptimizeFunctionOnNextCall(bar);"
               "bar();");

    Handle<JSFunction> bar =
        v8::Utils::OpenHandle(
            *v8::Handle<v8::Function>::Cast(
                CcTest::global()->Get(v8_str("bar"))));
    code = scope.CloseAndEscape(Handle<Code>(bar->code()));
  }

  // Now make sure that a gc should get rid of the function
  for (int i = 0; i < 4; i++) {
    heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  }

  ASSERT(code->marked_for_deoptimization());
}

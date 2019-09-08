// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"
#include "test/cctest/cctest.h"

#include "src/api/api-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/execution/execution.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/test-feedback-vector.h"

namespace v8 {
namespace internal {

namespace {

#define CHECK_SLOT_KIND(helper, index, expected_kind) \
  CHECK_EQ(expected_kind, helper.vector()->GetKind(helper.slot(index)));


static Handle<JSFunction> GetFunction(const char* name) {
  v8::MaybeLocal<v8::Value> v8_f = CcTest::global()->Get(
      CcTest::isolate()->GetCurrentContext(), v8_str(name));
  Handle<JSFunction> f =
      Handle<JSFunction>::cast(v8::Utils::OpenHandle(*v8_f.ToLocalChecked()));
  return f;
}


TEST(VectorStructure) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(isolate->allocator(), ZONE_NAME);

  Handle<FeedbackVector> vector;

  {
    FeedbackVectorSpec one_slot(&zone);
    one_slot.AddForInSlot();
    vector = NewFeedbackVector(isolate, &one_slot);
    FeedbackVectorHelper helper(vector);
    CHECK_EQ(1, helper.slot_count());
  }

  {
    FeedbackVectorSpec one_icslot(&zone);
    one_icslot.AddCallICSlot();
    vector = NewFeedbackVector(isolate, &one_icslot);
    FeedbackVectorHelper helper(vector);
    CHECK_EQ(1, helper.slot_count());
  }

  {
    FeedbackVectorSpec spec(&zone);
    for (int i = 0; i < 3; i++) {
      spec.AddForInSlot();
    }
    for (int i = 0; i < 5; i++) {
      spec.AddCallICSlot();
    }
    vector = NewFeedbackVector(isolate, &spec);
    FeedbackVectorHelper helper(vector);
    CHECK_EQ(8, helper.slot_count());

    int index = vector->GetIndex(helper.slot(0));

    CHECK_EQ(helper.slot(0), vector->ToSlot(index));

    index = vector->GetIndex(helper.slot(3));
    CHECK_EQ(helper.slot(3), vector->ToSlot(index));

    index = vector->GetIndex(helper.slot(7));
    CHECK_EQ(3 + 4 * FeedbackMetadata::GetSlotSize(FeedbackSlotKind::kCall),
             index);
    CHECK_EQ(helper.slot(7), vector->ToSlot(index));

    CHECK_EQ(3 + 5 * FeedbackMetadata::GetSlotSize(FeedbackSlotKind::kCall),
             vector->length());
  }

  {
    FeedbackVectorSpec spec(&zone);
    spec.AddForInSlot();
    spec.AddFeedbackCellForCreateClosure();
    spec.AddForInSlot();
    vector = NewFeedbackVector(isolate, &spec);
    FeedbackVectorHelper helper(vector);
    FeedbackCell cell = *vector->GetClosureFeedbackCell(0);
    CHECK_EQ(cell.value(), *factory->undefined_value());
  }
}


// IC slots need an encoding to recognize what is in there.
TEST(VectorICMetadata) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Zone zone(isolate->allocator(), ZONE_NAME);

  FeedbackVectorSpec spec(&zone);
  // Set metadata.
  for (int i = 0; i < 40; i++) {
    switch (i % 4) {
      case 0:
        spec.AddForInSlot();
        break;
      case 1:
        spec.AddCallICSlot();
        break;
      case 2:
        spec.AddLoadICSlot();
        break;
      case 3:
        spec.AddKeyedLoadICSlot();
        break;
    }
  }

  Handle<FeedbackVector> vector = NewFeedbackVector(isolate, &spec);
  FeedbackVectorHelper helper(vector);
  CHECK_EQ(40, helper.slot_count());

  // Meanwhile set some feedback values and type feedback values to
  // verify the data structure remains intact.
  vector->Set(FeedbackSlot(0), MaybeObject::FromObject(*vector));

  // Verify the metadata is correctly set up from the spec.
  for (int i = 0; i < 40; i++) {
    FeedbackSlotKind kind = vector->GetKind(helper.slot(i));
    switch (i % 4) {
      case 0:
        CHECK_EQ(FeedbackSlotKind::kForIn, kind);
        break;
      case 1:
        CHECK_EQ(FeedbackSlotKind::kCall, kind);
        break;
      case 2:
        CHECK_EQ(FeedbackSlotKind::kLoadProperty, kind);
        break;
      case 3:
        CHECK_EQ(FeedbackSlotKind::kLoadKeyed, kind);
        break;
    }
  }
}


TEST(VectorCallICStates) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; };"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());

  CompileRun("f(function() { return 16; })");
  CHECK_EQ(GENERIC, nexus.ic_state());

  // After a collection, state should remain GENERIC.
  CcTest::CollectAllGarbage();
  CHECK_EQ(GENERIC, nexus.ic_state());
}

TEST(VectorCallFeedback) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = GetFunction("f");
  Handle<JSFunction> foo = GetFunction("foo");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);

  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  HeapObject heap_object;
  CHECK(nexus.GetFeedback()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(*foo, heap_object);

  CcTest::CollectAllGarbage();
  // It should stay monomorphic even after a GC.
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
}

TEST(VectorCallFeedbackForArray) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function f(a) { a(); };"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(Array);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);

  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  HeapObject heap_object;
  CHECK(nexus.GetFeedback()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(*isolate->array_function(), heap_object);

  CcTest::CollectAllGarbage();
  // It should stay monomorphic even after a GC.
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
}

size_t GetFeedbackVectorLength(Isolate* isolate, const char* src,
                               bool with_oneshot_opt) {
  i::FLAG_enable_one_shot_optimization = with_oneshot_opt;
  i::Handle<i::Object> i_object = v8::Utils::OpenHandle(*CompileRun(src));
  i::Handle<i::JSFunction> f = i::Handle<i::JSFunction>::cast(i_object);
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  return feedback_vector->length();
}

TEST(OneShotCallICSlotCount) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  if (i::FLAG_lazy_feedback_allocation) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  i::FLAG_compilation_cache = false;

  const char* no_call = R"(
    function f1() {};
    function f2() {};
    (function() {
      return arguments.callee;
    })();
  )";
  // len = 2 * 1 ldaNamed property
  CHECK_EQ(GetFeedbackVectorLength(isolate, no_call, false), 2);
  // no slots of named property loads/stores in one shot
  CHECK_EQ(GetFeedbackVectorLength(isolate, no_call, true), 0);

  const char* single_call = R"(
    function f1() {};
    function f2() {};
    (function() {
      f1();
      return arguments.callee;
    })();
  )";
  // len = 2 * 1 ldaNamed Slot + 2 * 1 CachedGlobalSlot + 2 * 1 CallICSlot
  CHECK_EQ(GetFeedbackVectorLength(isolate, single_call, false), 6);
  // len = 2 * 1 CachedGlobalSlot
  CHECK_EQ(GetFeedbackVectorLength(isolate, single_call, true), 2);

  const char* multiple_calls = R"(
    function f1() {};
    function f2() {};
    (function() {
      f1();
      f2();
      f1();
      f2();
      return arguments.callee;
    })();
  )";
  // len = 2 * 1 ldaNamedSlot + 2 *  2 CachedGlobalSlot (one for each unique
  // function) + 2 * 4 CallICSlot (one for each function call)
  CHECK_EQ(GetFeedbackVectorLength(isolate, multiple_calls, false), 14);
  // CachedGlobalSlot (one for each unique function)
  // len = 2 * 2 CachedGlobalSlot (one for each unique function)
  CHECK_EQ(GetFeedbackVectorLength(isolate, multiple_calls, true), 4);
}

TEST(VectorCallCounts) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());

  CompileRun("f(foo); f(foo);");
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  CHECK_EQ(3, nexus.GetCallCount());

  // Send the IC megamorphic, but we should still have incrementing counts.
  CompileRun("f(function() { return 12; });");
  CHECK_EQ(GENERIC, nexus.ic_state());
  CHECK_EQ(4, nexus.GetCallCount());
}

TEST(VectorConstructCounts) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function Foo() {}"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { new a(); } f(Foo);");
  Handle<JSFunction> f = GetFunction("f");
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);

  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());

  CHECK(feedback_vector->Get(slot)->IsWeak());

  CompileRun("f(Foo); f(Foo);");
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  CHECK_EQ(3, nexus.GetCallCount());

  // Send the IC megamorphic, but we should still have incrementing counts.
  CompileRun("f(function() {});");
  CHECK_EQ(GENERIC, nexus.ic_state());
  CHECK_EQ(4, nexus.GetCallCount());
}

TEST(VectorSpeculationMode) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function Foo() {}"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { new a(); } f(Foo);");
  Handle<JSFunction> f = GetFunction("f");
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);

  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(SpeculationMode::kAllowSpeculation, nexus.GetSpeculationMode());

  CompileRun("f(Foo); f(Foo);");
  CHECK_EQ(3, nexus.GetCallCount());
  CHECK_EQ(SpeculationMode::kAllowSpeculation, nexus.GetSpeculationMode());

  nexus.SetSpeculationMode(SpeculationMode::kDisallowSpeculation);
  CHECK_EQ(SpeculationMode::kDisallowSpeculation, nexus.GetSpeculationMode());
  CHECK_EQ(3, nexus.GetCallCount());

  nexus.SetSpeculationMode(SpeculationMode::kAllowSpeculation);
  CHECK_EQ(SpeculationMode::kAllowSpeculation, nexus.GetSpeculationMode());
  CHECK_EQ(3, nexus.GetCallCount());
}

TEST(VectorLoadICStates) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "var o = { foo: 3 };"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { return a.foo; } f(o);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);

  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  // Verify that the monomorphic map is the one we expect.
  v8::MaybeLocal<v8::Value> v8_o =
      CcTest::global()->Get(context.local(), v8_str("o"));
  Handle<JSObject> o =
      Handle<JSObject>::cast(v8::Utils::OpenHandle(*v8_o.ToLocalChecked()));
  CHECK_EQ(o->map(), nexus.GetFirstMap());

  // Now go polymorphic.
  CompileRun("f({ blarg: 3, foo: 2 })");
  CHECK_EQ(POLYMORPHIC, nexus.ic_state());

  CompileRun(
      "delete o.foo;"
      "f(o)");
  CHECK_EQ(POLYMORPHIC, nexus.ic_state());

  CompileRun("f({ blarg: 3, torino: 10, foo: 2 })");
  CHECK_EQ(POLYMORPHIC, nexus.ic_state());
  MapHandles maps;
  nexus.ExtractMaps(&maps);
  CHECK_EQ(4, maps.size());

  // Finally driven megamorphic.
  CompileRun("f({ blarg: 3, gran: 3, torino: 10, foo: 2 })");
  CHECK_EQ(MEGAMORPHIC, nexus.ic_state());
  CHECK(nexus.GetFirstMap().is_null());

  // After a collection, state should not be reset to PREMONOMORPHIC.
  CcTest::CollectAllGarbage();
  CHECK_EQ(MEGAMORPHIC, nexus.ic_state());
}

TEST(VectorLoadGlobalICSlotSharing) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Function f has 5 LoadGlobalICs: 3 for {o} references outside of "typeof"
  // operator and 2 for {o} references inside "typeof" operator.
  CompileRun(
      "o = 10;"
      "function f() {"
      "  var x = o || 10;"
      "  var y = typeof o;"
      "  return o , typeof o, x , y, o;"
      "}"
      "%EnsureFeedbackVectorForFunction(f);"
      "f();");
  Handle<JSFunction> f = GetFunction("f");
  // There should be two IC slots for {o} references outside and inside
  // typeof operator respectively.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorHelper helper(feedback_vector);
  CHECK_EQ(2, helper.slot_count());
  CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
  CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kLoadGlobalInsideTypeof);
  FeedbackSlot slot1 = helper.slot(0);
  FeedbackSlot slot2 = helper.slot(1);
  CHECK_EQ(MONOMORPHIC, FeedbackNexus(feedback_vector, slot1).ic_state());
  CHECK_EQ(MONOMORPHIC, FeedbackNexus(feedback_vector, slot2).ic_state());
}


TEST(VectorLoadICOnSmi) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "var o = { foo: 3 };"
      "%EnsureFeedbackVectorForFunction(f);"
      "function f(a) { return a.foo; } f(34);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
  // Verify that the monomorphic map is the one we expect.
  Map number_map = ReadOnlyRoots(heap).heap_number_map();
  CHECK_EQ(number_map, nexus.GetFirstMap());

  // Now go polymorphic on o.
  CompileRun("f(o)");
  CHECK_EQ(POLYMORPHIC, nexus.ic_state());

  MapHandles maps;
  nexus.ExtractMaps(&maps);
  CHECK_EQ(2, maps.size());

  // One of the maps should be the o map.
  v8::MaybeLocal<v8::Value> v8_o =
      CcTest::global()->Get(context.local(), v8_str("o"));
  Handle<JSObject> o =
      Handle<JSObject>::cast(v8::Utils::OpenHandle(*v8_o.ToLocalChecked()));
  bool number_map_found = false;
  bool o_map_found = false;
  for (Handle<Map> current : maps) {
    if (*current == number_map)
      number_map_found = true;
    else if (*current == o->map())
      o_map_found = true;
  }
  CHECK(number_map_found && o_map_found);

  // The degree of polymorphism doesn't change.
  CompileRun("f(100)");
  CHECK_EQ(POLYMORPHIC, nexus.ic_state());
  MapHandles maps2;
  nexus.ExtractMaps(&maps2);
  CHECK_EQ(2, maps2.size());
}


TEST(ReferenceContextAllocatesNoSlots) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  {
    CompileRun(
        "function testvar(x) {"
        "  y = x;"
        "  y = a;"
        "  return y;"
        "}"
        "%EnsureFeedbackVectorForFunction(testvar);"
        "a = 3;"
        "testvar({});");

    Handle<JSFunction> f = GetFunction("testvar");

    // There should be two LOAD_ICs, one for a and one for y at the end.
    Handle<FeedbackVector> feedback_vector =
        handle(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(3, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kStoreGlobalSloppy);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 2, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
  }

  {
    CompileRun(
        "function testprop(x) {"
        "  'use strict';"
        "  x.blue = a;"
        "}"
        "%EnsureFeedbackVectorForFunction(testprop);"
        "testprop({ blue: 3 });");

    Handle<JSFunction> f = GetFunction("testprop");

    // There should be one LOAD_IC, for the load of a.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(2, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kStoreNamedStrict);
  }

  {
    CompileRun(
        "function testpropfunc(x) {"
        "  x().blue = a;"
        "  return x().blue;"
        "}"
        "%EnsureFeedbackVectorForFunction(testpropfunc);"
        "function makeresult() { return { blue: 3 }; }"
        "testpropfunc(makeresult);");

    Handle<JSFunction> f = GetFunction("testpropfunc");

    // There should be 1 LOAD_GLOBAL_IC to load x (in both cases), 2 CALL_ICs
    // to call x and a LOAD_IC to load blue.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(5, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kCall);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 2, FeedbackSlotKind::kStoreNamedSloppy);
    CHECK_SLOT_KIND(helper, 3, FeedbackSlotKind::kCall);
    CHECK_SLOT_KIND(helper, 4, FeedbackSlotKind::kLoadProperty);
  }

  {
    CompileRun(
        "function testkeyedprop(x) {"
        "  x[0] = a;"
        "  return x[0];"
        "}"
        "%EnsureFeedbackVectorForFunction(testkeyedprop);"
        "testkeyedprop([0, 1, 2]);");

    Handle<JSFunction> f = GetFunction("testkeyedprop");

    // There should be 1 LOAD_GLOBAL_ICs for the load of a, and one
    // KEYED_LOAD_IC for the load of x[0] in the return statement.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(3, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kStoreKeyedSloppy);
    CHECK_SLOT_KIND(helper, 2, FeedbackSlotKind::kLoadKeyed);
  }

  {
    CompileRun(
        "function testkeyedprop(x) {"
        "  'use strict';"
        "  x[0] = a;"
        "  return x[0];"
        "}"
        "%EnsureFeedbackVectorForFunction(testkeyedprop);"
        "testkeyedprop([0, 1, 2]);");

    Handle<JSFunction> f = GetFunction("testkeyedprop");

    // There should be 1 LOAD_GLOBAL_ICs for the load of a, and one
    // KEYED_LOAD_IC for the load of x[0] in the return statement.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(3, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kStoreKeyedStrict);
    CHECK_SLOT_KIND(helper, 2, FeedbackSlotKind::kLoadKeyed);
  }

  {
    CompileRun(
        "function testcompound(x) {"
        "  'use strict';"
        "  x.old = x.young = x.in_between = a;"
        "  return x.old + x.young;"
        "}"
        "%EnsureFeedbackVectorForFunction(testcompound);"
        "testcompound({ old: 3, young: 3, in_between: 3 });");

    Handle<JSFunction> f = GetFunction("testcompound");

    // There should be 1 LOAD_GLOBAL_IC for load of a and 2 LOAD_ICs, for load
    // of x.old and x.young.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(7, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLoadGlobalNotInsideTypeof);
    CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kStoreNamedStrict);
    CHECK_SLOT_KIND(helper, 2, FeedbackSlotKind::kStoreNamedStrict);
    CHECK_SLOT_KIND(helper, 3, FeedbackSlotKind::kStoreNamedStrict);
    CHECK_SLOT_KIND(helper, 4, FeedbackSlotKind::kBinaryOp);
    CHECK_SLOT_KIND(helper, 5, FeedbackSlotKind::kLoadProperty);
    CHECK_SLOT_KIND(helper, 6, FeedbackSlotKind::kLoadProperty);
  }
}


TEST(VectorStoreICBasic) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());

  CompileRun(
      "function f(a) {"
      "  a.foo = 5;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "var a = { foo: 3 };"
      "f(a);"
      "f(a);"
      "f(a);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC slot.
  Handle<FeedbackVector> feedback_vector(f->feedback_vector(), f->GetIsolate());
  FeedbackVectorHelper helper(feedback_vector);
  CHECK_EQ(1, helper.slot_count());
  FeedbackSlot slot(0);
  FeedbackNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
}

TEST(StoreOwnIC) {
  if (!i::FLAG_use_ic) return;
  if (i::FLAG_always_opt) return;
  FLAG_allow_natives_syntax = true;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());

  CompileRun(
      "function f(v) {"
      "  return {a: 0, b: v, c: 0};"
      "}"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(1);"
      "f(2);"
      "f(3);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC slot.
  Handle<FeedbackVector> feedback_vector(f->feedback_vector(), f->GetIsolate());
  FeedbackVectorHelper helper(feedback_vector);
  CHECK_EQ(2, helper.slot_count());
  CHECK_SLOT_KIND(helper, 0, FeedbackSlotKind::kLiteral);
  CHECK_SLOT_KIND(helper, 1, FeedbackSlotKind::kStoreOwnNamed);
  FeedbackNexus nexus(feedback_vector, helper.slot(1));
  CHECK_EQ(MONOMORPHIC, nexus.ic_state());
}

}  // namespace

}  // namespace internal
}  // namespace v8

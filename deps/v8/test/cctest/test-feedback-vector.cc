// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/api.h"
#include "src/debug/debug.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/global-handles.h"
#include "src/macro-assembler.h"
#include "src/objects.h"
#include "test/cctest/test-feedback-vector.h"

using namespace v8::internal;

namespace {

#define CHECK_SLOT_KIND(helper, index, expected_kind) \
  CHECK_EQ(expected_kind, helper.vector()->GetKind(helper.slot(index)));


static Handle<JSFunction> GetFunction(const char* name) {
  v8::MaybeLocal<v8::Value> v8_f = CcTest::global()->Get(
      v8::Isolate::GetCurrent()->GetCurrentContext(), v8_str(name));
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

  // Empty vectors are the empty fixed array.
  StaticFeedbackVectorSpec empty;
  Handle<FeedbackVector> vector = NewFeedbackVector(isolate, &empty);
  CHECK(Handle<FixedArray>::cast(vector).is_identical_to(
      factory->empty_feedback_vector()));
  // Which can nonetheless be queried.
  CHECK(vector->is_empty());

  {
    FeedbackVectorSpec one_slot(&zone);
    one_slot.AddGeneralSlot();
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
      spec.AddGeneralSlot();
    }
    for (int i = 0; i < 5; i++) {
      spec.AddCallICSlot();
    }
    vector = NewFeedbackVector(isolate, &spec);
    FeedbackVectorHelper helper(vector);
    CHECK_EQ(8, helper.slot_count());

    int index = vector->GetIndex(helper.slot(0));

    CHECK_EQ(FeedbackVector::kReservedIndexCount, index);
    CHECK_EQ(helper.slot(0), vector->ToSlot(index));

    index = vector->GetIndex(helper.slot(3));
    CHECK_EQ(FeedbackVector::kReservedIndexCount + 3, index);
    CHECK_EQ(helper.slot(3), vector->ToSlot(index));

    index = vector->GetIndex(helper.slot(7));
    CHECK_EQ(
        FeedbackVector::kReservedIndexCount + 3 +
            4 * FeedbackMetadata::GetSlotSize(FeedbackVectorSlotKind::CALL_IC),
        index);
    CHECK_EQ(helper.slot(7), vector->ToSlot(index));

    CHECK_EQ(
        FeedbackVector::kReservedIndexCount + 3 +
            5 * FeedbackMetadata::GetSlotSize(FeedbackVectorSlotKind::CALL_IC),
        vector->length());
  }

  {
    FeedbackVectorSpec spec(&zone);
    spec.AddGeneralSlot();
    spec.AddCreateClosureSlot(5);
    spec.AddGeneralSlot();
    vector = NewFeedbackVector(isolate, &spec);
    FeedbackVectorHelper helper(vector);
    CHECK_EQ(1, FeedbackMetadata::GetSlotSize(
                    FeedbackVectorSlotKind::CREATE_CLOSURE));
    FeedbackVectorSlot slot = helper.slot(1);
    FixedArray* array = FixedArray::cast(vector->Get(slot));
    CHECK_EQ(array, *factory->empty_literals_array());
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
        spec.AddGeneralSlot();
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
  vector->Set(FeedbackVectorSlot(0), *vector);

  // Verify the metadata is correctly set up from the spec.
  for (int i = 0; i < 40; i++) {
    FeedbackVectorSlotKind kind = vector->GetKind(helper.slot(i));
    switch (i % 4) {
      case 0:
        CHECK_EQ(FeedbackVectorSlotKind::GENERAL, kind);
        break;
      case 1:
        CHECK_EQ(FeedbackVectorSlotKind::CALL_IC, kind);
        break;
      case 2:
        CHECK_EQ(FeedbackVectorSlotKind::LOAD_IC, kind);
        break;
      case 3:
        CHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC, kind);
        break;
    }
  }
}


TEST(VectorSlotClearing) {
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Zone zone(isolate->allocator(), ZONE_NAME);

  // We only test clearing FeedbackVectorSlots, not FeedbackVectorSlots.
  // The reason is that FeedbackVectorSlots need a full code environment
  // to fully test (See VectorICProfilerStatistics test below).
  FeedbackVectorSpec spec(&zone);
  for (int i = 0; i < 5; i++) {
    spec.AddGeneralSlot();
  }
  Handle<FeedbackVector> vector = NewFeedbackVector(isolate, &spec);
  FeedbackVectorHelper helper(vector);

  // Fill with information
  vector->Set(helper.slot(0), Smi::FromInt(1));
  Handle<WeakCell> cell = factory->NewWeakCell(factory->fixed_array_map());
  vector->Set(helper.slot(1), *cell);
  Handle<AllocationSite> site = factory->NewAllocationSite();
  vector->Set(helper.slot(2), *site);

  // GC time clearing leaves slots alone.
  vector->ClearSlotsAtGCTime(NULL);
  Object* obj = vector->Get(helper.slot(1));
  CHECK(obj->IsWeakCell() && !WeakCell::cast(obj)->cleared());

  vector->ClearSlots(NULL);

  // The feedback vector slots are cleared. AllocationSites are still granted
  // an exemption from clearing, as are smis.
  CHECK_EQ(Smi::FromInt(1), vector->Get(helper.slot(0)));
  CHECK_EQ(*FeedbackVector::UninitializedSentinel(isolate),
           vector->Get(helper.slot(1)));
  CHECK(vector->Get(helper.slot(2))->IsAllocationSite());
}


TEST(VectorCallICStates) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorSlot slot(0);
  CallICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  // CallIC doesn't return map feedback.
  CHECK(!nexus.FindFirstMap());

  CompileRun("f(function() { return 16; })");
  CHECK_EQ(GENERIC, nexus.StateFromFeedback());

  // After a collection, state should remain GENERIC.
  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  CHECK_EQ(GENERIC, nexus.StateFromFeedback());
}

TEST(VectorCallFeedbackForArray) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "function f(a) { a(); } f(Array);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorSlot slot(0);
  CallICNexus nexus(feedback_vector, slot);

  // A call to Array is special, it contains an AllocationSite as feedback.
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  CHECK(nexus.GetFeedback()->IsAllocationSite());

  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  // It should stay monomorphic even after a GC.
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
}

TEST(VectorCallCounts) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function foo() { return 17; }"
      "function f(a) { a(); } f(foo);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorSlot slot(0);
  CallICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());

  CompileRun("f(foo); f(foo);");
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  CHECK_EQ(3, nexus.ExtractCallCount());

  // Send the IC megamorphic, but we should still have incrementing counts.
  CompileRun("f(function() { return 12; });");
  CHECK_EQ(GENERIC, nexus.StateFromFeedback());
  CHECK_EQ(4, nexus.ExtractCallCount());
}

TEST(VectorConstructCounts) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "function Foo() {}"
      "function f(a) { new a(); } f(Foo);");
  Handle<JSFunction> f = GetFunction("f");
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);

  FeedbackVectorSlot slot(0);
  CallICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());

  CHECK(feedback_vector->Get(slot)->IsWeakCell());

  CompileRun("f(Foo); f(Foo);");
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  CHECK_EQ(3, nexus.ExtractCallCount());

  // Send the IC megamorphic, but we should still have incrementing counts.
  CompileRun("f(function() {});");
  CHECK_EQ(GENERIC, nexus.StateFromFeedback());
  CHECK_EQ(4, nexus.ExtractCallCount());
}

TEST(VectorLoadICStates) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "var o = { foo: 3 };"
      "function f(a) { return a.foo; } f(o);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorSlot slot(0);
  LoadICNexus nexus(feedback_vector, slot);
  CHECK_EQ(PREMONOMORPHIC, nexus.StateFromFeedback());

  CompileRun("f(o)");
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  // Verify that the monomorphic map is the one we expect.
  v8::MaybeLocal<v8::Value> v8_o =
      CcTest::global()->Get(context.local(), v8_str("o"));
  Handle<JSObject> o =
      Handle<JSObject>::cast(v8::Utils::OpenHandle(*v8_o.ToLocalChecked()));
  CHECK_EQ(o->map(), nexus.FindFirstMap());

  // Now go polymorphic.
  CompileRun("f({ blarg: 3, foo: 2 })");
  CHECK_EQ(POLYMORPHIC, nexus.StateFromFeedback());

  CompileRun(
      "delete o.foo;"
      "f(o)");
  CHECK_EQ(POLYMORPHIC, nexus.StateFromFeedback());

  CompileRun("f({ blarg: 3, torino: 10, foo: 2 })");
  CHECK_EQ(POLYMORPHIC, nexus.StateFromFeedback());
  MapHandleList maps;
  nexus.FindAllMaps(&maps);
  CHECK_EQ(4, maps.length());

  // Finally driven megamorphic.
  CompileRun("f({ blarg: 3, gran: 3, torino: 10, foo: 2 })");
  CHECK_EQ(MEGAMORPHIC, nexus.StateFromFeedback());
  CHECK(!nexus.FindFirstMap());

  // After a collection, state should not be reset to PREMONOMORPHIC.
  CcTest::CollectAllGarbage(i::Heap::kFinalizeIncrementalMarkingMask);
  CHECK_EQ(MEGAMORPHIC, nexus.StateFromFeedback());
}


TEST(VectorLoadICSlotSharing) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();

  // Function f has 3 LoadICs, one for each o, but the ICs share the same
  // feedback vector IC slot.
  CompileRun(
      "o = 10;"
      "function f() {"
      "  var x = o || 10;"
      "  return o , x , o;"
      "}"
      "f();");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC slot.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorHelper helper(feedback_vector);
  CHECK_EQ(1, helper.slot_count());
  FeedbackVectorSlot slot(0);
  LoadGlobalICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
}


TEST(VectorLoadICOnSmi) {
  if (i::FLAG_always_opt) return;
  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  // Make sure function f has a call that uses a type feedback slot.
  CompileRun(
      "var o = { foo: 3 };"
      "function f(a) { return a.foo; } f(o);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC.
  Handle<FeedbackVector> feedback_vector =
      Handle<FeedbackVector>(f->feedback_vector(), isolate);
  FeedbackVectorSlot slot(0);
  LoadICNexus nexus(feedback_vector, slot);
  CHECK_EQ(PREMONOMORPHIC, nexus.StateFromFeedback());

  CompileRun("f(34)");
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
  // Verify that the monomorphic map is the one we expect.
  Map* number_map = heap->heap_number_map();
  CHECK_EQ(number_map, nexus.FindFirstMap());

  // Now go polymorphic on o.
  CompileRun("f(o)");
  CHECK_EQ(POLYMORPHIC, nexus.StateFromFeedback());

  MapHandleList maps;
  nexus.FindAllMaps(&maps);
  CHECK_EQ(2, maps.length());

  // One of the maps should be the o map.
  v8::MaybeLocal<v8::Value> v8_o =
      CcTest::global()->Get(context.local(), v8_str("o"));
  Handle<JSObject> o =
      Handle<JSObject>::cast(v8::Utils::OpenHandle(*v8_o.ToLocalChecked()));
  bool number_map_found = false;
  bool o_map_found = false;
  for (int i = 0; i < maps.length(); i++) {
    Handle<Map> current = maps[i];
    if (*current == number_map)
      number_map_found = true;
    else if (*current == o->map())
      o_map_found = true;
  }
  CHECK(number_map_found && o_map_found);

  // The degree of polymorphism doesn't change.
  CompileRun("f(100)");
  CHECK_EQ(POLYMORPHIC, nexus.StateFromFeedback());
  MapHandleList maps2;
  nexus.FindAllMaps(&maps2);
  CHECK_EQ(2, maps2.length());
}


TEST(ReferenceContextAllocatesNoSlots) {
  if (i::FLAG_always_opt) return;
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
        "a = 3;"
        "testvar({});");

    Handle<JSFunction> f = GetFunction("testvar");

    // There should be two LOAD_ICs, one for a and one for y at the end.
    Handle<FeedbackVector> feedback_vector =
        handle(f->feedback_vector(), isolate);
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(4, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 1, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
    CHECK_SLOT_KIND(helper, 2, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 3, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
  }

  {
    CompileRun(
        "function testprop(x) {"
        "  x.blue = a;"
        "}"
        "testprop({ blue: 3 });");

    Handle<JSFunction> f = GetFunction("testprop");

    // There should be one LOAD_IC, for the load of a.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector());
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(2, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
    CHECK_SLOT_KIND(helper, 1, FeedbackVectorSlotKind::STORE_IC);
  }

  {
    CompileRun(
        "function testpropfunc(x) {"
        "  x().blue = a;"
        "  return x().blue;"
        "}"
        "function makeresult() { return { blue: 3 }; }"
        "testpropfunc(makeresult);");

    Handle<JSFunction> f = GetFunction("testpropfunc");

    // There should be 1 LOAD_GLOBAL_IC to load x (in both cases), 2 CALL_ICs
    // to call x and a LOAD_IC to load blue.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector());
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(5, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackVectorSlotKind::CALL_IC);
    CHECK_SLOT_KIND(helper, 1, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
    CHECK_SLOT_KIND(helper, 2, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 3, FeedbackVectorSlotKind::CALL_IC);
    CHECK_SLOT_KIND(helper, 4, FeedbackVectorSlotKind::LOAD_IC);
  }

  {
    CompileRun(
        "function testkeyedprop(x) {"
        "  x[0] = a;"
        "  return x[0];"
        "}"
        "testkeyedprop([0, 1, 2]);");

    Handle<JSFunction> f = GetFunction("testkeyedprop");

    // There should be 1 LOAD_GLOBAL_ICs for the load of a, and one
    // KEYED_LOAD_IC for the load of x[0] in the return statement.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector());
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(3, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
    CHECK_SLOT_KIND(helper, 1, FeedbackVectorSlotKind::KEYED_STORE_IC);
    CHECK_SLOT_KIND(helper, 2, FeedbackVectorSlotKind::KEYED_LOAD_IC);
  }

  {
    CompileRun(
        "function testcompound(x) {"
        "  x.old = x.young = x.in_between = a;"
        "  return x.old + x.young;"
        "}"
        "testcompound({ old: 3, young: 3, in_between: 3 });");

    Handle<JSFunction> f = GetFunction("testcompound");

    // There should be 1 LOAD_GLOBAL_IC for load of a and 2 LOAD_ICs, for load
    // of x.old and x.young.
    Handle<FeedbackVector> feedback_vector(f->feedback_vector());
    FeedbackVectorHelper helper(feedback_vector);
    CHECK_EQ(7, helper.slot_count());
    CHECK_SLOT_KIND(helper, 0, FeedbackVectorSlotKind::LOAD_GLOBAL_IC);
    CHECK_SLOT_KIND(helper, 1, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 2, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 3, FeedbackVectorSlotKind::STORE_IC);
    CHECK_SLOT_KIND(helper, 4, FeedbackVectorSlotKind::LOAD_IC);
    CHECK_SLOT_KIND(helper, 5, FeedbackVectorSlotKind::LOAD_IC);
    CHECK_SLOT_KIND(helper, 6, FeedbackVectorSlotKind::INTERPRETER_BINARYOP_IC);
  }
}


TEST(VectorStoreICBasic) {
  if (i::FLAG_always_opt) return;

  CcTest::InitializeVM();
  LocalContext context;
  v8::HandleScope scope(context->GetIsolate());

  CompileRun(
      "function f(a) {"
      "  a.foo = 5;"
      "}"
      "var a = { foo: 3 };"
      "f(a);"
      "f(a);"
      "f(a);");
  Handle<JSFunction> f = GetFunction("f");
  // There should be one IC slot.
  Handle<FeedbackVector> feedback_vector(f->feedback_vector());
  FeedbackVectorHelper helper(feedback_vector);
  CHECK_EQ(1, helper.slot_count());
  FeedbackVectorSlot slot(0);
  StoreICNexus nexus(feedback_vector, slot);
  CHECK_EQ(MONOMORPHIC, nexus.StateFromFeedback());
}

}  // namespace

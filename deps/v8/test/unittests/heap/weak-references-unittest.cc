// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/ic/handler-configuration.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/smi.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace heap {

using WeakReferencesTest = TestWithHeapInternalsAndContext;

namespace {

Handle<LoadHandler> CreateLoadHandlerForTest(
    Factory* factory, AllocationType allocation = AllocationType::kYoung) {
  Handle<LoadHandler> result = factory->NewLoadHandler(1, allocation);
  result->set_smi_handler(Smi::zero());
  result->set_validity_cell(Smi::zero());
  result->set_data1(Smi::zero());
  return result;
}

}  // namespace

TEST_F(WeakReferencesTest, WeakReferencesBasic) {
  ManualGCScope manual_gc_scope(i_isolate());
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  IndirectHandle<LoadHandler> lh = CreateLoadHandlerForTest(factory());

  if (!v8_flags.single_generation) CHECK(HeapLayout::InYoungGeneration(*lh));

  Tagged<MaybeObject> code_object = lh->data1();
  CHECK(IsSmi(code_object));
  InvokeMajorGC();
  CHECK(!HeapLayout::InYoungGeneration(*lh));
  CHECK_EQ(code_object, lh->data1());

  {
    HandleScope inner_scope(i_isolate());

    // Create a new Code.
    Assembler assm(i_isolate()->allocator(), AssemblerOptions{});
    assm.nop();  // supported on all architectures
    CodeDesc desc;
    assm.GetCode(i_isolate(), &desc);
    IndirectHandle<Code> code =
        Factory::CodeBuilder(i_isolate(), desc, CodeKind::FOR_TESTING).Build();
    CHECK(IsCode(*code));

    // We cannot store the Code object itself into the tagged field as it will
    // be located outside of the main pointer compression cage when the sandbox
    // is enabled. So instead we use the Code's wrapper object.
    lh->set_data1(MakeWeak(code->wrapper()));
    Tagged<HeapObject> code_wrapper_heap_object;
    CHECK(lh->data1().GetHeapObjectIfWeak(&code_wrapper_heap_object));
    CHECK_EQ(code->wrapper(), code_wrapper_heap_object);

    InvokeMajorGC();

    CHECK(lh->data1().GetHeapObjectIfWeak(&code_wrapper_heap_object));
    CHECK_EQ(code->wrapper(), code_wrapper_heap_object);
  }  // code will go out of scope.

  InvokeMajorGC();
  CHECK(lh->data1().IsCleared());
}

TEST_F(WeakReferencesTest, WeakReferencesOldToOld) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an old space object.
  ManualGCScope manual_gc_scope(i_isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  DirectHandle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory(), AllocationType::kOld);
  CHECK(heap()->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  DirectHandle<FixedArray> fixed_array =
      factory()->NewFixedArray(1, AllocationType::kOld);
  CHECK(heap()->InOldSpace(*fixed_array));
  lh->set_data1(MakeWeak(*fixed_array));

  NormalPage* page_before_gc = NormalPage::FromHeapObject(*fixed_array);
  ForceEvacuationCandidate(page_before_gc);
  InvokeMajorGC();
  CHECK(heap()->InOldSpace(*fixed_array));

  Tagged<HeapObject> heap_object;
  CHECK(lh->data1().GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST_F(WeakReferencesTest, WeakReferencesOldToNew) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object.
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(i_isolate());

  DirectHandle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory(), AllocationType::kOld);
  CHECK(heap()->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  DirectHandle<FixedArray> fixed_array = factory()->NewFixedArray(1);
  CHECK(HeapLayout::InYoungGeneration(*fixed_array));
  lh->set_data1(MakeWeak(*fixed_array));

  InvokeMajorGC();

  Tagged<HeapObject> heap_object;
  CHECK(lh->data1().GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST_F(WeakReferencesTest, WeakReferencesOldToNewScavenged) {
  if (v8_flags.single_generation) return;
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object, which is then scavenged.
  ManualGCScope manual_gc_scope(i_isolate());

  DirectHandle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory(), AllocationType::kOld);
  CHECK(heap()->InOldSpace(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  DirectHandle<FixedArray> fixed_array = factory()->NewFixedArray(1);
  CHECK(HeapLayout::InYoungGeneration(*fixed_array));
  lh->set_data1(MakeWeak(*fixed_array));

  InvokeMinorGC();

  Tagged<HeapObject> heap_object;
  CHECK(lh->data1().GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST_F(WeakReferencesTest, WeakReferencesOldToCleared) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and is cleared.
  ManualGCScope manual_gc_scope(i_isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  DirectHandle<LoadHandler> lh =
      CreateLoadHandlerForTest(factory(), AllocationType::kOld);
  CHECK(heap()->InOldSpace(*lh));
  lh->set_data1(kClearedWeakValue);

  InvokeMajorGC();
  CHECK(lh->data1().IsCleared());
}

TEST_F(WeakReferencesTest, ObjectWithWeakFieldDies) {
  if (!v8_flags.incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope(i_isolate());

  {
    HandleScope outer_scope(i_isolate());
    DirectHandle<LoadHandler> lh = CreateLoadHandlerForTest(factory());
    CHECK(IsNewObjectInCorrectGeneration(*lh));
    {
      HandleScope inner_scope(i_isolate());
      // Create a new FixedArray which the LoadHandler will point to.
      DirectHandle<FixedArray> fixed_array = factory()->NewFixedArray(1);
      CHECK(IsNewObjectInCorrectGeneration(*fixed_array));
      lh->set_data1(MakeWeak(*fixed_array));
      // inner_scope will go out of scope, so when marking the next time,
      // *fixed_array will stay white.
    }

    // Do marking steps; this will store *lh into the list for later processing
    // (since it points to a white object).
    SimulateIncrementalMarking(true);
  }  // outer_scope goes out of scope

  // lh will die
  InvokeMinorGC();

  // This used to crash when processing the dead weak reference.
  InvokeMajorGC();
}

TEST_F(WeakReferencesTest, ObjectWithWeakReferencePromoted) {
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(i_isolate());

  DirectHandle<LoadHandler> lh = CreateLoadHandlerForTest(factory());
  CHECK(HeapLayout::InYoungGeneration(*lh));

  // Create a new FixedArray which the LoadHandler will point to.
  DirectHandle<FixedArray> fixed_array = factory()->NewFixedArray(1);
  CHECK(HeapLayout::InYoungGeneration(*fixed_array));
  lh->set_data1(MakeWeak(*fixed_array));

  EmptyNewSpaceUsingGC();
  CHECK(heap()->InOldSpace(*lh));
  CHECK(heap()->InOldSpace(*fixed_array));

  Tagged<HeapObject> heap_object;
  CHECK(lh->data1().GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST_F(WeakReferencesTest, ObjectWithClearedWeakReferencePromoted) {
  if (v8_flags.single_generation || v8_flags.stress_incremental_marking) return;
  ManualGCScope manual_gc_scope(i_isolate());

  DirectHandle<LoadHandler> lh = CreateLoadHandlerForTest(factory());
  CHECK(HeapLayout::InYoungGeneration(*lh));

  lh->set_data1(kClearedWeakValue);

  EmptyNewSpaceUsingGC();
  CHECK(heap()->InOldSpace(*lh));
  CHECK(lh->data1().IsCleared());

  InvokeMajorGC();
  CHECK(lh->data1().IsCleared());
}

TEST_F(WeakReferencesTest, WeakReferenceWriteBarrier) {
  if (!v8_flags.incremental_marking) {
    return;
  }

  ManualGCScope manual_gc_scope(i_isolate());

  DirectHandle<LoadHandler> lh = CreateLoadHandlerForTest(factory());
  CHECK(IsNewObjectInCorrectGeneration(*lh));

  v8::Global<Value> global_lh(v8_isolate(), Utils::ToLocal(lh));

  {
    HandleScope inner_scope(i_isolate());

    // Create a new FixedArray which the LoadHandler will point to.
    DirectHandle<FixedArray> fixed_array1 = factory()->NewFixedArray(1);
    CHECK(IsNewObjectInCorrectGeneration(*fixed_array1));
    lh->set_data1(MakeWeak(*fixed_array1));

    SimulateIncrementalMarking(true);

    DirectHandle<FixedArray> fixed_array2 = factory()->NewFixedArray(1);
    CHECK(IsNewObjectInCorrectGeneration(*fixed_array2));
    // This write will trigger the write barrier.
    lh->set_data1(MakeWeak(*fixed_array2));
  }

  InvokeMajorGC();

  // Check that the write barrier treated the weak reference as strong.
  CHECK(lh->data1().IsWeak());
}

TEST_F(WeakReferencesTest, EmptyWeakArray) {
  DirectHandle<WeakFixedArray> array = factory()->empty_weak_fixed_array();
  CHECK(IsWeakFixedArray(*array));
  CHECK(!IsFixedArray(*array));
  CHECK_EQ(array->length().value(), 0u);
}

TEST_F(WeakReferencesTest, WeakArraysBasic) {
  if (v8_flags.single_generation) return;

  ManualGCScope manual_gc_scope(i_isolate());
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  const uint32_t length = 4;
  IndirectHandle<WeakFixedArray> array = factory()->NewWeakFixedArray(length);
  CHECK(IsWeakFixedArray(*array));
  CHECK(!IsFixedArray(*array));
  CHECK_EQ(array->length().value(), length);

  CHECK(HeapLayout::InYoungGeneration(*array));

  for (uint32_t i = 0; i < length; ++i) {
    Tagged<HeapObject> heap_object;
    CHECK(array->get(i).GetHeapObjectIfStrong(&heap_object));
    CHECK_EQ(heap_object, ReadOnlyRoots(heap()).undefined_value());
  }

  IndirectHandle<HeapObject> saved;
  {
    HandleScope inner_scope(i_isolate());
    IndirectHandle<FixedArray> index0 = factory()->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    IndirectHandle<FixedArray> index1 = factory()->NewFixedArray(1);
    index1->set(0, Smi::FromInt(2017));

    IndirectHandle<FixedArray> index2 = factory()->NewFixedArray(1);
    index2->set(0, Smi::FromInt(2018));
    IndirectHandle<FixedArray> index3 = factory()->NewFixedArray(1);
    index3->set(0, Smi::FromInt(2019));

    array->set(0, MakeWeak(*index0));
    array->set(1, MakeWeak(*index1));
    array->set(2, *index2);
    array->set(3, MakeWeak(*index3));
    saved = inner_scope.CloseAndEscape(index1);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  InvokeMinorGC();
  Tagged<HeapObject> heap_object;
  CHECK(array->get(0).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2016);
  CHECK(array->get(1).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2017);
  CHECK(array->get(2).GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2018);
  CHECK(array->get(3).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2019);

  InvokeMajorGC();
  CHECK(heap()->InOldSpace(*array));
  CHECK(array->get(0).IsCleared());
  CHECK(array->get(1).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2017);
  CHECK(array->get(2).GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2018);
  CHECK(array->get(3).IsCleared());
}

TEST_F(WeakReferencesTest, WeakArrayListBasic) {
  if (v8_flags.single_generation) return;

  ManualGCScope manual_gc_scope(i_isolate());
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  Handle<WeakArrayList> array(ReadOnlyRoots(heap()).empty_weak_array_list(),
                              i_isolate());
  CHECK(IsWeakArrayList(*array));
  CHECK(!IsFixedArray(*array));
  CHECK(!IsWeakFixedArray(*array));
  CHECK_EQ(array->length().value(), 0u);

  DirectHandle<FixedArray> index2 = factory()->NewFixedArray(1);
  index2->set(0, Smi::FromInt(2017));

  {
    HandleScope inner_scope(i_isolate());
    DirectHandle<FixedArray> index0 = factory()->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    DirectHandle<FixedArray> index4 = factory()->NewFixedArray(1);
    index4->set(0, Smi::FromInt(2018));
    DirectHandle<FixedArray> index6 = factory()->NewFixedArray(1);
    index6->set(0, Smi::FromInt(2019));

    array = WeakArrayList::AddToEnd(i_isolate(), array,
                                    MaybeObjectDirectHandle::Weak(index0));
    array = WeakArrayList::AddToEnd(
        i_isolate(), array,
        MaybeObjectDirectHandle(Smi::FromInt(1), i_isolate()));
    CHECK_EQ(array->length().value(), 2u);

    array = WeakArrayList::AddToEnd(i_isolate(), array,
                                    MaybeObjectDirectHandle::Weak(index2));
    array = WeakArrayList::AddToEnd(
        i_isolate(), array,
        MaybeObjectDirectHandle(Smi::FromInt(3), i_isolate()));
    CHECK_EQ(array->length().value(), 4u);

    array = WeakArrayList::AddToEnd(i_isolate(), array,
                                    MaybeObjectDirectHandle::Weak(index4));
    array = WeakArrayList::AddToEnd(
        i_isolate(), array,
        MaybeObjectDirectHandle(Smi::FromInt(5), i_isolate()));
    CHECK_EQ(array->length().value(), 6u);

    array = WeakArrayList::AddToEnd(i_isolate(), array,
                                    MaybeObjectDirectHandle::Weak(index6));
    array = WeakArrayList::AddToEnd(
        i_isolate(), array,
        MaybeObjectDirectHandle(Smi::FromInt(7), i_isolate()));
    CHECK_EQ(array->length().value(), 8u);

    CHECK(IsNewObjectInCorrectGeneration(*array));

    CHECK_EQ(array->get(0), MakeWeak(*index0));
    CHECK_EQ(array->get(1).ToSmi().value(), 1);

    CHECK_EQ(array->get(2), MakeWeak(*index2));
    CHECK_EQ(array->get(3).ToSmi().value(), 3);

    CHECK_EQ(array->get(4), MakeWeak(*index4));
    CHECK_EQ(array->get(5).ToSmi().value(), 5);

    CHECK_EQ(array->get(6), MakeWeak(*index6));
    array = inner_scope.CloseAndEscape(array);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  InvokeMinorGC();
  Tagged<HeapObject> heap_object;
  CHECK_EQ(array->length().value(), 8u);
  CHECK(array->get(0).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2016);
  CHECK_EQ(array->get(1).ToSmi().value(), 1);

  CHECK(array->get(2).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2017);
  CHECK_EQ(array->get(3).ToSmi().value(), 3);

  CHECK(array->get(4).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2018);
  CHECK_EQ(array->get(5).ToSmi().value(), 5);

  CHECK(array->get(6).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2019);
  CHECK_EQ(array->get(7).ToSmi().value(), 7);

  InvokeMajorGC();
  CHECK(heap()->InOldSpace(*array));
  CHECK_EQ(array->length().value(), 8u);
  CHECK(array->get(0).IsCleared());
  CHECK_EQ(array->get(1).ToSmi().value(), 1);

  CHECK(array->get(2).GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Cast<Smi>(Cast<FixedArray>(heap_object)->get(0)).value(), 2017);
  CHECK_EQ(array->get(3).ToSmi().value(), 3);

  CHECK(array->get(4).IsCleared());
  CHECK_EQ(array->get(5).ToSmi().value(), 5);

  CHECK(array->get(6).IsCleared());
  CHECK_EQ(array->get(7).ToSmi().value(), 7);
}

TEST_F(WeakReferencesTest, WeakArrayListRemove) {
  ManualGCScope manual_gc_scope(i_isolate());

  Handle<WeakArrayList> array(ReadOnlyRoots(heap()).empty_weak_array_list(),
                              i_isolate());

  DirectHandle<FixedArray> elem0 = factory()->NewFixedArray(1);
  DirectHandle<FixedArray> elem1 = factory()->NewFixedArray(1);
  DirectHandle<FixedArray> elem2 = factory()->NewFixedArray(1);

  array = WeakArrayList::AddToEnd(i_isolate(), array,
                                  MaybeObjectDirectHandle::Weak(elem0));
  array = WeakArrayList::AddToEnd(i_isolate(), array,
                                  MaybeObjectDirectHandle::Weak(elem1));
  array = WeakArrayList::AddToEnd(i_isolate(), array,
                                  MaybeObjectDirectHandle::Weak(elem2));

  CHECK_EQ(array->length().value(), 3u);
  CHECK_EQ(array->get(0), MakeWeak(*elem0));
  CHECK_EQ(array->get(1), MakeWeak(*elem1));
  CHECK_EQ(array->get(2), MakeWeak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectDirectHandle::Weak(elem1)));

  CHECK_EQ(array->length().value(), 2u);
  CHECK_EQ(array->get(0), MakeWeak(*elem0));
  CHECK_EQ(array->get(1), MakeWeak(*elem2));

  CHECK(!array->RemoveOne(MaybeObjectDirectHandle::Weak(elem1)));

  CHECK_EQ(array->length().value(), 2u);
  CHECK_EQ(array->get(0), MakeWeak(*elem0));
  CHECK_EQ(array->get(1), MakeWeak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectDirectHandle::Weak(elem0)));

  CHECK_EQ(array->length().value(), 1u);
  CHECK_EQ(array->get(0), MakeWeak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectDirectHandle::Weak(elem2)));

  CHECK_EQ(array->length().value(), 0u);
}

TEST_F(WeakReferencesTest, ProtectedWeakFixedArray) {
  ManualGCScope manual_gc_scope(i_isolate());

  IndirectHandle<ProtectedWeakFixedArray> array =
      factory()->NewProtectedWeakFixedArray(5);

  IndirectHandle<TrustedFixedArray> elem1 = factory()->NewTrustedFixedArray(1);
  IndirectHandle<TrustedFixedArray> elem3 = factory()->NewTrustedFixedArray(1);
  array->set(1, MakeWeak(*elem1));
  array->set(3, MakeWeak(*elem3));

  {
    HandleScope inner_scope(i_isolate());
    DirectHandle<TrustedFixedArray> elem0 = factory()->NewTrustedFixedArray(1);
    DirectHandle<TrustedFixedArray> elem2 = factory()->NewTrustedFixedArray(1);
    DirectHandle<TrustedFixedArray> elem4 = factory()->NewTrustedFixedArray(1);
    array->set(0, MakeWeak(*elem0));
    array->set(2, MakeWeak(*elem2));
    array->set(4, MakeWeak(*elem4));
    InvokeMajorGC();
    CHECK_EQ(array->get(0).GetHeapObjectAssumeWeak(), *elem0);
    CHECK_EQ(array->get(1).GetHeapObjectAssumeWeak(), *elem1);
    CHECK_EQ(array->get(2).GetHeapObjectAssumeWeak(), *elem2);
    CHECK_EQ(array->get(3).GetHeapObjectAssumeWeak(), *elem3);
    CHECK_EQ(array->get(4).GetHeapObjectAssumeWeak(), *elem4);
  }

  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
  InvokeMajorGC();
  CHECK(array->get(0).IsCleared());
  CHECK_EQ(array->get(1).GetHeapObjectAssumeWeak(), *elem1);
  CHECK(array->get(2).IsCleared());
  CHECK_EQ(array->get(3).GetHeapObjectAssumeWeak(), *elem3);
  CHECK(array->get(4).IsCleared());
}

TEST_F(WeakReferencesTest, Regress7768) {
  if (!v8_flags.incremental_marking) {
    return;
  }
  FlagScope<bool> allow_natives_syntax(&v8_flags.allow_natives_syntax, true);
  FlagScope<bool> no_turbo_inlining(&v8_flags.turbo_inlining, false);
  ManualGCScope manual_gc_scope(i_isolate());

  // Create an optimized code which will contain a weak reference to another
  // function ("f"). The weak reference is the only reference to the function.
  RunJS(
      "function myfunc(f) { f(); } "
      "%PrepareFunctionForOptimization(myfunc); "
      "(function wrapper() { "
      "   function f() {}; myfunc(f); myfunc(f); "
      "   %OptimizeFunctionOnNextCall(myfunc); myfunc(f); "
      "   %ClearFunctionFeedback(wrapper);"
      "})(); "
      "%ClearFunctionFeedback(myfunc);");

  // Do marking steps; this will store the objects pointed by myfunc for later
  // processing.
  SimulateIncrementalMarking(true);

  // Deoptimize the code; now the pointers inside it will be replaced with
  // undefined, and the weak_objects_in_code is the only place pointing to the
  // function f.
  RunJS("%DeoptimizeFunction(myfunc);");

  // The object pointed to by the weak reference won't be scavenged.
  InvokeMinorGC();

  // Make sure the memory where it's stored is invalidated, so that we'll crash
  // if we try to access it.
  if (!v8_flags.minor_ms) heap()->ReduceNewSpaceSizeForTesting();
  heap()->memory_allocator()->ReleasePooledChunksImmediately();

  // This used to crash when processing the dead weak reference.
  InvokeMajorGC();
}

TEST_F(WeakReferencesTest, PrototypeUsersBasic) {
  Handle<WeakArrayList> array(ReadOnlyRoots(heap()).empty_weak_array_list(),
                              i_isolate());

  // Add some objects into the array.
  int index = -1;
  {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
    CHECK_EQ(array->length().value(), index + 1);
  }
  CHECK_EQ(index, 1);

  int empty_index = index;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index);

  // Even though we have an empty slot, we still add to the end.
  int last_index = index;
  uint32_t old_capacity = array->capacity().value();
  while (!array->IsFull()) {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length().value(), index + 1);
    last_index = index;
  }

  // The next addition will fill the empty slot.
  {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
  }
  CHECK_EQ(index, empty_index);

  // The next addition will make the array grow again.
  {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
    CHECK_EQ(array->length().value(), index + 1);
    last_index = index;
  }
  CHECK_GT(array->capacity().value(), old_capacity);

  // Make multiple slots empty.
  int empty_index1 = 1;
  int empty_index2 = 2;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index1);
  PrototypeUsers::MarkSlotEmpty(*array, empty_index2);

  // Fill the array (still adding to the end)
  old_capacity = array->capacity().value();
  while (!array->IsFull()) {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length().value(), index + 1);
    last_index = index;
  }

  // Make sure we use the empty slots in (reverse) order.
  {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
  }
  CHECK_EQ(index, empty_index2);

  {
    DirectHandle<Map> map = factory()->NewContextfulMapForCurrentContext(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, map, &index);
  }
  CHECK_EQ(index, empty_index1);
}

namespace {

Tagged<HeapObject> saved_heap_object;

void TestCompactCallback(Tagged<HeapObject> value, int old_index,
                         int new_index) {
  saved_heap_object = value;
  CHECK_EQ(old_index, 2);
  CHECK_EQ(new_index, 1);
}

}  // namespace

TEST_F(WeakReferencesTest, PrototypeUsersCompacted) {
  ManualGCScope manual_gc_scope(i_isolate());
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  Handle<WeakArrayList> array(ReadOnlyRoots(heap()).empty_weak_array_list(),
                              i_isolate());

  // Add some objects into the array.
  int index = -1;
  DirectHandle<Map> map_cleared_by_user =
      factory()->NewContextfulMapForCurrentContext(JS_OBJECT_TYPE,
                                                   JSObject::kHeaderSize);
  array = PrototypeUsers::Add(i_isolate(), array, map_cleared_by_user, &index);
  CHECK_EQ(index, 1);
  DirectHandle<Map> live_map = factory()->NewContextfulMapForCurrentContext(
      JS_OBJECT_TYPE, JSObject::kHeaderSize);
  array = PrototypeUsers::Add(i_isolate(), array, live_map, &index);
  CHECK_EQ(index, 2);
  {
    HandleScope inner_scope(i_isolate());
    DirectHandle<Map> soon_dead_map =
        factory()->NewContextfulMapForCurrentContext(JS_OBJECT_TYPE,
                                                     JSObject::kHeaderSize);
    array = PrototypeUsers::Add(i_isolate(), array, soon_dead_map, &index);
    CHECK_EQ(index, 3);

    array = inner_scope.CloseAndEscape(array);
  }

  PrototypeUsers::MarkSlotEmpty(*array, 1);
  InvokeMajorGC();
  CHECK(array->get(3).IsCleared());

  CHECK_EQ(array->length().value(), 3 + PrototypeUsers::kFirstIndex);
  Tagged<WeakArrayList> new_array =
      PrototypeUsers::Compact(array, heap(), TestCompactCallback);
  CHECK_EQ(new_array->length().value(), 1 + PrototypeUsers::kFirstIndex);
  CHECK_EQ(saved_heap_object, *live_map);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

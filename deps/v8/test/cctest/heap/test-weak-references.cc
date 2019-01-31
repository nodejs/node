// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects/smi.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Handle<FeedbackVector> CreateFeedbackVectorForTest(
    v8::Isolate* isolate, Factory* factory,
    PretenureFlag pretenure_flag = NOT_TENURED) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::Local<v8::Script> script =
      v8::Script::Compile(isolate->GetCurrentContext(),
                          v8::String::NewFromUtf8(isolate, "function foo() {}",
                                                  v8::NewStringType::kNormal)
                              .ToLocalChecked())
          .ToLocalChecked();
  Handle<Object> obj = v8::Utils::OpenHandle(*script);
  Handle<SharedFunctionInfo> shared_function =
      Handle<SharedFunctionInfo>(JSFunction::cast(*obj)->shared(), i_isolate);
  Handle<FeedbackVector> fv =
      factory->NewFeedbackVector(shared_function, pretenure_flag);
  return fv;
}

TEST(WeakReferencesBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);

  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(Heap::InNewSpace(*fv));

  MaybeObject code_object = fv->optimized_code_weak_or_smi();
  CHECK(code_object->IsSmi());
  CcTest::CollectAllGarbage();
  CHECK(Heap::InNewSpace(*fv));
  CHECK_EQ(code_object, fv->optimized_code_weak_or_smi());

  {
    HandleScope inner_scope(isolate);

    // Create a new Code.
    Assembler assm(AssemblerOptions{});
    assm.nop();  // supported on all architectures
    CodeDesc desc;
    assm.GetCode(isolate, &desc);
    Handle<Code> code =
        isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
    CHECK(code->IsCode());

    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*code));
    HeapObject code_heap_object;
    CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(
        &code_heap_object));
    CHECK_EQ(*code, code_heap_object);

    CcTest::CollectAllGarbage();

    CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(
        &code_heap_object));
    CHECK_EQ(*code, code_heap_object);
  }  // code will go out of scope.

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());
}

TEST(WeakReferencesOldToOld) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an old space object.
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1, TENURED);
  CHECK(heap->InOldSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  Page* page_before_gc = Page::FromHeapObject(*fixed_array);
  heap::ForceEvacuationCandidate(page_before_gc);
  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNew) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectAllGarbage();

  HeapObject heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToNewScavenged) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and referring to an new space object, which is then scavenged.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);

  HeapObject heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(WeakReferencesOldToCleared) {
  // Like WeakReferencesBasic, but the updated weak slot is in the old space,
  // and is cleared.
  ManualGCScope manual_gc_scope;
  FLAG_manual_evacuation_candidates_selection = true;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory, TENURED);
  CHECK(heap->InOldSpace(*fv));
  fv->set_optimized_code_weak_or_smi(
      HeapObjectReference::ClearedValue(isolate));

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());
}

TEST(ObjectMovesBeforeClearingWeakField) {
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(Heap::InNewSpace(*fv));
  FeedbackVector fv_location = *fv;
  {
    HandleScope inner_scope(isolate);
    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
    CHECK(Heap::InNewSpace(*fixed_array));
    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));
    // inner_scope will go out of scope, so when marking the next time,
    // *fixed_array will stay white.
  }

  // Do marking steps; this will store *fv into the list for later processing
  // (since it points to a white object).
  SimulateIncrementalMarking(heap, true);

  // Scavenger will move *fv.
  CcTest::CollectGarbage(NEW_SPACE);
  FeedbackVector new_fv_location = *fv;
  CHECK_NE(fv_location, new_fv_location);
  CHECK(fv->optimized_code_weak_or_smi()->IsWeak());

  // Now we try to clear *fv.
  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());
}

TEST(ObjectWithWeakFieldDies) {
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  {
    HandleScope outer_scope(isolate);
    Handle<FeedbackVector> fv =
        CreateFeedbackVectorForTest(CcTest::isolate(), factory);
    CHECK(Heap::InNewSpace(*fv));
    {
      HandleScope inner_scope(isolate);
      // Create a new FixedArray which the FeedbackVector will point to.
      Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
      CHECK(Heap::InNewSpace(*fixed_array));
      fv->set_optimized_code_weak_or_smi(
          HeapObjectReference::Weak(*fixed_array));
      // inner_scope will go out of scope, so when marking the next time,
      // *fixed_array will stay white.
    }

    // Do marking steps; this will store *fv into the list for later processing
    // (since it points to a white object).
    SimulateIncrementalMarking(heap, true);
  }  // outer_scope goes out of scope

  // fv will die
  CcTest::CollectGarbage(NEW_SPACE);

  // This used to crash when processing the dead weak reference.
  CcTest::CollectAllGarbage();
}

TEST(ObjectWithWeakReferencePromoted) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(Heap::InNewSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(Heap::InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(heap_object, *fixed_array);
}

TEST(ObjectWithClearedWeakReferencePromoted) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(Heap::InNewSpace(*fv));

  fv->set_optimized_code_weak_or_smi(
      HeapObjectReference::ClearedValue(isolate));

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(Heap::InNewSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsCleared());
}

TEST(WeakReferenceWriteBarrier) {
  if (!FLAG_incremental_marking) {
    return;
  }

  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();

  HandleScope outer_scope(isolate);
  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(Heap::InNewSpace(*fv));

  {
    HandleScope inner_scope(isolate);

    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array1 = factory->NewFixedArray(1);
    CHECK(Heap::InNewSpace(*fixed_array1));
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array1));

    SimulateIncrementalMarking(heap, true);

    Handle<FixedArray> fixed_array2 = factory->NewFixedArray(1);
    CHECK(Heap::InNewSpace(*fixed_array2));
    // This write will trigger the write barrier.
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array2));
  }

  CcTest::CollectAllGarbage();

  // Check that the write barrier treated the weak reference as strong.
  CHECK(fv->optimized_code_weak_or_smi()->IsWeak());
}

TEST(EmptyWeakArray) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope outer_scope(isolate);

  Handle<WeakFixedArray> array = factory->empty_weak_fixed_array();
  CHECK(array->IsWeakFixedArray());
  CHECK(!array->IsFixedArray());
  CHECK_EQ(array->length(), 0);
}

TEST(WeakArraysBasic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  const int length = 4;
  Handle<WeakFixedArray> array = factory->NewWeakFixedArray(length);
  CHECK(array->IsWeakFixedArray());
  CHECK(!array->IsFixedArray());
  CHECK_EQ(array->length(), length);
  CHECK(Heap::InNewSpace(*array));

  for (int i = 0; i < length; ++i) {
    HeapObject heap_object;
    CHECK(array->Get(i)->GetHeapObjectIfStrong(&heap_object));
    CHECK_EQ(heap_object, ReadOnlyRoots(heap).undefined_value());
  }

  Handle<HeapObject> saved;
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> index0 = factory->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    Handle<FixedArray> index1 = factory->NewFixedArray(1);
    index1->set(0, Smi::FromInt(2017));

    Handle<FixedArray> index2 = factory->NewFixedArray(1);
    index2->set(0, Smi::FromInt(2018));
    Handle<FixedArray> index3 = factory->NewFixedArray(1);
    index3->set(0, Smi::FromInt(2019));

    array->Set(0, HeapObjectReference::Weak(*index0));
    array->Set(1, HeapObjectReference::Weak(*index1));
    array->Set(2, HeapObjectReference::Strong(*index2));
    array->Set(3, HeapObjectReference::Weak(*index3));
    saved = inner_scope.CloseAndEscape(index1);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  CcTest::CollectGarbage(NEW_SPACE);
  HeapObject heap_object;
  CHECK(array->Get(0)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2016);
  CHECK(array->Get(1)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK(array->Get(2)->GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK(array->Get(3)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2019);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK(array->Get(0)->IsCleared());
  CHECK(array->Get(1)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK(array->Get(2)->GetHeapObjectIfStrong(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK(array->Get(3)->IsCleared());
}

TEST(WeakArrayListBasic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);
  CHECK(array->IsWeakArrayList());
  CHECK(!array->IsFixedArray());
  CHECK(!array->IsWeakFixedArray());
  CHECK_EQ(array->length(), 0);

  Handle<FixedArray> index2 = factory->NewFixedArray(1);
  index2->set(0, Smi::FromInt(2017));

  Handle<HeapObject> saved;
  {
    HandleScope inner_scope(isolate);
    Handle<FixedArray> index0 = factory->NewFixedArray(1);
    index0->set(0, Smi::FromInt(2016));
    Handle<FixedArray> index4 = factory->NewFixedArray(1);
    index4->set(0, Smi::FromInt(2018));
    Handle<FixedArray> index6 = factory->NewFixedArray(1);
    index6->set(0, Smi::FromInt(2019));

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index0));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(1), isolate));
    CHECK_EQ(array->length(), 2);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index2));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(3), isolate));
    CHECK_EQ(array->length(), 4);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index4));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(5), isolate));
    CHECK_EQ(array->length(), 6);

    array = WeakArrayList::AddToEnd(isolate, array,
                                    MaybeObjectHandle::Weak(index6));
    array = WeakArrayList::AddToEnd(
        isolate, array, MaybeObjectHandle(Smi::FromInt(7), isolate));
    CHECK_EQ(array->length(), 8);

    CHECK(Heap::InNewSpace(*array));

    CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*index0));
    CHECK_EQ(array->Get(1).ToSmi().value(), 1);

    CHECK_EQ(array->Get(2), HeapObjectReference::Weak(*index2));
    CHECK_EQ(array->Get(3).ToSmi().value(), 3);

    CHECK_EQ(array->Get(4), HeapObjectReference::Weak(*index4));
    CHECK_EQ(array->Get(5).ToSmi().value(), 5);

    CHECK_EQ(array->Get(6), HeapObjectReference::Weak(*index6));
    array = inner_scope.CloseAndEscape(array);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  CcTest::CollectGarbage(NEW_SPACE);
  HeapObject heap_object;
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2016);
  CHECK_EQ(array->Get(1).ToSmi().value(), 1);

  CHECK(array->Get(2)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK_EQ(array->Get(3).ToSmi().value(), 3);

  CHECK(array->Get(4)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK_EQ(array->Get(5).ToSmi().value(), 5);

  CHECK(array->Get(6)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2019);
  CHECK_EQ(array->Get(7).ToSmi().value(), 7);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->IsCleared());
  CHECK_EQ(array->Get(1).ToSmi().value(), 1);

  CHECK(array->Get(2)->GetHeapObjectIfWeak(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK_EQ(array->Get(3).ToSmi().value(), 3);

  CHECK(array->Get(4)->IsCleared());
  CHECK_EQ(array->Get(5).ToSmi().value(), 5);

  CHECK(array->Get(6)->IsCleared());
  CHECK_EQ(array->Get(7).ToSmi().value(), 7);
}

TEST(WeakArrayListRemove) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  Handle<FixedArray> elem0 = factory->NewFixedArray(1);
  Handle<FixedArray> elem1 = factory->NewFixedArray(1);
  Handle<FixedArray> elem2 = factory->NewFixedArray(1);

  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem0));
  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem1));
  array =
      WeakArrayList::AddToEnd(isolate, array, MaybeObjectHandle::Weak(elem2));

  CHECK_EQ(array->length(), 3);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem1));
  CHECK_EQ(array->Get(2), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem1)));

  CHECK_EQ(array->length(), 2);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem2));

  CHECK(!array->RemoveOne(MaybeObjectHandle::Weak(elem1)));

  CHECK_EQ(array->length(), 2);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem0));
  CHECK_EQ(array->Get(1), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem0)));

  CHECK_EQ(array->length(), 1);
  CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*elem2));

  CHECK(array->RemoveOne(MaybeObjectHandle::Weak(elem2)));

  CHECK_EQ(array->length(), 0);
}

TEST(Regress7768) {
  i::FLAG_allow_natives_syntax = true;
  i::FLAG_turbo_inlining = false;
  if (!FLAG_incremental_marking) {
    return;
  }
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);
  // Create an optimized code which will contain a weak reference to another
  // function ("f"). The weak reference is the only reference to the function.
  CompileRun(
      "function myfunc(f) { f(); } "
      "(function wrapper() { "
      "   function f() {}; myfunc(f); myfunc(f); "
      "   %OptimizeFunctionOnNextCall(myfunc); myfunc(f); "
      "   %ClearFunctionFeedback(wrapper);"
      "})(); "
      "%ClearFunctionFeedback(myfunc);");

  // Do marking steps; this will store the objects pointed by myfunc for later
  // processing.
  SimulateIncrementalMarking(heap, true);

  // Deoptimize the code; now the pointers inside it will be replaced with
  // undefined, and the weak_objects_in_code is the only place pointing to the
  // function f.
  CompileRun("%DeoptimizeFunction(myfunc);");

  // The object pointed to by the weak reference won't be scavenged.
  CcTest::CollectGarbage(NEW_SPACE);

  // Make sure the memory where it's stored is invalidated, so that we'll crash
  // if we try to access it.
  HeapTester::UncommitFromSpace(heap);

  // This used to crash when processing the dead weak reference.
  CcTest::CollectAllGarbage();
}

TEST(PrototypeUsersBasic) {
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  // Add some objects into the array.
  int index = -1;
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(array->length(), index + 1);
  }
  CHECK_EQ(index, 1);

  int empty_index = index;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index);

  // Even though we have an empty slot, we still add to the end.
  int last_index = index;
  int old_capacity = array->capacity();
  while (!array->IsFull()) {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }

  // The next addition will fill the empty slot.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index);

  // The next addition will make the arrow grow again.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }
  CHECK_GT(array->capacity(), old_capacity);

  // Make multiple slots empty.
  int empty_index1 = 1;
  int empty_index2 = 2;
  PrototypeUsers::MarkSlotEmpty(*array, empty_index1);
  PrototypeUsers::MarkSlotEmpty(*array, empty_index2);

  // Fill the array (still adding to the end)
  old_capacity = array->capacity();
  while (!array->IsFull()) {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
    CHECK_EQ(index, last_index + 1);
    CHECK_EQ(array->length(), index + 1);
    last_index = index;
  }

  // Make sure we use the empty slots in (reverse) order.
  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index2);

  {
    Handle<Map> map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, map, &index);
  }
  CHECK_EQ(index, empty_index1);
}

namespace {

HeapObject saved_heap_object;

static void TestCompactCallback(HeapObject value, int old_index,
                                int new_index) {
  saved_heap_object = value;
  CHECK_EQ(old_index, 2);
  CHECK_EQ(new_index, 1);
}

}  // namespace

TEST(PrototypeUsersCompacted) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(ReadOnlyRoots(heap).empty_weak_array_list(),
                              isolate);

  // Add some objects into the array.
  int index = -1;
  Handle<Map> map_cleared_by_user =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  array = PrototypeUsers::Add(isolate, array, map_cleared_by_user, &index);
  CHECK_EQ(index, 1);
  Handle<Map> live_map = factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  array = PrototypeUsers::Add(isolate, array, live_map, &index);
  CHECK_EQ(index, 2);
  {
    HandleScope inner_scope(isolate);
    Handle<Map> soon_dead_map =
        factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array = PrototypeUsers::Add(isolate, array, soon_dead_map, &index);
    CHECK_EQ(index, 3);

    array = inner_scope.CloseAndEscape(array);
  }

  PrototypeUsers::MarkSlotEmpty(*array, 1);
  CcTest::CollectAllGarbage();
  CHECK(array->Get(3)->IsCleared());

  CHECK_EQ(array->length(), 3 + PrototypeUsers::kFirstIndex);
  WeakArrayList new_array =
      PrototypeUsers::Compact(array, heap, TestCompactCallback);
  CHECK_EQ(new_array->length(), 1 + PrototypeUsers::kFirstIndex);
  CHECK_EQ(saved_heap_object, *live_map);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

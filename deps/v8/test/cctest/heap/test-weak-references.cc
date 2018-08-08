// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

Handle<FeedbackVector> CreateFeedbackVectorForTest(
    v8::Isolate* isolate, Factory* factory,
    PretenureFlag pretenure_flag = NOT_TENURED) {
  v8::Local<v8::Script> script =
      v8::Script::Compile(isolate->GetCurrentContext(),
                          v8::String::NewFromUtf8(isolate, "function foo() {}",
                                                  v8::NewStringType::kNormal)
                              .ToLocalChecked())
          .ToLocalChecked();
  Handle<Object> obj = v8::Utils::OpenHandle(*script);
  Handle<SharedFunctionInfo> shared_function =
      Handle<SharedFunctionInfo>(JSFunction::cast(*obj)->shared());
  Handle<FeedbackVector> fv =
      factory->NewFeedbackVector(shared_function, pretenure_flag);
  return fv;
}

TEST(WeakReferencesBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<FeedbackVector> fv =
      CreateFeedbackVectorForTest(CcTest::isolate(), factory);
  CHECK(heap->InNewSpace(*fv));

  MaybeObject* code_object = fv->optimized_code_weak_or_smi();
  CHECK(code_object->IsSmi());
  CcTest::CollectAllGarbage();
  CHECK(heap->InNewSpace(*fv));
  CHECK_EQ(code_object, fv->optimized_code_weak_or_smi());

  {
    HandleScope inner_scope(isolate);

    // Create a new Code.
    Assembler assm(isolate, nullptr, 0);
    assm.nop();  // supported on all architectures
    CodeDesc desc;
    assm.GetCode(isolate, &desc);
    Handle<Code> code =
        isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
    CHECK(code->IsCode());

    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*code));
    HeapObject* code_heap_object;
    CHECK(
        fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);

    CcTest::CollectAllGarbage();

    CHECK(
        fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&code_heap_object));
    CHECK_EQ(*code, code_heap_object);
  }  // code will go out of scope.

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
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

  Page* page_before_gc = Page::FromAddress(fixed_array->address());
  heap::ForceEvacuationCandidate(page_before_gc);
  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
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
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectAllGarbage();

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
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
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
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
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::ClearedValue());

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
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
  CHECK(heap->InNewSpace(*fv));
  FeedbackVector* fv_location = *fv;
  {
    HandleScope inner_scope(isolate);
    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array));
    fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));
    // inner_scope will go out of scope, so when marking the next time,
    // *fixed_array will stay white.
  }

  // Do marking steps; this will store *fv into the list for later processing
  // (since it points to a white object).
  SimulateIncrementalMarking(heap, true);

  // Scavenger will move *fv.
  CcTest::CollectGarbage(NEW_SPACE);
  FeedbackVector* new_fv_location = *fv;
  CHECK_NE(fv_location, new_fv_location);
  CHECK(fv->optimized_code_weak_or_smi()->IsWeakHeapObject());

  // Now we try to clear *fv.
  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
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
    CHECK(heap->InNewSpace(*fv));
    {
      HandleScope inner_scope(isolate);
      // Create a new FixedArray which the FeedbackVector will point to.
      Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
      CHECK(heap->InNewSpace(*fixed_array));
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
  CHECK(heap->InNewSpace(*fv));

  // Create a new FixedArray which the FeedbackVector will point to.
  Handle<FixedArray> fixed_array = factory->NewFixedArray(1);
  CHECK(heap->InNewSpace(*fixed_array));
  fv->set_optimized_code_weak_or_smi(HeapObjectReference::Weak(*fixed_array));

  CcTest::CollectGarbage(NEW_SPACE);
  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(heap->InOldSpace(*fixed_array));

  HeapObject* heap_object;
  CHECK(fv->optimized_code_weak_or_smi()->ToWeakHeapObject(&heap_object));
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
  CHECK(heap->InNewSpace(*fv));

  fv->set_optimized_code_weak_or_smi(HeapObjectReference::ClearedValue());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InNewSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());

  CcTest::CollectGarbage(NEW_SPACE);
  CHECK(heap->InOldSpace(*fv));
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());

  CcTest::CollectAllGarbage();
  CHECK(fv->optimized_code_weak_or_smi()->IsClearedWeakHeapObject());
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
  CHECK(heap->InNewSpace(*fv));

  {
    HandleScope inner_scope(isolate);

    // Create a new FixedArray which the FeedbackVector will point to.
    Handle<FixedArray> fixed_array1 = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array1));
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array1));

    SimulateIncrementalMarking(heap, true);

    Handle<FixedArray> fixed_array2 = factory->NewFixedArray(1);
    CHECK(heap->InNewSpace(*fixed_array2));
    // This write will trigger the write barrier.
    fv->set_optimized_code_weak_or_smi(
        HeapObjectReference::Weak(*fixed_array2));
  }

  CcTest::CollectAllGarbage();

  // Check that the write barrier treated the weak reference as strong.
  CHECK(fv->optimized_code_weak_or_smi()->IsWeakHeapObject());
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
  CHECK(heap->InNewSpace(*array));

  for (int i = 0; i < length; ++i) {
    HeapObject* heap_object;
    CHECK(array->Get(i)->ToStrongHeapObject(&heap_object));
    CHECK_EQ(heap_object, heap->undefined_value());
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
  HeapObject* heap_object;
  CHECK(array->Get(0)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2016);
  CHECK(array->Get(1)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK(array->Get(2)->ToStrongHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK(array->Get(3)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2019);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK(array->Get(0)->IsClearedWeakHeapObject());
  CHECK(array->Get(1)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK(array->Get(2)->ToStrongHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK(array->Get(3)->IsClearedWeakHeapObject());
}

TEST(WeakArrayListBasic) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  HandleScope outer_scope(isolate);

  Handle<WeakArrayList> array(heap->empty_weak_array_list());
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

    array = WeakArrayList::Add(array, index0, Smi::FromInt(1));
    CHECK_EQ(array->length(), 2);

    array = WeakArrayList::Add(array, index2, Smi::FromInt(3));
    CHECK_EQ(array->length(), 4);

    array = WeakArrayList::Add(array, index4, Smi::FromInt(5));
    CHECK_EQ(array->length(), 6);

    array = WeakArrayList::Add(array, index6, Smi::FromInt(7));
    CHECK_EQ(array->length(), 8);

    CHECK(heap->InNewSpace(*array));

    CHECK_EQ(array->Get(0), HeapObjectReference::Weak(*index0));
    CHECK_EQ(Smi::ToInt(array->Get(1)->ToSmi()), 1);

    CHECK_EQ(array->Get(2), HeapObjectReference::Weak(*index2));
    CHECK_EQ(Smi::ToInt(array->Get(3)->ToSmi()), 3);

    CHECK_EQ(array->Get(4), HeapObjectReference::Weak(*index4));
    CHECK_EQ(Smi::ToInt(array->Get(5)->ToSmi()), 5);

    CHECK_EQ(array->Get(6), HeapObjectReference::Weak(*index6));
    array = inner_scope.CloseAndEscape(array);
  }  // inner_scope goes out of scope.

  // The references are only cleared by the mark-compact (scavenger treats weak
  // references as strong). Thus we need to GC until the array reaches old
  // space.

  // TODO(marja): update this when/if we do handle weak references in the new
  // space.
  CcTest::CollectGarbage(NEW_SPACE);
  HeapObject* heap_object;
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2016);
  CHECK_EQ(Smi::ToInt(array->Get(1)->ToSmi()), 1);

  CHECK(array->Get(2)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK_EQ(Smi::ToInt(array->Get(3)->ToSmi()), 3);

  CHECK(array->Get(4)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2018);
  CHECK_EQ(Smi::ToInt(array->Get(5)->ToSmi()), 5);

  CHECK(array->Get(6)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2019);
  CHECK_EQ(Smi::ToInt(array->Get(7)->ToSmi()), 7);

  CcTest::CollectAllGarbage();
  CHECK(heap->InOldSpace(*array));
  CHECK_EQ(array->length(), 8);
  CHECK(array->Get(0)->IsClearedWeakHeapObject());
  CHECK_EQ(Smi::ToInt(array->Get(1)->ToSmi()), 1);

  CHECK(array->Get(2)->ToWeakHeapObject(&heap_object));
  CHECK_EQ(Smi::cast(FixedArray::cast(heap_object)->get(0))->value(), 2017);
  CHECK_EQ(Smi::ToInt(array->Get(3)->ToSmi()), 3);

  CHECK(array->Get(4)->IsClearedWeakHeapObject());
  CHECK_EQ(Smi::ToInt(array->Get(5)->ToSmi()), 5);

  CHECK(array->Get(6)->IsClearedWeakHeapObject());
  CHECK_EQ(Smi::ToInt(array->Get(7)->ToSmi()), 7);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include <vector>

#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/live-object-range-inl.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/mutable-page-inl.h"
#include "src/heap/normal-page-inl.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using MarkCompactTest = TestWithHeapInternalsAndContext;

TEST_F(MarkCompactTest, Promotion) {
  if (v8_flags.single_generation) return;
  Isolate* isolate = i_isolate();
  ManualGCScope manual_gc_scope(isolate);
  v8::HandleScope sc(v8_isolate());
  Heap* heap = isolate->heap();

  SealCurrentObjects();

  int array_length = FixedArrayLenFromSize(kMaxRegularHeapObjectSize);
  DirectHandle<FixedArray> array =
      isolate->factory()->NewFixedArray(array_length);

  // Array should be in the new space.
  EXPECT_TRUE(heap->InSpace(*array, NEW_SPACE));
  InvokeMajorGC();
  InvokeMajorGC();
  EXPECT_TRUE(heap->InSpace(*array, OLD_SPACE));
}

TEST_F(MarkCompactTest, MarkCompactCollector) {
  ManualGCScope manual_gc_scope(i_isolate());
  FlagScope<bool> no_incremental_marking(&v8_flags.incremental_marking, false);
  FlagScope<int> no_retain_maps(&v8_flags.retain_maps_for_n_gc, 0);

  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();

  v8::HandleScope sc(v8_isolate());
  DirectHandle<JSGlobalObject> global(isolate->context()->global_object(),
                                      isolate);

  // call mark-compact when heap is empty
  InvokeMajorGC();

  AllocationResult allocation;
  if (!v8_flags.single_generation) {
    // keep allocating garbage in new space until it fails
    const uint32_t arraysize = 100;
    do {
      allocation =
          AllocateFixedArrayForTest(heap, arraysize, AllocationType::kYoung);
    } while (!allocation.IsFailure());
    InvokeMinorGC();
    AllocateFixedArrayForTest(heap, arraysize, AllocationType::kYoung)
        .ToObjectChecked();
  }

  // keep allocating maps until it fails
  do {
    allocation = AllocateMapForTest(isolate);
  } while (!allocation.IsFailure());
  InvokeMajorGC();
  AllocateMapForTest(isolate).ToObjectChecked();

  {
    HandleScope scope(isolate);
    // allocate a garbage
    DirectHandle<String> func_name =
        factory->InternalizeUtf8String("theFunction");
    DirectHandle<JSFunction> function =
        factory->NewFunctionForTesting(func_name);
    Object::SetProperty(isolate, global, func_name, function).Check();

    factory->NewJSObject(function);
  }

  InvokeMajorGC();

  {
    HandleScope scope(isolate);
    DirectHandle<String> func_name =
        factory->InternalizeUtf8String("theFunction");
    EXPECT_EQ(Just(true),
              JSReceiver::HasOwnProperty(isolate, global, func_name));
    DirectHandle<Object> func_value =
        Object::GetProperty(isolate, global, func_name).ToHandleChecked();
    ASSERT_TRUE(IsJSFunction(*func_value));
    DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
    DirectHandle<JSObject> obj = factory->NewJSObject(function);

    DirectHandle<String> obj_name = factory->InternalizeUtf8String("theObject");
    Object::SetProperty(isolate, global, obj_name, obj).Check();
    DirectHandle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    DirectHandle<Smi> twenty_three(Smi::FromInt(23), isolate);
    Object::SetProperty(isolate, obj, prop_name, twenty_three).Check();
  }

  InvokeMajorGC();

  {
    HandleScope scope(isolate);
    DirectHandle<String> obj_name = factory->InternalizeUtf8String("theObject");
    EXPECT_EQ(Just(true),
              JSReceiver::HasOwnProperty(isolate, global, obj_name));
    DirectHandle<Object> object =
        Object::GetProperty(isolate, global, obj_name).ToHandleChecked();
    ASSERT_TRUE(IsJSObject(*object));
    DirectHandle<String> prop_name = factory->InternalizeUtf8String("theSlot");
    EXPECT_EQ(*Object::GetProperty(isolate, Cast<JSObject>(object), prop_name)
                   .ToHandleChecked(),
              Smi::FromInt(23));
  }
}

TEST_F(MarkCompactTest, DoNotEvacuatePinnedPages) {
  if (!v8_flags.compact || !v8_flags.single_generation ||
      v8_flags.precise_object_pinning) {
    return;
  }

  ManualGCScope manual_gc_scope(i_isolate());
  FlagScope<bool> compact_on_every_full_gc(&v8_flags.compact_on_every_full_gc,
                                           true);

  v8::HandleScope sc(v8_isolate());
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

  SealCurrentObjects();

  std::vector<Handle<FixedArray>> handles = CreatePadding(
      static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
      AllocationType::kOld);
  ASSERT_FALSE(handles.empty());

  MemoryChunk* chunk = MemoryChunk::FromHeapObject(*handles.front());
  auto* page = MutablePage::FromHeapObject(isolate, *handles.front());

  EXPECT_TRUE(heap->InSpace(*handles.front(), OLD_SPACE));
  page->set_is_pinned_for_testing(true);

  InvokeMajorGC();
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);

  // The pinned flag should prevent the page from moving.
  for (DirectHandle<FixedArray> object : handles) {
    EXPECT_EQ(chunk, MemoryChunk::FromHeapObject(*object));
  }

  page->set_is_pinned_for_testing(false);

  InvokeMajorGC();
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);

  // `compact_on_every_full_gc` ensures that this page is an evacuation
  // candidate, so with the pin flag cleared compaction should now move it.
  for (DirectHandle<FixedArray> object : handles) {
    EXPECT_NE(chunk, MemoryChunk::FromHeapObject(*object));
  }
}

TEST_F(MarkCompactTest, Regress5829) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(i_isolate());
  v8::HandleScope sc(v8_isolate());
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  SealCurrentObjects();
  IncrementalMarking* marking = heap->incremental_marking();
  if (heap->sweeping_in_progress()) {
    heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                  CompleteSweepingReason::kTesting);
  }
  EXPECT_TRUE(marking->IsMarking() || marking->IsStopped());
  if (marking->IsStopped()) {
    heap->StartIncrementalMarking(GCFlag::kNoFlags,
                                  GarbageCollectionReason::kTesting);
  }
  ASSERT_TRUE(marking->IsMarking());
  EXPECT_TRUE(marking->black_allocation());
  DirectHandle<FixedArray> array =
      isolate->factory()->NewFixedArray(10, AllocationType::kOld);
  Address old_end = array->address() + array->Size();
  // Right trim the array without clearing the mark bits.
  array->set_length(9);
  heap->CreateFillerObjectAt(old_end - kTaggedSize, kTaggedSize);
  heap->FreeMainThreadLinearAllocationAreas();
  NormalPage* page = NormalPage::FromAddress(array->address());
  for (auto object_and_size : LiveObjectRange(page)) {
    EXPECT_FALSE(IsFreeSpaceOrFiller(object_and_size.first));
  }
}

}  // namespace internal
}  // namespace v8

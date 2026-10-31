// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/concurrent-marking.h"
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
using MarkCompactPlatformTest = TestWithPlatformAndHeapInternals;

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

TEST_F(MarkCompactTest, IncrementalMarkingUsingTasks) {
  if (!v8_flags.incremental_marking) return;
  FLAG_VALUE_SCOPE(stress_concurrent_allocation, false);
  FLAG_VALUE_SCOPE(stress_incremental_marking, false);

  IncrementalMarking* marking = heap()->incremental_marking();
  if (marking->IsMarking()) {
    InvokeAtomicMajorGC();
  }
  SimulateFullSpace(heap()->old_space());
  EXPECT_TRUE(marking->IsStopped());

  heap()->StartIncrementalMarking(GCFlag::kNoFlags,
                                  GarbageCollectionReason::kTesting);
  EXPECT_TRUE(marking->IsMajorMarking());
  while (marking->IsMajorMarking()) {
    ASSERT_TRUE(
        v8::platform::PumpMessageLoop(V8::GetCurrentPlatform(), v8_isolate()));
  }
  EXPECT_TRUE(marking->IsStopped());
}

TEST_F(MarkCompactTest, ConcurrentMarkingMarkedBytes) {
  if (!v8_flags.incremental_marking || !v8_flags.concurrent_marking) return;
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  HandleScope sc(isolate);
  DirectHandle<FixedArray> root = isolate->factory()->NewFixedArray(1000000);
  InvokeMajorGC();
  if (!heap->incremental_marking()->IsStopped()) return;

  // Store array in Global such that it is part of the root set when
  // starting incremental marking.
  v8::Global<Value> global_root(v8_isolate(),
                                Utils::ToLocal(Cast<Object>(root)));

  SimulateIncrementalMarking(false);
  // Ensure that objects are published to the global marking worklist such that
  // the concurrent markers can pick it up.
  heap->mark_compact_collector()->local_marking_worklists()->Publish();
  heap->concurrent_marking()->JoinJobForTesting();
  EXPECT_GE(heap->concurrent_marking()->TotalMarkedBytes(),
            static_cast<size_t>(root->Size()));
}

TEST_F(MarkCompactPlatformTest, ConcurrentMarkingStoppedOnTeardown) {
  if (!v8_flags.incremental_marking || !v8_flags.concurrent_marking) return;

  // Create and dispose a standalone Isolate directly because IsolateWrapper
  // pumps the message loop before Dispose(), which would finish marking early.
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
    Factory* factory = i_isolate->factory();

    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    for (int i = 0; i < 10000; i++) {
      factory->NewJSWeakMap();
    }

    Heap* heap = i_isolate->heap();
    SimulateIncrementalMarking(heap, false);
  }

  isolate->Dispose();
}

namespace {

void CheckInvariantsOfAbortedPage(NormalPage* page) {
  // Check invariants:
  // 1) Markbits are cleared
  // 2) The page is not marked as evacuation candidate anymore
  // 3) The page is not marked as aborted compaction anymore.
  EXPECT_TRUE(page->marking_bitmap()->IsClean());
  EXPECT_FALSE(page->is_evacuation_candidate());
  EXPECT_FALSE(page->evacuation_was_aborted());
}

void CheckAllObjectsOnPage(const std::vector<Handle<FixedArray>>& handles,
                           NormalPage* page) {
  for (DirectHandle<FixedArray> fixed_array : handles) {
    EXPECT_EQ(NormalPage::FromHeapObject(*fixed_array), page);
  }
}

int GetObjectSize(int objects_per_page) {
  int allocatable =
      static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage());
  // Make sure that object_size is a multiple of kTaggedSize.
  int object_size =
      ((allocatable / kTaggedSize) / objects_per_page) * kTaggedSize;
  return std::min(kMaxRegularHeapObjectSize, object_size);
}

}  // namespace

TEST_F(MarkCompactTest, CompactionFullAbortedPage) {
  if (!v8_flags.compact) return;
  // Test the scenario where we reach OOM during compaction and the whole page
  // is aborted.

  // Disable concurrent sweeping to ensure memory is in an expected state, i.e.,
  // we can reach the state of a half aborted page.
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  ManualGCScope manual_gc_scope(isolate);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  auto reset_oom = [](void* data, size_t limit, size_t) -> size_t {
    reinterpret_cast<MarkCompactTest*>(data)->SetForceOOM(false);
    return limit;
  };
  heap->AddNearHeapLimitCallback(reset_oom, this);
  {
    HandleScope scope1(isolate);

    SealCurrentObjects();

    {
      HandleScope scope2(isolate);
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      std::vector<Handle<FixedArray>> compaction_page_handles = CreatePadding(
          static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
          AllocationType::kOld);
      ASSERT_FALSE(compaction_page_handles.empty());
      NormalPage* to_be_aborted_page =
          NormalPage::FromHeapObject(*compaction_page_handles.front());
      to_be_aborted_page->set_forced_evacuation_candidate_for_testing(true);
      CheckAllObjectsOnPage(compaction_page_handles, to_be_aborted_page);

      SetForceOOM(true);
      InvokeMajorGC();
      heap->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kV8Only,
          CompleteSweepingReason::kTesting);

      // Check that all handles still point to the same page, i.e., compaction
      // has been aborted on the page.
      for (DirectHandle<FixedArray> object : compaction_page_handles) {
        EXPECT_EQ(to_be_aborted_page, NormalPage::FromHeapObject(*object));
      }
      CheckInvariantsOfAbortedPage(to_be_aborted_page);
    }
  }
  heap->RemoveNearHeapLimitCallback(reset_oom, 0u);
}

TEST_F(MarkCompactTest, CompactionPartiallyAbortedPage) {
  if (!v8_flags.compact) return;
  // Test the scenario where we reach OOM during compaction and parts of the
  // page have already been migrated to a new one.

  // Disable concurrent sweeping to ensure memory is in an expected state, i.e.,
  // we can reach the state of a half aborted page.
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  ManualGCScope manual_gc_scope(isolate);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  const int objects_per_page = 10;
  const int object_size = GetObjectSize(objects_per_page);

  auto reset_oom = [](void* data, size_t limit, size_t) -> size_t {
    reinterpret_cast<MarkCompactTest*>(data)->SetForceOOM(false);
    return limit;
  };
  heap->AddNearHeapLimitCallback(reset_oom, this);
  {
    HandleScope scope1(isolate);

    SealCurrentObjects();

    {
      HandleScope scope2(isolate);
      // Fill another page with objects of size {object_size} (last one is
      // properly adjusted).
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      std::vector<Handle<FixedArray>> compaction_page_handles = CreatePadding(
          static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
          AllocationType::kOld, object_size);
      ASSERT_FALSE(compaction_page_handles.empty());
      NormalPage* to_be_aborted_page =
          NormalPage::FromHeapObject(*compaction_page_handles.front());
      to_be_aborted_page->set_forced_evacuation_candidate_for_testing(true);
      CheckAllObjectsOnPage(compaction_page_handles, to_be_aborted_page);

      {
        // Add another page that is filled with {num_objects} objects of size
        // {object_size}.
        HandleScope scope3(isolate);
        ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                                 AllocationOrigin::kRuntime));
        const int num_objects = 3;
        std::vector<Handle<FixedArray>> page_to_fill_handles = CreatePadding(
            object_size * num_objects, AllocationType::kOld, object_size);
        ASSERT_FALSE(page_to_fill_handles.empty());
        NormalPage* page_to_fill =
            NormalPage::FromAddress(page_to_fill_handles.front()->address());

        SetForceOOM(true);
        InvokeMajorGC();
        heap->EnsureSweepingCompleted(
            Heap::SweepingForcedFinalizationMode::kV8Only,
            CompleteSweepingReason::kTesting);

        bool migration_aborted = false;
        for (DirectHandle<FixedArray> object : compaction_page_handles) {
          // Once compaction has been aborted, all following objects still have
          // to be on the initial page.
          EXPECT_TRUE(
              !migration_aborted ||
              (NormalPage::FromHeapObject(*object) == to_be_aborted_page));
          if (NormalPage::FromHeapObject(*object) == to_be_aborted_page) {
            // This object has not been migrated.
            migration_aborted = true;
          } else {
            EXPECT_EQ(NormalPage::FromHeapObject(*object), page_to_fill);
          }
        }
        // Check that we actually created a scenario with a partially aborted
        // page.
        EXPECT_TRUE(migration_aborted);
        CheckInvariantsOfAbortedPage(to_be_aborted_page);
      }
    }
  }
  heap->RemoveNearHeapLimitCallback(reset_oom, 0u);
}

TEST_F(MarkCompactTest, CompactionPartiallyAbortedPageIntraAbortedPointers) {
  if (!v8_flags.compact) return;
  // Test the scenario where we reach OOM during compaction and parts of the
  // page have already been migrated to a new one. Objects on the aborted page
  // are linked together. This test makes sure that intra-aborted page pointers
  // get properly updated.

  // Disable concurrent sweeping to ensure memory is in an expected state, i.e.,
  // we can reach the state of a half aborted page.
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  ManualGCScope manual_gc_scope(isolate);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  const int objects_per_page = 10;
  const int object_size = GetObjectSize(objects_per_page);

  auto reset_oom = [](void* data, size_t limit, size_t) -> size_t {
    reinterpret_cast<MarkCompactTest*>(data)->SetForceOOM(false);
    return limit;
  };
  heap->AddNearHeapLimitCallback(reset_oom, this);
  {
    HandleScope scope1(isolate);
    IndirectHandle<FixedArray> root_array =
        isolate->factory()->NewFixedArray(10, AllocationType::kOld);

    SealCurrentObjects();

    NormalPage* to_be_aborted_page = nullptr;
    {
      HandleScope temporary_scope(isolate);
      // Fill a fresh page with objects of size {object_size} (last one is
      // properly adjusted).
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      std::vector<Handle<FixedArray>> compaction_page_handles = CreatePadding(
          static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
          AllocationType::kOld, object_size);
      ASSERT_FALSE(compaction_page_handles.empty());
      to_be_aborted_page =
          NormalPage::FromHeapObject(*compaction_page_handles.front());
      to_be_aborted_page->set_forced_evacuation_candidate_for_testing(true);
      for (size_t i = compaction_page_handles.size() - 1; i > 0; i--) {
        compaction_page_handles[i]->set(0, *compaction_page_handles[i - 1]);
      }
      root_array->set(0, *compaction_page_handles.back());
      CheckAllObjectsOnPage(compaction_page_handles, to_be_aborted_page);
    }
    {
      // Add another page that is filled with {num_objects} objects of size
      // {object_size}.
      HandleScope scope3(isolate);
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      const int num_objects = 2;
      int used_memory = object_size * num_objects;
      std::vector<Handle<FixedArray>> page_to_fill_handles =
          CreatePadding(used_memory, AllocationType::kOld, object_size);
      ASSERT_FALSE(page_to_fill_handles.empty());
      NormalPage* page_to_fill =
          NormalPage::FromHeapObject(*page_to_fill_handles.front());

      // We need to invoke GC without stack, otherwise no compaction is
      // performed.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

      SetForceOOM(true);
      InvokeMajorGC();
      heap->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kV8Only,
          CompleteSweepingReason::kTesting);

      // The following check makes sure that we compacted "some" objects, while
      // leaving others in place.
      bool in_place = true;
      IndirectHandle<FixedArray> current = root_array;
      while (current->get(0) != ReadOnlyRoots(heap).undefined_value()) {
        current = IndirectHandle<FixedArray>(Cast<FixedArray>(current->get(0)),
                                             isolate);
        ASSERT_TRUE(IsFixedArray(*current));
        if (NormalPage::FromHeapObject(*current) != to_be_aborted_page) {
          in_place = false;
        }
        bool on_aborted_page =
            NormalPage::FromHeapObject(*current) == to_be_aborted_page;
        bool on_fill_page =
            NormalPage::FromHeapObject(*current) == page_to_fill;
        EXPECT_TRUE((in_place && on_aborted_page) ||
                    (!in_place && on_fill_page));
      }
      // Check that we at least migrated one object, as otherwise the test would
      // not trigger.
      EXPECT_FALSE(in_place);
      CheckInvariantsOfAbortedPage(to_be_aborted_page);
    }
  }
  heap->RemoveNearHeapLimitCallback(reset_oom, 0u);
}

TEST_F(MarkCompactTest,
       CompactionPartiallyAbortedPageWithRememberedSetEntries) {
  if (!v8_flags.compact || v8_flags.single_generation) return;
  // Test the scenario where we reach OOM during compaction and parts of the
  // page have already been migrated to a new one. Objects on the aborted page
  // are linked together and the very first object on the aborted page points
  // into new space. The test verifies that the remembered set entries are
  // properly cleared and rebuilt after aborting a page. Failing to do so can
  // result in other objects being allocated in the free space where their
  // payload looks like a valid new space pointer.

  // Disable concurrent sweeping to ensure memory is in an expected state, i.e.,
  // we can reach the state of a half aborted page.
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  ManualGCScope manual_gc_scope(isolate);
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);

  const int objects_per_page = 10;
  const int object_size = GetObjectSize(objects_per_page);

  auto reset_oom = [](void* data, size_t limit, size_t) -> size_t {
    reinterpret_cast<MarkCompactTest*>(data)->SetForceOOM(false);
    return limit;
  };
  heap->AddNearHeapLimitCallback(reset_oom, this);
  {
    HandleScope scope1(isolate);
    IndirectHandle<FixedArray> root_array =
        isolate->factory()->NewFixedArray(10, AllocationType::kOld);
    SealCurrentObjects();

    NormalPage* to_be_aborted_page = nullptr;
    {
      HandleScope temporary_scope(isolate);
      // Fill another page with objects of size {object_size} (last one is
      // properly adjusted).
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      std::vector<Handle<FixedArray>> compaction_page_handles = CreatePadding(
          static_cast<int>(MemoryChunkLayout::AllocatableMemoryInDataPage()),
          AllocationType::kOld, object_size);
      ASSERT_FALSE(compaction_page_handles.empty());
      // Sanity check that we have enough space for linking up arrays.
      ASSERT_GE(compaction_page_handles.front()->length().value(), 2u);
      to_be_aborted_page =
          NormalPage::FromHeapObject(*compaction_page_handles.front());
      to_be_aborted_page->set_forced_evacuation_candidate_for_testing(true);

      for (size_t i = compaction_page_handles.size() - 1; i > 0; i--) {
        compaction_page_handles[i]->set(0, *compaction_page_handles[i - 1]);
      }
      root_array->set(0, *compaction_page_handles.back());
      DirectHandle<FixedArray> new_space_array =
          isolate->factory()->NewFixedArray(1, AllocationType::kYoung);
      EXPECT_TRUE(HeapLayout::InYoungGeneration(*new_space_array));
      compaction_page_handles.front()->set(1, *new_space_array);
      CheckAllObjectsOnPage(compaction_page_handles, to_be_aborted_page);
    }

    {
      // Add another page that is filled with {num_objects} objects of size
      // {object_size}.
      HandleScope scope3(isolate);
      ASSERT_TRUE(heap->old_space()->TryExpand(heap->main_thread_local_heap(),
                                               AllocationOrigin::kRuntime));
      const int num_objects = 2;
      int used_memory = object_size * num_objects;
      std::vector<Handle<FixedArray>> page_to_fill_handles =
          CreatePadding(used_memory, AllocationType::kOld, object_size);
      ASSERT_FALSE(page_to_fill_handles.empty());
      NormalPage* page_to_fill =
          NormalPage::FromHeapObject(*page_to_fill_handles.front());

      // We need to invoke GC without stack, otherwise no compaction is
      // performed.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

      SetForceOOM(true);
      InvokeMajorGC();
      heap->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kV8Only,
          CompleteSweepingReason::kTesting);

      // The following check makes sure that we compacted "some" objects, while
      // leaving others in place.
      bool in_place = true;
      IndirectHandle<FixedArray> current = root_array;
      while (current->get(0) != ReadOnlyRoots(heap).undefined_value()) {
        current = IndirectHandle<FixedArray>(Cast<FixedArray>(current->get(0)),
                                             isolate);
        EXPECT_FALSE(HeapLayout::InYoungGeneration(*current));
        ASSERT_TRUE(IsFixedArray(*current));
        if (NormalPage::FromHeapObject(*current) != to_be_aborted_page) {
          in_place = false;
        }
        bool on_aborted_page =
            NormalPage::FromHeapObject(*current) == to_be_aborted_page;
        bool on_fill_page =
            NormalPage::FromHeapObject(*current) == page_to_fill;
        EXPECT_TRUE((in_place && on_aborted_page) ||
                    (!in_place && on_fill_page));
      }
      // Check that we at least migrated one object, as otherwise the test would
      // not trigger.
      EXPECT_FALSE(in_place);
      CheckInvariantsOfAbortedPage(to_be_aborted_page);

      // Allocate a new object in new space.
      IndirectHandle<FixedArray> holder =
          isolate->factory()->NewFixedArray(10, AllocationType::kYoung);
      // Create a broken address that looks like a tagged pointer to a new space
      // object.
      Address broken_address = holder->address() + 2 * kTaggedSize + 1;
      // Convert it to a vector to create a string from it.
      base::Vector<const uint8_t> string_to_broken_address(
          reinterpret_cast<const uint8_t*>(&broken_address), kTaggedSize);

      IndirectHandle<String> string;
      do {
        // We know that the interesting slot will be on the aborted page and
        // hence we allocate until we get our string on the aborted page.
        // We used slot 1 in the fixed size array which corresponds to the
        // the first word in the string. Since the first object definitely
        // migrated we can just allocate until we hit the aborted page.
        string = isolate->factory()
                     ->NewStringFromOneByte(string_to_broken_address,
                                            AllocationType::kOld)
                     .ToHandleChecked();
      } while (NormalPage::FromHeapObject(*string) != to_be_aborted_page);

      // If remembered set entries are not properly filtered/reset for aborted
      // pages we have now a broken address at an object slot in old space and
      // the following scavenge will crash.
      InvokeMinorGC();
    }
  }
  heap->RemoveNearHeapLimitCallback(reset_oom, 0u);
}

}  // namespace internal
}  // namespace v8

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/heap/array-buffer-tracker.h"
#include "src/heap/spaces-inl.h"
#include "src/isolate.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace {

v8::Isolate* NewIsolateForPagePromotion(int min_semi_space_size = 8,
                                        int max_semi_space_size = 8) {
  // Parallel evacuation messes with fragmentation in a way that objects that
  // should be copied in semi space are promoted to old space because of
  // fragmentation.
  i::FLAG_parallel_compaction = false;
  i::FLAG_page_promotion = true;
  i::FLAG_page_promotion_threshold = 0;  // %
  i::FLAG_min_semi_space_size = min_semi_space_size;
  // We cannot optimize for size as we require a new space with more than one
  // page.
  i::FLAG_optimize_for_size = false;
  // Set max_semi_space_size because it could've been initialized by an
  // implication of optimize_for_size.
  i::FLAG_max_semi_space_size = max_semi_space_size;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  return isolate;
}

}  // namespace

namespace v8 {
namespace internal {

UNINITIALIZED_TEST(PagePromotion_NewToOld) {
  if (!i::FLAG_incremental_marking) return;
  if (!i::FLAG_page_promotion) return;

  v8::Isolate* isolate = NewIsolateForPagePromotion();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();

    std::vector<Handle<FixedArray>> handles;
    heap::SimulateFullSpace(heap->new_space(), &handles);
    heap->CollectGarbage(NEW_SPACE, i::GarbageCollectionReason::kTesting);
    CHECK_GT(handles.size(), 0u);
    // Last object in handles should definitely be on a page that does not
    // contain the age mark, thus qualifying for moving.
    Handle<FixedArray> last_object = handles.back();
    Page* to_be_promoted_page = Page::FromAddress(last_object->address());
    CHECK(!to_be_promoted_page->Contains(heap->new_space()->age_mark()));
    // To perform a sanity check on live bytes we need to mark the heap.
    heap::SimulateIncrementalMarking(heap, true);
    // Sanity check that the page meets the requirements for promotion.
    const int threshold_bytes =
        FLAG_page_promotion_threshold * Page::kAllocatableMemory / 100;
    CHECK_GE(MarkingState::Internal(to_be_promoted_page).live_bytes(),
             threshold_bytes);

    // Actual checks: The page is in new space first, but is moved to old space
    // during a full GC.
    CHECK(heap->new_space()->ContainsSlow(to_be_promoted_page->address()));
    CHECK(!heap->old_space()->ContainsSlow(to_be_promoted_page->address()));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(!heap->new_space()->ContainsSlow(to_be_promoted_page->address()));
    CHECK(heap->old_space()->ContainsSlow(to_be_promoted_page->address()));
  }
}

UNINITIALIZED_TEST(PagePromotion_NewToNew) {
  if (!i::FLAG_page_promotion) return;

  v8::Isolate* isolate = NewIsolateForPagePromotion();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();

    std::vector<Handle<FixedArray>> handles;
    heap::SimulateFullSpace(heap->new_space(), &handles);
    CHECK_GT(handles.size(), 0u);
    // Last object in handles should definitely be on a page that does not
    // contain the age mark, thus qualifying for moving.
    Handle<FixedArray> last_object = handles.back();
    Page* to_be_promoted_page = Page::FromAddress(last_object->address());
    CHECK(!to_be_promoted_page->Contains(heap->new_space()->age_mark()));
    CHECK(to_be_promoted_page->Contains(last_object->address()));
    CHECK(heap->new_space()->ToSpaceContainsSlow(last_object->address()));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(heap->new_space()->ToSpaceContainsSlow(last_object->address()));
    CHECK(to_be_promoted_page->Contains(last_object->address()));
  }
}

UNINITIALIZED_TEST(PagePromotion_NewToNewJSArrayBuffer) {
  if (!i::FLAG_page_promotion) return;

  // Test makes sure JSArrayBuffer backing stores are still tracked after
  // new-to-new promotion.
  v8::Isolate* isolate = NewIsolateForPagePromotion();
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();

    // Fill the current page which potentially contains the age mark.
    heap::FillCurrentPage(heap->new_space());
    // Allocate a buffer we would like to check against.
    Handle<JSArrayBuffer> buffer =
        i_isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared);
    CHECK(JSArrayBuffer::SetupAllocatingData(buffer, i_isolate, 100));
    std::vector<Handle<FixedArray>> handles;
    // Simulate a full space, filling the interesting page with live objects.
    heap::SimulateFullSpace(heap->new_space(), &handles);
    CHECK_GT(handles.size(), 0u);
    // First object in handles should be on the same page as the allocated
    // JSArrayBuffer.
    Handle<FixedArray> first_object = handles.front();
    Page* to_be_promoted_page = Page::FromAddress(first_object->address());
    CHECK(!to_be_promoted_page->Contains(heap->new_space()->age_mark()));
    CHECK(to_be_promoted_page->Contains(first_object->address()));
    CHECK(to_be_promoted_page->Contains(buffer->address()));
    CHECK(heap->new_space()->ToSpaceContainsSlow(first_object->address()));
    CHECK(heap->new_space()->ToSpaceContainsSlow(buffer->address()));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(heap->new_space()->ToSpaceContainsSlow(first_object->address()));
    CHECK(heap->new_space()->ToSpaceContainsSlow(buffer->address()));
    CHECK(to_be_promoted_page->Contains(first_object->address()));
    CHECK(to_be_promoted_page->Contains(buffer->address()));
    CHECK(ArrayBufferTracker::IsTracked(*buffer));
  }
}

UNINITIALIZED_HEAP_TEST(Regress658718) {
  if (!i::FLAG_page_promotion) return;

  v8::Isolate* isolate = NewIsolateForPagePromotion(4, 8);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();
    heap->delay_sweeper_tasks_for_testing_ = true;
    heap->new_space()->Grow();
    {
      v8::HandleScope inner_handle_scope(isolate);
      std::vector<Handle<FixedArray>> handles;
      heap::SimulateFullSpace(heap->new_space(), &handles);
      CHECK_GT(handles.size(), 0u);
      // Last object in handles should definitely be on a page that does not
      // contain the age mark, thus qualifying for moving.
      Handle<FixedArray> last_object = handles.back();
      Page* to_be_promoted_page = Page::FromAddress(last_object->address());
      CHECK(!to_be_promoted_page->Contains(heap->new_space()->age_mark()));
      CHECK(to_be_promoted_page->Contains(last_object->address()));
      CHECK(heap->new_space()->ToSpaceContainsSlow(last_object->address()));
      heap->CollectGarbage(OLD_SPACE, i::GarbageCollectionReason::kTesting);
      CHECK(heap->new_space()->ToSpaceContainsSlow(last_object->address()));
      CHECK(to_be_promoted_page->Contains(last_object->address()));
    }
    heap->CollectGarbage(NEW_SPACE, i::GarbageCollectionReason::kTesting);
    heap->new_space()->Shrink();
    heap->memory_allocator()->unmapper()->WaitUntilCompleted();
    heap->mark_compact_collector()->sweeper().StartSweeperTasks();
    heap->mark_compact_collector()->EnsureSweepingCompleted();
  }
}

}  // namespace internal
}  // namespace v8

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/heap-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace {

bool IsTrackedYoung(i::Heap* heap, i::ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_young;
}

bool IsTrackedOld(i::Heap* heap, i::ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_old;
}

bool IsTracked(i::Heap* heap, i::ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_young || in_old;
}

bool IsTracked(i::Heap* heap, i::JSArrayBuffer buffer) {
  return IsTracked(heap, buffer.extension());
}

}  // namespace

namespace v8 {
namespace internal {
namespace heap {

// The following tests make sure that JSArrayBuffer tracking works expected when
// moving the objects through various spaces during GC phases.

TEST(ArrayBuffer_OnlyMC) {
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
    extension = buf->extension();
    CHECK(v8_flags.single_generation ? IsTrackedOld(heap, extension)
                                     : IsTrackedYoung(heap, extension));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTrackedOld(heap, extension));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTrackedOld(heap, extension));
  }
  heap::GcAndSweep(heap, OLD_SPACE);
  CHECK(!IsTracked(heap, extension));
}

TEST(ArrayBuffer_OnlyScavenge) {
  if (v8_flags.single_generation) return;
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
    extension = buf->extension();
    CHECK(IsTrackedYoung(heap, extension));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTrackedYoung(heap, extension));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTrackedOld(heap, extension));
  }
  heap::GcAndSweep(heap, OLD_SPACE);
  CHECK(!IsTracked(heap, extension));
}

TEST(ArrayBuffer_ScavengeAndMC) {
  if (v8_flags.single_generation) return;
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
    extension = buf->extension();
    CHECK(IsTrackedYoung(heap, extension));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTrackedYoung(heap, extension));
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTrackedOld(heap, extension));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTrackedOld(heap, extension));
  }
  heap::GcAndSweep(heap, NEW_SPACE);
  CHECK(IsTrackedOld(heap, extension));
  heap::GcAndSweep(heap, OLD_SPACE);
  CHECK(!IsTracked(heap, extension));
}

TEST(ArrayBuffer_Compaction) {
  if (!v8_flags.compact) return;
  ManualGCScope manual_gc_scope;
  v8_flags.manual_evacuation_candidates_selection = true;
  v8_flags.concurrent_array_buffer_sweeping = false;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  heap::AbandonCurrentlyFreeMemory(heap->old_space());

  v8::HandleScope handle_scope(isolate);
  Local<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(isolate, 100);
  Handle<JSArrayBuffer> buf1 = v8::Utils::OpenHandle(*ab1);
  CHECK(IsTracked(heap, *buf1));
  heap::GcAndSweep(heap, OLD_SPACE);

  Page* page_before_gc = Page::FromHeapObject(*buf1);
  heap::ForceEvacuationCandidate(page_before_gc);
  CHECK(IsTracked(heap, *buf1));

  CcTest::CollectAllGarbage();

  Page* page_after_gc = Page::FromHeapObject(*buf1);
  CHECK(IsTracked(heap, *buf1));

  CHECK_NE(page_before_gc, page_after_gc);
}

TEST(ArrayBuffer_UnregisterDuringSweep) {
// Regular pages in old space (without compaction) are processed concurrently
// in the sweeper. If we happen to unregister a buffer (either explicitly, or
// implicitly through e.g. |Detach|) we need to sync with the sweeper
// task.
//
// Note: This test will will only fail on TSAN configurations.

// Disable verify-heap since it forces sweeping to be completed in the
// epilogue of the GC.
#ifdef VERIFY_HEAP
  i::v8_flags.verify_heap = false;
#endif  // VERIFY_HEAP
  ManualGCScope manual_gc_scope;
  i::v8_flags.concurrent_array_buffer_sweeping = false;

  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);

    {
      v8::HandleScope new_handle_scope(isolate);
      // Allocate another buffer on the same page to force processing a
      // non-empty set of buffers in the last GC.
      Local<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(isolate, 100);
      Handle<JSArrayBuffer> buf2 = v8::Utils::OpenHandle(*ab2);
      CHECK(IsTracked(heap, *buf));
      heap::GcAndSweep(heap, NEW_SPACE);
      CHECK(IsTracked(heap, *buf));
      heap::GcAndSweep(heap, NEW_SPACE);
      CHECK(IsTracked(heap, *buf));
      CHECK(IsTracked(heap, *buf2));
    }

    CcTest::CollectGarbage(OLD_SPACE);
    // |Detach| will cause the buffer to be |Unregister|ed. Without
    // barriers and proper synchronization this will trigger a data race on
    // TSAN.
    ab->Detach();
  }
}

TEST(ArrayBuffer_NonLivePromotion) {
  if (!v8_flags.incremental_marking || v8_flags.separate_gc_phases) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  ManualGCScope manual_gc_scope;
  // The test verifies that the marking state is preserved when promoting
  // a buffer to old space.
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  JSArrayBuffer raw_ab;
  {
    v8::HandleScope handle_scope(isolate);
    Handle<FixedArray> root =
        heap->isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
      root->set(0, *buf);  // Buffer that should not be promoted as live.
    }
    heap::SimulateIncrementalMarking(heap, false);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    raw_ab = JSArrayBuffer::cast(root->get(0));
    root->set(0, ReadOnlyRoots(heap).undefined_value());
    heap::SimulateIncrementalMarking(heap, true);
    // Prohibit page from being released.
    Page::FromHeapObject(raw_ab)->MarkNeverEvacuate();
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(!IsTracked(heap, raw_ab));
  }
}

TEST(ArrayBuffer_LivePromotion) {
  if (!v8_flags.incremental_marking || v8_flags.separate_gc_phases) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  ManualGCScope manual_gc_scope;
  // The test verifies that the marking state is preserved when promoting
  // a buffer to old space.
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  JSArrayBuffer raw_ab;
  {
    v8::HandleScope handle_scope(isolate);
    Handle<FixedArray> root =
        heap->isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
      root->set(0, *buf);  // Buffer that should be promoted as live.
    }
    // Store array in Global such that it is part of the root set when
    // starting incremental marking.
    v8::Global<Value> global_root(CcTest::isolate(),
                                  Utils::ToLocal(Handle<Object>::cast(root)));
    heap::SimulateIncrementalMarking(heap, true);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    heap::GcAndSweep(heap, NEW_SPACE);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    raw_ab = JSArrayBuffer::cast(root->get(0));
    root->set(0, ReadOnlyRoots(heap).undefined_value());
    // Prohibit page from being released.
    Page::FromHeapObject(raw_ab)->MarkNeverEvacuate();
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTracked(heap, raw_ab));
  }
}

TEST(ArrayBuffer_SemiSpaceCopyThenPagePromotion) {
  if (!i::v8_flags.incremental_marking) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  ManualGCScope manual_gc_scope;
  // The test verifies that the marking state is preserved across semispace
  // copy.
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  heap::SealCurrentObjects(heap);
  {
    v8::HandleScope handle_scope(isolate);
    Handle<FixedArray> root =
        heap->isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
      root->set(0, *buf);  // Buffer that should be promoted as live.
      Page::FromHeapObject(*buf)->MarkNeverEvacuate();
    }
    std::vector<Handle<FixedArray>> handles;
    // Make the whole page transition from new->old, getting the buffers
    // processed in the sweeper (relying on marking information) instead of
    // processing during newspace evacuation.
    heap::FillCurrentPage(heap->new_space(), &handles);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
    heap::GcAndSweep(heap, NEW_SPACE);
    heap::SimulateIncrementalMarking(heap, true);
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTracked(heap, JSArrayBuffer::cast(root->get(0))));
  }
}

TEST(ArrayBuffer_PagePromotion) {
  if (!i::v8_flags.incremental_marking || i::v8_flags.single_generation) return;
  i::v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope;
  // The test verifies that the marking state is preserved across semispace
  // copy.
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();

  heap::SealCurrentObjects(heap);
  {
    v8::HandleScope handle_scope(isolate);
    Handle<FixedArray> root =
        heap->isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
    ArrayBufferExtension* extension;
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      Handle<JSArrayBuffer> buf = v8::Utils::OpenHandle(*ab);
      extension = buf->extension();
      root->set(0, *buf);  // Buffer that should be promoted as live.
    }
    std::vector<Handle<FixedArray>> handles;
    // Create live objects on page such that the whole page gets promoted
    heap::FillCurrentPage(heap->new_space(), &handles);
    CHECK(IsTrackedYoung(heap, extension));
    heap::SimulateIncrementalMarking(heap, true);
    heap::GcAndSweep(heap, OLD_SPACE);
    CHECK(IsTrackedOld(heap, extension));
  }
}

UNINITIALIZED_TEST(ArrayBuffer_SemiSpaceCopyMultipleTasks) {
  if (v8_flags.optimize_for_size || v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope;
  // Test allocates JSArrayBuffer on different pages before triggering a
  // full GC that performs the semispace copy. If parallelized, this test
  // ensures proper synchronization in TSAN configurations.
  v8_flags.min_semi_space_size = std::max(2 * Page::kPageSize / MB, 1);
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();
    Heap* heap = i_isolate->heap();

    // Ensure heap is in a clean state.
    CcTest::CollectAllGarbage(i_isolate);
    CcTest::CollectAllGarbage(i_isolate);

    Local<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf1 = v8::Utils::OpenHandle(*ab1);
    heap::FillCurrentPage(heap->new_space());
    Local<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(isolate, 100);
    Handle<JSArrayBuffer> buf2 = v8::Utils::OpenHandle(*ab2);
    CHECK_NE(Page::FromHeapObject(*buf1), Page::FromHeapObject(*buf2));
    heap::GcAndSweep(heap, OLD_SPACE);
  }
  isolate->Dispose();
}

TEST(ArrayBuffer_ExternalBackingStoreSizeIncreases) {
  if (v8_flags.single_generation) return;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  ExternalBackingStoreType type = ExternalBackingStoreType::kArrayBuffer;

  const Space* space = v8_flags.incremental_marking
                           ? static_cast<Space*>(heap->new_space())
                           : static_cast<Space*>(heap->old_space());
  const size_t backing_store_before = space->ExternalBackingStoreBytes(type);
  {
    const size_t kArraybufferSize = 117;
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, kArraybufferSize);
    USE(ab);
    const size_t backing_store_after = space->ExternalBackingStoreBytes(type);
    CHECK_EQ(kArraybufferSize, backing_store_after - backing_store_before);
  }
}

TEST(ArrayBuffer_ExternalBackingStoreSizeDecreases) {
  if (v8_flags.single_generation) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  ExternalBackingStoreType type = ExternalBackingStoreType::kArrayBuffer;

  const size_t backing_store_before =
      heap->new_space()->ExternalBackingStoreBytes(type);
  {
    const size_t kArraybufferSize = 117;
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, kArraybufferSize);
    USE(ab);
  }
  heap::GcAndSweep(heap, OLD_SPACE);
  const size_t backing_store_after =
      heap->new_space()->ExternalBackingStoreBytes(type);
  CHECK_EQ(0, backing_store_after - backing_store_before);
}

TEST(ArrayBuffer_ExternalBackingStoreSizeIncreasesMarkCompact) {
  if (!v8_flags.compact) return;
  ManualGCScope manual_gc_scope;
  v8_flags.manual_evacuation_candidates_selection = true;
  v8_flags.concurrent_array_buffer_sweeping = false;
  CcTest::InitializeVM();
  LocalContext env;
  v8::Isolate* isolate = env->GetIsolate();
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  heap::AbandonCurrentlyFreeMemory(heap->old_space());
  ExternalBackingStoreType type = ExternalBackingStoreType::kArrayBuffer;

  const size_t backing_store_before =
      heap->old_space()->ExternalBackingStoreBytes(type);

  const size_t kArraybufferSize = 117;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab1 =
        v8::ArrayBuffer::New(isolate, kArraybufferSize);
    Handle<JSArrayBuffer> buf1 = v8::Utils::OpenHandle(*ab1);
    CHECK(IsTracked(heap, *buf1));
    heap::GcAndSweep(heap, OLD_SPACE);

    Page* page_before_gc = Page::FromHeapObject(*buf1);
    heap::ForceEvacuationCandidate(page_before_gc);
    CHECK(IsTracked(heap, *buf1));

    CcTest::CollectAllGarbage();

    const size_t backing_store_after =
        heap->old_space()->ExternalBackingStoreBytes(type);
    CHECK_EQ(kArraybufferSize, backing_store_after - backing_store_before);
  }

  heap::GcAndSweep(heap, OLD_SPACE);
  const size_t backing_store_after =
      heap->old_space()->ExternalBackingStoreBytes(type);
  CHECK_EQ(0, backing_store_after - backing_store_before);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

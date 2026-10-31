// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-sweeper.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/heap-inl.h"
#include "src/heap/mutable-page-inl.h"
#include "src/heap/normal-page-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {

bool IsTrackedYoung(Heap* heap, ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_young;
}

bool IsTrackedOld(Heap* heap, ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_old;
}

bool IsTracked(Heap* heap, ArrayBufferExtension* extension) {
  bool in_young = heap->array_buffer_sweeper()->young().ContainsSlow(extension);
  bool in_old = heap->array_buffer_sweeper()->old().ContainsSlow(extension);
  CHECK(!(in_young && in_old));
  return in_young || in_old;
}

bool IsTracked(Heap* heap, Tagged<JSArrayBuffer> buffer) {
  return IsTracked(heap, buffer->extension());
}

using ArrayBufferSweeperTest = TestWithHeapInternalsAndContext;

class ArrayBufferSweeperUninitializedTest
    : public TestWithPlatformAndHeapInternals {
 private:
  SaveFlags save_flags_;
};

}  // namespace

// The following tests make sure that JSArrayBuffer tracking works expected when
// moving the objects through various spaces during GC phases.

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_OnlyMC) {
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);
    extension = buf->extension();
    CHECK(v8_flags.single_generation ? IsTrackedOld(heap(), extension)
                                     : IsTrackedYoung(heap(), extension));
    InvokeAtomicMajorGC();
    CHECK(IsTrackedOld(heap(), extension));
    InvokeAtomicMajorGC();
    CHECK(IsTrackedOld(heap(), extension));
  }
  InvokeAtomicMajorGC();
  CHECK(!IsTracked(heap(), extension));
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_OnlyScavenge) {
  if (v8_flags.single_generation) return;
  if (v8_flags.scavenger_chaos_mode) return;
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);
    extension = buf->extension();
    CHECK(IsTrackedYoung(heap(), extension));
    InvokeAtomicMinorGC();
    CHECK(IsTrackedYoung(heap(), extension));
    InvokeAtomicMajorGC();
    CHECK(IsTrackedOld(heap(), extension));
  }
  InvokeAtomicMajorGC();
  CHECK(!IsTracked(heap(), extension));
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_ScavengeAndMC) {
  if (v8_flags.single_generation) return;
  if (v8_flags.scavenger_chaos_mode) return;
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::Isolate* isolate = v8_isolate();
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  ArrayBufferExtension* extension;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);
    extension = buf->extension();
    CHECK(IsTrackedYoung(heap(), extension));
    InvokeAtomicMinorGC();
    CHECK(IsTrackedYoung(heap(), extension));
    InvokeAtomicMajorGC();
    CHECK(IsTrackedOld(heap(), extension));
    InvokeAtomicMinorGC();
    CHECK(IsTrackedOld(heap(), extension));
  }
  InvokeAtomicMinorGC();
  CHECK(IsTrackedOld(heap(), extension));
  InvokeAtomicMajorGC();
  CHECK(!IsTracked(heap(), extension));
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_Compaction) {
  if (!v8_flags.compact) return;
  ManualGCScope manual_gc_scope(i_isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8_flags.concurrent_array_buffer_sweeping = false;
  v8::Isolate* isolate = v8_isolate();
  AbandonCurrentlyFreeMemory(heap()->old_space());

  Global<v8::ArrayBuffer> ab1_global;
  NormalPage* page_before_gc;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(isolate, 100);
    IndirectHandle<JSArrayBuffer> buf1 = v8::Utils::OpenIndirectHandle(*ab1);
    CHECK(IsTracked(heap(), *buf1));
    InvokeAtomicMajorGC();

    page_before_gc = NormalPage::FromHeapObject(*buf1);
    ForceEvacuationCandidate(page_before_gc);
    CHECK(IsTracked(heap(), *buf1));
    ab1_global.Reset(isolate, ab1);
  }
  {
    // We need to invoke GC without stack, otherwise no compaction is
    // performed.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  {
    v8::HandleScope scope(isolate);
    DirectHandle<JSArrayBuffer> buf1 =
        v8::Utils::OpenDirectHandle(*ab1_global.Get(isolate));
    NormalPage* page_after_gc = NormalPage::FromHeapObject(*buf1);
    CHECK(IsTracked(heap(), *buf1));

    CHECK_NE(page_before_gc, page_after_gc);
  }
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_UnregisterDuringSweep) {
// Regular pages in old space (without compaction) are processed concurrently
// in the sweeper. If we happen to unregister a buffer (either explicitly, or
// implicitly through e.g. |Detach|) we need to sync with the sweeper
// task.
//
// Note: This test will will only fail on TSAN configurations.

// Disable verify-heap since it forces sweeping to be completed in the
// epilogue of the GC.
#ifdef VERIFY_HEAP
  v8_flags.verify_heap = false;
#endif  // VERIFY_HEAP
  ManualGCScope manual_gc_scope(i_isolate());
  v8_flags.concurrent_array_buffer_sweeping = false;

  v8::Isolate* isolate = v8_isolate();
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);

    {
      v8::HandleScope new_handle_scope(isolate);
      // Allocate another buffer on the same page to force processing a
      // non-empty set of buffers in the last GC.
      Local<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(isolate, 100);
      DirectHandle<JSArrayBuffer> buf2 = v8::Utils::OpenDirectHandle(*ab2);
      CHECK(IsTracked(heap(), *buf));
      InvokeAtomicMinorGC();
      CHECK(IsTracked(heap(), *buf));
      InvokeAtomicMinorGC();
      CHECK(IsTracked(heap(), *buf));
      CHECK(IsTracked(heap(), *buf2));
    }

    InvokeMajorGC();
    // |Detach| will cause the buffer to be |Unregister|ed. Without
    // barriers and proper synchronization this will trigger a data race on
    // TSAN.
    ab->Detach(v8::Local<v8::String>()).Check();
  }
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_SemiSpaceCopyThenPagePromotion) {
  if (!v8_flags.incremental_marking) return;
  if (v8_flags.minor_ms) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  v8_flags.scavenger_precise_object_pinning = false;
  ManualGCScope manual_gc_scope(i_isolate());
  // The test verifies that the marking state is preserved across semispace
  // copy.
  v8::Isolate* isolate = v8_isolate();

  SealCurrentObjects();
  {
    v8::HandleScope handle_scope(isolate);
    DirectHandle<FixedArray> root =
        factory()->NewFixedArray(1, AllocationType::kOld);
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);
      root->set(0, *buf);  // Buffer that should be promoted as live.
      MutablePage::FromHeapObject(i_isolate(), *buf)->MarkNeverEvacuate();
    }
    std::vector<Handle<FixedArray>> handles;
    // Make the whole page transition from new->old, getting the buffers
    // processed in the sweeper (relying on marking information) instead of
    // processing during newspace evacuation.
    FillCurrentPage(heap()->new_space(), &handles);
    CHECK(IsTracked(heap(), Cast<JSArrayBuffer>(root->get(0))));
    {
      // CSS prevent semi space copying in Scavenger.
      DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
      InvokeAtomicMinorGC();
    }
    SimulateIncrementalMarking(true);
    InvokeAtomicMajorGC();
    CHECK(IsTracked(heap(), Cast<JSArrayBuffer>(root->get(0))));
  }
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_PagePromotion) {
  if (!v8_flags.incremental_marking || v8_flags.single_generation) return;
  v8_flags.concurrent_array_buffer_sweeping = false;

  ManualGCScope manual_gc_scope(i_isolate());
  // The test verifies that the marking state is preserved across semispace
  // copy.
  v8::Isolate* isolate = v8_isolate();

  SealCurrentObjects();
  {
    v8::HandleScope handle_scope(isolate);
    DirectHandle<FixedArray> root =
        factory()->NewFixedArray(1, AllocationType::kOld);
    ArrayBufferExtension* extension;
    {
      v8::HandleScope new_handle_scope(isolate);
      Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, 100);
      DirectHandle<JSArrayBuffer> buf = v8::Utils::OpenDirectHandle(*ab);
      extension = buf->extension();
      root->set(0, *buf);  // Buffer that should be promoted as live.
    }
    std::vector<Handle<FixedArray>> handles;
    // Create live objects on page such that the whole page gets promoted
    FillCurrentPage(heap()->new_space(), &handles);
    CHECK(IsTrackedYoung(heap(), extension));
    SimulateIncrementalMarking(true);
    InvokeAtomicMajorGC();
    CHECK(IsTrackedOld(heap(), extension));
  }
}

TEST_F(ArrayBufferSweeperUninitializedTest,
       ArrayBuffer_SemiSpaceCopyMultipleTasks) {
  if (v8_flags.optimize_for_size || v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(nullptr);
  // Test allocates JSArrayBuffer on different pages before triggering a
  // full GC that performs the semispace copy. If parallelized, this test
  // ensures proper synchronization in TSAN configurations.
  v8_flags.min_semi_space_size = std::max(2 * NormalPage::kPageSize / MB, 1);
  v8::Isolate::CreateParams create_params;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  create_params.array_buffer_allocator = allocator.get();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);
    Heap* heap = i_isolate->heap();

    // Ensure heap is in a clean state.
    InvokeMajorGC(i_isolate);
    InvokeMajorGC(i_isolate);

    Local<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf1 = v8::Utils::OpenDirectHandle(*ab1);
    FillCurrentPage(heap->new_space());
    Local<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(isolate, 100);
    DirectHandle<JSArrayBuffer> buf2 = v8::Utils::OpenDirectHandle(*ab2);
    CHECK_NE(NormalPage::FromHeapObject(*buf1),
             NormalPage::FromHeapObject(*buf2));
    InvokeAtomicMajorGC(i_isolate);
  }
  isolate->Dispose();
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_ExternalBackingStoreSizeIncreases) {
  if (v8_flags.single_generation) return;
  v8::Isolate* isolate = v8_isolate();

  const size_t backing_store_before =
      heap()->array_buffer_sweeper()->BytesForTesting();
  {
    const size_t kArraybufferSize = 117;
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, kArraybufferSize);
    USE(ab);
    const size_t backing_store_after =
        heap()->array_buffer_sweeper()->BytesForTesting();
    CHECK_EQ(kArraybufferSize, backing_store_after - backing_store_before);
  }
}

TEST_F(ArrayBufferSweeperTest, ArrayBuffer_ExternalBackingStoreSizeDecreases) {
  if (v8_flags.single_generation) return;
  v8_flags.concurrent_array_buffer_sweeping = false;
  v8::Isolate* isolate = v8_isolate();

  const size_t backing_store_before =
      heap()->array_buffer_sweeper()->BytesForTesting();
  {
    const size_t kArraybufferSize = 117;
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, kArraybufferSize);
    USE(ab);
  }
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
  InvokeAtomicMajorGC();
  const size_t backing_store_after =
      heap()->array_buffer_sweeper()->BytesForTesting();
  CHECK_EQ(0u, backing_store_after - backing_store_before);
}

TEST_F(ArrayBufferSweeperTest,
       ArrayBuffer_ExternalBackingStoreSizeIncreasesMarkCompact) {
  if (!v8_flags.compact) return;
  ManualGCScope manual_gc_scope(i_isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  v8_flags.concurrent_array_buffer_sweeping = false;
  v8::Isolate* isolate = v8_isolate();
  AbandonCurrentlyFreeMemory(heap()->old_space());

  // We need to invoke GC without stack, otherwise some objects may survive.
  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());

  const size_t backing_store_before =
      heap()->array_buffer_sweeper()->BytesForTesting();

  const size_t kArraybufferSize = 117;
  {
    v8::HandleScope handle_scope(isolate);
    Local<v8::ArrayBuffer> ab1 =
        v8::ArrayBuffer::New(isolate, kArraybufferSize);
    IndirectHandle<JSArrayBuffer> buf1 = v8::Utils::OpenIndirectHandle(*ab1);
    CHECK(IsTracked(heap(), *buf1));
    InvokeAtomicMajorGC();

    NormalPage* page_before_gc = NormalPage::FromHeapObject(*buf1);
    ForceEvacuationCandidate(page_before_gc);
    CHECK(IsTracked(heap(), *buf1));

    InvokeMajorGC();

    const size_t backing_store_after =
        heap()->array_buffer_sweeper()->BytesForTesting();
    CHECK_EQ(kArraybufferSize, backing_store_after - backing_store_before);
  }

  InvokeAtomicMajorGC();
  const size_t backing_store_after =
      heap()->array_buffer_sweeper()->BytesForTesting();
  CHECK_EQ(0u, backing_store_after - backing_store_before);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

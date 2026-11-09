// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/marking-state-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"

namespace v8::internal {

using HeapWriteBarrierTest = TestWithHeapInternalsAndContext;

#if V8_VERIFY_WRITE_BARRIERS

TEST_F(HeapWriteBarrierTest, NoSafepointInWriteBarrierModeScope) {
  LocalHeap* local_heap = isolate()->main_thread_local_heap();
  EXPECT_DEATH_IF_SUPPORTED(
      {
        WriteBarrierModeScope scope(
            *isolate()->roots_table().empty_fixed_array(),
            SKIP_WRITE_BARRIER_SCOPE);
        local_heap->Safepoint();
      },
      "");
}

TEST_F(HeapWriteBarrierTest, NoAllocationInWriteBarrierModeScope) {
  HandleScope handle_scope(isolate());
  EXPECT_DEATH_IF_SUPPORTED(
      {
        WriteBarrierModeScope scope(
            *isolate()->roots_table().empty_fixed_array(),
            SKIP_WRITE_BARRIER_SCOPE);
        isolate()->factory()->NewFixedArray(1);
      },
      "");
}

TEST_F(HeapWriteBarrierTest, NoSkipWriteBarrierOnOldObject) {
  HandleScope scope(isolate());
  DirectHandle<HeapNumber> number = i_isolate()->factory()->NewHeapNumber(10.0);
  DirectHandle<FixedArray> latest =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
  EXPECT_DEATH_IF_SUPPORTED(
      { latest->set(0, *number, SKIP_WRITE_BARRIER); }, "");
}

TEST_F(HeapWriteBarrierTest, NoSkipWriteBarrierOnPreviousYoungAllocation) {
  HandleScope scope(isolate());
  DirectHandle<HeapNumber> number = i_isolate()->factory()->NewHeapNumber(10.0);
  DirectHandle<FixedArray> previous =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kYoung);
  DirectHandle<FixedArray> latest =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kYoung);
  latest->set(0, *number, SKIP_WRITE_BARRIER);
  EXPECT_DEATH_IF_SUPPORTED(
      { previous->set(0, *number, SKIP_WRITE_BARRIER); }, "");
}

TEST_F(HeapWriteBarrierTest,
       NoSkipWriteBarrierOnYoungAllocationAfterSafepoint) {
  HandleScope scope(isolate());
  DirectHandle<HeapNumber> number = i_isolate()->factory()->NewHeapNumber(10.0);
  DirectHandle<FixedArray> latest =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kYoung);
  i_isolate()->main_thread_local_heap()->Safepoint();
  EXPECT_DEATH_IF_SUPPORTED(
      { latest->set(0, *number, SKIP_WRITE_BARRIER); }, "");
}

#endif  // V8_VERIFY_WRITE_BARRIERS

TEST_F(HeapWriteBarrierTest, WriteBarrier_Marking) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(i_isolate());
  DirectHandle<FixedArray> objects = factory()->NewFixedArray(3);
  v8::Global<Value> global_objects(v8_isolate(), Utils::ToLocal(objects));
  {
    // Make sure that these objects are not immediately reachable from
    // the roots to prevent them being marked grey at the start of marking.
    HandleScope inner(i_isolate());
    DirectHandle<FixedArray> host = factory()->NewFixedArray(1);
    DirectHandle<HeapNumber> value1 = factory()->NewHeapNumber(1.1);
    DirectHandle<HeapNumber> value2 = factory()->NewHeapNumber(1.2);
    objects->set(0, *host);
    objects->set(1, *value1);
    objects->set(2, *value2);
  }
  SimulateIncrementalMarking(false);
  Tagged<FixedArray> host = Cast<FixedArray>(objects->get(0));
  Tagged<HeapObject> value1 = Cast<HeapObject>(objects->get(1));
  Tagged<HeapObject> value2 = Cast<HeapObject>(objects->get(2));
  EXPECT_TRUE(heap()->marking_state()->IsUnmarked(host));
  EXPECT_TRUE(heap()->marking_state()->IsUnmarked(value1));
  // Trigger the barrier for the unmarked host and expect the bail out.
  WriteBarrier::MarkingForTesting(host, host->RawFieldOfElementAt(0), value1);
  EXPECT_TRUE(heap()->marking_state()->IsMarked(value1));

  EXPECT_TRUE(heap()->marking_state()->IsUnmarked(value2));
  WriteBarrier::MarkingForTesting(host, host->RawFieldOfElementAt(0), value2);
  EXPECT_TRUE(heap()->marking_state()->IsMarked(value2));
  SimulateIncrementalMarking(true);
  EXPECT_TRUE(heap()->marking_state()->IsMarked(host));
  EXPECT_TRUE(heap()->marking_state()->IsMarked(value1));
  EXPECT_TRUE(heap()->marking_state()->IsMarked(value2));
}

TEST_F(HeapWriteBarrierTest, WriteBarrier_MarkingExtension) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(i_isolate());
  DirectHandle<FixedArray> objects = factory()->NewFixedArray(1);
  ArrayBufferExtension* extension;
  {
    HandleScope inner(i_isolate());
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(v8_isolate(), 100);
    DirectHandle<JSArrayBuffer> host = v8::Utils::OpenDirectHandle(*ab);
    extension = host->extension();
    objects->set(0, *host);
  }
  SimulateIncrementalMarking(false);
  Tagged<JSArrayBuffer> host = Cast<JSArrayBuffer>(objects->get(0));
  EXPECT_TRUE(heap()->marking_state()->IsUnmarked(host));
  EXPECT_FALSE(extension->IsMarked());
  WriteBarrier::ForArrayBufferExtension(host, extension);
  // Concurrent marking barrier should mark the value now.
  EXPECT_TRUE(extension->IsMarked());
  // Keep object alive using the global handle.
  v8::Global<ArrayBuffer> global_host(
      v8_isolate(), Utils::ToLocal(direct_handle(host, i_isolate())));
  SimulateIncrementalMarking(true);
  EXPECT_TRUE(heap()->marking_state()->IsMarked(host));
  EXPECT_TRUE(extension->IsMarked());
}

}  // namespace v8::internal

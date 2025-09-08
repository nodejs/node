// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/local-heap.h"
#include "src/objects/objects.h"
#include "test/unittests/test-utils.h"

namespace v8::internal {

using HeapWriteBarrierTest = TestWithIsolate;

#if V8_VERIFY_WRITE_BARRIERS

TEST_F(HeapWriteBarrierTest, NoSafepointInWriteBarrierModeScope) {
  v8_flags.verify_write_barriers = true;
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
  v8_flags.verify_write_barriers = true;
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
  if (v8_flags.disable_write_barriers) return;
  v8_flags.verify_write_barriers = true;
  HandleScope scope(isolate());
  DirectHandle<HeapNumber> number = i_isolate()->factory()->NewHeapNumber(10.0);
  DirectHandle<FixedArray> latest =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kOld);
  EXPECT_DEATH_IF_SUPPORTED(
      { latest->set(0, *number, SKIP_WRITE_BARRIER); }, "");
}

TEST_F(HeapWriteBarrierTest, NoSkipWriteBarrierOnPreviousYoungAllocation) {
  if (v8_flags.disable_write_barriers) return;
  v8_flags.verify_write_barriers = true;
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
  if (v8_flags.disable_write_barriers) return;
  v8_flags.verify_write_barriers = true;
  HandleScope scope(isolate());
  DirectHandle<HeapNumber> number = i_isolate()->factory()->NewHeapNumber(10.0);
  DirectHandle<FixedArray> latest =
      i_isolate()->factory()->NewFixedArray(1, AllocationType::kYoung);
  i_isolate()->main_thread_local_heap()->Safepoint();
  EXPECT_DEATH_IF_SUPPORTED(
      { latest->set(0, *number, SKIP_WRITE_BARRIER); }, "");
}

#endif  // V8_VERIFY_WRITE_BARRIERS

}  // namespace v8::internal

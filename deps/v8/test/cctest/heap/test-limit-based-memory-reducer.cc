// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-controller.h"
#include "src/heap/safepoint.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

class MemoryReducerMockPlatform : public MockPlatform {
 public:
  MemoryReducerMockPlatform() : MockPlatform() {
    // The MemoryReducer is constructed alongside the Heap, which happens
    // before the test bodies are run. So, we choose which MemoryReducer we use
    // here.
    v8_flags.memory_reducer_limit_based = true;
  }
};

TEST_WITH_PLATFORM(LimitBasedMemoryReducerTriggersGC,
                   MemoryReducerMockPlatform) {
  if (!i::v8_flags.incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  v8_flags.stress_incremental_marking = false;
  v8_flags.memory_reducer_delay_ms = 0;
  v8::Isolate* isolate = CcTest::isolate();
  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = CcTest::NewContext(isolate);
    v8::Context::Scope context_scope(context);
    Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    Heap* heap = i_isolate->heap();

    i::IncrementalMarking* marking = heap->incremental_marking();
    auto* memory_reducer =
        static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

    heap->CollectGarbage(i::OLD_SPACE, i::GarbageCollectionReason::kTesting);

    CHECK(platform.PendingTask());
    CHECK(memory_reducer->is_scheduled());
    CHECK(platform.PendingTask());

    i::heap::SimulateFullSpace(heap->old_space());
    i_isolate->factory()->NewFixedArray(
        v8::internal::V8HeapTrait::kMinimumAllocationLimitGrowingStep,
        AllocationType::kOld);

    // Run the MemoryReducer task.
    platform.PerformTask();

    // Check that we do a GC here, and reschedule memory reducer.
    CHECK(!marking->IsStopped());
    CHECK(platform.PendingTask());
    CHECK(!memory_reducer->is_scheduled());

    CHECK(marking->IsMajorMarking());
    while (marking->IsMajorMarking()) {
      platform.PerformTask();
    }
    CHECK(marking->IsStopped());

    // We did a GC, so memory reducer should be rescheduled.
    CHECK(platform.PendingTask());
    CHECK(memory_reducer->is_scheduled());

    // Run the memory reducer task. Since we haven't allocated since the last
    // GC, this should not reschedule.
    platform.PerformTask();

    CHECK(!platform.PendingTask());
    CHECK(!memory_reducer->is_scheduled());
    CHECK(marking->IsStopped());
  }
}

TEST_WITH_PLATFORM(LimitBasedMemoryReducerNotifyMarkCompact,
                   MemoryReducerMockPlatform) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms = 0;

  ManualGCScope manual_gc_scope;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

  LocalContext env;
  HandleScope scope(isolate);
  heap->limits()->UpdateConsumedAfterGC();

  CHECK(!platform.PendingTask());
  CHECK(!memory_reducer->is_scheduled());

  // Trigger MemoryReducer and run a GC.
  memory_reducer->NotifyMarkCompact(0);

  CHECK(memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());
  platform.PerformTask();

  // We did not do a GC, so don't reschedule.
  CHECK(!platform.PendingTask());
  CHECK(!memory_reducer->is_scheduled());
}

TEST_WITH_PLATFORM(LimitBasedMemoryReducerBackgrounded,
                   MemoryReducerMockPlatform) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms = 0;

  ManualGCScope manual_gc_scope;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

  LocalContext env;
  HandleScope scope(isolate);
  heap->limits()->UpdateConsumedAfterGC();

  CHECK(!memory_reducer->is_scheduled());
  CHECK(!platform.PendingTask());

  // Send to background.
  isolate->SetPriority(v8::Isolate::Priority::kBestEffort);

  // MemoryReducer should not be running yet, but the task to start it should
  // be scheduled by us going to background.
  CHECK(!memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());

  // Run the task to schedule MemoryReducer.
  platform.PerformTask();

  // Verify that MemoryReducer was scheduled.
  CHECK(memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());

  // Run the memory reducer task.
  platform.PerformTask();

  // Check that we don't reschedule the memory reducer here, because we didn't
  // do a GC.
  CHECK(!memory_reducer->is_scheduled());
  CHECK(!platform.PendingTask());
}

TEST_WITH_PLATFORM(LimitBasedMemoryReducerOvershoot,
                   MemoryReducerMockPlatform) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms = 0;

  ManualGCScope manual_gc_scope;
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  i::IncrementalMarking* marking = heap->incremental_marking();
  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

  LocalContext env;
  HandleScope scope(isolate);
  heap->limits()->UpdateConsumedAfterGC();

  // Allocate 20MB in old space after the simulated GC and start major
  // incremental marking.
  v8_flags.memory_reducer_for_small_heaps = false;
  for (int i = 0; i < 20; i++) {
    isolate->factory()->NewFixedArray(1024 * 1024 / i::kTaggedSize,
                                      AllocationType::kOld);
  }
  heap->StartIncrementalMarking(i::GCFlag::kNoFlags,
                                i::GarbageCollectionReason::kTesting);
  CHECK(marking->IsMajorMarking());

  size_t limit_before = heap->limits()->old_generation_allocation_limit();
  size_t consumed = heap->OldGenerationConsumedBytes();
  CHECK_GE(limit_before, consumed);

  // Send to background while major marking is running, which schedules
  // MemoryReducer.
  isolate->SetPriority(v8::Isolate::Priority::kBestEffort);
  CHECK(!memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());

  // Run ActivateMemoryReducerTask to schedule LimitBasedMemoryReducer.
  platform.PerformTask();
  CHECK(memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());

  // Run LimitBasedMemoryReducer::TimerTask while major marking is running.
  platform.PerformTask();
  CHECK(!memory_reducer->is_scheduled());

  // Limits must not be shrunk while major incremental marking is running.
  CHECK_EQ(limit_before, heap->limits()->old_generation_allocation_limit());
  CHECK_GE(heap->limits()->old_generation_allocation_limit(), consumed);
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

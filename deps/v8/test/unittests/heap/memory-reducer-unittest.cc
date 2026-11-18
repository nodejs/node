// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-reducer.h"

#include <limits>
#include <memory>
#include <utility>

#include "include/cppgc/heap-state.h"
#include "include/v8-platform.h"
#include "src/flags/flags.h"
#include "src/heap/cppgc-internal/heap-base.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/libplatform/default-platform.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace memory_reducer_unittest {

MemoryReducer::Event MarkCompactEvent(double time_ms,
                                      bool next_gc_likely_to_collect_more,
                                      size_t committed_memory) {
  MemoryReducer::Event event;
  event.type = MemoryReducer::kMarkCompact;
  event.time_ms = time_ms;
  event.next_gc_likely_to_collect_more = next_gc_likely_to_collect_more;
  event.committed_memory = committed_memory;
  return event;
}

MemoryReducer::Event MarkCompactEventGarbageLeft(double time_ms,
                                                 size_t committed_memory) {
  return MarkCompactEvent(time_ms, true, committed_memory);
}

MemoryReducer::Event MarkCompactEventNoGarbageLeft(double time_ms,
                                                   size_t committed_memory) {
  return MarkCompactEvent(time_ms, false, committed_memory);
}


MemoryReducer::Event TimerEvent(double time_ms,
                                bool should_start_incremental_gc,
                                bool can_start_incremental_gc) {
  MemoryReducer::Event event;
  event.type = MemoryReducer::kTimer;
  event.time_ms = time_ms;
  event.should_start_incremental_gc = should_start_incremental_gc;
  event.can_start_incremental_gc = can_start_incremental_gc;
  return event;
}

MemoryReducer::Event TimerEventLowAllocationRate(double time_ms) {
  return TimerEvent(time_ms, true, true);
}


MemoryReducer::Event TimerEventHighAllocationRate(double time_ms) {
  return TimerEvent(time_ms, false, true);
}


MemoryReducer::Event TimerEventPendingGC(double time_ms) {
  return TimerEvent(time_ms, true, false);
}

MemoryReducer::Event PossibleGarbageEvent(double time_ms) {
  MemoryReducer::Event event;
  event.type = MemoryReducer::kPossibleGarbage;
  event.time_ms = time_ms;
  return event;
}


TEST(MemoryReducer, FromDoneToDone) {
  MemoryReducer::State state0(MemoryReducer::State::CreateDone(1.0, 0)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(state0, TimerEventLowAllocationRate(0));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());

  state1 = MemoryReducer::Step(state0, TimerEventHighAllocationRate(0));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());

  state1 = MemoryReducer::Step(state0, TimerEventPendingGC(0));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());

  state1 = MemoryReducer::Step(
      state0,
      MarkCompactEventGarbageLeft(0, MemoryReducer::kCommittedMemoryDelta - 1));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());

  state0 = MemoryReducer::State::CreateDone(1, 1000 * MB);
  state1 = MemoryReducer::Step(
      state0, MarkCompactEventGarbageLeft(
                  0, static_cast<size_t>(
                         1000 * MB * MemoryReducer::kCommittedMemoryFactor) -
                         1));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
}


TEST(MemoryReducer, FromDoneToWait) {
  if (!v8_flags.incremental_marking) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateDone(1.0, 0)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(
      state0,
      MarkCompactEventGarbageLeft(2, MemoryReducer::kCommittedMemoryDelta));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(v8_flags.memory_reducer_delay_ms + 2, state1.next_gc_start_ms());
  EXPECT_EQ(0, state1.started_gcs());
  EXPECT_EQ(2, state1.last_gc_time_ms());

  state1 = MemoryReducer::Step(
      state0,
      MarkCompactEventNoGarbageLeft(2, MemoryReducer::kCommittedMemoryDelta));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(v8_flags.memory_reducer_delay_ms + 2, state1.next_gc_start_ms());
  EXPECT_EQ(0, state1.started_gcs());
  EXPECT_EQ(2, state1.last_gc_time_ms());

  state1 = MemoryReducer::Step(state0, PossibleGarbageEvent(0));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(v8_flags.gc_memory_reducer_start_delay_ms,
            state1.next_gc_start_ms());
  EXPECT_EQ(0, state1.started_gcs());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());

  state0 = MemoryReducer::State::CreateDone(1, 1000 * MB);
  state1 = MemoryReducer::Step(
      state0, MarkCompactEventGarbageLeft(
                  2, static_cast<size_t>(
                         1000 * MB * MemoryReducer::kCommittedMemoryFactor)));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(v8_flags.memory_reducer_delay_ms + 2, state1.next_gc_start_ms());
  EXPECT_EQ(0, state1.started_gcs());
  EXPECT_EQ(2, state1.last_gc_time_ms());
}


TEST(MemoryReducer, FromWaitToWait) {
  if (!v8_flags.incremental_marking) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateWait(
      MemoryReducer::MaxNumberOfGCs() - 1, 1000.0, 1)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(state0, PossibleGarbageEvent(2000));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(state0.next_gc_start_ms(), state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(
      state0, TimerEventLowAllocationRate(state0.next_gc_start_ms() - 1));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(state0.next_gc_start_ms(), state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, TimerEventHighAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + v8_flags.memory_reducer_delay_ms, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, TimerEventPendingGC(2000));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + v8_flags.memory_reducer_delay_ms, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, MarkCompactEventGarbageLeft(2000, 0));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + v8_flags.memory_reducer_delay_ms, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
  EXPECT_EQ(2000, state1.last_gc_time_ms());

  state1 = MemoryReducer::Step(state0, MarkCompactEventNoGarbageLeft(2000, 0));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + v8_flags.memory_reducer_delay_ms, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
  EXPECT_EQ(2000, state1.last_gc_time_ms());

  state0 = MemoryReducer::State::CreateWait(MemoryReducer::MaxNumberOfGCs() - 1,
                                            1000.0, 0);

  state1 = MemoryReducer::Step(
      state0,
      TimerEventHighAllocationRate(MemoryReducer::kWatchdogDelayMs + 1));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(
      MemoryReducer::kWatchdogDelayMs + 1 + v8_flags.memory_reducer_delay_ms,
      state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());

  state0 = MemoryReducer::State::CreateWait(MemoryReducer::MaxNumberOfGCs() - 1,
                                            1000.0, 1);
  state1 = MemoryReducer::Step(state0, TimerEventHighAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + v8_flags.memory_reducer_delay_ms, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());
}


TEST(MemoryReducer, FromWaitToRun) {
  if (!v8_flags.incremental_marking) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateWait(0, 1000.0, 1)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(
      state0, TimerEventLowAllocationRate(state0.next_gc_start_ms() + 1));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs() + 1, state1.started_gcs());

  state1 = MemoryReducer::Step(
      state0,
      TimerEventHighAllocationRate(MemoryReducer::kWatchdogDelayMs + 2));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs() + 1, state1.started_gcs());
}


TEST(MemoryReducer, FromWaitToDone) {
  if (!v8_flags.incremental_marking) return;
  if (MemoryReducer::MaxNumberOfGCs() <= 1) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateWait(
      MemoryReducer::MaxNumberOfGCs(), 0.0, 1)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(state0, TimerEventLowAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());

  state1 = MemoryReducer::Step(state0, TimerEventHighAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());

  state1 = MemoryReducer::Step(state0, TimerEventPendingGC(2000));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
  EXPECT_EQ(state0.last_gc_time_ms(), state1.last_gc_time_ms());
}


TEST(MemoryReducer, FromRunToRun) {
  if (!v8_flags.incremental_marking) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateRun(1)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  state1 = MemoryReducer::Step(state0, TimerEventLowAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, TimerEventHighAllocationRate(2000));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, TimerEventPendingGC(2000));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());

  state1 = MemoryReducer::Step(state0, PossibleGarbageEvent(2000));
  EXPECT_EQ(MemoryReducer::kRun, state1.id());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
}


TEST(MemoryReducer, FromRunToDone) {
  if (!v8_flags.incremental_marking) return;

  const int started_gcs = MemoryReducer::MaxNumberOfGCs() > 1 ? 2 : 1;
  MemoryReducer::State state0(MemoryReducer::State::CreateRun(started_gcs));
  MemoryReducer::State state1 =
      MemoryReducer::Step(state0, MarkCompactEventNoGarbageLeft(2000, 0));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
  EXPECT_EQ(2000, state1.last_gc_time_ms());

  state0 = MemoryReducer::State::CreateRun(MemoryReducer::MaxNumberOfGCs());

  state1 = MemoryReducer::Step(state0, MarkCompactEventGarbageLeft(2000, 0));
  EXPECT_EQ(MemoryReducer::kDone, state1.id());
}


TEST(MemoryReducer, FromRunToWait) {
  if (!v8_flags.incremental_marking) return;
  if (MemoryReducer::MaxNumberOfGCs() <= 1) return;

  MemoryReducer::State state0(MemoryReducer::State::CreateRun(2)),
      state1(MemoryReducer::State::CreateDone(1.0, 0));

  if (MemoryReducer::MaxNumberOfGCs() > 2) {
    state1 = MemoryReducer::Step(state0, MarkCompactEventGarbageLeft(2000, 0));
    EXPECT_EQ(MemoryReducer::kWait, state1.id());
    EXPECT_EQ(2000 + MemoryReducer::kShortDelayMs, state1.next_gc_start_ms());
    EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
    EXPECT_EQ(2000, state1.last_gc_time_ms());
  }

  state0 = MemoryReducer::State::CreateRun(1);

  state1 = MemoryReducer::Step(state0, MarkCompactEventNoGarbageLeft(2000, 0));
  EXPECT_EQ(MemoryReducer::kWait, state1.id());
  EXPECT_EQ(2000 + MemoryReducer::kShortDelayMs, state1.next_gc_start_ms());
  EXPECT_EQ(state0.started_gcs(), state1.started_gcs());
  EXPECT_EQ(2000, state1.last_gc_time_ms());
}

namespace {

class MockTaskRunner final : public v8::TaskRunner {
 public:
  void PostTaskImpl(std::unique_ptr<v8::Task> task,
                    const v8::SourceLocation& location) override {
    task_ = std::move(task);
  }

  void PostNonNestableTaskImpl(std::unique_ptr<v8::Task> task,
                               const v8::SourceLocation& location) override {
    PostTask(std::move(task));
  }

  void PostDelayedTaskImpl(std::unique_ptr<v8::Task> task,
                           double delay_in_seconds,
                           const v8::SourceLocation& location) override {
    delay_ = delay_in_seconds;
    PostTask(std::move(task));
  }

  void PostNonNestableDelayedTaskImpl(
      std::unique_ptr<v8::Task> task, double delay_in_seconds,
      const v8::SourceLocation& location) override {
    PostDelayedTaskImpl(std::move(task), delay_in_seconds, location);
  }

  void PostIdleTaskImpl(std::unique_ptr<v8::IdleTask> task,
                        const v8::SourceLocation& location) override {
    UNREACHABLE();
  }

  bool IdleTasksEnabled() override { return false; }
  bool NonNestableTasksEnabled() const override { return true; }
  bool NonNestableDelayedTasksEnabled() const override { return true; }

  bool PendingTask() const { return task_ != nullptr; }

  void PerformTask() {
    std::unique_ptr<v8::Task> task = std::move(task_);
    task->Run();
  }

 private:
  double delay_ = -1;
  std::unique_ptr<v8::Task> task_;
};

class MemoryReducerMockPlatform final : public v8::platform::DefaultPlatform {
 public:
  explicit MemoryReducerMockPlatform(v8::Platform* default_platform)
      : default_platform_(default_platform),
        task_runner_(std::make_shared<MockTaskRunner>()) {
    CHECK_NOT_NULL(default_platform_);
    // The MemoryReducer is constructed alongside the Heap, which happens
    // before the test bodies are run. So, we choose which MemoryReducer we use
    // here.
    v8_flags.memory_reducer_limit_based.Set(true);
    v8_flags.stress_incremental_marking.Set(false);
    EnsureBackgroundTaskRunnerInitialized();
    V8::SetPlatformForTesting(this);
  }

  ~MemoryReducerMockPlatform() override {
    if (V8::GetCurrentPlatform() == this) {
      V8::SetPlatformForTesting(default_platform_);
    }
  }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate*, v8::TaskPriority) override {
    return task_runner_;
  }

  bool PendingTask() const { return task_runner_->PendingTask(); }
  void PerformTask() { task_runner_->PerformTask(); }

 private:
  SaveFlags save_flags_;
  v8::Platform* default_platform_;
  std::shared_ptr<MockTaskRunner> task_runner_;
};

template <typename TMixin>
class WithMemoryReducerMockPlatformMixin : public TMixin {
 public:
  WithMemoryReducerMockPlatformMixin() : mock_platform_(this->platform()) {}
  WithMemoryReducerMockPlatformMixin(
      const WithMemoryReducerMockPlatformMixin&) = delete;
  WithMemoryReducerMockPlatformMixin& operator=(
      const WithMemoryReducerMockPlatformMixin&) = delete;

  MemoryReducerMockPlatform& mock_platform() { return mock_platform_; }

 private:
  MemoryReducerMockPlatform mock_platform_;
};

using LimitBasedMemoryReducerTest =                              //
    WithContextMixin<                                            //
        WithHeapInternals<                                       //
            WithInternalIsolateMixin<                            //
                WithIsolateScopeMixin<                           //
                    WithIsolateMixin<                            //
                        WithCppHeap<                             //
                            WithMemoryReducerMockPlatformMixin<  //
                                TestWithPlatform>>>>>>>;

}  // namespace

TEST_F(LimitBasedMemoryReducerTest, LimitBasedMemoryReducerTriggersGC) {
  if (!v8_flags.incremental_marking) return;
  v8_flags.stress_concurrent_allocation.Set(false);  // For SimulateFullSpace.
  v8_flags.stress_incremental_marking.Set(false);
  v8_flags.memory_reducer_delay_ms.Set(0);

  Isolate* i_isolate = this->i_isolate();
  Heap* heap = this->heap();
  MemoryReducerMockPlatform& platform = mock_platform();

  IncrementalMarking* marking = heap->incremental_marking();
  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

  heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

  CHECK(platform.PendingTask());
  CHECK(memory_reducer->is_scheduled());
  CHECK(platform.PendingTask());

  SimulateFullSpace(heap->old_space());
  i_isolate->factory()->NewFixedArray(
      V8HeapTrait::kMinimumAllocationLimitGrowingStep / kTaggedSize,
      AllocationType::kOld);

  // Run the MemoryReducer task.
  platform.PerformTask();

  // Check that we do a GC here, and reschedule memory reducer.
  CHECK(!marking->IsStopped());
  CHECK(heap->ShouldReduceMemory());
  CHECK(platform.PendingTask());
  CHECK(!memory_reducer->is_scheduled());

  CHECK(marking->IsMajorMarking());
  while (marking->IsMajorMarking()) {
    platform.PerformTask();
  }
  CHECK(marking->IsStopped());
  CHECK_EQ(cppgc::EmbedderStackState::kNoHeapPointers,
           cppgc::internal::HeapBase::From(heap->cpp_heap()->GetHeapHandle())
               .stack_state_of_prev_gc());

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

TEST_F(LimitBasedMemoryReducerTest, LimitBasedMemoryReducerNotifyMarkCompact) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms.Set(0);

  ManualGCScope manual_gc_scope(i_isolate());
  Heap* heap = this->heap();
  MemoryReducerMockPlatform& platform = mock_platform();

  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

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

TEST_F(LimitBasedMemoryReducerTest, LimitBasedMemoryReducerBackgrounded) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms.Set(0);

  ManualGCScope manual_gc_scope(i_isolate());
  Isolate* isolate = i_isolate();
  Heap* heap = this->heap();
  MemoryReducerMockPlatform& platform = mock_platform();

  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

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

TEST_F(LimitBasedMemoryReducerTest, LimitBasedMemoryReducerOvershoot) {
  if (v8_flags.single_generation || !v8_flags.memory_reducer) return;
  v8_flags.memory_reducer_delay_ms.Set(0);

  ManualGCScope manual_gc_scope(i_isolate());
  Isolate* isolate = i_isolate();
  Heap* heap = this->heap();
  MemoryReducerMockPlatform& platform = mock_platform();

  IncrementalMarking* marking = heap->incremental_marking();
  auto* memory_reducer =
      static_cast<LimitBasedMemoryReducer*>(heap->memory_reducer());

  heap->limits()->UpdateConsumedAfterGC();

  // Allocate 20MB in old space after the simulated GC and start major
  // incremental marking.
  v8_flags.memory_reducer_for_small_heaps.Set(false);
  for (int i = 0; i < 20; i++) {
    isolate->factory()->NewFixedArray(1024 * 1024 / kTaggedSize,
                                      AllocationType::kOld);
  }
  heap->StartIncrementalMarking(GCFlag::kNoFlags,
                                GarbageCollectionReason::kTesting);
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

}  // namespace memory_reducer_unittest

}  // namespace internal
}  // namespace v8

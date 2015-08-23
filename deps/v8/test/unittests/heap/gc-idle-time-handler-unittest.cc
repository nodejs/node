// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/heap/gc-idle-time-handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

class GCIdleTimeHandlerTest : public ::testing::Test {
 public:
  GCIdleTimeHandlerTest() {}
  virtual ~GCIdleTimeHandlerTest() {}

  GCIdleTimeHandler* handler() { return &handler_; }

  GCIdleTimeHandler::HeapState DefaultHeapState() {
    GCIdleTimeHandler::HeapState result;
    result.contexts_disposed = 0;
    result.contexts_disposal_rate = GCIdleTimeHandler::kHighContextDisposalRate;
    result.size_of_objects = kSizeOfObjects;
    result.incremental_marking_stopped = false;
    result.sweeping_in_progress = false;
    result.sweeping_completed = false;
    result.mark_compact_speed_in_bytes_per_ms = kMarkCompactSpeed;
    result.incremental_marking_speed_in_bytes_per_ms = kMarkingSpeed;
    result.scavenge_speed_in_bytes_per_ms = kScavengeSpeed;
    result.used_new_space_size = 0;
    result.new_space_capacity = kNewSpaceCapacity;
    result.new_space_allocation_throughput_in_bytes_per_ms =
        kNewSpaceAllocationThroughput;
    return result;
  }

  static const size_t kSizeOfObjects = 100 * MB;
  static const size_t kMarkCompactSpeed = 200 * KB;
  static const size_t kMarkingSpeed = 200 * KB;
  static const size_t kScavengeSpeed = 100 * KB;
  static const size_t kNewSpaceCapacity = 1 * MB;
  static const size_t kNewSpaceAllocationThroughput = 10 * KB;
  static const int kMaxNotifications = 100;

 private:
  GCIdleTimeHandler handler_;
};

}  // namespace


TEST(GCIdleTimeHandler, EstimateMarkingStepSizeInitial) {
  size_t step_size = GCIdleTimeHandler::EstimateMarkingStepSize(1, 0);
  EXPECT_EQ(
      static_cast<size_t>(GCIdleTimeHandler::kInitialConservativeMarkingSpeed *
                          GCIdleTimeHandler::kConservativeTimeRatio),
      step_size);
}


TEST(GCIdleTimeHandler, EstimateMarkingStepSizeNonZero) {
  size_t marking_speed_in_bytes_per_millisecond = 100;
  size_t step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
      1, marking_speed_in_bytes_per_millisecond);
  EXPECT_EQ(static_cast<size_t>(marking_speed_in_bytes_per_millisecond *
                                GCIdleTimeHandler::kConservativeTimeRatio),
            step_size);
}


TEST(GCIdleTimeHandler, EstimateMarkingStepSizeOverflow1) {
  size_t step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
      10, std::numeric_limits<size_t>::max());
  EXPECT_EQ(static_cast<size_t>(GCIdleTimeHandler::kMaximumMarkingStepSize),
            step_size);
}


TEST(GCIdleTimeHandler, EstimateMarkingStepSizeOverflow2) {
  size_t step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
      std::numeric_limits<size_t>::max(), 10);
  EXPECT_EQ(static_cast<size_t>(GCIdleTimeHandler::kMaximumMarkingStepSize),
            step_size);
}


TEST(GCIdleTimeHandler, EstimateMarkCompactTimeInitial) {
  size_t size = 100 * MB;
  size_t time = GCIdleTimeHandler::EstimateMarkCompactTime(size, 0);
  EXPECT_EQ(size / GCIdleTimeHandler::kInitialConservativeMarkCompactSpeed,
            time);
}


TEST(GCIdleTimeHandler, EstimateMarkCompactTimeNonZero) {
  size_t size = 100 * MB;
  size_t speed = 1 * MB;
  size_t time = GCIdleTimeHandler::EstimateMarkCompactTime(size, speed);
  EXPECT_EQ(size / speed, time);
}


TEST(GCIdleTimeHandler, EstimateMarkCompactTimeMax) {
  size_t size = std::numeric_limits<size_t>::max();
  size_t speed = 1;
  size_t time = GCIdleTimeHandler::EstimateMarkCompactTime(size, speed);
  EXPECT_EQ(GCIdleTimeHandler::kMaxMarkCompactTimeInMs, time);
}


TEST_F(GCIdleTimeHandlerTest, DoScavengeEmptyNewSpace) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  int idle_time_ms = 16;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, DoScavengeFullNewSpace) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.used_new_space_size = kNewSpaceCapacity;
  int idle_time_ms = 16;
  EXPECT_TRUE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, DoScavengeUnknownScavengeSpeed) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.used_new_space_size = kNewSpaceCapacity;
  heap_state.scavenge_speed_in_bytes_per_ms = 0;
  int idle_time_ms = 8;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, DoScavengeLowScavengeSpeed) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.used_new_space_size = kNewSpaceCapacity;
  heap_state.scavenge_speed_in_bytes_per_ms = 1 * KB;
  int idle_time_ms = 16;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, DoScavengeHighScavengeSpeed) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.used_new_space_size = kNewSpaceCapacity;
  heap_state.scavenge_speed_in_bytes_per_ms = kNewSpaceCapacity;
  int idle_time_ms = 16;
  EXPECT_TRUE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, DoNotScavengeSmallNewSpaceSize) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.used_new_space_size = (MB / 2) - 1;
  heap_state.scavenge_speed_in_bytes_per_ms = kNewSpaceCapacity;
  int idle_time_ms = 16;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoScavenge(
      idle_time_ms, heap_state.new_space_capacity,
      heap_state.used_new_space_size, heap_state.scavenge_speed_in_bytes_per_ms,
      heap_state.new_space_allocation_throughput_in_bytes_per_ms));
}


TEST_F(GCIdleTimeHandlerTest, ShouldDoMarkCompact) {
  size_t idle_time_ms = GCIdleTimeHandler::kMaxScheduledIdleTime;
  EXPECT_TRUE(GCIdleTimeHandler::ShouldDoMarkCompact(idle_time_ms, 0, 0));
}


TEST_F(GCIdleTimeHandlerTest, DontDoMarkCompact) {
  size_t idle_time_ms = 1;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoMarkCompact(
      idle_time_ms, kSizeOfObjects, kMarkingSpeed));
}


TEST_F(GCIdleTimeHandlerTest, ShouldDoFinalIncrementalMarkCompact) {
  size_t idle_time_ms = 16;
  EXPECT_TRUE(GCIdleTimeHandler::ShouldDoFinalIncrementalMarkCompact(
      idle_time_ms, 0, 0));
}


TEST_F(GCIdleTimeHandlerTest, DontDoFinalIncrementalMarkCompact) {
  size_t idle_time_ms = 1;
  EXPECT_FALSE(GCIdleTimeHandler::ShouldDoFinalIncrementalMarkCompact(
      idle_time_ms, kSizeOfObjects, kMarkingSpeed));
}


TEST_F(GCIdleTimeHandlerTest, ContextDisposeLowRate) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_NOTHING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ContextDisposeHighRate) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate - 1;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_FULL_GC, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeZeroIdleTime) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate = 1.0;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_FULL_GC, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, IncrementalMarking1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  size_t speed = heap_state.incremental_marking_speed_in_bytes_per_ms;
  double idle_time_ms = 10;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  EXPECT_GT(speed * static_cast<size_t>(idle_time_ms),
            static_cast<size_t>(action.parameter));
  EXPECT_LT(0, action.parameter);
}


TEST_F(GCIdleTimeHandlerTest, IncrementalMarking2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  size_t speed = heap_state.incremental_marking_speed_in_bytes_per_ms;
  double idle_time_ms = 10;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  EXPECT_GT(speed * static_cast<size_t>(idle_time_ms),
            static_cast<size_t>(action.parameter));
  EXPECT_LT(0, action.parameter);
}


TEST_F(GCIdleTimeHandlerTest, NotEnoughTime) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, FinalizeSweeping) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.sweeping_in_progress = true;
  heap_state.sweeping_completed = true;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_FINALIZE_SWEEPING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, CannotFinalizeSweeping) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.sweeping_in_progress = true;
  heap_state.sweeping_completed = false;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_NOTHING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, Scavenge) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  int idle_time_ms = 10;
  heap_state.used_new_space_size =
      heap_state.new_space_capacity -
      (kNewSpaceAllocationThroughput * idle_time_ms);
  GCIdleTimeAction action =
      handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
  EXPECT_EQ(DO_SCAVENGE, action.type);
  heap_state.used_new_space_size = 0;
}


TEST_F(GCIdleTimeHandlerTest, ScavengeAndDone) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  int idle_time_ms = 10;
  heap_state.incremental_marking_stopped = true;
  heap_state.used_new_space_size =
      heap_state.new_space_capacity -
      (kNewSpaceAllocationThroughput * idle_time_ms);
  GCIdleTimeAction action =
      handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
  EXPECT_EQ(DO_SCAVENGE, action.type);
  heap_state.used_new_space_size = 0;
  action = handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, DoNotStartIncrementalMarking) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ContinueAfterStop) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
  heap_state.incremental_marking_stopped = false;
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ZeroIdleTimeNothingToDo) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(0, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
  }
}


TEST_F(GCIdleTimeHandlerTest, SmallIdleTimeNothingToDo) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(10, heap_state);
    EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
  }
}


TEST_F(GCIdleTimeHandlerTest, DoneIfNotMakingProgressOnSweeping) {
  // Regression test for crbug.com/489323.
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();

  // Simulate sweeping being in-progress but not complete.
  heap_state.incremental_marking_stopped = true;
  heap_state.sweeping_in_progress = true;
  heap_state.sweeping_completed = false;
  double idle_time_ms = 10.0;
  for (int i = 0; i < GCIdleTimeHandler::kMaxNoProgressIdleTimes; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
  }
  // We should return DONE after not making progress for some time.
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, DoneIfNotMakingProgressOnIncrementalMarking) {
  // Regression test for crbug.com/489323.
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();

  // Simulate incremental marking stopped and not eligible to start.
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  // We should return DONE if we cannot start incremental marking.
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}

}  // namespace internal
}  // namespace v8

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
    result.can_start_incremental_marking = true;
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

  void TransitionToReduceMemoryMode(
      const GCIdleTimeHandler::HeapState& heap_state) {
    handler()->NotifyScavenge();
    EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
    double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
    int limit = GCIdleTimeHandler::kLongIdleNotificationsBeforeMutatorIsIdle;
    bool incremental = !heap_state.incremental_marking_stopped ||
                       heap_state.can_start_incremental_marking;
    for (int i = 0; i < limit; i++) {
      GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
      if (incremental) {
        EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
      } else {
        EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
      }
    }
    handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(GCIdleTimeHandler::kReduceMemory, handler()->mode());
  }

  void TransitionToDoneMode(const GCIdleTimeHandler::HeapState& heap_state,
                            double idle_time_ms,
                            GCIdleTimeActionType expected) {
    EXPECT_EQ(GCIdleTimeHandler::kReduceMemory, handler()->mode());
    int limit = GCIdleTimeHandler::kMaxIdleMarkCompacts;
    for (int i = 0; i < limit; i++) {
      GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
      EXPECT_EQ(expected, action.type);
      EXPECT_TRUE(action.reduce_memory);
      handler()->NotifyMarkCompact();
      handler()->NotifyIdleMarkCompact();
    }
    handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(GCIdleTimeHandler::kDone, handler()->mode());
  }

  void TransitionToReduceLatencyMode(
      const GCIdleTimeHandler::HeapState& heap_state) {
    EXPECT_EQ(GCIdleTimeHandler::kDone, handler()->mode());
    int limit = GCIdleTimeHandler::kMarkCompactsBeforeMutatorIsActive;
    double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
    for (int i = 0; i < limit; i++) {
      GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
      EXPECT_EQ(DONE, action.type);
      handler()->NotifyMarkCompact();
    }
    handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
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
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, ContextDisposeHighRate) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate - 1;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_FULL_GC, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeZeroIdleTime) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate = 1.0;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_FULL_GC, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate = 1.0;
  heap_state.incremental_marking_stopped = true;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate = 1.0;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, IncrementalMarking1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  size_t speed = heap_state.incremental_marking_speed_in_bytes_per_ms;
  double idle_time_ms = 10;
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    EXPECT_GT(speed * static_cast<size_t>(idle_time_ms),
              static_cast<size_t>(action.parameter));
    EXPECT_LT(0, action.parameter);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, IncrementalMarking2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  size_t speed = heap_state.incremental_marking_speed_in_bytes_per_ms;
  double idle_time_ms = 10;
  for (int mode = 0; mode < 1; mode++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    EXPECT_GT(speed * static_cast<size_t>(idle_time_ms),
              static_cast<size_t>(action.parameter));
    EXPECT_LT(0, action.parameter);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, NotEnoughTime) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_NOTHING, action.type);
  TransitionToReduceMemoryMode(heap_state);
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, FinalizeSweeping) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  for (int mode = 0; mode < 1; mode++) {
    heap_state.sweeping_in_progress = true;
    heap_state.sweeping_completed = true;
    double idle_time_ms = 10.0;
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_FINALIZE_SWEEPING, action.type);
    heap_state.sweeping_in_progress = false;
    heap_state.sweeping_completed = false;
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, CannotFinalizeSweeping) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  for (int mode = 0; mode < 1; mode++) {
    heap_state.sweeping_in_progress = true;
    heap_state.sweeping_completed = false;
    double idle_time_ms = 10.0;
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
    heap_state.sweeping_in_progress = false;
    heap_state.sweeping_completed = false;
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, Scavenge) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  int idle_time_ms = 10;
  for (int mode = 0; mode < 1; mode++) {
    heap_state.used_new_space_size =
        heap_state.new_space_capacity -
        (kNewSpaceAllocationThroughput * idle_time_ms);
    GCIdleTimeAction action =
        handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
    EXPECT_EQ(DO_SCAVENGE, action.type);
    heap_state.used_new_space_size = 0;
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, ScavengeAndDone) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  int idle_time_ms = 10;
  heap_state.can_start_incremental_marking = false;
  heap_state.incremental_marking_stopped = true;
  for (int mode = 0; mode < 1; mode++) {
    heap_state.used_new_space_size =
        heap_state.new_space_capacity -
        (kNewSpaceAllocationThroughput * idle_time_ms);
    GCIdleTimeAction action =
        handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
    EXPECT_EQ(DO_SCAVENGE, action.type);
    heap_state.used_new_space_size = 0;
    action = handler()->Compute(static_cast<double>(idle_time_ms), heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
    TransitionToReduceMemoryMode(heap_state);
  }
}


TEST_F(GCIdleTimeHandlerTest, StopEventually1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
  bool stopped = false;
  for (int i = 0; i < kMaxNotifications && !stopped; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    if (action.type == DO_INCREMENTAL_MARKING || action.type == DO_FULL_GC) {
      handler()->NotifyMarkCompact();
      handler()->NotifyIdleMarkCompact();
    }
    if (action.type == DONE) stopped = true;
  }
  EXPECT_TRUE(stopped);
}


TEST_F(GCIdleTimeHandlerTest, StopEventually2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed + 1);
  TransitionToReduceMemoryMode(heap_state);
  TransitionToDoneMode(heap_state, idle_time_ms, DO_FULL_GC);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, StopEventually3) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = 10;
  TransitionToReduceMemoryMode(heap_state);
  TransitionToDoneMode(heap_state, idle_time_ms, DO_INCREMENTAL_MARKING);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ContinueAfterStop1) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  size_t speed = heap_state.mark_compact_speed_in_bytes_per_ms;
  double idle_time_ms =
      static_cast<double>(heap_state.size_of_objects / speed + 1);
  TransitionToReduceMemoryMode(heap_state);
  TransitionToDoneMode(heap_state, idle_time_ms, DO_FULL_GC);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
  TransitionToReduceLatencyMode(heap_state);
  heap_state.can_start_incremental_marking = true;
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  EXPECT_FALSE(action.reduce_memory);
  EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
}


TEST_F(GCIdleTimeHandlerTest, ContinueAfterStop2) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = 10;
  TransitionToReduceMemoryMode(heap_state);
  TransitionToDoneMode(heap_state, idle_time_ms, DO_INCREMENTAL_MARKING);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
  TransitionToReduceLatencyMode(heap_state);
  heap_state.can_start_incremental_marking = true;
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  EXPECT_FALSE(action.reduce_memory);
  EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
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
  heap_state.can_start_incremental_marking = false;
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(10, heap_state);
    EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
  }
}


TEST_F(GCIdleTimeHandlerTest, StayInReduceLatencyModeBecauseOfScavenges) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
  int limit = GCIdleTimeHandler::kLongIdleNotificationsBeforeMutatorIsIdle;
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
    if ((i + 1) % limit == 0) handler()->NotifyScavenge();
    EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
  }
}


TEST_F(GCIdleTimeHandlerTest, StayInReduceLatencyModeBecauseOfMarkCompacts) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
  int limit = GCIdleTimeHandler::kLongIdleNotificationsBeforeMutatorIsIdle;
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
    if ((i + 1) % limit == 0) handler()->NotifyMarkCompact();
    EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
  }
}


TEST_F(GCIdleTimeHandlerTest, ReduceMemoryToReduceLatency) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
  int limit = GCIdleTimeHandler::kMaxIdleMarkCompacts;
  for (int idle_gc = 0; idle_gc < limit; idle_gc++) {
    TransitionToReduceMemoryMode(heap_state);
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    EXPECT_TRUE(action.reduce_memory);
    EXPECT_EQ(GCIdleTimeHandler::kReduceMemory, handler()->mode());
    for (int i = 0; i < idle_gc; i++) {
      action = handler()->Compute(idle_time_ms, heap_state);
      EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
      EXPECT_TRUE(action.reduce_memory);
      // ReduceMemory mode should tolerate one mutator GC per idle GC.
      handler()->NotifyScavenge();
      // Notify idle GC.
      handler()->NotifyMarkCompact();
      handler()->NotifyIdleMarkCompact();
    }
    // Transition to ReduceLatency mode after doing |idle_gc| idle GCs.
    handler()->NotifyScavenge();
    action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
    EXPECT_FALSE(action.reduce_memory);
    EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
  }
}


TEST_F(GCIdleTimeHandlerTest, ReduceMemoryToDone) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = GCIdleTimeHandler::kMinLongIdleTime;
  int limit = GCIdleTimeHandler::kMaxIdleMarkCompacts;
  TransitionToReduceMemoryMode(heap_state);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  EXPECT_TRUE(action.reduce_memory);
  for (int i = 0; i < limit; i++) {
    action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
    EXPECT_TRUE(action.reduce_memory);
    EXPECT_EQ(GCIdleTimeHandler::kReduceMemory, handler()->mode());
    // ReduceMemory mode should tolerate one mutator GC per idle GC.
    handler()->NotifyScavenge();
    // Notify idle GC.
    handler()->NotifyMarkCompact();
    handler()->NotifyIdleMarkCompact();
  }
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, DoneIfNotMakingProgressOnSweeping) {
  // Regression test for crbug.com/489323.
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();

  // Simulate sweeping being in-progress but not complete.
  heap_state.incremental_marking_stopped = true;
  heap_state.can_start_incremental_marking = false;
  heap_state.sweeping_in_progress = true;
  heap_state.sweeping_completed = false;
  double idle_time_ms = 10.0;
  for (int i = 0; i < GCIdleTimeHandler::kMaxNoProgressIdleTimesPerMode; i++) {
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
  heap_state.can_start_incremental_marking = false;
  double idle_time_ms = 10.0;
  for (int i = 0; i < GCIdleTimeHandler::kMaxNoProgressIdleTimesPerMode; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
  }
  // We should return DONE after not making progress for some time.
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, BackgroundReduceLatencyToReduceMemory) {
  GCIdleTimeHandler::HeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = false;
  heap_state.can_start_incremental_marking = true;
  double idle_time_ms = GCIdleTimeHandler::kMinBackgroundIdleTime;
  handler()->NotifyScavenge();
  EXPECT_EQ(GCIdleTimeHandler::kReduceLatency, handler()->mode());
  int limit =
      GCIdleTimeHandler::kBackgroundIdleNotificationsBeforeMutatorIsIdle;
  for (int i = 0; i < limit; i++) {
    GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
    EXPECT_EQ(DO_INCREMENTAL_MARKING, action.type);
  }
  handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(GCIdleTimeHandler::kReduceMemory, handler()->mode());
}

}  // namespace internal
}  // namespace v8

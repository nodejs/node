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

  GCIdleTimeHeapState DefaultHeapState() {
    GCIdleTimeHeapState result;
    result.contexts_disposed = 0;
    result.contexts_disposal_rate = GCIdleTimeHandler::kHighContextDisposalRate;
    result.incremental_marking_stopped = false;
    return result;
  }

  static const size_t kSizeOfObjects = 100 * MB;
  static const size_t kMarkCompactSpeed = 200 * KB;
  static const size_t kMarkingSpeed = 200 * KB;
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
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_NOTHING, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ContextDisposeHighRate) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate - 1;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_FULL_GC, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeZeroIdleTime) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate = 1.0;
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_FULL_GC, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime1) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate;
  size_t speed = kMarkCompactSpeed;
  double idle_time_ms = static_cast<double>(kSizeOfObjects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_STEP, action.type);
}


TEST_F(GCIdleTimeHandlerTest, AfterContextDisposeSmallIdleTime2) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.contexts_disposed = 1;
  heap_state.contexts_disposal_rate =
      GCIdleTimeHandler::kHighContextDisposalRate;
  size_t speed = kMarkCompactSpeed;
  double idle_time_ms = static_cast<double>(kSizeOfObjects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_STEP, action.type);
}


TEST_F(GCIdleTimeHandlerTest, IncrementalMarking1) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  double idle_time_ms = 10;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_STEP, action.type);
}


TEST_F(GCIdleTimeHandlerTest, NotEnoughTime) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  size_t speed = kMarkCompactSpeed;
  double idle_time_ms = static_cast<double>(kSizeOfObjects / speed - 1);
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, DoNotStartIncrementalMarking) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ContinueAfterStop) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
  heap_state.incremental_marking_stopped = false;
  action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DO_INCREMENTAL_STEP, action.type);
}


TEST_F(GCIdleTimeHandlerTest, ZeroIdleTimeNothingToDo) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(0, heap_state);
    EXPECT_EQ(DO_NOTHING, action.type);
  }
}


TEST_F(GCIdleTimeHandlerTest, SmallIdleTimeNothingToDo) {
  GCIdleTimeHeapState heap_state = DefaultHeapState();
  heap_state.incremental_marking_stopped = true;
  for (int i = 0; i < kMaxNotifications; i++) {
    GCIdleTimeAction action = handler()->Compute(10, heap_state);
    EXPECT_TRUE(DO_NOTHING == action.type || DONE == action.type);
  }
}


TEST_F(GCIdleTimeHandlerTest, DoneIfNotMakingProgressOnIncrementalMarking) {
  // Regression test for crbug.com/489323.
  GCIdleTimeHeapState heap_state = DefaultHeapState();

  // Simulate incremental marking stopped and not eligible to start.
  heap_state.incremental_marking_stopped = true;
  double idle_time_ms = 10.0;
  // We should return DONE if we cannot start incremental marking.
  GCIdleTimeAction action = handler()->Compute(idle_time_ms, heap_state);
  EXPECT_EQ(DONE, action.type);
}

}  // namespace internal
}  // namespace v8
